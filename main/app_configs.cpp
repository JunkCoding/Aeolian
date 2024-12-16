// --------------------------------------------------------------------------
// debug support
// --------------------------------------------------------------------------



// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#include <stdio.h>  // printf()
#include <string.h> // memcpy()

#include <esp_types.h>
#include <esp_flash.h>
#include <esp_partition.h>

#include <lwip/ip_addr.h> // IP4_ADDR()

#include "app_configs.h"
#include "app_main.h"
#include "app_utils.h"

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#define EXTRA_BLOCKS_AT_END 1

#define CFG_DATA_NUM_BLOCKS 3
#define CFG_DATA_START_SEC (INITDATAPOS / SPI_FLASH_SEC_SIZE - (CFG_DATA_NUM_BLOCKS + EXTRA_BLOCKS_AT_END))

#define CFG_DATA_START_ADDR (INITDATAPOS - (CFG_DATA_NUM_BLOCKS + EXTRA_BLOCKS_AT_END) * SPI_FLASH_SEC_SIZE)
#define CFG_START_MARKER 0xABCDABCD
//#define CFG_START_MARKER 0xDEADBEEF

#define NO_DATA_AVAILABLE -1
#define NO_WRITE_BLOCK_AVAILABLE -2
#define NO_MEMORY_AVAILABLE -3

#define MARKER_SIZE 2
#define TMP_BUFSIZE 128

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static int align_len (int buf_size, int* len);
static int _config_get_defaults (const_settings_t* defaults);
static int _config_get_user (void);
static int user_config_get_start (void);
static int user_config_check_integrity (void);

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

typedef struct
{
  uint32_t* buf;
  uint32_t start; // 8 .. 0x2FFF; start address of record in flash, offset to CFG_DATA_START_ADDR
  uint32_t write; // 8 .. 0x2FFF; address for the next record to write
} user_settings_t;

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

user_settings_t user_settings = {NULL, 0, 0};

static settings_t* config_list;


// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

bool str2ip (const char* str, void* ip)
{
  int i;
  const char* start;

  start = str;
  for (i = 0; i < 4; i++)
  {
    /* The digit being processed.  */
    char c;
    /* The value of this byte.  */
    int n = 0;
    while (true)
    {
      c = *start;
      start++;
      if (c >= '0' && c <= '9')
      {
        n *= 10;
        n += c - '0';
      }
      /*
       * We insist on stopping at "." if we are still parsing
       * the first, second, or third numbers. If we have reached
       * the end of the numbers, we will allow any character.
       */
      else if ((i < 3 && c == '.') || i == 3)
      {
        break;
      }
      else
      {
        return 0;
      }
    }

    if (n >= 256)
    {
      return 0;
    }

    ((uint8_t*)ip)[i] = n;
  }

  return 1;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
int str2int_array (char* str, int* array, int num_values)
{
  // const char str[] = "comma separated,input,,,some fields,,empty";
  const char delims[] = ",";
  int cnt = 0;
  int value = 0;

  do
  {
    if (array != NULL && cnt >= num_values)
    {
      break;
    }
    size_t len = strcspn (str, delims);
    F_LOGV(true, true, LC_GREY, "\"%.*s\" - %d", (int)len, str, (int)len);

    if (len > 0)
    {
      value = strtol (str, NULL, 0);
      F_LOGV(true, true, LC_GREY, "%3d %8d", cnt, value);

      if (array != NULL)
      {
        array[cnt] = value;
      }

      cnt++;
    }

    str += len;
  }

  while (*str++);

  return cnt;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// if len is equal or greater than bufsize,
//    len will be one less for the terminating zero

static int align_len (int buf_size, int* len)
{
  if (*len >= buf_size) // space for the terminating zero?
  {
    *len = buf_size - 1; // no: make it
  }

  int len4 = (*len + 3) & ~3;

  return len4;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

static Configuration_List_t* Configuration_List = NULL;
static int Num_Configurations = 0;
static int Num_Keywords = 0;

void _config_build_list (Configuration_List_t* cfg_list, int num_cfgs)
{
  config_list = (settings_t*)pvPortMalloc (sizeof (settings_t) * ID_MAX);
  if ( config_list == NULL )
  {
    F_LOGE(true, true, LC_RED, "pvPortMalloc failed allocating 'config_list' (%d bytes)", (sizeof (settings_t) * ID_MAX));
    return;
  }
  Configuration_List = cfg_list;
  Num_Configurations = num_cfgs;

  F_LOGV(true, true, LC_GREY, "_config_build_list()");

  // first do the default values
  for (int i = 0; i < num_cfgs; i++)
  {
    Num_Keywords += cfg_list[i]->num_keywords;

    const_settings_t* defaults = cfg_list[i]->defaults;
    if (defaults != NULL)
    {
      F_LOGV(true, true, LC_GREY, "i = %d, num_cfgs = %d", i, num_cfgs);
      _config_get_defaults (defaults);
    }
  }

  // parse the list stored in the configuration section of the flash
  _config_get_user ();
}

Configuration_List_t* get_configuration_list (void)
{
  return Configuration_List;
}

int get_num_configurations (void)
{
  return Num_Configurations;
}
int get_num_keywords (void)
{
  return Num_Keywords;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
static int _config_get_defaults (const_settings_t* defaults)
{
  F_LOGV(true, true, LC_GREY, "_config_get_defaults");

  int i = 0;
  while (true)
  {
    cfg_mode_t cfg_mode __attribute__ ((aligned (4)));

    if ( ((uint32_t)(&defaults[i].mode) & 3) != 0 )
    {
      F_LOGE(true, true, LC_BRIGHT_RED, "Src isn't 32bit aligned");
    }
    else if ( ((uint32_t)(&cfg_mode) & 3) != 0 )
    {
      F_LOGE(true, true, LC_BRIGHT_RED, "Dest isn't 32bit aligned");
    }
    else
    {
      memcpy (&cfg_mode, &defaults[i].mode, sizeof (cfg_mode));

      if ( cfg_mode.mode == 0xFFFFFFFF ) // end of list
      {
        return true;
      }
      else if ( cfg_mode.valid >= RECORD_VALID && cfg_mode.id < ID_MAX )
      {
        if ( cfg_mode.type == TEXT )
        {
          // copy mode and pointer to the text
          if ( ((uint32_t)(&defaults[i]) & 3) != 0 )
          {
            F_LOGE(true, true, LC_BRIGHT_RED, "Src isn't 32bit aligned");
          }
          else if ( ((uint32_t)(&config_list[cfg_mode.id]) & 3) != 0 )
          {
            F_LOGE(true, true, LC_BRIGHT_RED, "Dest isn't 32bit aligned");
          }
          else
          {
            memcpy (&config_list[cfg_mode.id], &defaults[i], sizeof (settings_t));
          }
        }
        else if ( cfg_mode.type == NUMARRAY )
        {
          // alloate memory for the text/string from the defaults
          // len is the number of bytes of the string with the values
          int len = cfg_mode.len;
          int len4 = (len + 3) & ~3;

          char *buf = (char *)pvPortMalloc (len4);
          if ( buf == NULL )
          {
            F_LOGE(true, true, LC_RED, "pvPortMalloc failed allocating 'buf' (%d bytes)", len4);
          }
          else
          {
            if ( ((uint32_t)defaults[i].text & 3) != 0 )
            {
              F_LOGE(true, true, LC_BRIGHT_RED, "Src isn't 32bit aligned");
            }
            else if ( ((uint32_t)buf & 3) != 0 )
            {
              F_LOGE(true, true, LC_BRIGHT_RED, "Dest isn't 32bit aligned");
            }
            else
            {
              memcpy (buf, defaults[i].text, len4);
              buf[len] = 0; // terminate the string

              // determine the number of integers in the text string
              int cnt = str2int_array (buf, NULL, 0);
              F_LOGV(true, true, LC_GREY, "NumArray has %d elements", cnt);

              if ( cnt > 0 )
              {
                // allocate memory to store the array of integers
                // check if memory is available for the integer and it's big enough
                int *val_array = (int *)config_list[cfg_mode.id].text;
                if ( val_array != NULL )
                {
                  int size_array = config_list[cfg_mode.id].len;
                  if ( cnt != size_array )
                  {
                    // not enough space to store the values into array
                    vPortFree (val_array);
                    val_array = NULL;
                  }
                }

                if ( val_array == NULL )
                {
                  val_array = (int *)pvPortMalloc (sizeof (int) * cnt);
                  if ( val_array == NULL )
                  {
                    F_LOGE(true, true, LC_YELLOW, "pvPortMalloc failed allocating 'val_array' (%d bytes)", (sizeof (int) * cnt));
                  }
                }

                if ( val_array != NULL )
                {
                  str2int_array (buf, val_array, cnt);
                }
                else
                {
                  cnt = 0;
                }

                cfg_mode.len = sizeof (uint32_t) * cnt;
                config_list[cfg_mode.id].mode = cfg_mode.mode;
                config_list[cfg_mode.id].text = (char *)val_array;

                vPortFree (buf);
                buf = NULL;
              }
              else
              {
                // no values in array
                cfg_mode.len = 0;
                config_list[cfg_mode.id].mode = cfg_mode.mode;
                config_list[cfg_mode.id].text = NULL;
              }
            }
          }
        }
        else
        {
          char buf[TMP_BUFSIZE + 1] __attribute__ ((aligned (4))) = {};

          uint32_t val = 0;
          int len = cfg_mode.len;
          int len4 = align_len (TMP_BUFSIZE, &len);

          if ( ((uint32_t)defaults[i].text & 3) != 0 )
          {
            F_LOGE(true, true, LC_BRIGHT_RED, "Src isn't 32bit aligned");
          }
          else if ( ((uint32_t)buf & 3) != 0 )
          {
            F_LOGE(true, true, LC_BRIGHT_RED, "Dest isn't 32bit aligned");
          }
          else
          {
            memcpy (buf, defaults[i].text, len4);
            buf[len] = 0; // zero terminate string

            if ( cfg_mode.type == NUMBER )
            {
              val = str2int (buf);
            }
            else if ( cfg_mode.type == IP_ADDR )
            {
              str2ip (buf, &val);
            }
            else if ( cfg_mode.type == FLAG )
            {
              val = str2int (buf);

              if ( val != 0 )
              {
                val = 1;
              }
            }
            else
            {
              val = 0;
            }

            cfg_mode.len = sizeof (uint32_t); // because value is not a string
            config_list[cfg_mode.id].mode = cfg_mode.mode;
            config_list[cfg_mode.id].val = val;
          }
        }
      }
      else
      {
        F_LOGE(true, true, LC_RED, "Something went wrong");
      }

      i++;
    }
  }
  return false;
}

// --------------------------------------------------------------------------
// copy the configuration string into the callers buffer
// terminate the string with zero
// --------------------------------------------------------------------------
int _config_get (int id, char* buf, int buf_size)
{
  if (id == config_list[id].id && config_list[id].valid >= RECORD_VALID)
  {
    int len = config_list[id].len;
    int len4 = align_len (buf_size, &len);

    F_LOGV(true, true, LC_GREY, "buf = %p, config_list[id].text = %p, len = %d, len4 = %d, buf_size = %d, id = %x", buf, config_list[id].text, len, len4, buf_size, id);

    if (buf)
    {
      if ( ((uint32_t)buf & 3) != 0 )
      {
        F_LOGE(true, true, LC_BRIGHT_RED, "Src isn't 32bit aligned");
      }
      else if ( ((uint32_t)config_list[id].text & 3) != 0 )
      {
        F_LOGE(true, true, LC_BRIGHT_RED, "Dest isn't 32bit aligned");
      }
      else
      {
        if (config_list[id].valid == DEFAULT_RECORD)
        {
          memcpy (buf, config_list[id].text, len4);
        }
        else if (config_list[id].valid == SPI_FLASH_RECORD)
        {
          user_config_read ((uint32_t)config_list[id].text, buf, len4);
        }
        else
        {
          len = 0;
        }

        buf[len] = 0;
        return len;
      }
    }
  }

  F_LOGE(true, true, LC_RED, "_config_get() id 0x%02x FAILED", id);

  return -1;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
int _config_get_int (int id, int* val)
{
  if (id == config_list[id].id && config_list[id].valid >= RECORD_VALID)
  {
    if (config_list[id].type == NUMBER || config_list[id].type == FLAG)
    {
      *val = config_list[id].val;
      return 1;
    }
  }

  F_LOGE(true, true, LC_RED, "_config_get_int() id 0x%02x FAILED", id);

  return 0;
}
int _config_get_bool (int id, bool* val)
{
  if (id == config_list[id].id && config_list[id].valid >= RECORD_VALID)
  {
    if (config_list[id].type == NUMBER || config_list[id].type == FLAG)
    {
      *val = config_list[id].val;
      return 1;
    }
  }

  F_LOGE(true, true, LC_RED, "_config_get_bool() id 0x%02x FAILED", id);

  return 0;
}
int _config_get_uint8 (int id, uint8_t* val)
{
  if (id == config_list[id].id && config_list[id].valid >= RECORD_VALID)
  {
    if (config_list[id].type == NUMBER || config_list[id].type == FLAG)
    {
      *val = config_list[id].val;
      return 1;
    }
  }

  F_LOGV(true, true, LC_RED, "_config_get_uint8() id 0x%02x FAILED", id);

  return 0;
}


// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
void _config_print_settings (void)
{
  F_LOGV(true, true, LC_GREY, "(ID_MAX = %d)", ID_MAX);

  int i;

  for (i = 0; i < ID_MAX; i++)
  {
    cfg_mode_t cfg_mode;
    uint32_t val;

    cfg_mode.mode = config_list[i].mode;

    if (cfg_mode.id != 0 && cfg_mode.valid >= RECORD_VALID)
    {
      if (cfg_mode.type == TEXT || cfg_mode.type == NUMARRAY)
      {
        char buf[TMP_BUFSIZE + 1] __attribute__ ((aligned (4))) = {};
        int buf_size = TMP_BUFSIZE;
        int len = cfg_mode.len;
        int len4 = align_len (buf_size, &len);

        if ( ((uint32_t)config_list[i].text & 3) != 0 )
        {
          F_LOGE(true, true, LC_BRIGHT_RED, "Src isn't 32bit aligned");
        }
        else if ( ((uint32_t)buf & 3) != 0 )
        {
          F_LOGE(true, true, LC_BRIGHT_RED, "Dest isn't 32bit aligned");
        }
        else
        {
          if (config_list[i].valid == DEFAULT_RECORD)
          {
            memcpy (buf, config_list[i].text, len4);
          }
          else
          {
            uint32_t addr = (uint32_t)config_list[i].text;
            user_config_read (addr, buf, len4);
          }

          if (cfg_mode.type == TEXT)
          {
            buf[len] = 0; // zero terminate string
            F_LOGV(true, true, LC_GREY, "%3d: id 0x%02x Text     len %3d \"%s\"", i, cfg_mode.id, len, buf);
          }
          else
          {
            int num_vals = cfg_mode.len / sizeof (int);
            int* val_array = (int*)buf;

            F_LOGV(true, true, LC_GREY, "%3d: id 0x%02x NumArray len %3d ", i, cfg_mode.id, num_vals);

            for (int j = 0; j < num_vals; j++)
            {
              F_LOGV(true, true, LC_GREY, "%d, ", val_array[j]);
            }
          }
        }
      }
      else if (cfg_mode.type == NUMBER)
      {
        val = config_list[i].val;
        F_LOGV(true, true, LC_GREY, "%3d: id 0x%02x Number         %d", i, cfg_mode.id, val);
      }
      else if (cfg_mode.type == IP_ADDR)
      {
        val = config_list[i].val;
        //FIXME//printf(__func__, "%3d: id 0x%02x Ip_Addr        "IPSTR"", i, cfg_mode.id, IP2STR(&val));
      }
      else if (cfg_mode.type == FLAG)
      {
        val = config_list[i].val;
        F_LOGV(true, true, LC_GREY, "%3d: id 0x%02x Flag           %s", i, cfg_mode.id, val == 0?"false":"true");
      }
      else
      {
        val = 0;
      }
    }
  }
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

enum _cfg_get_user_rc
{
  fail = false,
  done = true,
  working
};

static int _config_get_user (void)
{
  int rc = working;

  if (user_settings.buf == NULL)
  {
    user_settings.buf = (uint32_t*)pvPortMalloc (SPI_FLASH_SEC_SIZE);
    if ( user_settings.buf == NULL )
    {
      F_LOGW(true, true, LC_YELLOW, "pvPortMalloc failed allocating 'user_settings.buf' (%d bytes)", SPI_FLASH_SEC_SIZE);
    }
  }

  if (user_settings.buf != NULL)
  {
    if (user_settings.start == 0)
    {
      user_config_get_start ();
    }

    if (user_settings.start != 0)
    {
      uint32_t rd_addr = user_settings.start; // 8 .. 0x2FFF

      int i = 0;
      for (i = 0; i < CFG_DATA_NUM_BLOCKS; i++)
      {
        // wrap address at user configuration data section end
        if (rd_addr >= SPI_FLASH_SEC_SIZE * CFG_DATA_NUM_BLOCKS)
        {
          rd_addr -= SPI_FLASH_SEC_SIZE * CFG_DATA_NUM_BLOCKS;
        }

        if ((rd_addr & (SPI_FLASH_SEC_SIZE - 1)) == 0)
        {
          rd_addr += sizeof (uint32_t) * 2;
        }

        uint32_t* buf32 = user_settings.buf;
        int num_words = (SPI_FLASH_SEC_SIZE - (rd_addr & (SPI_FLASH_SEC_SIZE - 1))) / sizeof (uint32_t);

        F_LOGV(true, true, LC_GREY, "from: 0x%04x", rd_addr);
        user_config_read (rd_addr, (char*)buf32, num_words * sizeof (uint32_t));

        while (num_words > 0)
        {
          cfg_mode_t cfg_mode;
          cfg_mode.mode = *buf32++;
          num_words--;

          if (cfg_mode.mode == 0xFFFFFFFF)
          {
            user_settings.write = rd_addr;
            F_LOGV(true, true, LC_GREY, "user_settings.write: 0x%04x", rd_addr);
            rc = done;
            break;
          }

          rd_addr += sizeof (cfg_mode_t);

          if (cfg_mode.valid >= RECORD_VALID)
          {
            if (cfg_mode.id < ID_MAX)
            {
              if (cfg_mode.type == TEXT || cfg_mode.type == NUMARRAY)
              {
                // copy mode and text field
                config_list[cfg_mode.id].mode = cfg_mode.mode;

                // the text field points to a location in the spi flash
                config_list[cfg_mode.id].text = (char*)(rd_addr);

                F_LOGV(true, true, LC_GREY, "got user config id 0x%02x, mode: 0x%08x text: 0x%08x", cfg_mode.id, config_list[cfg_mode.id].mode, (uint32_t)config_list[cfg_mode.id].text);

                int len4 = (cfg_mode.len + 3) & ~3;
                rd_addr += len4;
                buf32 += len4 / sizeof (uint32_t);
                num_words -= len4 / sizeof (uint32_t);
              }
              else
              {
                config_list[cfg_mode.id].mode = cfg_mode.mode;
                config_list[cfg_mode.id].val = *buf32++;
                F_LOGV(true, true, LC_GREY, "got user config id 0x%02x, mode: 0x%08x value: %d", cfg_mode.id, config_list[cfg_mode.id].mode, config_list[cfg_mode.id].val);
                rd_addr += sizeof (uint32_t);
                num_words -= sizeof (uint32_t);
              }
            }
            else
            {
              int len4 = (cfg_mode.len + 3) & ~3;
              rd_addr += len4;
              buf32 += len4 / sizeof (uint32_t);
              num_words -= len4 / sizeof (uint32_t);
            }
          }
          else if (cfg_mode.mode == 0)
          {
            // skip fill words
          }
          else
          {
            // something goes wrong.
            F_LOGE(true, true, LC_RED, "_config_get_user: something goes wrong at 0x%04x, 0x%08x", rd_addr, cfg_mode.mode);
            // what to do?
            rc = fail;
            break;
          }
        }

        if (rc != working)
        {
          // leave the for loop
          break;
        }

        if ( (rd_addr & (SPI_FLASH_SEC_SIZE - 1)) != 0 )
        {
          F_LOGE(true, true, LC_BRIGHT_RED, "rd_addr doesn't match begin of next block");
        }
      }
    }

    vPortFree (user_settings.buf);
    user_settings.buf = NULL;
  }
  else
  {
    rc = fail;
  }

  return rc;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
static int user_config_get_start (void)
{
  // Counter / Return value
  int i = 0;
  uint32_t marker[MARKER_SIZE] = {};
  uint32_t marker_loc = 0;

  F_LOGV(true, true, LC_GREY, "begin");

  user_settings.start = 0;
  do
  {
    F_LOGI(true, false, LC_GREY, "Looking for marker (0x%04x) in block %d ... ", CFG_START_MARKER, i);

    user_config_read (marker_loc, (char*)marker, (sizeof (uint32_t) * MARKER_SIZE));

    F_LOGI (false, true, LC_GREY, "found (0x%04x)", marker[0]);

    marker_loc += SPI_FLASH_SEC_SIZE;
  }
  while ((marker[0] != CFG_START_MARKER) && ++i < CFG_DATA_NUM_BLOCKS);

  if (i < CFG_DATA_NUM_BLOCKS)
  {
    F_LOGW(true, true, LC_GREEN, "marker (0x%08x) found in block %d", marker[0], i);

    user_settings.start = marker[1];

    i = user_settings.start;
  }
  else
  {
    F_LOGE(true, true, LC_RED, "no marker found");

    i = user_config_check_integrity ();
  }

  return i;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// a) check record integrity of every block,
// b) if not ok, erase block
// c) write marker to the end of the last block (repair)

enum
{
  unknown      = 0,
  first_valid  = 1,
  second_valid = 2,
  maybe_valid  = 0x40,
  invalid      = 0x80,
  maybe_erased = 0xf0,
  erased       = 0xFF
};

static int user_config_check_integrity (void)
{
  F_LOGW(true, true, LC_BRIGHT_YELLOW, "begin");

#if ( ESP_IDF_VERSION_MAJOR < 5 )
#endif

  uint8_t status[CFG_DATA_NUM_BLOCKS];
  uint i;
  const esp_partition_t *nvs_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, NVS_PARTITION);

  for (i = 0; i < CFG_DATA_NUM_BLOCKS; i++)
  {
    status[i] = unknown;
    uint32_t* buf32 = user_settings.buf;
    uint32_t addr = i * SPI_FLASH_SEC_SIZE;

    user_config_read (addr, (char*)buf32, SPI_FLASH_SEC_SIZE);
    int num_words = SPI_FLASH_SEC_SIZE / sizeof (uint32_t);

    // first location of a block contains the marker or is erased.
    if (*buf32 == 0xFFFFFFFF)
    {
      // this block potenially is the second valid block
      // check if there are valid data
      status[i] = second_valid;
    }
    else if (*buf32 == CFG_START_MARKER)
    {
      // this block potenially is the first valid block
      // check if there are valid data
      status[i] = first_valid;
    }
    else
    {
      // block has an invalid marker
      // so a) check if there are valid data or
      //    b) erase this block <--
      status[i] = invalid;
    }
    buf32++;
    num_words--;

    if (status[i] != invalid)
    {
      // check the current block
      // read the word after the marker (first location) and
      // skip a record which starts in the previous block
      if (*buf32 != 0xFFFFFFFF) // end of list
      {
        uint32_t skip = *buf32 - addr - (sizeof (uint32_t) * MARKER_SIZE);
        buf32++;
        num_words--;
        buf32 += skip / sizeof (uint32_t);
        num_words -= skip / sizeof (uint32_t);
      }

      while (num_words > 0)
      {
        cfg_mode_t cfg_mode;
        cfg_mode.mode = *buf32++;
        num_words--;

        if (cfg_mode.mode == 0xFFFFFFFF) // end of list
        {
          // flash sector seems to be erased
          if (status[i] == second_valid)
          {
            status[i] = erased;
          }
        }
        else
        {
          if (status[i] == erased)
          {
            status[i] = invalid;
            break;
          }

          if (cfg_mode.mode == 0) // skip fill words
          {
          }
          else if (cfg_mode.valid >= RECORD_VALID) // check for a usable record valid code
          {
            if (cfg_mode.type == TEXT || cfg_mode.type == NUMARRAY)
            {
              uint32_t len4 = (cfg_mode.len + 3) & ~3;
              buf32 += len4 / sizeof (uint32_t);
              num_words -= len4 / sizeof (uint32_t);
            }
            else if (cfg_mode.type > TEXT && cfg_mode.type <= FLAG)
            {
              buf32++;
              num_words--;
            }
            else
            {
              // not a vaild type found
              status[i] = invalid;
              break;
            }
          }
          else
          {
            // record is not valid, but we can check its integrity
            status[i] = invalid;
            break;
          }
        }
      }
    }

    if (status[i] == invalid)
    {
      uint32_t page = i + CFG_DATA_START_ADDR / SPI_FLASH_SEC_SIZE;
      F_LOGE(true, true, LC_RED, "Erase page 0x%04x", page);

      esp_flash_erase_region(nvs_partition->flash_chip, page, nvs_partition->size);

      status[i] = erased;
    }
  }

  // are all blockes empty? Then write marker to the begin of the first block
  int first = -1;
  int second = -1;
  for (i = 0; i < CFG_DATA_NUM_BLOCKS; i++)
  {
    F_LOGI(true, true, LC_GREY, "check integrity block %d: 0x%02x", i, status[i]);
    if (status[i] == first_valid)
    {
      first = i;
    }
    if (status[i] == second_valid)
    {
      second = i;
    }
  }

  uint32_t marker_loc = 0;
  if (first == -1 && second == -1)
  {
    // no block with data found
    F_LOGE(true, true, LC_RED, "write marker");
    uint32_t marker[MARKER_SIZE];
    marker[0] = CFG_START_MARKER;
    marker[1] = (sizeof (uint32_t) * MARKER_SIZE);
    user_config_write (marker_loc, (char*)marker, (sizeof (uint32_t) * MARKER_SIZE));
  }
  else
  {
    marker_loc = first * SPI_FLASH_SEC_SIZE;
  }

  marker_loc += (sizeof (uint32_t) * MARKER_SIZE);

  return marker_loc;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
int user_config_read (uint32_t addr, char* buf, int len)
{
  int rd_len = 0;
  const esp_partition_t *nvs_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, NVS_PARTITION);

  if ( (addr & 3) != 0 )
  {
    F_LOGE(true, true, LC_BRIGHT_RED, "Src isn't 32bit aligned");
  }
  else if ( ((uint32_t)(buf) & 3) != 0 )
  {
    F_LOGE(true, true, LC_BRIGHT_RED, "Dest isn't 32bit aligned");
  }
  else
  {
    while  (len > 0 )
    {
      // number of 32bit words to read
      int len4m = len & ~3;
      int idx = addr & (SPI_FLASH_SEC_SIZE - 1);

      // wrap address at user configuration data section end
      if (addr >= SPI_FLASH_SEC_SIZE * CFG_DATA_NUM_BLOCKS)
      {
        F_LOGW(true, true, LC_BRIGHT_YELLOW, "wrap address at section end 0x%04x %d", addr, len);
        addr -= SPI_FLASH_SEC_SIZE * CFG_DATA_NUM_BLOCKS;
      }

      uint32_t rd_addr = CFG_DATA_START_ADDR + addr;

      if (idx + len4m > SPI_FLASH_SEC_SIZE)
      {
        len4m = SPI_FLASH_SEC_SIZE - idx;
      }

      if ( len4m > 0 )
      {
        F_LOGV(true, true, LC_GREY, "esp_flash_read %d, 0x%04x", len4m, rd_addr);
        esp_flash_read(nvs_partition->flash_chip, (uint32_t*)buf, rd_addr, len4m);
        addr += len4m;
        buf += len4m;
        len -= len4m;
        idx += len4m;
        rd_len += len4m;
      }
      else if ( len < 4 )
      {
        uint32_t last;
        F_LOGV(true, true, LC_GREY, "esp_flash_read %d, 0x%04x", len, rd_addr);
        esp_flash_read(nvs_partition->flash_chip, &last, rd_addr, sizeof (last));
        memcpy (buf, &last, len);
        addr += len;
        buf += len;
        len = 0;
        idx += len;
        rd_len += sizeof (uint32_t);
      }

      if ( idx > SPI_FLASH_SEC_SIZE )
      {
        F_LOGE(true, true, LC_BRIGHT_RED, "addr overflow error 0x%04x 0x%04x", idx, SPI_FLASH_SEC_SIZE);
      }
      else if ( idx == SPI_FLASH_SEC_SIZE )
      {
        addr = (addr & ~(SPI_FLASH_SEC_SIZE - 1)) + (sizeof (uint32_t) * MARKER_SIZE);
        rd_len += sizeof (uint32_t) * 2;
      }
    }
  }

  return rd_len; // number of bytes not read, normally zero
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
int user_config_write (uint32_t addr, char *str, int len)
{
  int wr_len = 0;
  const esp_partition_t *nvs_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, NVS_PARTITION);

  if ( ((uint32_t)(str) & 3) != 0 )
  {
    F_LOGE(true, true, LC_BRIGHT_RED, "Src isn't 32bit aligned");
  }
  else if ( (addr & 3) != 0 )
  {
    F_LOGE(true, true, LC_BRIGHT_RED, "Dest isn't 32bit aligned");
  }
  else
  {
    wr_len = (len + 3) & ~3;

    while ( len > 0 )
    {
      int len4m = len & ~3; // number of 32bit words to write
      int idx = addr & (SPI_FLASH_SEC_SIZE - 1);

      // wrap address at user configuration data section end
      if (addr >= SPI_FLASH_SEC_SIZE * CFG_DATA_NUM_BLOCKS)
      {
        addr -= SPI_FLASH_SEC_SIZE * CFG_DATA_NUM_BLOCKS;
      }

      uint32_t wr_addr = CFG_DATA_START_ADDR + addr;

      if (idx + len > SPI_FLASH_SEC_SIZE)
      {
        len = SPI_FLASH_SEC_SIZE - idx; // number of bytes to write to the current block
      }

      if (len4m > 0)
      {
        F_LOGI(true, true, LC_GREY, "spi flash write %d bytes, 0x%04x", len, wr_addr);

        esp_flash_write(nvs_partition->flash_chip, (uint32_t*)str, wr_addr, len4m);

        addr += len4m;
        str += len4m;
        len -= len4m;
        idx += len4m;
      }
      else if (len < 4)
      {
        uint32_t last = 0;
        memcpy (&last, str, len);

        F_LOGI(true, true, LC_GREY, "spi flash write %d, 0x%04x", len, wr_addr);

        esp_flash_write(nvs_partition->flash_chip, &last, wr_addr, sizeof (last));

        addr += len;
        str += len;
        len = 0;
        idx += len;
      }

      if (idx == SPI_FLASH_SEC_SIZE)
      {
        // set the start address of the next record in the new block
        addr &= ~(SPI_FLASH_SEC_SIZE - 1);

        uint32_t start = addr + ((len + 3) & ~3) + (sizeof (uint32_t) * MARKER_SIZE);
        uint32_t wr_addr = CFG_DATA_START_ADDR + addr + sizeof (uint32_t);

        F_LOGI(true, true, LC_GREY, "spi flash write at 0x%04x next record 0x%04x", wr_addr, start);

        esp_flash_write(nvs_partition->flash_chip, &start, wr_addr, sizeof (start)); // write record start address

        addr += sizeof (uint32_t) * 2;
        wr_len += sizeof (uint32_t) * 2;
      }
      else if (idx > SPI_FLASH_SEC_SIZE)
      {
        F_LOGE(true, true, LC_RED, "addr overflow error 0x%04x 0x%04x", idx, SPI_FLASH_SEC_SIZE);
      }
    }
  }

  return wr_len;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
char* _config_save_str (int id, char* str, int len, int type)
{
  F_LOGI(true, true, LC_GREY, "0x%02x \"%s\", len %d, type %d", id, S (str), len, type);

  // write new value to the user configuration section in the flash
  cfg_mode_t cfg_mode;
  cfg_mode.id     = id;
  cfg_mode.type   = type;
  cfg_mode.len    = len == 0?strlen (str):len; // don't save termination zero
  cfg_mode.valid  = SPI_FLASH_RECORD;

  // write new str to the user configuration section in the flash
  uint32_t wr_addr __attribute__ ((aligned (4))) = _config_save (cfg_mode, str, 0);

  if (wr_addr > 0 && id < ID_MAX)
  {
    config_list[id].mode = cfg_mode.mode;
    config_list[id].text = (char*)wr_addr;
  }

  return (char*)wr_addr;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
char* _config_save_int (int id, int value, int type)
{
  F_LOGI(true, true, LC_GREY, "0x%02x %d %d", id, value, type);

  cfg_mode_t cfg_mode;
  cfg_mode.id     = id;
  cfg_mode.type   = type;
  cfg_mode.len    = sizeof (uint32_t); // because value is not a string
  cfg_mode.valid  = SPI_FLASH_RECORD;

  // write new value to the user configuration section in the flash
  uint32_t wr_addr = _config_save (cfg_mode, NULL, value);

  // update config_list
  config_list[id].mode  = cfg_mode.mode;
  config_list[id].val   = value;

  return (char*)wr_addr;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------
int _config_save (cfg_mode_t cfg_mode, char* str, int value)
{
  if (((uint32_t)str & 3) != 0)
  {
    F_LOGE(true, true, LC_RED, "not 32bit aligned");
    return -1;
  }

  F_LOGI(true, true, LC_GREY, "0x%08x \"%s\" %d", cfg_mode.mode, S (str), value);

  uint32_t addr = user_settings.write;
  int wr_addr = -1;

  int len = cfg_mode.len;
  int len4 = (len + 3) & ~3;
  int current_block = addr / SPI_FLASH_SEC_SIZE;
  int next_block = (addr + sizeof (cfg_mode_t) + len4 - 1) / SPI_FLASH_SEC_SIZE;
  const esp_partition_t *nvs_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, NVS_PARTITION);

  if (next_block != current_block)
  {
    F_LOGW(true, true, LC_BRIGHT_YELLOW, "will write %d bytes into next block from 0x%04x to 0x%04x", len4, addr, addr + len4);

    int gap_size = SPI_FLASH_SEC_SIZE - (addr & (SPI_FLASH_SEC_SIZE - 1));
    // write skip date until the end of the current block
    cfg_mode_t cfg_mode_skip __attribute__ ((aligned (4)));
    cfg_mode_skip.id     = ID_SKIP_DATA;
    cfg_mode_skip.type   = FILLDATA;
    cfg_mode_skip.len    = gap_size - sizeof (cfg_mode_t);
    cfg_mode_skip.valid  = SPI_FLASH_RECORD;

    F_LOGI(true, true, LC_GREY, "Fill gap from 0x%04x gap_size %d", addr, gap_size);
    user_config_write (addr, (char*)&cfg_mode_skip.mode, sizeof (cfg_mode_t));
    addr += +gap_size;
    addr += sizeof (uint32_t) * MARKER_SIZE; // reserve the first two words for a marker
    current_block++;
    F_LOGI(true, true, LC_GREY, "next addr to write 0x%04x", addr);

    int first = (current_block + 1) % CFG_DATA_NUM_BLOCKS;
    uint32_t marker_loc = first * SPI_FLASH_SEC_SIZE;
    uint32_t marker[MARKER_SIZE];

    F_LOGW(true, true, LC_BRIGHT_YELLOW, "look for marker at next block %d, 0x%04x", first, marker_loc);
    user_config_read (marker_loc, (char*)marker, (sizeof (uint32_t) * MARKER_SIZE));
    if (marker[0] == CFG_START_MARKER)
    {
      F_LOGW(true, true, LC_BRIGHT_YELLOW, "if so: copy the records from the oldest block to the current block");

      int i;
      for (i = 0; i < ID_MAX; i++)
      {
        settings_t* cfg = &config_list[i];

        if (cfg->id != 0 && cfg->valid == SPI_FLASH_RECORD) // user defeined record stored in spi flash
        {
          if (cfg->type == TEXT || cfg->type == NUMARRAY || cfg->type == STRUCTURE)
          {
            uint32_t text = (uint32_t)cfg->text;
            if ((text / SPI_FLASH_SEC_SIZE) == first)
            {
              int wr_len = user_config_write (addr, (char*)&cfg->mode, sizeof (cfg_mode_t));
              addr += wr_len;

              char buf[cfg->len + 1];
              user_config_read (text, buf, cfg->len);

              F_LOGW(true, true, LC_BRIGHT_YELLOW, "spi flash write %d bytes of \"%s\" to 0x%04x", len, S (buf), addr);
              wr_len = user_config_write (addr, buf, len);
              addr += wr_len;
            }
          }
          else if (cfg->type > TEXT && cfg->type <= FLAG)
          {
            int wr_len = user_config_write (addr, (char*)cfg, sizeof (settings_t));
            addr += wr_len;
          }
        }
      }

      int num_words = SPI_FLASH_SEC_SIZE / sizeof (uint32_t);
      uint32_t rd_addr = marker_loc + (sizeof (uint32_t) * MARKER_SIZE);
      num_words--;
      num_words--;

      while (num_words > 0)
      {
        cfg_mode_t cfg_mode __attribute__ ((aligned (4)));
        esp_flash_read(nvs_partition->flash_chip, (uint32_t*)&cfg_mode, rd_addr, sizeof (cfg_mode));
        num_words--;

        if (cfg_mode.mode == 0xFFFFFFFF) // end of list
        {
          break;
        }
        else if (cfg_mode.mode == 0) // skip fill words
        {
        }
        else if (cfg_mode.valid >= RECORD_VALID) // check for a usable record valid code
        {
          uint32_t len4 = (cfg_mode.len + 3) & ~3;

          if (cfg_mode.id == ID_EXTRA_DATA && cfg_mode.valid != RECORD_ERASED)
          {
            uint8_t buf[len4] __attribute__ ((aligned (4)));
            esp_flash_read(nvs_partition->flash_chip, (uint32_t*)buf, rd_addr, len4);

            int wr_len = user_config_write (addr, (char*)&cfg_mode, sizeof (cfg_mode_t));
            addr += wr_len;
            wr_len = user_config_write (addr, (char*)buf, len4);
            addr += wr_len;
            num_words -= len4 / sizeof (uint32_t);
          }

          rd_addr += len4;
          num_words -= len4 / sizeof (uint32_t);
        }
        else // record is not valid
        {
        }
      }

      // erase start block
      uint32_t page = first + CFG_DATA_START_ADDR / SPI_FLASH_SEC_SIZE;
      F_LOGW(true, true, LC_BRIGHT_YELLOW, "Erase page 0x%04x", page);
      esp_flash_erase_region(NULL, page, 0x1000);

      // write marker to its next block
      int next = (first + 1) % CFG_DATA_NUM_BLOCKS;
      uint32_t marker_loc __attribute__ ((aligned (4))) = next * SPI_FLASH_SEC_SIZE;
      marker[0] = CFG_START_MARKER;
      marker[1] = marker_loc + (sizeof (uint32_t) * MARKER_SIZE);
      F_LOGW(true, true, LC_BRIGHT_YELLOW, "write marker to block %d at 0x%08x", next, marker_loc);
      user_config_write (marker_loc, (char*)&marker[0], (sizeof (uint32_t) * MARKER_SIZE));
    }
  }

  // write the record to the spi flash.
  if (cfg_mode.type == TEXT || cfg_mode.type == NUMARRAY || cfg_mode.type == STRUCTURE)
  {
    int wr_len = user_config_write (addr, (char*)&cfg_mode.mode, sizeof (cfg_mode_t));
    addr += wr_len;
    wr_addr = addr;
    F_LOGI(true, true, LC_GREY, "spi flash write %d bytes of \"%s\" to 0x%04x", len, str, addr);
    wr_len = user_config_write (addr, str, len);
    addr += wr_len;
  }
  else if (cfg_mode.type > TEXT && cfg_mode.type <= FLAG)
  {
    int wr_len = user_config_write (addr, (char*)&cfg_mode.mode, sizeof (cfg_mode));

    addr += wr_len;
    wr_addr = addr;

    F_LOGW(true, true, LC_BRIGHT_YELLOW, "esp_flash_write value 0x%08x to 0x%04x", value, addr);

    wr_len = user_config_write (addr, (char*)&value, sizeof (value));
    addr += wr_len;
  }

  user_settings.write = addr;

  F_LOGI(true, true, LC_GREY, "user_settings.write: 0x%04x", wr_addr);

  return wr_addr; // start address in spi flash of last written string or value
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

// dump the settings stored in the spi flash

int user_config_print (void)
{
  F_LOGV(true, true, LC_GREY, "SPI_FLASH_SEC_SIZE = %d, CFG_DATA_NUM_BLOCKS = %d", SPI_FLASH_SEC_SIZE, CFG_DATA_NUM_BLOCKS);

  int rc = working;

  if (user_settings.buf == NULL)
  {
    user_settings.buf = (uint32_t*)pvPortMalloc (SPI_FLASH_SEC_SIZE);
    if ( user_settings.buf == NULL )
    {
      F_LOGE(true, true, LC_RED, "pvPortMalloc failed allocating 'user_settings.buf' (%d bytes)", SPI_FLASH_SEC_SIZE);
    }
  }

  if (user_settings.buf != NULL)
  {
    if (user_settings.start == 0)
    {
      // MJ OK
      user_config_get_start ();
    }

    if (user_settings.start != 0)
    {
      uint32_t rd_addr = user_settings.start;

      F_LOGI(true, true, LC_GREY, "+--------+------+----------+-----+-----+------------------------");
      F_LOGI(true, true, LC_GREY, "| Addr   | id   | Type     |valid| Len | Value");
      F_LOGI(true, true, LC_GREY, "+--------+------+----------+-----+-----+------------------------");

      for (int i = 0; i < CFG_DATA_NUM_BLOCKS; i++)
      {
        if (rd_addr >= SPI_FLASH_SEC_SIZE * CFG_DATA_NUM_BLOCKS)
        {
          rd_addr -= SPI_FLASH_SEC_SIZE * CFG_DATA_NUM_BLOCKS;
        }

        if ((rd_addr & (SPI_FLASH_SEC_SIZE - 1)) == 0)
        {
          rd_addr += sizeof (uint32_t) * 2;
        }

        uint32_t* buf32 = user_settings.buf;
        int num_words = (SPI_FLASH_SEC_SIZE - (rd_addr & (SPI_FLASH_SEC_SIZE - 1))) / sizeof (uint32_t);

        F_LOGV(true, true, LC_GREY, "_config_get_user from: 0x%04x", rd_addr);
        user_config_read (rd_addr, (char*)buf32, num_words * sizeof (uint32_t));

        while (num_words > 0)
        {
          cfg_mode_t cfg_mode;
          cfg_mode.mode = *buf32++;
          num_words--;
          uint32_t id_addr = rd_addr;

          if (cfg_mode.mode == 0xFFFFFFFF) // end of list
          {
            F_LOGI(true, true, LC_GREY, "+--------+------+----------+-----+-----+------------------------");

            rc = done;
            break;
          }

          rd_addr += sizeof (cfg_mode_t);

          if (cfg_mode.valid >= RECORD_VALID)
          {
            char buf[TMP_BUFSIZE + 1] __attribute__ ((aligned (4))) = {};

            if ( (rd_addr & 3) != 0 )
            {
              F_LOGE(true, true, LC_BRIGHT_RED, "Src isn't 32bit aligned");
            }
            else if ( ((uint32_t)buf & 3) != 0 )
            {
              F_LOGE(true, true, LC_BRIGHT_RED, "Dest isn't 32bit aligned");
            }
            else
            {

              int len = cfg_mode.len;
              int len4 = align_len (sizeof (buf), &len);

              memcpy (buf, buf32, len4);
              rd_addr += len4;
              buf32 += len4 / sizeof (uint32_t);
              num_words -= len4 / sizeof (uint32_t);

              switch (cfg_mode.type)
              {
                case TEXT:
                  {
                    buf[len] = 0;
                    F_LOGI(true, true, LC_GREY, "| 0x%04x | 0x%02x | Text     |  %2d | %3d | \"%s\"", id_addr, cfg_mode.id, cfg_mode.valid & 0xF, len, buf);
                    break;
                  }
                case NUMARRAY:
                  {
                    F_LOGI(true, true, LC_GREY, "| 0x%04x | 0x%02x | NumArray |  %2d | %3d | ", id_addr, cfg_mode.id, cfg_mode.valid & 0xF, len);
                    int* val_array = (int*)buf;
                    int num_vals = (cfg_mode.len + sizeof (int) - 1) / sizeof (int);
                    for (int j = 0; j < num_vals; j++)
                    {
                      F_LOGI(true, true, LC_GREY, "%d, ", val_array[j]);
                    }
                    break;
                  }
                case STRUCTURE:
                  {
                    F_LOGI(true, true, LC_GREY, "| 0x%04x | 0x%02x | Structure|  %2d | %3d | ", id_addr, cfg_mode.id, cfg_mode.valid & 0xF, len);
                    uint32_t* val_array = (uint32_t*)buf;
                    int num_vals = (cfg_mode.len + sizeof (int) - 1) / sizeof (int);
                    for (int j = 0; j < num_vals; j++)
                    {
                      F_LOGI(true, true, LC_GREY, "0x%08x, ", val_array[j]);
                    }
                    break;
                  }
                case FILLDATA:
                  {
                    F_LOGI(true, true, LC_GREY, "| 0x%04x | 0x%02x | FillData |  %2d | %3d | ", id_addr, cfg_mode.id, cfg_mode.valid & 0xF, len);
                    uint32_t* val_array = (uint32_t*)buf;
                    int num_vals = (cfg_mode.len + sizeof (int) - 1) / sizeof (int);
                    for (int j = 0; j < num_vals; j++)
                    {
                      F_LOGI(true, true, LC_GREY, "0x%08x, ", val_array[j]);
                    }
                    break;
                  }
                default:
                  {
                    uint32_t val = *buf32++;
                    rd_addr += sizeof (uint32_t);
                    num_words -= sizeof (uint32_t);

                    switch (cfg_mode.type)
                    {
                      case NUMBER:
                        F_LOGI(true, true, LC_GREY, "| 0x%04x | 0x%02x | Number   |  %2d |     | 0x%08x : %10d", id_addr, cfg_mode.id, cfg_mode.valid & 0xF, val, val);
                        break;
                      case IP_ADDR:
                        F_LOGW(true, true, LC_BRIGHT_YELLOW, "FIXME!?");
                        break;
                      case FLAG:
                        F_LOGW(true, true, LC_BRIGHT_YELLOW, "OK: cfg_mode.type = %d, cfg_mode.mode = %d", cfg_mode.type, cfg_mode.mode);
                        F_LOGI(true, true, LC_GREY, "| 0x%04x | 0x%02x | Flag     |  %2d |     | %s", id_addr, cfg_mode.id, cfg_mode.valid & 0xF, val == 0?"False":"True");
                        break;
                      default:
                        F_LOGI(true, true, LC_GREY, "| 0x%04x | 0x%02x | Unknown  |  %2d |     | 0x%08x : %10d", id_addr, cfg_mode.id, cfg_mode.valid & 0xF, val, val);
                    }
                  }
              }
            }
          }
          else if (cfg_mode.mode == 0)
          {
            // skip fill words
          }
          else
          {
            F_LOGW(true, true, LC_BRIGHT_YELLOW, "NA: cfg_mode.type = %d, cfg_mode.mode = %d", cfg_mode.type, cfg_mode.mode);
            F_LOGE(true, true, LC_RED, "_config_get_user: something goes wrong at 0x%04x, 0x%08x", rd_addr, cfg_mode.mode);
            // what to do?
            //rc = fail;
            //break;
          }
        }

        if ( rc != working )
        {
          break;
        }

        if ( (rd_addr & (SPI_FLASH_SEC_SIZE - 1)) != 0 )
        {
          F_LOGE(true, true, LC_BRIGHT_RED, "rd_addr doesn't match begin of next block");
        }
      }
    }

    vPortFree (user_settings.buf);
    user_settings.buf = NULL;
  }
  else
  {
    rc = fail;
  }

  return rc;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

uint32_t user_config_invalidate (uint32_t addr)
{
  F_LOGV(true, true, LC_GREY, "0x%08x", addr);

  const esp_partition_t *nvs_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, NVS_PARTITION);

  // addr points to the value or the begin of the string
  // the contol block is 4 bytes before

  uint32_t cfg_addr = addr - sizeof (cfg_mode_t);

  if (cfg_addr >= 4 && cfg_addr < SPI_FLASH_SEC_SIZE * CFG_DATA_NUM_BLOCKS)
  {
    cfg_mode_t cfg_mode;
    uint32_t spi_addr = CFG_DATA_START_ADDR + cfg_addr;

    esp_flash_read(nvs_partition->flash_chip, (uint32_t*)&cfg_mode, spi_addr, sizeof (cfg_mode));

    if (cfg_mode.valid != RECORD_ERASED)
    {
      cfg_mode.valid = RECORD_ERASED;
      esp_flash_write(nvs_partition->flash_chip, (uint32_t*)&cfg_mode, spi_addr, sizeof (cfg_mode));
    }
  }
  else
  {
    F_LOGE(true, true, LC_RED, "addr 0x%08x out of user configuration data space", (uint32_t)addr);
  }

  return addr;
}

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

//void call_back(void *arg, uint32_t *flash_rd_addr);

int user_config_scan (int id, int(call_back) (...), void* arg)
{
  F_LOGV(true, true, LC_GREY, "begin");

  int rc = working;

  if (user_settings.start == 0)
  {
    user_config_get_start ();
  }

  if (user_settings.start != 0)
  {
    uint32_t rd_addr = user_settings.start; // 8 .. 0x2FFF
    int i = 0;
    for (i = 0; i < CFG_DATA_NUM_BLOCKS; i++)
    {
      // wrap address at user configuration data section end
      if (rd_addr >= SPI_FLASH_SEC_SIZE * CFG_DATA_NUM_BLOCKS)
      {
        rd_addr -= SPI_FLASH_SEC_SIZE * CFG_DATA_NUM_BLOCKS;
      }

      // parse the configuration in the spi-flash for valid switching timers setting
      if ((rd_addr & (SPI_FLASH_SEC_SIZE - 1)) == 0)
      {
        rd_addr += (sizeof (uint32_t) * MARKER_SIZE);
      }

      int num_words = (SPI_FLASH_SEC_SIZE - (rd_addr & (SPI_FLASH_SEC_SIZE - 1))) / sizeof (uint32_t);
      while (num_words > 0)
      {
        uint32_t buf32[TMP_BUFSIZE + 1];
        int rd_words = TMP_BUFSIZE;

        if (rd_words > num_words)
        {
          rd_words = num_words;
        }

        user_config_read (rd_addr, (char*)buf32, rd_words);
        cfg_mode_t* cfg_mode = (cfg_mode_t*)buf32;

        if (cfg_mode->mode == 0xFFFFFFFF) // end of list
        {
          user_settings.write = rd_addr;
          F_LOGV(true, true, LC_GREY, "user_settings.write: 0x%04x", rd_addr);
          rc = done;
          break;
        }

        rd_addr += sizeof (cfg_mode_t);
        num_words--;

        if (cfg_mode->valid >= RECORD_VALID)
        {
          uint32_t len4 = (cfg_mode->len + 3) & ~3;

          if ((cfg_mode->id == id) && cfg_mode->valid != RECORD_ERASED)
          {
            if ( call_back )
            {
              call_back (buf32, cfg_mode->len, rd_addr, arg); // call_back function handles the destionation
            }
          }

          rd_addr += len4;
          num_words -= len4 / sizeof (uint32_t);
        }
        else if (cfg_mode->mode == 0) // skip fill words
        {
        }
        else // record is not valid
        {
          F_LOGW(true, true, LC_BRIGHT_YELLOW, "record is not valid: 0x%04x", rd_addr);
        }

        //FIXMEsystem_soft_wdt_feed();
      }

      if (rc != working)
      {
        break; // leave the for loop
      }

      if ( (rd_addr & (SPI_FLASH_SEC_SIZE - 1)) != 0 )
      {
        F_LOGE(true, true, LC_BRIGHT_RED, "rd_addr doesn't match begin of next block");
      }
    }
  }

  return rc;
}
