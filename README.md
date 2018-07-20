Compact Random-Access Binary

## What is CRAB?

CRAB is a Compact Random-Access Binary format. It is designed for information
that changes rarely, but is read very often.

It is designed to be `mmap`ed and used with minimal overhead. Often, it is
an order of magnitude smaller than the same data in other formats.

This repository contains the canonical C implementation of CRAB. Other
versions may exist.


## What do CRAB files look like?

A CRAB file contains a file header, followed by several section headers.

Each file has a schema and a purpose within that schema. This project itself
defines a schema with several sections, some of which are expected to appear
in all or most CRAB files, while others are rare.

Section 0 always contains the mapping from IDs to URLs. Generally, if you
open a CRAB file you want to read it entirely, and also scan the section
information of all the sections to see which have an interesting purpose.

CRAB files may be merged, but you should probably only do that if they use
different tables from the same schema.

It is entirely legal for sections to overlap or appear in a different order
than "natural". However, some tools will "naturalize" them.

The maximum file size is 2⁶⁴-1 (16 EiB) and the maximum section size is
2³²-1 (32 GiB), although some section types may place further restrictions -
e.g. you might have a compact string section limited to 2²⁴-1 (16 MiB).

Some sections may refer to other sections. These are always stored
relatively in the file so that relocations are unnecessary.

All fields are big-endian, and all sections are 8-byte aligned.

For details, see `crab.h`.


## What is CRAB used for?

My motivation for writing it was to load the Unicode database efficiently.

As such, CRAB almost seems designed to act like a single database table,
with each major section representing one column. However, even within the
original motivator, this is not always true.

When you create your own schema, you are expected to write a user-friendly
library on *top* of CRAB.


## How do I use the CRAB library?

Build `lib/libcrab.so`, then `#include "crab.h"`.

The API is written in C but object-oriented. It's not complicated.


## How do I use the `crab` tool?

Build `bin/crab`, then run it. It's not complicated.

Note that a $ORIGIN-relative rpath is added, at least with the default
LDFLAGS. Contrary to popular report, this is not actually a security concern
as long as you don't make `crab` setuid or something silly like that.


## Is `libcrab.so`'s ABI stable?

Everything in `crab.h`, the sole public header, is stable.

Other functions may be exported for exclusive use by the same-version `crab`
binary. As long as a single package contains both files (and nobody else
uses them), this shouldn't cause any problems.


## BUGS/TODO

* Due to using `mmap`, can't open files > 2 GiB on 32-bit platforms.
  One possibility would be to only map sections as needed, but you probably
  shouldn't be using CRAB files that large anyway.
* Need to implement the "ugly, but even more minimal" API.
* Need to write a pkg-config file and a `make install` target.
* Need to write all the `format.h` stuff, including CFBS.
* Need to actually write all the tooling.
* Need to write a python cffi wrapper.
* Need to write a `make install` target.
