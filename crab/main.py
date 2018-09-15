import argparse
import os
import sys

from .crab import CrabFile, CrabPurpose, CRAB_SCHEMA
from .table import Table


class BetterHelpFormatter(argparse.HelpFormatter):
    def _format_action(self, action):
        if not isinstance(action, argparse._SubParsersAction) or action.help:
            return super()._format_action(action)

        # don't format the metavar, just the subactions
        parts = []
        for subaction in self._iter_indented_subactions(action):
            parts.append(self._format_action(subaction))

        # return a single string
        return self._join_parts(parts)

def u16(s):
    v = int(s)
    if 0 < v < (2**16-1):
        return v
    raise TypeError('int out of range')

def u32(s):
    v = int(s)
    if 0 < v < (2**32-1):
        return v
    raise TypeError('int out of range')

def read_blob(blob_filename):
    if not blob_filename:
        return b''
    with open(blob_filename, 'rb') as f:
        return f.read()

def make_parser():
    exe = os.path.basename(sys.executable)
    main_parser = argparse.ArgumentParser(description='''Perform simple CRAB tasks, for the trivial cases where it's not worth writing a program that uses the library.''',
            prog='%s -m crab' % exe, allow_abbrev=False,
            formatter_class=BetterHelpFormatter)
    subparsers = main_parser.add_subparsers(title='subcommands', dest='subcommand', metavar='SUBCOMMAND')

    new_parser = subparsers.add_parser('new', help='Create a new, empty, CRAB file.')
    new_parser.add_argument('filename', help='CRAB file to create', type=str)

    list_parser = subparsers.add_parser('list', help='List sections of a CRAB file.')
    list_parser.add_argument('filename', help='CRAB file to tabulate', type=str)

    add_parser = subparsers.add_parser('add', help='Add a section to a CRAB file.')
    add_parser.add_argument('filename', type=str)
    # this is tricky, we want multi-blob at once with different schemas
    # thus, parse as "remainder" then handle it ourselves
    add_parser.add_argument('remainder', nargs=argparse.REMAINDER, metavar='--schema=|--purpose=|blob')

    repurpose_parser = subparsers.add_parser('repurpose', help='Assign schema and purpose to a section to a CRAB file.')
    repurpose_parser.add_argument('filename', type=str)
    repurpose_parser.add_argument('section', type=u32)
    repurpose_parser.add_argument('schema', type=str)
    repurpose_parser.add_argument('purpose', type=u16)

    store_parser = subparsers.add_parser('store', help='Assign data to a section to a CRAB file.')
    store_parser.add_argument('filename', type=str)
    store_parser.add_argument('section', type=u32)
    store_parser.add_argument('blob', type=str)

    wipe_parser = subparsers.add_parser('wipe', help='Remove data from a section to a CRAB file.')
    wipe_parser.add_argument('filename', type=str)
    wipe_parser.add_argument('section', type=u32)

    dump_parser = subparsers.add_parser('dump', help='Get contents of a section of a CRAB file.')
    dump_parser.add_argument('filename', type=str)
    dump_parser.add_argument('section', type=u32)
    dump_parser.add_argument('outfile', type=str)

    return main_parser

def cmd_new(filename):
    with CrabFile(filename, new=True) as c:
        c.save(reopen=False)

def cmd_list(filename):
    with CrabFile(filename) as c:
        t = Table()
        while t.phase():
            t.emit('#')
            t.emit('Schema')
            t.emit('P')
            t.emit('sz')
            t.end_row()
            t.divider_row()

            num_sections = c.num_sections()
            for i in range(num_sections):
                s = c.section(i)
                t.emit(s.number())
                t.emit(s.schema())
                t.emit(s.purpose())
                t.emit(len(s.data()))
                t.end_row()

def cmd_add(filename, remainder):
    # parse the "mixed" remainder
    schema = CRAB_SCHEMA
    purpose = CrabPurpose.Raw

    blobs = []
    for a in remainder:
        if a.startswith('--schema='):
            schema = a[len('--schema='):]
            purpose = CrabPurpose.Error
            continue
        if a.startswith('--purpose='):
            purpose = u16(a[len('--purpose='):])
            continue
        blobs.append((schema, purpose, a))
    if not blobs:
        sys.exit('no blobs added!')

    keepalive = []
    with CrabFile(filename) as c:
        for schema, purpose, blob in blobs:
            s = c.add_section()
            s.set_schema_and_purpose(schema, purpose)
            data = read_blob(blob)
            keepalive.append(data)
            s.set_data(data, borrow=True)
        c.save(reopen=False)

def cmd_repurpose(filename, section, schema, purpose):
    with CrabFile(filename) as c:
        s = c.section(section)
        s.set_schema_and_purpose(schema, purpose)
        c.save(reopen=False)

def cmd_store(filename, section, blob):
    with CrabFile(filename) as c:
        s = c.section(section)
        data = read_blob(blob) # keepalive
        s.set_data(data, borrow=True)
        c.save(reopen=False)

def cmd_wipe(filename, section):
    with CrabFile(filename) as c:
        s = c.section(section)
        s.set_data(b'')
        s.set_schema_and_purpose(CRAB_SCHEMA, CrabPurpose.Error)
        c.save(reopen=False)

def cmd_dump(filename, section, outfile):
    with CrabFile(filename) as c, \
            open(outfile, 'wb') as out:
        s = c.section(section)
        data = s.data()
        out.write(data)

def main():
    main_parser = make_parser()
    ns = main_parser.parse_args()
    cmd = ns.subcommand; del ns.subcommand
    globals()['cmd_' + cmd](**ns.__dict__)
