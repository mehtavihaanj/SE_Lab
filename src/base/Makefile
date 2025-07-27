# STUDENTS: DO NOT MODIFY.
#
# Definitions

CC = gcc
CC_FLAGS = -Wall -g3 -I../../include -I../../include/base -I../../include/pipe -I../../include/cache -pthread
CC_FLAGS += $(EXTRA_FLAGS)
CC_OPTIONS = -c
CC_SO_OPTIONS = -shared -fpic
CC_DL_OPTIONS = -rdynamic
RM = /bin/rm -f
LD = gcc
LIBS = -ldl
MD = gccmakedep

SRCS := \
archsim.c \
elf_loader.c \
err_handler.c \
handle_args.c hw_elts.c \
interface.c \
machine.c mem.c \
proc.c ptable.c

OBJS := $(SRCS:%.c=%.o)

TEST_SRCS := \
elf_loader.c \
err_handler.c \
handle_args.c hw_elts.c \
interface.c \
machine.c mem.c \
proc.c ptable.c

TEST_OBJS := $(TEST_SRCS:%.c=%.o)

# Generic rules

%.o: %.c
	${CC} ${CC_OPTIONS} ${CC_FLAGS} $<

# Targets

all: se clean

se: ${OBJS}

test: ${TEST_OBJS}

depend:
	${MD} -- ${CC_OPTIONS} ${CC_FLAGS} -- ${SRCS}

clean:
	${RM} *.o *.so *.bak
