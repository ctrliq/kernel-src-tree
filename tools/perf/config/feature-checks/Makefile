
FILES=					\
	test-hello			\
	test-stackprotector-all		\
	test-stackprotector		\
	test-volatile-register-var	\
	test-fortify-source		\
	test-bionic			\
	test-libelf			\
	test-glibc			\
	test-dwarf			\
	test-libnuma

CC := $(CC) -MD

all: $(FILES)

BUILD = $(CC) -o $(OUTPUT)$@ $@.c

###############################

test-all:
	$(BUILD) -Werror -fstack-protector -fstack-protector-all -Wvolatile-register-var -O2 -Werror -D_FORTIFY_SOURCE=2 -ldw -lelf -lnuma -lunwind -lunwind-x86_64 -lelf -laudit -I/usr/include/slang -lslang $(shell pkg-config --libs --cflags gtk+-2.0 2>/dev/null) $(FLAGS_PERL_EMBED) $(FLAGS_PYTHON_EMBED) -DPACKAGE='perf' -DPACKAGE=perf -lbfd -ldl

test-hello:
	$(BUILD)

test-stackprotector-all:
	$(BUILD) -Werror -fstack-protector-all

test-stackprotector:
	$(BUILD) -Werror -fstack-protector

test-volatile-register-var:
	$(BUILD) -Werror -Wvolatile-register-var

test-fortify-source:
	$(BUILD) -O2 -Werror -D_FORTIFY_SOURCE=2

test-bionic:
	$(BUILD)

test-libelf:
	$(BUILD) -lelf

test-glibc:
	$(BUILD)

test-dwarf:
	$(BUILD) -ldw

test-libnuma:
	$(BUILD) -lnuma

-include *.d */*.d

###############################

clean:
	rm -f $(FILES) *.d
