# Modify these two lines to choose your compiler and compile time
# flags.

CC=gcc
INC= -I../../include -I../../include/base -I../../include/pipe -I../../include/cache
CC_FLAGS= -Wall -Wno-unused-function -g3 -O0 ${INC}
CC_OPTIONS = -c
CC_SO_OPTIONS = -shared -fpic
CC_DL_OPTIONS = -rdynamic

##################################################
# You shouldn't need to modify anything below here
##################################################
SRCS := \
csim.c \
cache.c

OBJS := $(SRCS:%.c=%.o)

# Generic rules

%.o: %.c
	${CC} ${CC_OPTIONS} ${CC_FLAGS} $<


all: csim

# cache.o: cache.c
# 	${CC} ${INC} ${CFLAGS} -c -o cache.o cache.c

se: all

test: all

csim: ${OBJS}
	$(CC) $(CC_FLAGS) -o ../../bin/$@ ${OBJS} -lm

# test-cache: csim test-csim.c
# 	$(CC) $(CFLAGS) -o test-csim test-csim.c

clean:
	rm -f test-csim csim *.o *.exe *~ 
