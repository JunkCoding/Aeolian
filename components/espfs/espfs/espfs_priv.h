/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include <stdint.h>
#include <esp_flash_spi_init.h>
#include "espfs_format.h"


typedef struct espfs_fs_t espfs_fs_t;
typedef struct espfs_file_t espfs_file_t;

struct espfs_fs_t {
    spi_flash_mmap_handle_t mmap_handle;
    const espfs_fs_header_t *header;
    const espfs_hashtable_entry_t *hashtable;
    const espfs_sorttable_entry_t *sorttable;
};

struct espfs_file_t {
    const espfs_fs_t *fs;
    const espfs_file_header_t *fh;
    uint8_t *raw_start;
    uint8_t *raw_ptr;
    uint32_t raw_len;
    uint32_t file_pos;
    void *user;
};
