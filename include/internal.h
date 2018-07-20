/*
    CRAB - Compact Random-Access Binary
    Copyright © 2018  Ben Longbons

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#pragma once

#include "fwd.h"

#include <stddef.h>
#include <stdint.h>


#define CRAB_MAGIC "\x83""CRB\r\n\x1a\n"

typedef struct CrabFileHeader CrabFileHeader;
typedef struct CrabSectionHeader CrabSectionHeader;

struct CrabFile
{
    CrabFileHeader *file_header;

    char *filename;
    size_t filename_len;
    int flags;

    uint32_t num_sections;
    CrabSection **sections;

    const char *error_message;
    int error_number;
};

struct CrabSection
{
    CrabFile *c;

    uint32_t section_number;
    uint16_t local_schema_id;
    uint16_t purpose;
    const char *schema;

    CrabAbstractData *data;
    size_t data_size;

    int flags;
};

/* because GCC 6 makes life a *lot* easier */
struct __attribute__((scalar_storage_order("big-endian"))) CrabSectionHeader
{
    uint64_t offset;
    uint32_t size;
    uint16_t schema;
    uint16_t purpose;
};

struct __attribute__((scalar_storage_order("big-endian"))) CrabFileHeader
{
    char magic[8];
    uint64_t size;
    uint32_t reserved;
    uint32_t num_sections;
    CrabSectionHeader section_info[0];
};
