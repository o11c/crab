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
#include "table.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"


struct Table
{
    FILE *out;
    int phase, inhibitions;
    const char *horiz, *vert, *cross, *pad;

    /* These are filled in in the first phase. */
    size_t ncols;
    size_t *col_widths;

    size_t log_col, tw;
    size_t softspace;
};

Table *CRAB_TABLE_VAR;
#undef CRAB_TABLE_VAR
#define CRAB_TABLE_VAR t

Table *crab_table_new(FILE *out)
{
    Table *t = calloc(1, sizeof(*t));
    if (!t)
        die("calloc");
    t->out = out;
    /*
    table_drawing("-", " | ", "-+-", " ");
    table_drawing("─", " │ ", "─┼─", " ");
    */
    table_drawing("═", " │ ", "═╪═", " ");
    return t;
}

void crab_table_drawing(Table *t, const char *horiz, const char *vert, const char *cross, const char *pad)
{
    if (horiz)
        t->horiz = horiz;
    if (vert)
        t->vert = vert;
    if (cross)
        t->cross = cross;
    if (pad)
        t->pad = pad;
}

/*
    Phase 0: before the loop has begun - maybe add some setup options here?
    Phase 1: go through the loop and record all the sizes
    Phase 2: go through the loop and actually emit the cells
    Phase 3: after the loop has ended ... and table has been freed!
*/
bool crab_table_phase(Table *t)
{
    assert (0 <= t->phase && t->phase <= 2);
    assert (t->log_col == 0);
    if (++t->phase == 3)
    {
        free(t->col_widths);
        free(t);
        return false;
    }
    return true;
}

void crab_table_divider_row(Table *t)
{
    size_t i, j;
    if (t->phase == 1)
        return;
    for (i = 0; i < t->ncols; ++i)
    {
        if (i)
            fputs(t->cross, t->out);
        for (j = 0; j < t->col_widths[i]; ++j)
            fputs(t->horiz, t->out);
    }
    fputc('\n', t->out);
}

void crab_table_end_row(Table *t)
{
    t->log_col = 0;
    t->tw = 0;
    t->softspace = 0;
    if (t->phase == 2)
        fputc('\n', t->out);
}

/*
    If you write:

    table_emits("foo");
    table_hold(1);
    table_emits("bar");
    table_emits("baz");
    table_emits("qux");
    table_end_row();

    ... then the output will be:

    foo | barbaz | qux
*/
void crab_table_hold(Table *t, int h)
{
    t->inhibitions += h;
}


void crab_table_emitc(Table *t, char c)
{
    char s[2] = {c, 0};
    table_emits(s);
}

void crab_table_emits(Table *t, const char *s)
{
    /*
        TODO once the unicode database library is available, use it.
        TODO at least support UTF-8 in the meantime.
        (but current data is ASCII-only, and I'm lazy).
    */
    size_t len = strlen(s);

    if (t->log_col == t->ncols)
    {
        t->col_widths = realloc(t->col_widths, sizeof(*t->col_widths) * ++t->ncols);
        if (!t->col_widths)
            die("realloc");
        t->col_widths[t->log_col] = 0;
    }
    assert (t->log_col < t->ncols);

    if (t->phase == 2)
    {
        if (t->softspace)
        {
            t->softspace -= 1;
            while (t->softspace)
            {
                fputs(t->pad, t->out);
                t->softspace -= 1;
            }
            fputs(t->vert, t->out);
        }
        fputs(s, t->out);
    }

    t->tw += len;
    if (t->tw > t->col_widths[t->log_col])
        t->col_widths[t->log_col] = t->tw;

    --t->inhibitions;
    if (t->inhibitions < 0)
    {
        t->softspace = 1 + t->col_widths[t->log_col] - t->tw;

        t->tw = 0;
        ++t->log_col;
        t->inhibitions = 0;
    }
}

void crab_table_emitu(Table *t, uintmax_t i)
{
    char str[sizeof(i)*3+1];
    sprintf(str, "%ju", i);
    table_emits(str);
}

void crab_table_emiti(Table *t, intmax_t i)
{
    char str[1+sizeof(i)*3+1];
    sprintf(str, "%jd", i);
    table_emits(str);
}
