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


#pragma GCC visibility push(default)

#define unlikely(expr) __builtin_expect(!!(expr), 0)
#define likely(expr) __builtin_expect(!!(expr), 1)

#define die crab_die
#define die2 crab_die2

__attribute__((noreturn))
void die(const char *what);

__attribute__((noreturn))
void die2(const char *what, int e);

#define TRY(f, args)    TRY2(-1, f, args)
#define TRY_P(f, args)  TRY2(NULL, f, args)
#define TRY_B(f, args)  TRY2(false, f, args)
#define TRY2(r, f, args)        \
({                              \
    __auto_type _rv = f args;   \
    if (unlikely(_rv == (r)))   \
        ERROR(#f);              \
    _rv;                        \
})

#define ERROR(f) die(f)

#pragma GCC visibility pop
