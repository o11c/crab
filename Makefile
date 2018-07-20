CC = gcc -std=c89 -fsanitize=address,undefined -fno-omit-frame-pointer
CFLAGS = -g -g3 -O1
CPPFLAGS =
LDLIBS =
LDFLAGS = -O1 -Wl,-rpath,'$${ORIGIN}/../lib'


ALL = lib/libcrab.so bin/crab

override CFLAGS += -fno-common

override CFLAGS += -Werror=all -Werror=extra -Werror=format=2
override CFLAGS += -Werror=unused -Werror=unused-result -Werror=undef
override CFLAGS += -Werror=write-strings -Werror=int-conversion
override CFLAGS += -Werror=missing-declarations -Werror=redundant-decls

override CFLAGS += -MMD -MP
override CPPFLAGS += -I include


all: ${ALL}
clean:
	rm -rf bin/ lib/ obj/

bin/%:
	@mkdir -p ${@D}
	${CC} ${LDFLAGS} $^ ${LDLIBS} -o $@
lib/%.so:
	@mkdir -p ${@D}
	rm -f $@
	${CC} -Wl,-soname,$*.so.0 -shared ${LDFLAGS} $^ ${LDLIBS} -o $@.0
	chmod -x $@.0
	ln -s $*.so.0 $@
obj/%.o: src/%.c
	@mkdir -p ${@D}
	${CC} -fPIC ${CPPFLAGS} ${CFLAGS} -c -o $@ $<

lib/libcrab.so: $(filter-out obj/main.o,$(patsubst src/%.c,obj/%.o,$(wildcard src/*.c)))

bin/crab: obj/main.o lib/libcrab.so

test: maint-source-per-header
maint-source-per-header:
	for h in include/*.h; do h=$${h#include/}; c=src/$${h%.h}.c; test -f $$c || echo '#include "'$$h'"' > $$c; done
test: test-copyright
test-copyright:
	find src/ include/ -type f -size +22c -exec grep -L Copyright {} +

PWD:=$(dir $(realpath $(firstword ${MAKEFILE_LIST})))
export PATH:=${PWD}bin:${PATH}
ASAN_OPTIONS =
ASAN_OPTIONS += detect_stack_use_after_return=1
#ASAN_OPTIONS += malloc_context_size=5
ASAN_OPTIONS += allocator_may_return_null=1
ASAN_OPTIONS += strict_string_checks=1
ASAN_OPTIONS += abort_on_error=1
empty =
space = ${empty} ${empty}
export ASAN_OPTIONS := $(subst ${space},:,${ASAN_OPTIONS})
shell:
	bash
test: test-commands
test-commands: bin/crab
	crab help
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

-include obj/*.d
