ENABLE_ASAN = yes

CC = gcc -std=c89
CFLAGS = -g -g3 -O1
CPPFLAGS =
LDLIBS =
LDFLAGS = -O1 -Wl,-rpath,'$${ORIGIN}/../lib'

ifeq '${ENABLE_ASAN}' 'yes'
override CC += -fsanitize=address,undefined -fno-omit-frame-pointer
LD_PRELOAD_FOR_DLOPEN := $(shell ${CC} -print-file-name=libasan.so):$${LD_PRELOAD}
endif

ALL = lib/libcrab.so bin/crab

override CFLAGS += -fno-common -fvisibility=hidden

override CFLAGS += -Werror=all -Werror=extra -Werror=format=2
override CFLAGS += -Werror=unused -Werror=unused-result -Werror=undef
override CFLAGS += -Werror=write-strings -Werror=int-conversion
override CFLAGS += -Werror=missing-declarations -Werror=redundant-decls

override CFLAGS += -MMD -MP
override CPPFLAGS += -I include


default: ${ALL}
all: default build-python-extension
clean: clean-c
clean-c:
	rm -rf bin/ lib/ obj/

bin/%:
	@mkdir -p ${@D}
	${CC} ${LDFLAGS} $^ ${LDLIBS} -o $@
lib/%.so: lib/%.pic.a
	rm -f $@
	${CC} -Wl,-soname,$*.so.0 -shared ${LDFLAGS} -Wl,--push-state -Wl,-whole-archive $^ -Wl,--pop-state ${LDLIBS} -o $@.0
	chmod -x $@.0
	ln -s $*.so.0 $@
lib/%.pic.a:
	@mkdir -p ${@D}
	rm -f $@
	ar cr $@ $^
obj/%.o: src/%.c
	@mkdir -p ${@D}
	${CC} -fPIC ${CPPFLAGS} ${CFLAGS} -c -o $@ $<

lib/libcrab.pic.a: $(filter-out obj/main.o,$(patsubst src/%.c,obj/%.o,$(wildcard src/*.c)))

bin/crab: obj/main.o lib/libcrab.so

test: maint-source-per-header
maint-source-per-header:
	for h in include/*.h; do h=$${h#include/}; c=src/$${h%.h}.c; test -f $$c || echo '#include "'$$h'"' > $$c; done
test: test-copyright
test-copyright:
	find src/ include/ -type f -size +22c -exec grep -L Copyright {} +

PWD:=$(dir $(realpath $(firstword ${MAKEFILE_LIST})))
PYTHON3 := $(shell which python3)
export PATH := ${PWD}bin:${PATH}
ASAN_OPTIONS =
ASAN_OPTIONS += detect_stack_use_after_return=1
#ASAN_OPTIONS += malloc_context_size=5
ASAN_OPTIONS += allocator_may_return_null=1
ASAN_OPTIONS += strict_string_checks=1
ASAN_OPTIONS += abort_on_error=1
empty =
space = ${empty} ${empty}
export ASAN_OPTIONS := $(subst ${space},:,${ASAN_OPTIONS})
bin/python3:
	echo '#!/bin/sh' > bin/python3
	echo 'ASAN_OPTIONS=$${ASAN_OPTIONS}:detect_leaks=0 ${PYTHON3} "$$@"' >> bin/python3
	chmod +x bin/python3
py3 = LD_PRELOAD=${LD_PRELOAD_FOR_DLOPEN} python3
shell: bin/python3
	LD_PRELOAD=${LD_PRELOAD_FOR_DLOPEN} bash
python: bin/python3
	${py3}
# Both test-commands and test-python-commands create/write the same files
# and python-unittest checks them.
.NOTPARALLEL:
test: test-commands
test-commands: bin/crab
	crab --help
	crab new test-data/empty.crab
	crab list test-data/empty.crab
	cp test-data/empty.crab test-data/hello.crab
	crab add test-data/hello.crab test-data/hello.txt
	crab list test-data/hello.crab
	crab add test-data/hello.crab test-data/hello.txt --schema=bogus:whatever --purpose=5 ''
	crab list test-data/hello.crab
	crab repurpose test-data/hello.crab 3 bogus:something-else 6
	crab list test-data/hello.crab
	crab store test-data/hello.crab 4 test-data/random.bin
	crab list test-data/hello.crab
	crab wipe test-data/hello.crab 3
	crab list test-data/hello.crab
	crab dump test-data/hello.crab 2 /dev/stdout
	crab dump test-data/hello.crab 4 test-data/random.bin
test-python-commands: bin/python3 build-python-extension lib/libcrab.so
	${py3} -m crab --help
	${py3} -m crab new test-data/empty.crab
	${py3} -m crab list test-data/empty.crab
	cp test-data/empty.crab test-data/hello.crab
	${py3} -m crab add test-data/hello.crab test-data/hello.txt
	${py3} -m crab list test-data/hello.crab
	${py3} -m crab add test-data/hello.crab test-data/hello.txt --schema=bogus:whatever --purpose=5 ''
	${py3} -m crab list test-data/hello.crab
	${py3} -m crab repurpose test-data/hello.crab 3 bogus:something-else 6
	${py3} -m crab list test-data/hello.crab
	${py3} -m crab store test-data/hello.crab 4 test-data/random.bin
	${py3} -m crab list test-data/hello.crab
	${py3} -m crab wipe test-data/hello.crab 3
	${py3} -m crab list test-data/hello.crab
	${py3} -m crab dump test-data/hello.crab 2 /dev/stdout
	${py3} -m crab dump test-data/hello.crab 4 test-data/random.bin
build-python-extension:
	${PYTHON3} -m crab.crab_build
clean: clean-python
clean-python:
	rm -f crab/_crab.[co] crab/_crab.*.so
test: python-unittest
python-unittest: bin/python3 build-python-extension lib/libcrab.so
	${py3} -m unittest discover

# not called by default; don't care about parallel problems.
python-pytest: bin/python3 build-python-extension lib/libcrab.so
	${py3} -m pytest
python-help:
	${py3} -m crab --help
	${py3} -m crab new --help
	${py3} -m crab list --help
	${py3} -m crab add --help
	${py3} -m crab repurpose --help
	${py3} -m crab store --help
	${py3} -m crab wipe --help
	${py3} -m crab dump --help

-include obj/*.d
