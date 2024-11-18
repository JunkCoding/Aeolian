/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This is a read-only filesystem that uses a sorted hash table to locate
 * objects in a monolithic binary. The binary is generated by the mkespfsimg
 * tool that comes with this source distribution.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <esp_partition.h>
#include <esp_flash_spi_init.h>
#include <spi_flash_mmap.h>
#include "espfs_priv.h"
#include "espfs_log.h"
#include "espfs_format.h"
#include "espfs.h"

#include "heatshrink_decoder.h"

espfs_fs_t* espfs_init (espfs_config_t* conf)
{
  espfs_fs_t* fs = pvPortMalloc (sizeof (espfs_fs_t));
  if ( fs == NULL )
  {
    ESPFS_LOGE (__func__, "pvPortMalloc failed allocating 'fs'");
    return NULL;
  }
  memset (fs, 0, sizeof (espfs_fs_t));

  ESPFS_LOGV (__func__, "%p", fs);

  fs->header = ( espfs_fs_header_t* )conf->addr;
  if ( fs->header == NULL )
  {
#if defined (ESP_PLATFORM)
    esp_partition_subtype_t subtype = conf->part_label?
      ESP_PARTITION_SUBTYPE_ANY:
      ESP_PARTITION_SUBTYPE_DATA_ESPHTTPD;
    const esp_partition_t* partition = esp_partition_find_first (ESP_PARTITION_TYPE_DATA, subtype, conf->part_label);

    if ( partition == NULL )
    {
      ESPFS_LOGE (__func__, "unable to find espfs partition");
      goto err_out;
    }

    if ( esp_partition_mmap (partition, 0, partition->size,
      SPI_FLASH_MMAP_DATA, ( const void** )&fs->header,
      &fs->mmap_handle) != ESP_OK )
    {
      ESPFS_LOGE (__func__, "mmap failed");
      goto err_out;
    }
#else
    ESPFS_LOGE (__func__, "flash mmap not enabled and addr is NULL");
    goto err_out;
#endif
  }

  if ( fs->header->magic != ESPFS_MAGIC )
  {
    ESPFS_LOGE (__func__, "magic not found");
    ESPFS_LOGE (__func__, "%08X <> %08X", fs->header->magic, ESPFS_MAGIC);
    //ESPFS_LOGE(__func__, "%08X ", &fs->header->magic);
    goto err_out;
  }

  if ( fs->header->version_major != ESPFS_VERSION_MAJOR )
  {
    ESPFS_LOGE (__func__, "espfs version %d.%d not supported",
            fs->header->version_major, fs->header->version_minor);
    goto err_out;
  }

  fs->hashtable = ( const void* )fs->header + fs->header->len;
  fs->sorttable = ( const void* )fs->hashtable + (sizeof (espfs_hashtable_entry_t) * fs->header->num_objects);

  return fs;

err_out:
  espfs_deinit (fs);
  return NULL;
}

void espfs_deinit (espfs_fs_t* fs)
{
  ESPFS_LOGV (__func__, "%p", fs);

#if defined(ESP_PLATFORM)
  if ( fs->mmap_handle )
  {
    spi_flash_munmap (fs->mmap_handle);
  }
#endif
  vPortFree (fs);
}

const char* espfs_get_path (espfs_fs_t* fs, uint16_t index)
{
  assert (fs != NULL);

  if ( index >= fs->header->num_objects )
  {
    return NULL;
  }

  const espfs_sorttable_entry_t* entry = fs->sorttable + index;
  const espfs_object_header_t* object = ( const void* )fs->header +
    entry->offset;
  return ( const char* )object + object->len;
}

static uint32_t hash_path (const char* path)
{
  uint32_t hash = 5381;
  const uint8_t* p = ( uint8_t* )path;

  while ( *p )
  {
/* hash = hash * 33 + *p */
    hash = (hash << 5) + hash + *p;
    p++;
  }

  return hash;
}

static const void* find_object (espfs_fs_t* fs, const char* path)
{
  assert (fs != NULL);

  while ( *path == '/' )
  {
    path++;
  }
  ESPFS_LOGV (__func__, "%s", path);

  uint32_t hash = hash_path (path);
  ESPFS_LOGV (__func__, "hash %08x", (unsigned int)hash);

  int first = 0;
  int last = fs->header->num_objects - 1;
  int middle;
  const espfs_hashtable_entry_t* entry;

  while ( first <= last )
  {
    middle = first + (last - first) / 2;
    entry = fs->hashtable + middle;
    if ( entry->hash == hash )
    {
      break;
    }
    else if ( entry->hash < hash )
    {
      first = middle + 1;
    }
    else
    {
      last = middle - 1;
    }
  }

  if ( first > last )
  {
    ESPFS_LOGV (__func__, "no match");
    return NULL;
  }

  /* be optimistic and test the first match */
  espfs_object_header_t* object = ( void* )fs->header + entry->offset;
  if ( strcmp (path, ( char* )object + object->len) == 0 )
  {
    ESPFS_LOGV (__func__, "object %d", middle);
    return object;
  }

  /* hash collision, move entry to the first match */
  ESPFS_LOGV (__func__, "hash collision");
  int skip = middle;
  while ( middle > 0 )
  {
    entry = fs->hashtable + middle;
    if ( (entry - 1)->hash != hash )
    {
      break;
    }
    middle--;
  }

  /* walk through canidates and look for a match */
  do
  {
    if ( middle != skip )
    {
      object = ( void* )fs->header + entry->offset;
      if ( strcmp (path, ( const char* )object + object->len) == 0 )
      {
        ESPFS_LOGV (__func__, "object %d", middle);
        return object;
      }
    }
    entry++;
    middle++;
  }
  while ( (middle < fs->header->num_objects) && (entry->hash == hash) );

  ESPFS_LOGW (__func__, "unable to find object");
  return NULL;
}

bool espfs_stat (espfs_fs_t* fs, const char* path, espfs_stat_t* stat)
{
  assert (fs != NULL);

  const espfs_object_header_t* object = find_object (fs, path);
  if ( object == NULL )
  {
    ESPFS_LOGD (__func__, "object not found: %s", path);
    return false;
  }

  memset (stat, 0, sizeof (espfs_stat_t));
  stat->type = object->type;
  stat->index = object->index;
  if ( object->type == ESPFS_TYPE_FILE )
  {
    const espfs_file_header_t* fh = ( const espfs_file_header_t* )object;
    stat->flags = fh->flags;
    stat->compression = fh->compression;
    stat->size = fh->file_len;
  }

  return true;
}

// Open a file and return a pointer to the file desc struct.
espfs_file_t* espfs_fopen (espfs_fs_t* fs, const char* path)
{
  assert (fs != NULL);

  const espfs_object_header_t* object = find_object (fs, path);
  if ( (object == NULL) || (object->type != ESPFS_TYPE_FILE) )
  {
    ESPFS_LOGD (__func__, "file not found: %s", path);
    return NULL;
  }

  const espfs_file_header_t* fh = ( const espfs_file_header_t* )object;

  espfs_file_t* f = pvPortMalloc (sizeof (espfs_file_t));
  if ( f == NULL )
  {
    ESPFS_LOGE (__func__, "pvPortMalloc failed allocating 'f'");
    goto err_out;
  }
  memset (f, 0, sizeof (espfs_file_t));

  ESPFS_LOGV (__func__, "%p", f);

  f->fs = fs;
  f->fh = fh;

  if ( fh->compression == ESPFS_COMPRESSION_NONE )
  {
    f->raw_start = ( void* )object + object->len + object->path_len;
    f->raw_ptr = f->raw_start;
    f->raw_len = fh->data_len;
  }
  else

    if ( f->fh->compression == ESPFS_COMPRESSION_HEATSHRINK )
    {
/* defer initialization until use */
    }
    else
    {
      ESPFS_LOGE (__func__, "unrecognized compression type %d", fh->compression);
      goto err_out;
    }

  return f;

err_out:
  espfs_fclose (f);
  return NULL;
}

// Close the file.
void espfs_fclose (espfs_file_t* f)
{
  if ( f == NULL )
  {
/* do nothing */
    return;
  }

  ESPFS_LOGV (__func__, "%p", f);


  if ( f->fh->compression == ESPFS_COMPRESSION_HEATSHRINK )
  {
    heatshrink_decoder* hsd = f->user;
    if ( hsd != NULL )
    {
      ESPFS_LOGV (__func__, "heatshrink_decoder_free");
      heatshrink_decoder_free (hsd);
    }
  }

  free (f);
}

void espfs_fstat (espfs_file_t* f, espfs_stat_t* stat)
{
  assert (f != NULL);

  memset (stat, 0, sizeof (espfs_stat_t));
  stat->type = f->fh->object.type;
  stat->index = f->fh->object.index;
  stat->flags = f->fh->flags;
  stat->compression = f->fh->compression;
  stat->size = f->fh->file_len;
}

// Read len bytes from the given file into buf. Returns the actual amount of bytes read.
ssize_t espfs_fread (espfs_file_t* f, void* buf, size_t len)
{
  assert (f != NULL);

  if ( f->fh->compression == ESPFS_COMPRESSION_NONE )
  {
    size_t remaining = f->fh->file_len - (f->raw_ptr - f->raw_start);
    if ( len > remaining )
    {
      len = remaining;
    }
    memcpy (buf, f->raw_ptr, len);
    f->file_pos += len;
    f->raw_ptr += len;
    return len;
  }

  if ( f->fh->compression == ESPFS_COMPRESSION_HEATSHRINK )
  {
    size_t decoded = 0;
    size_t rlen;

    if ( f->file_pos >= f->fh->file_len )
    {
      return 0;
    }

    heatshrink_decoder* hsd = ( heatshrink_decoder* )f->user;
    if ( hsd == NULL )
    {
      espfs_heatshrink_header_t* hsh = ( void* )&f->fh->object +
        f->fh->object.len + f->fh->object.path_len;
      ESPFS_LOGV (__func__, "heatshrink_decoder_alloc");
      hsd = heatshrink_decoder_alloc (16, hsh->window_sz2,
              hsh->lookahead_sz2);
      if ( hsd == NULL )
      {
        ESPFS_LOGE (__func__, "heatshrink_decoder_alloc");
        return -1;
      }

      f->user = hsd;
      f->raw_start = ( void* )hsh + sizeof (espfs_heatshrink_header_t);
      f->raw_ptr = f->raw_start;
      f->raw_len = f->fh->data_len - sizeof (espfs_heatshrink_header_t);
    }

    while ( decoded < len )
    {
/* feed data into the decoder */
      size_t remain = f->raw_len - (f->raw_ptr - f->raw_start);
      if ( remain > 0 )
      {
        HSD_sink_res res = heatshrink_decoder_sink (hsd, f->raw_ptr,
                (remain > 16)?16:remain, &rlen);
        if ( res < 0 )
        {
          ESPFS_LOGE (__func__, "heatshrink_decoder_sink");
          return -1;
        }
        f->raw_ptr += rlen;
      }

      HSD_poll_res res = heatshrink_decoder_poll (hsd, ( uint8_t* )buf,
              len - decoded, &rlen);
      if ( res < 0 )
      {
        ESPFS_LOGE (__func__, "heatshrink_decoder_poll");
        return -1;
      }
      f->file_pos += rlen;
      buf += rlen;
      decoded += rlen;

      if ( remain == 0 )
      {
        if ( f->file_pos == f->fh->file_len )
        {
          HSD_finish_res res = heatshrink_decoder_finish (hsd);
          if ( res < 0 )
          {
            ESPFS_LOGE (__func__, "heatshrink_decoder_finish");
            return -1;
          }
          ESPFS_LOGV (__func__, "heatshrink_decoder_finish");
        }
        return decoded;
      }
    }
    return len;
  }

  return -1;
}

// Seek in the file.
ssize_t espfs_fseek (espfs_file_t* f, long offset, int mode)
{
  assert (f != NULL);

  uint32_t new_pos = f->file_pos;

  if ( mode == SEEK_SET )
  {
    if ( offset < 0 )
    {
      return -1;
    }
    else if ( offset == 0 )
    {
      new_pos = 0;
    }
    else
    {
      if ( offset > f->fh->file_len )
      {
        offset = f->fh->file_len;
      }
      new_pos = offset;
    }
  }
  else if ( mode == SEEK_CUR )
  {
    if ( offset < 0 )
    {
      if ( ( long )f->file_pos + offset < 0 )
      {
        new_pos = 0;
      }
      else
      {
        new_pos += offset;
      }
    }
    else if ( offset > 0 )
    {
      if ( f->file_pos + offset > f->fh->file_len )
      {
        new_pos = f->fh->file_len;
      }
      else
      {
        new_pos += offset;
      }
    }
  }
  else if ( mode == SEEK_END )
  {
    if ( offset < 0 )
    {
      if ( ( long )f->fh->file_len + offset < 0 )
      {
        new_pos = 0;
      }
      else
      {
        new_pos = f->fh->file_len + offset;
      }
    }
    else if ( offset == 0 )
    {
      new_pos = f->fh->file_len;
    }
    else
    {
      return -1;
    }
  }
  else
  {
    return -1;
  }

  if ( f->fh->compression == ESPFS_COMPRESSION_NONE )
  {
    f->file_pos = new_pos;
    f->raw_ptr = f->raw_start + new_pos;
  }

  if ( f->fh->compression == ESPFS_COMPRESSION_HEATSHRINK )
  {
    heatshrink_decoder* hsd = ( heatshrink_decoder* )f->user;

    if ( new_pos < f->file_pos )
    {
      if ( hsd != NULL )
      {
        ESPFS_LOGV (__func__, "heatshrink_decoder_reset");
        heatshrink_decoder_reset (hsd);
      }
      f->file_pos = 0;
    }

    if ( f->file_pos == 0 )
    {
      f->raw_ptr = f->raw_start;
    }

    if ( new_pos == f->fh->file_len )
    {
      f->file_pos = new_pos;
      f->raw_ptr = f->raw_start + f->raw_len;
      return f->file_pos;
    }

    while ( new_pos > f->file_pos )
    {
      uint8_t buf[16];
      espfs_fread (f, buf, sizeof (buf));
    }
  }

  return f->file_pos;
}

size_t espfs_ftell (espfs_file_t* f)
{
  assert (f != NULL);

  return f->file_pos;
}

ssize_t espfs_faccess (espfs_file_t* f, void** buf)
{
  assert (f != NULL);

  if ( f->fh->compression != ESPFS_COMPRESSION_NONE )
  {
    return -1;
  }
  *buf = f->raw_start;
  return f->fh->file_len;
}
