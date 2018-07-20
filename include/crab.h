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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


enum CrabFileFlag
{
    /*
        Map with writeability - allow existing sections to have their data
        directly modified, rather than only replacing the section entirely.

        Use with caution.
    */
    CRAB_FILE_FLAG_WRITE = 0x01,
    /*
        Create a new CRAB file with the given name, rather than opening an
        existing one.

        Note that the file will not actually be written until crab_save() is
        called.
    */
    CRAB_FILE_FLAG_NEW = 0x02,
    /*
        (Try to) return a non-NULL pointer so that you can call crab_error.
    */
    CRAB_FILE_FLAG_ERROR = 0x04,
    /*
        For all operations on this file, print errors.
    */
    CRAB_FILE_FLAG_PERROR = 0x08,
};

enum CrabSectionFlag
{
    /*
        Instead of copying the data, take ownership.

        It must have been allocated with `malloc`.
    */
    CRAB_SECTION_FLAG_OWN = 0x01,
    /*
        Instead of copying the data, assume it outlives this.

        Normally this means you robbed another file.
    */
    CRAB_SECTION_FLAG_BORROW = 0x02,
};

enum CrabCloseFlag
{
    /*
        After saving, reopen to save memory.

        This will preserve the `CrabFile` and `CrabSection` pointers, but
        invalidate the data and schema pointers.
    */
    CRAB_CLOSE_FLAG_REOPEN = 0x01,
};


/*
    Map a CRAB file from disk.
*/
CrabFile *crab_file_open(const char *filename, int flags);
/*
    Release all resources associated with the CRAB file.

    This does NOT save any changes.
*/
bool crab_file_close(CrabFile *c);
/*
    Write a CRAB file to disk.
*/
bool crab_file_save(CrabFile *c, int flags);
/*
    Fetch details about the most recent error to occur.

    You should call this if, and only if, some other function returns falsy.
*/
void crab_file_error(CrabFile *c, const char **msg, int *no);

/*
    Current number of valid indices.
*/
uint32_t crab_file_num_sections(CrabFile *c);
/*
    Get a section by index.

    This must not be outlive the file.
*/
CrabSection *crab_file_section(CrabFile *c, uint32_t i);
/*
    Add a new, empty section.

    You'll probably want to set its purpose and data.
*/
CrabSection *crab_file_section_add(CrabFile *c);

/*
    Get the index of this section within the file.
*/
uint32_t crab_section_number(CrabSection *s);
/*
    Get the schema URL for this section.

    Most CRAB files use exactly two schemas: the builtin one for special
    sections, and one specialized for the actual use-case.
*/
const char *crab_section_schema(CrabSection *s);
/*
    Get the purpose number for this section.

    Purpose numbers are defined within a schema, and are often similar to
    column "names" within a database table.
*/
uint16_t crab_section_purpose(CrabSection *s);
/*
    Set the schema and purpose for a section.
*/
bool crab_section_set_schema_and_purpose(CrabSection *s, const char *schema, uint16_t purpose);
/*
    How much big the section's data is.
*/
size_t crab_section_data_size(CrabSection *s);
/*
    Get an abstract pointer to the section's data.

    You should cast this to whatever type is appropriate for the given
    purpose, and verify that the size is big enough.

    For most schema/purposes, the section data has a small header, followed by
    an array of data. Sometimes there are some number of references to
    other sections, using relative offsets for easy relocation.
*/
CrabAbstractData *crab_section_data(CrabSection *s);
/*
    Copy the schema, purpose, and data from another section.
*/
bool crab_section_copy(CrabSection *s, int flags, CrabSection *other);
/*
    Copy the data into the section.
*/
bool crab_section_set_data(CrabSection *s, int flags, CrabAbstractData *data, size_t size);
