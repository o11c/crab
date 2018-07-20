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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>


/*
    WARNING: this API is unstable

    Quick & dirty tabularizer. Use with an extra loop, like:

    table_new(stdout);
    while (table_phase())
    {
        table_emits("header1");
        table_emits("header2");
        table_end_row();
        table_divider_row();
        for (i = 0; i < num_rows)
        {
            table_emits("data1");
            table_emits("data2");
            table_end_row();
        }
    }
*/
typedef struct Table Table;

/*
    Convenient aliases.

    Uses a global variably by default for single-threaded programs, but
    can be re-`#define`ed for flexibility.
*/
#define CRAB_TABLE_VAR crab_table_global
#define table_new(...) (CRAB_TABLE_VAR = crab_table_new(__VA_ARGS__), (void)0)
#define table_drawing(...) crab_table_drawing(CRAB_TABLE_VAR, __VA_ARGS__)
#define table_phase() (crab_table_phase(CRAB_TABLE_VAR) || (CRAB_TABLE_VAR = NULL))
#define table_divider_row() crab_table_divider_row(CRAB_TABLE_VAR)
#define table_end_row() crab_table_end_row(CRAB_TABLE_VAR)
#define table_hold(...) crab_table_hold(CRAB_TABLE_VAR, __VA_ARGS__)

#define table_emitc(...) crab_table_emitc(CRAB_TABLE_VAR, __VA_ARGS__)
#define table_emits(...) crab_table_emits(CRAB_TABLE_VAR, __VA_ARGS__)
#define table_emitu(...) crab_table_emitu(CRAB_TABLE_VAR, __VA_ARGS__)
#define table_emiti(...) crab_table_emiti(CRAB_TABLE_VAR, __VA_ARGS__)


extern Table *CRAB_TABLE_VAR;

Table *crab_table_new(FILE *out);
void crab_table_drawing(Table *t, const char *horiz, const char *vert, const char *cross, const char *pad);
bool crab_table_phase(Table *t);
void crab_table_divider_row(Table *t);
void crab_table_end_row(Table *t);
void crab_table_hold(Table *t, int h);

void crab_table_emitc(Table *t, char c);
void crab_table_emits(Table *t, const char *s);
void crab_table_emitu(Table *t, uintmax_t i);
void crab_table_emiti(Table *t, intmax_t i);
