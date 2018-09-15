import enum
import errno

from ._crab import ffi as _ffi, lib as _lib


def _constant_to_camel(ident):
    words = ident.split('_')
    words = [w.title() for w in words]
    return ''.join(words)

def _make_enum(prefix):
    name = _constant_to_camel(prefix)
    values = {}
    for k, v in vars(_lib).items():
        if not k.startswith(prefix):
            continue
        values[_constant_to_camel(k[len(prefix):])] = v
    #  IntEnum by design; there are different "enum"s for each schema.
    globals()[name] = enum.IntEnum(name, values)
_make_enum('CRAB_PURPOSE_')
# don't expose enums for flags since I'm targetting python 3.5
# and using bool kwargs is cleaner anyway

# This is `#define`d as a string literal, which CFFI can't handle yet.
CRAB_SCHEMA = 'https://o11c.github.io/crab/schema.html'

class CrabFile:
    def __init__(self, filename, *, write=False, new=False, perror=False):
        ''' Open/create a CRAB file.

            If `write` is True, the data in the file may be written
            directly.  This is not needed for ordinary section manipulation,
            and should be used with extreme caution.

            If `new` is True, an existing file will not be opened, but the
            filename will still be used when `save()` is called.

            If `perror` is True, errors will be sent to stderr as well as
            raising a python exception. Note that unrecoverable errors also
            exist.
        '''
        # forced - it exists for our benefit, after all!
        flags = _lib.CRAB_FILE_FLAG_ERROR
        if write:
            flags |= _lib.CRAB_FILE_FLAG_WRITE
        if new:
            flags |= _lib.CRAB_FILE_FLAG_NEW
        if perror:
            flags |= _lib.CRAB_FILE_FLAG_PERROR
        raw = _lib.crab_file_open(filename.encode('utf-8'), flags)
        if raw == _ffi.NULL:
            raise OSError(_ffi.errno, 'malloc: %s' % errno.strerror(_ffi.errno))
        self._raw = _ffi.gc(raw, _lib.crab_file_close)
        self.raise_error(always=False)

    def close(self):
        ''' Immediately close a CRAB file, instead of relying on the GC.

            Note that CRAB files do not keep an open file descriptor.
        '''
        if 1:
            _ffi.gc(self._raw, None)
            rv = _lib.crab_file_close(self._raw)
            assert rv, 'errors in `close` should abort() before this!'
        self._raw = None

    def __enter__(self):
        return self
    def __exit__(self, ty, v, tb):
        self.close()

    def raise_error(self, *, always=True):
        ''' Utility function to turn message+errno pairs into exceptions.

            Usually this library will take care of this all for you.
        '''
        msg_ptr = _ffi.new('char **')
        no_ptr = _ffi.new('int *')
        _lib.crab_file_error(self._raw, msg_ptr, no_ptr)
        msg = msg_ptr[0]
        no = no_ptr[0]
        if msg == _ffi.NULL:
            if not always:
                return
            raise TypeError('expected an error to exist!')
        msg = _ffi.string(msg).decode('ascii')
        raise OSError(no, '%s: %s' % (msg, errno.strerror(no)))

    def save(self, *, reopen):
        ''' Save the current sections to the file.

            If `reopen` is True, then re-`mmap` the sections from the new
            file to save memory. This is pointless if you're just going to
            close the file immediately. IMPORTANT: This will invalidate,
            without checking, any existing Section.data() pointers (although
            if they were borrowed there will now be multiple value pointers).

            This uses the "exclusive creation + atomic rename" paradigm.
        '''
        flags = 0
        if reopen:
            flags |= _lib.CRAB_SAVE_FLAG_REOPEN
        if not _lib.crab_file_save(self._raw, flags):
            self.raise_error()

    def num_sections(self):
        ''' Number of sections in the file.
        '''
        return _lib.crab_file_num_sections(self._raw)

    def section(self, i):
        ''' Get one of the existing sections.

            Currently, this returns a different Python object each time, but
            it might do weak caching later?
        '''
        # TODO weakref cache?
        raw_section = _lib.crab_file_section(self._raw, i)
        if raw_section == _ffi.NULL:
            self.raise_error()
        return CrabSection(self, raw_section)

    def add_section(self):
        ''' Add a section to the file.
        '''
        raw_section = _lib.crab_file_section_add(self._raw)
        if raw_section == _ffi.NULL:
            self.raise_error()
        return CrabSection(self, raw_section)


class CrabSection:
    def __init__(self, c, raw):
        ''' <internal, call `CrabFile.section` instead>
        '''
        # keep file alive
        self._crab_file = c
        self._raw = raw

    def __hash__(self):
        return hash((self._crab_file, self._raw))

    def __eq__(self, other):
        if not isinstance(other, CrabSection):
            return NotImplemented
        return (self._crab_file, self._raw) == (other._crab_file, other._raw)

    def raise_error(self):
        ''' Utility function to call raise_error() on the containing file.
        '''
        return self._crab_file.raise_error()

    def number(self):
        ''' Get the number of this section.

            Normally, you'd already know this from how you *acquired* the
            section object, but just in case ...
        '''
        return _lib.crab_section_number(self._raw)

    def schema(self):
        ''' Return the schema URL for this section.
        '''
        rv = _lib.crab_section_schema(self._raw)
        if rv == _ffi.NULL:
            self.raise_error()
        return _ffi.string(rv).decode('ascii')

    def purpose(self):
        ''' Return the purpose within the schema, as an integer.

            Likely you should be thinking in terms of a schema-provided enum.
        '''
        return _lib.crab_section_purpose(self._raw)

    def set_schema_and_purpose(self, schema, purpose):
        ''' Set the schema URL and purpose number together.

            It doesn't make sense to set either on its own.
        '''
        schema = schema.encode('ascii')
        if not _lib.crab_section_set_schema_and_purpose(self._raw, schema, purpose):
            self.raise_error()

    def data(self):
        ''' View the data content of the section.

            The returned buffer is invalidated, without checking, by either
            `.close()` or `.save(reopen=True)`. Use `[:]` to make a copy.
        '''
        sz = _lib.crab_section_data_size(self._raw)
        ptr = _lib.crab_section_data(self._raw)
        return _ffi.buffer(ptr, sz)

    def set_data(self, b, *, own=False, borrow=False):
        ''' Set the section's data directly.

            If `own` is True, the pointer must be `malloc`ed; the section
            will take ownership. In python, this is rarely what you want.

            If `borrow` is True, the caller must keep the pointer valid
            until the file is closed or save(reopen=True)ed. This is useful
            if it's a large non-`malloc` allocation, for example.

            If both are False, the data will be copied.

            It is an error for both to be True.
        '''
        flags = 0
        if own:
            flags |= _lib.CRAB_SECTION_FLAG_OWN
        if borrow:
            flags |= _lib.CRAB_SECTION_FLAG_BORROW
        b = _ffi.from_buffer(b)
        if not _lib.crab_section_set_data(self._raw, flags, _ffi.cast('CrabAbstractData *', b), len(b)):
            self.raise_error()

    def copy_from(self, other, *, own=False, borrow=False):
        ''' Set the section's data from another section.

            If `own` is True, the data will be stolen; the other section's
            data will become zero-sized. Beware if the other section was
            previously borrowed or was still mapped from the file.

            If `borrow` is True, then the other section's data must remain
            valid until this section's file is closed or saved(reopen=True)ed.
            There are a lot of corner cases here (was the other section's
            data already borrowed? what if you reopen the other file?).

            If both are False, the data will be copied.

            It is an error for both to be True.
        '''
        flags = 0
        if own:
            flags |= _lib.CRAB_SECTION_FLAG_OWN
        if borrow:
            flags |= _lib.CRAB_SECTION_FLAG_BORROW
        if not _lib.crab_section_copy(self._raw, flags, other._raw):
            self.raise_error()
