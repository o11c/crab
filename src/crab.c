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
#define _XOPEN_SOURCE 500
#include "crab.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "format.h"
#include "internal.h"
#include "schema.h"
#include "util.h"


/* This macro captures `c` implicitly. */
#undef ERROR
#define ERROR(f)        ERROR2(f, errno)
#define ERROR2(f, e)            \
({                              \
    c->error_message = (f);     \
    c->error_number = (e);      \
    goto err;                   \
})

static void maybe_perror(CrabFile *c)
{
    if (c->flags & CRAB_FILE_FLAG_PERROR)
    {
        errno = c->error_number;
        perror(c->error_message);
    }
}

static void *memdup(const void *p, size_t len)
{
    void *rv = malloc(len);
    if (rv)
        memcpy(rv, p, len);
    return rv;
}
static void *memdup_plus(const void *p, size_t len, size_t plus)
{
    void *rv = malloc(len + plus);
    if (rv)
        memcpy(rv, p, len);
    return rv;
}

static bool update_schemas(CrabFile *c)
{
    bool okay = true;
    uint32_t i;
    CrabSection *schema_section = c->sections[0];
    CrabSchemaData *schema_data = (CrabSchemaData *)schema_section->data;
    CrabSection *string_section = c->sections[schema_section->section_number + schema_data->string_section];
    char *string_data = (char *)string_section->data;
    size_t string_data_size = string_section->data_size;

    const uint64_t fixed_size = offsetof(CrabSchemaData, schemas);
    const uint64_t unit_size = sizeof(schema_data->schemas[0]);
    uint16_t num_schemas = schema_data->num_schemas;
    uint32_t num_sections = c->num_sections;

    if (schema_section->data_size != fixed_size + num_schemas * unit_size)
        okay = false;
    for (i = 0; i < num_sections; ++i)
    {
        CrabSection *s = c->sections[i];
        uint16_t schema_id = s->local_schema_id;
        uint32_t url, url_start, url_len, url_end;
        if (!okay)
            goto err;
        if (schema_id >= num_schemas)
            goto set_err;
        url = schema_data->schemas[schema_id].url;
        url_start = url >> STRING_SIZE_BITS;
        url_len = url % (1 << STRING_SIZE_BITS);
        /* No overflow, since inputs only have 32 bits *between* them. */
        url_end = url_start + url_len;
        if (url_end >= string_data_size)
            goto set_err;
        if (string_data[url_end])
            goto set_err;
        s->schema = string_data + url_start;
        continue;

    set_err:
        okay = false;
    err:
        /* no printing; handled by caller */
        s->schema = NULL;
    }
    return okay;
}

static void crab_file_open_partial(CrabFile *c, bool all)
{
    int fd = -1;

    if (!c->filename)
        ERROR("strdup");
    if (c->flags & CRAB_FILE_FLAG_NEW)
    {
        CrabSection *schema_section;
        CrabSection *string_section;

        if (!all)
            die2("CRAB_FILE_FLAG_NEW", EINVAL);
        c->flags &= ~CRAB_FILE_FLAG_NEW;

        c->sections = TRY_P(calloc, (2, sizeof(*c->sections)));
        c->num_sections = 2;

        string_section = c->sections[1] = TRY_P(calloc, (1, sizeof(*c->sections[1])));
        string_section->c = c;
        string_section->section_number = 1;
        string_section->data = (CrabAbstractData *)strdup(CRAB_SCHEMA);
        string_section->data_size = strlen(CRAB_SCHEMA) + 1;
        string_section->schema = (char *)string_section->data;
        string_section->local_schema_id = 0;
        string_section->purpose = CRAB_PURPOSE_SUPPLEMENTARY;
        string_section->flags = CRAB_SECTION_FLAG_OWN;

        schema_section = c->sections[0] = TRY_P(calloc, (1, sizeof(*c->sections[0])));
        schema_section->c = c;
        schema_section->section_number = 0;
        {
            CrabSchemaData *sd;
            size_t fixed_size = offsetof(CrabSchemaData, schemas);
            size_t var_size = 1 * sizeof(sd->schemas[0]);
            sd = TRY_P(calloc, (1, fixed_size + var_size));
            sd->string_section = 1 - 0;
            sd->num_schemas = 1;
            sd->schemas[0].url = (0 << STRING_SIZE_BITS) | strlen(CRAB_SCHEMA);
            schema_section->data = (CrabAbstractData *)sd;
            schema_section->data_size = fixed_size + var_size;
        }
        schema_section->schema = (char *)string_section->data;
        schema_section->local_schema_id = 0;
        schema_section->purpose = CRAB_PURPOSE_SCHEMA;
        schema_section->flags = CRAB_SECTION_FLAG_OWN;

        goto out;
    }

    {
        uint32_t i;
        struct stat stat_buf;
        uint64_t file_size; /* logically size_t */
        CrabFileHeader *header;
        const uint64_t first_sectioninfo_offset = offsetof(CrabFileHeader, section_info);
        const uint64_t sectioninfo_size = sizeof(header->section_info[0]);
        uint64_t num_sections; /* logically uint32_t */

        fd = TRY(open, (c->filename, c->flags & CRAB_FILE_FLAG_WRITE ? O_RDWR : O_RDONLY));
        TRY(fstat, (fd, &stat_buf));
        file_size = (uint64_t)stat_buf.st_size;
        if (file_size != (size_t)file_size)
            ERROR2("<file size>", EOVERFLOW);
        if (file_size < first_sectioninfo_offset + 1 * sectioninfo_size)
            goto fmt_err;

        header = TRY2(MAP_FAILED, mmap, (NULL, file_size, (c->flags & CRAB_FILE_FLAG_WRITE ? PROT_WRITE : 0) | PROT_READ, MAP_PRIVATE, fd, 0));
        if (header->size != file_size)
        {
            TRY(munmap, (c->file_header, file_size));
            goto fmt_err;
        }
        c->file_header = header;
        if (memcmp(header->magic, CRAB_MAGIC, 8) != 0)
            goto fmt_err;
        num_sections = header->num_sections;
        if (header->num_sections < 1)
            goto fmt_err;
        /* due to having 32-bit inputs, this cannot overflow */
        if (file_size < first_sectioninfo_offset + num_sections * sectioninfo_size)
            goto fmt_err;

        if (all)
        {
            c->sections = TRY_P(calloc, (num_sections, sizeof(c->sections[0])));
            c->num_sections = num_sections;
        }
        else
        {
            if (c->num_sections != num_sections)
                die2("<num_sections mismatch>", EINVAL);
        }
        for (i = 0; i < num_sections; ++i)
        {
            CrabSection *s;
            uint64_t section_offset = header->section_info[i].offset;
            uint64_t section_size = header->section_info[i].size;
            /* with overflow check */
            uint64_t section_end = section_offset + section_size;
            if (section_end < section_offset)
                goto fmt_err;

            if (section_end > file_size)
                goto fmt_err;

            if (all)
                c->sections[i] = TRY_P(calloc, (1, sizeof(*c->sections[i])));
            s = c->sections[i];
            s->c = c;
            s->section_number = i;
            s->local_schema_id = header->section_info[i].schema;
            s->purpose = header->section_info[i].purpose;
            /* s->schema = ...; */
            s->data = (CrabAbstractData *)((char *)header + header->section_info[i].offset);
            s->data_size = header->section_info[i].size;
        }
        if (0 + ((CrabSchemaData *)c->sections[0]->data)->string_section >= num_sections)
            goto fmt_err;
        if (!update_schemas(c))
            goto fmt_err;

        goto out;
    }

fmt_err:
    c->error_message = "<file format>";
    c->error_number = EINVAL;
err:
    maybe_perror(c);
out:
    if (fd != -1)
    {
        if (-1 == close(fd))
            die("close");
    }
    return;
}
CrabFile *crab_file_open(const char *filename, int flags)
{
    CrabFile *c = calloc(1, sizeof(*c));
    if (!c)
        return NULL;
    c->flags = flags;
    if (filename)
    {
        c->filename = strdup(filename);
        c->filename_len = strlen(filename);
    }
    else
        errno = EINVAL;
    crab_file_open_partial(c, true);
    if (c->error_message)
    {
        if (!(c->flags & CRAB_FILE_FLAG_ERROR))
        {
            crab_file_close(c);
            c = NULL;
        }
    }
    return c;
}

static bool crab_file_close_partial(CrabFile *c, bool all)
{
    uint32_t i;
    for (i = 0; i < c->num_sections; ++i)
    {
        CrabSection *s = c->sections[i];
        if (s)
        {
            if (s->flags & CRAB_SECTION_FLAG_OWN)
                free(s->data);
            if (all)
                free(s);
            c->sections[i] = NULL;
        }
    }
    if (c->file_header)
    {
        TRY(munmap, (c->file_header, c->file_header->size));
        c->file_header = NULL;
    }
    if (all)
    {
        free(c->sections);
        free(c->filename);
        free(c);
    }
    return true;

err:
    errno = c->error_number;
    die(c->error_message);
}

bool crab_file_close(CrabFile *c)
{
    return crab_file_close_partial(c, true);
}

static bool fwrite_harder(FILE *fp, const void *ptr, size_t sz)
{
    const char *c = ptr;
    while (sz)
    {
        size_t rv = fwrite(c, 1, sz, fp);
        if (!rv)
            return false;
        ptr += rv;
        sz -= rv;
    }
    return true;
}
bool crab_file_save(CrabFile *c, int flags)
{
    static char zeros[8] = "";

    bool ok = true;
    FILE *fp = NULL;
    char *filename_tmp = NULL;
    CrabFileHeader fh;
    CrabSectionHeader sh;
    uint32_t i;
    uint32_t num_sections = c->num_sections;
    size_t section_offset;
    size_t file_size = offsetof(CrabFileHeader, section_info);
    file_size += num_sections * sizeof(CrabSectionHeader);
    section_offset = file_size;
    for (i = 0; i < num_sections; ++i)
    {
        if (file_size & 7)
            abort();
        file_size += c->sections[i]->data_size;
        if (file_size & 7)
            file_size += 8 - (file_size & 7);
    }

    {
        filename_tmp = TRY_P(memdup_plus, (c->filename, c->filename_len + 1, strlen(".new")));
        strcpy(filename_tmp + c->filename_len, ".new");
        fp = TRY_P(fopen, (filename_tmp, "w"));

        memcpy(fh.magic, CRAB_MAGIC, 8);
        fh.size = file_size;
        fh.reserved = 0;
        fh.num_sections = num_sections;
        TRY_B(fwrite_harder, (fp, &fh, offsetof(CrabFileHeader, section_info)));

        for (i = 0; i < num_sections; ++i)
        {
            CrabSection *s = c->sections[i];
            if (section_offset & 7)
                abort();
            sh.offset = section_offset;
            sh.size = s->data_size;
            sh.schema = s->local_schema_id;
            sh.purpose = s->purpose;
            TRY_B(fwrite_harder, (fp, &sh, sizeof(sh)));
            section_offset += c->sections[i]->data_size;
            if (section_offset & 7)
                section_offset += 8 - (section_offset & 7);
        }

        for (i = 0; i < num_sections; ++i)
        {
            CrabSection *s = c->sections[i];
            TRY_B(fwrite_harder, (fp, s->data, s->data_size));
            if (s->data_size & 7)
                TRY_B(fwrite_harder, (fp, zeros, 8 - (s->data_size & 7)));
        }

        TRY(fflush, (fp));
        if (-1 == fclose(fp))
            die("fclose");
        fp = NULL;
        TRY(rename, (filename_tmp, c->filename));
    }

    if (flags & CRAB_CLOSE_FLAG_REOPEN)
    {
        crab_file_close_partial(c, false);
        crab_file_open_partial(c, false);
    }
    goto out;
err:
    ok = false;
    maybe_perror(c);
out:
    if (filename_tmp)
        free(filename_tmp);
    if (fp != NULL)
    {
        if (-1 == fclose(fp))
            die("fclose");
    }
    return ok;
}

void crab_file_error(CrabFile *c, const char **msg, int *no)
{
    *msg = c->error_message;
    *no = c->error_number;
}


uint32_t crab_file_num_sections(CrabFile *c)
{
    return c->num_sections;
}

CrabSection *crab_file_section(CrabFile *c, uint32_t i)
{
    if (i < c->num_sections)
        return c->sections[i];
    c->error_message = "<section index>";
    c->error_number = EINVAL;
    maybe_perror(c);
    return NULL;
}

static char *add_schema(CrabFile *c, const char *schema_url, uint16_t *schema_id)
{
    size_t schema_url_len1 = strlen(schema_url) + 1;
    CrabSection *schema_section = c->sections[0];
    CrabSchemaData *schema_data = (CrabSchemaData *)schema_section->data;
    CrabSection *string_section = c->sections[schema_section->section_number + schema_data->string_section];
    char *string_data = (char *)string_section->data;
    size_t string_data_size = string_section->data_size;
    uint16_t num_schemas = schema_data->num_schemas;
    uint16_t i;
    /* We can afford less checking because update_schemas() has succeeded. */
    for (i = 0; i < num_schemas; ++i)
    {
        uint32_t url, url_start;
        char *url_string;
        url = schema_data->schemas[i].url;
        url_start = url >> STRING_SIZE_BITS;
        /* No overflow, since inputs only have 32 bits *between* them. */
        url_string = string_data + url_start;
        if (strcmp(schema_url, url_string) == 0)
        {
            *schema_id = i;
            return url_string;
        }
    }

    /* we have to add a new one */
    *schema_id = num_schemas;
    {
        if (!(uint16_t)(num_schemas + 1))
            ERROR2("<num schemas>", EOVERFLOW);
        size_t new_size = schema_section->data_size + sizeof(schema_data->schemas[0]);
        if (schema_section->flags & CRAB_SECTION_FLAG_OWN)
        {
            schema_data = TRY_P(realloc, (schema_data, new_size));
        }
        else
        {
            schema_data = TRY_P(memdup_plus, (schema_data, schema_section->data_size, sizeof(schema_data->schemas[0])));
            schema_section->flags |= CRAB_SECTION_FLAG_OWN;
        }
        schema_section->data = (CrabAbstractData *)schema_data;
        schema_section->data_size = new_size;
    }
    {
        size_t new_size = string_section->data_size + schema_url_len1;
        if (schema_url_len1 >= (1 << STRING_SIZE_BITS))
            ERROR2("<string bytes>", EOVERFLOW);
        if (new_size >= (1 << (32 - STRING_SIZE_BITS)))
            ERROR2("<string bytes>", EOVERFLOW);
        if (string_section->flags & CRAB_SECTION_FLAG_OWN)
        {
            string_data = TRY_P(realloc, (string_data, new_size));
        }
        else
        {
            string_data = TRY_P(memdup_plus, (string_data, string_section->data_size, schema_url_len1));
            string_section->flags |= CRAB_SECTION_FLAG_OWN;
        }
        string_section->data = (CrabAbstractData *)string_data;
        string_section->data_size = new_size;
    }
    {
        uint32_t url = (string_data_size << STRING_SIZE_BITS) | (schema_url_len1 - 1);
        uint16_t num_schemas = schema_data->num_schemas;
        memcpy(string_data + string_data_size, schema_url, schema_url_len1);
        schema_data->schemas[num_schemas].url = url;
        schema_data->schemas[num_schemas].reserved = 0;
        schema_data->num_schemas = num_schemas + 1;
    }
    update_schemas(c);
    return string_data + string_data_size;
err:
    return NULL;
}
CrabSection *crab_file_section_add(CrabFile *c)
{
    CrabSection *s = NULL;
    uint32_t si = c->num_sections;
    uint32_t new_num_sections = si + 1;
    if (!new_num_sections)
        ERROR2("<num sections>", EOVERFLOW);
    c->sections = TRY_P(realloc, (c->sections, new_num_sections * sizeof(c->sections[0])));
    s = c->sections[si] = TRY_P(calloc, (1, sizeof(*c->sections[si])));
    s->c = c;
    s->section_number = si;
    s->schema = TRY_P(add_schema, (c, CRAB_SCHEMA, &s->local_schema_id));

    c->num_sections = new_num_sections;
    return s;
err:
    free(s);
    maybe_perror(c);
    return NULL;
}


uint32_t crab_section_number(CrabSection *s)
{
    return s->section_number;
}

const char *crab_section_schema(CrabSection *s)
{
    return s->schema;
}

uint16_t crab_section_purpose(CrabSection *s)
{
    return s->purpose;
}

bool crab_section_set_schema_and_purpose(CrabSection *s, const char *schema, uint16_t purpose)
{
    CrabFile *c = s->c;

    s->schema = TRY_P(add_schema, (c, schema, &s->local_schema_id));
    s->purpose = purpose;
    return true;
err:
    maybe_perror(c);
    return false;
}

size_t crab_section_data_size(CrabSection *s)
{
    return s->data_size;
}

CrabAbstractData *crab_section_data(CrabSection *s)
{
    return s->data;
}

bool crab_section_copy(CrabSection *s, int flags, CrabSection *other)
{
    CrabFile *c = s->c;
    uint16_t new_schema_id;
    char *new_schema = TRY_P(add_schema, (c, CRAB_SCHEMA, &new_schema_id));
    uint16_t new_purpose = other->purpose;
    if (flags & CRAB_SECTION_FLAG_OWN)
    {
        if (s->flags & CRAB_SECTION_FLAG_OWN)
            free(s->data);
        s->flags = other->flags;
        s->data = other->data;
        s->data_size = other->data_size;
        other->flags = 0;
        other->data = NULL;
        other->data_size = 0;
    }
    else if (flags & CRAB_SECTION_FLAG_BORROW)
    {
        if (s->flags & CRAB_SECTION_FLAG_OWN)
            free(s->data);
        s->flags = 0;
        s->data = other->data;
        s->data_size = other->data_size;
    }
    else
    {
        size_t data_size = other->data_size;
        CrabAbstractData *new_data = TRY_P(malloc, (data_size));
        memcpy(new_data, other->data, data_size);
        if (s->flags & CRAB_SECTION_FLAG_OWN)
            free(s->data);
        s->flags = 0;
        s->data = new_data;
        s->data_size = data_size;
    }
    s->schema = new_schema;
    s->local_schema_id = new_schema_id;
    s->purpose = new_purpose;
    return true;
err:
    maybe_perror(c);
    return false;
}

bool crab_section_set_data(CrabSection *s, int flags, CrabAbstractData *data, size_t size)
{
    CrabFile *c = s->c;
    if (!size)
        data = NULL;
    if (!data)
        flags = CRAB_SECTION_FLAG_BORROW;
    /* These are handled the same here; different when the data is freed. */
    if (flags & (CRAB_SECTION_FLAG_OWN | CRAB_SECTION_FLAG_BORROW))
    {
        if (s->flags & CRAB_SECTION_FLAG_OWN)
            free(s->data);
        s->data = data;
    }
    else
    {
        CrabAbstractData *new_data = TRY_P(memdup, (data, size));
        flags |= CRAB_SECTION_FLAG_OWN;
        if (s->flags & CRAB_SECTION_FLAG_OWN)
            free(s->data);
        s->data = new_data;
    }
    s->data_size = size;
    s->flags = flags;
    return true;
err:
    maybe_perror(c);
    return false;
}
