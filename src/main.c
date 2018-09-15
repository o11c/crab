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
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "crab.h"
#include "schema.h"
#include "table.h"
#include "util.h"


static uint16_t parse_u16(const char *arg)
{
    char *end;
    unsigned long v = strtoul(arg, &end, 0);
    uint16_t rv = v;
    errno = EOVERFLOW;
    if (!v || rv == (uint16_t)-1 || v != rv || *end || !isdigit(*arg))
        die("strtoul");
    return rv;
}
static uint32_t parse_u32(const char *arg)
{
    char *end;
    unsigned long v = strtoul(arg, &end, 0);
    uint32_t rv = v;
    errno = EOVERFLOW;
    if (!v || rv == (uint32_t)-1 || v != rv || *end || !isdigit(*arg))
        die("strtoul");
    return rv;
}
static CrabAbstractData *mmap_file(const char *fn, size_t *rv_size)
{
    int fd = TRY(open, (fn, O_RDONLY));
    CrabAbstractData *rv;
    struct stat stat_buf;
    size_t file_size;
    TRY(fstat, (fd, &stat_buf));
    file_size = stat_buf.st_size;
    if (file_size != (uintmax_t)stat_buf.st_size)
        die2("<file size>", EOVERFLOW);
    rv = TRY2(MAP_FAILED, mmap, (NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0));
    TRY(close, (fd));
    *rv_size = file_size;
    return rv;
}

typedef int (*Cmd)(int argc, char **argv);

static int cmd_help(int argc, char **argv);
static int cmd_new(int argc, char **argv)
{
    CrabFile *c;
    if (argc != 1)
    {
        puts("Usage: `crab new <filename.crab>`");
        return 1;
    }
    c = crab_file_open(argv[0], CRAB_FILE_FLAG_PERROR | CRAB_FILE_FLAG_NEW);
    if (!c)
        return 1;
    if (!crab_file_save(c, 0))
    {
        (void)crab_file_close(c);
        return 1;
    }
    if (!crab_file_close(c))
        return 1;
    return 0;
}
static int cmd_list(int argc, char **argv)
{
    CrabFile *c;
    uint32_t num_sections, i;
    if (argc != 1)
    {
        puts("Usage: `crab list <filename.crab>`");
        return 1;
    }
    c = crab_file_open(argv[0], CRAB_FILE_FLAG_PERROR);
    if (!c)
        return 1;
    table_new(stdout);
    while (table_phase())
    {
        table_emits("#");
        table_emits("Schema");
        table_emits("P");
        table_emits("sz");
        table_end_row();
        table_divider_row();

        num_sections = crab_file_num_sections(c);
        for (i = 0; i < num_sections; ++i)
        {
            CrabSection *s = crab_file_section(c, i);
            table_emitu(crab_section_number(s));
            table_emits(crab_section_schema(s));
            table_emitu(crab_section_purpose(s));
            table_emitu(crab_section_data_size(s));
            table_end_row();
        }
    }

    if (!crab_file_close(c))
        return 1;
    return 0;
}
static int cmd_add(int argc, char **argv)
{
    CrabFile *c = NULL;
    CrabSection *s;
    const char *schema = CRAB_SCHEMA;
    uint16_t purpose = CRAB_PURPOSE_RAW;
    int sections_added = 0, i;
    CrabAbstractData *blob = 0;
    size_t blob_size = 0;
    if (argc < 1)
        goto usage;
    c = crab_file_open(argv[0], CRAB_FILE_FLAG_PERROR);
    if (!c)
        return 1;
    for (i = 1; i < argc; ++i)
    {
        if (strncmp(argv[i], "--schema=", strlen("--schema=")) == 0)
        {
            schema = argv[i] + strlen("--schema=");
            purpose = CRAB_PURPOSE_ERROR;
            continue;
        }
        if (strncmp(argv[i], "--purpose=", strlen("--purpose=")) == 0)
        {
            purpose = parse_u16(argv[i] + strlen("--purpose="));
            continue;
        }
        ++sections_added;
        if (*argv[i])
        {
            blob = TRY_P(mmap_file, (argv[i], &blob_size));
        }

        s = TRY_P(crab_file_section_add, (c));
        TRY_B(crab_section_set_schema_and_purpose, (s, schema, purpose));
        if (blob)
        {
            TRY_B(crab_section_set_data, (s, 0, blob, blob_size));

            TRY(munmap, (blob, blob_size));
            blob = NULL;
            blob_size = 0;
        }
    }

    if (!sections_added)
        goto usage;
    TRY_B(crab_file_save, (c, 0));
    TRY_B(crab_file_close, (c));
    return 0;

usage:
    puts("Usage: crab add <filename.crab> [--schema=<url>] [--purpose=<number>] {<blob> | ''}...");
    if (c)
        TRY_B(crab_file_close, (c));
    if (blob)
        TRY(munmap, (blob, blob_size));
    return 1;
}
static int cmd_repurpose(int argc, char **argv)
{
    CrabFile *c = NULL;
    uint32_t section;
    CrabSection *s;
    const char *schema;
    uint16_t purpose;
    if (argc != 4)
    {
        puts("Usage: crab repurpose <filename.crab> <section-number> <schema> <purpose>");
        return 1;
    }
    c = crab_file_open(argv[0], CRAB_FILE_FLAG_PERROR);
    if (!c)
        return 1;

    section = parse_u32(argv[1]);
    schema = argv[2];
    purpose = parse_u16(argv[3]);
    s = crab_file_section(c, section);
    if (!s)
        goto fail;
    if (!crab_section_set_schema_and_purpose(s, schema, purpose))
        goto fail;
    if (!crab_file_save(c, 0))
        goto fail;

    if (!crab_file_close(c))
        return 1;
    return 0;
fail:
    if (c)
        (void)crab_file_close(c);
    return 1;
}
static int cmd_store(int argc, char **argv)
{
    CrabFile *c = NULL;
    uint32_t section;
    CrabSection *s;
    CrabAbstractData *blob = 0;
    size_t blob_size = 0;
    if (argc != 3)
    {
        puts("Usage: crab store <filename.crab> <section-number> {<blob> | ''}");
        return 1;
    }
    c = crab_file_open(argv[0], CRAB_FILE_FLAG_PERROR);
    if (!c)
        return 1;

    section = parse_u32(argv[1]);
    if (*argv[2])
    {
        blob = mmap_file(argv[2], &blob_size);
        if (!blob)
            goto fail;
    }
    s = crab_file_section(c, section);
    if (!s)
        goto fail;
    if (!crab_section_set_data(s, CRAB_SECTION_FLAG_BORROW, blob, blob_size))
        goto fail;

    if (!crab_file_save(c, 0))
        goto fail;
    if (blob)
        TRY(munmap, (blob, blob_size));
    if (!crab_file_close(c))
        return 1;
    return 0;
fail:
    if (blob)
        TRY(munmap, (blob, blob_size));
    if (c)
        (void)crab_file_close(c);
    return 1;
}
static int cmd_wipe(int argc, char **argv)
{
    CrabFile *c = NULL;
    uint32_t section;
    CrabSection *s;
    if (argc != 2)
    {
        puts("Usage: crab wipe <filename.crab> <section-number>");
        return 1;
    }
    c = crab_file_open(argv[0], CRAB_FILE_FLAG_PERROR);
    if (!c)
        return 1;

    section = parse_u32(argv[1]);
    s = crab_file_section(c, section);
    if (!s)
        goto fail;
    if (!crab_section_set_data(s, 0, NULL, 0))
        goto fail;
    if (!crab_section_set_schema_and_purpose(s, CRAB_SCHEMA, CRAB_PURPOSE_ERROR))
        goto fail;

    if (!crab_file_save(c, 0))
        goto fail;
    if (!crab_file_close(c))
        return 1;
    return 0;
fail:
    if (c)
        (void)crab_file_close(c);
    return 1;
}
static int cmd_dump(int argc, char **argv)
{
    CrabFile *c = NULL;
    uint32_t section;
    CrabSection *s;
    char *data;
    size_t data_size;
    FILE *out = NULL;
    if (argc != 3)
    {
        puts("Usage: crab dump <filename.crab> <section-number> <out-file>");
        return 1;
    }
    c = crab_file_open(argv[0], CRAB_FILE_FLAG_PERROR);
    if (!c)
        return 1;

    section = parse_u32(argv[1]);
    out = fopen(argv[2], "w");
    if (!out)
        goto fail;
    s = crab_file_section(c, section);
    if (!s)
        goto fail;
    data = (char *)crab_section_data(s);
    data_size = crab_section_data_size(s);
    while (data_size)
    {
        size_t tmp = fwrite(data, 1, data_size, out);
        if (!tmp)
            goto fail;
        data_size -= tmp;
        data += tmp;
    }

    TRY(fflush, (out));
    TRY(fclose, (out));
    if (!crab_file_close(c))
        return 1;
    return 0;
fail:
    if (out)
        TRY(fclose, (out));
    (void)crab_file_close(c);
    return 1;
}

struct
{
    const char *name;
    Cmd command;
    const char *help;
} commands[] =
{
    {"--help", cmd_help, "Get help."},
    {"new", cmd_new, "Create a new, empty, CRAB file."},
    {"list", cmd_list, "List sections of a CRAB file."},
    {"add", cmd_add, "Add a section to a CRAB file."},
    {"repurpose", cmd_repurpose, "Assign schema and purpose to a section to a CRAB file."},
    {"store", cmd_store, "Assign data to a section to a CRAB file."},
    {"wipe", cmd_wipe, "Remove data from a section to a CRAB file."},
    {"dump", cmd_dump, "Get contents of a section of a CRAB file."},
};
#define NUM_COMMANDS (sizeof(commands)/sizeof(commands[0]))

static int cmd_help(int argc, char **argv)
{
    size_t i;
    if (argc != 0)
    {
        puts("`crab help` does not take any arguments");
        return 1;
    }
    (void)argv;
    printf("Subcommands:\n\n");
    table_new(stdout);
    table_drawing(NULL, "  ", NULL, NULL);
    while (table_phase())
    {
        for (i = 0; i < NUM_COMMANDS; ++i)
        {
            table_hold(1);
            table_emits("  crab ");
            table_emits(commands[i].name);

            table_emits(commands[i].help);
            table_end_row();
        }
    }
    puts("");
    return 0;
}

int main(int argc, char **argv)
{
    if (argc >= 2)
    {
        size_t i;
        if (strcmp(argv[1], "-h") == 0)
            argv[1] = (char *)"--help";
        for (i = 0; i < NUM_COMMANDS; ++i)
        {
            if (strcmp(argv[1], commands[i].name) == 0)
                return commands[i].command(argc - 2, argv + 2);
        }
    }
    puts("Try `crab help`.");
    return 1;
}
