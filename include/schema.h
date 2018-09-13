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

#include <stdint.h>


#define CRAB_SCHEMA "https://o11c.github.io/crab/schema.html"

typedef struct CrabSchemaData CrabSchemaData;

enum CrabPurpose
{
    /*
        Usually should not occur in files, but can occur as a placeholder if
        you delete a section, to avoid breaking relative offsets.
    */
    CRAB_PURPOSE_ERROR = 0,
    /*
        Data that should be interpreted simply as a sequence of bytes.
    */
    CRAB_PURPOSE_RAW = 1,
    /*
        Data that should only be referred to from other sections.
    */
    CRAB_PURPOSE_SUPPLEMENTARY = 2,
    /*
        CrabSchemaData

        This is always used for section 0.
    */
    CRAB_PURPOSE_SCHEMA = 3,
    /*
        CrabPurposeData

        Textual forms of "purpose" for the schema. This need not exist, and
        if it does exist there it need not be in any particular place. In
        fact, there may be more than one (e.g. in case of merges).
    */
    CRAB_PURPOSE_PURPOSE = 4,
};

/* purpose = 3 */
struct __attribute__((scalar_storage_order("big-endian"))) CrabSchemaData
{
    uint32_t string_section;
    uint16_t reserved;
    uint16_t num_schemas;
    struct __attribute__((scalar_storage_order("big-endian")))
    {
        uint32_t url;
        uint32_t reserved;
    } schemas[0];
};

/*
    purpose = 4

    Note that, by design, this does not use schema indices, allowing
    oblivious merges.
*/
struct __attribute__((scalar_storage_order("big-endian"))) CrabPurposeData
{
    uint32_t string_section;
    uint32_t schema_url;
    uint32_t num_supplements;
    uint16_t reserved;
    uint16_t num_purposes;
    struct __attribute__((scalar_storage_order("big-endian")))
    {
        uint32_t purpose;
        uint32_t reserved;
    } purposes[0];
};
