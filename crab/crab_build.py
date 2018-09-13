from cffi import FFI
import glob
import os

ffibuilder = FFI()

headers = '''
fwd.h
crab.h
schema.h
'''.split()

ffibuilder.set_source('crab._crab',
        ''.join(['#include "%s"\n' % h for h in headers]),
        include_dirs=['./include/'],
        library_dirs=['./lib/'],
        libraries=['crab'],
        runtime_library_dirs=['${ORIGIN}/../lib']
)
for h in headers:
    with open(os.path.join('include', h)) as f:
        s = f.read().replace('#', '//#')
        # hack: remove unparseable types from schema.h
        s = s.split('struct __attribute__((scalar_storage_order("big-endian")))')[0]
        ffibuilder.cdef(s)

if __name__ == '__main__':
    ffibuilder.compile(verbose=True)
