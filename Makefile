# Definitions

CC = gcc
CC_FLAGS = -Wall -g3 -Iinclude -Iinclude/base -Iinclude/pipe -Iinclude/cache -pthread

EXTRA_FLAGS = -DEC
export EXTRA_FLAGS

CC_OPTIONS = -c
CC_SO_OPTIONS = -shared -fpic
CC_DL_OPTIONS = -rdynamic
RM = /bin/rm -f
MV = mv -f
LD = gcc
LIBS = -ldl
MD = gccmakedep

# Generic rules

%.o: %.c
	${CC} ${CC_OPTIONS} ${CC_FLAGS} $<

# Targets

week4: tidy pipe test clean
week3: tidy pipe test clean
week2: tidy pipeminus test clean
week1: tidy test clean

pipe:
	$(eval EXTRA_FLAGS += -DPIPE -UPARALLEL)
	(cd src && make se)
	${CC} ${CC_FLAGS} -I instr -o bin/se `/bin/ls src/base/*.o src/pipe/*.o src/cache/cache.o`

parallel:
	$(eval EXTRA_FLAGS += -DPARALLEL -DPIPE)
	(cd src && make se)
	${CC} ${CC_FLAGS} -I instr -o bin/se `/bin/ls src/base/*.o src/pipe/*.o src/cache/cache.o`

pipeminus:
	$(eval EXTRA_FLAGS += -UPIPE -UPARALLEL)
	(cd src && make se)
	${CC} ${CC_FLAGS} -I instr -o bin/se `/bin/ls src/base/*.o src/pipe/*.o src/cache/cache.o`

parallel_pipeminus:
	$(eval EXTRA_FLAGS += -UPIPE -DPARALLEL)
	(cd src && make se)
	${CC} ${CC_FLAGS} -I instr -o bin/se `/bin/ls src/base/*.o src/pipe/*.o src/cache/cache.o`

test:
	(cd src && make $@)
	${CC} ${CC_FLAGS} -I instr -o bin/test-se src/testbench/test-se.o
	${CC} ${CC_FLAGS} -I instr -o bin/test-csim src/testbench/test-csim.o
	${CC} ${CC_FLAGS} -I instr -o bin/test-hw `/bin/ls src/base/elf_loader.o src/base/err_handler.o src/base/hw_elts.o src/base/interface.o src/base/machine.o src/base/mem.o src/base/proc.o src/base/ptable.o src/pipe/*.o src/cache/cache.o src/testbench/test-hw.o`

depend:
	(cd src && make $@)

clean:
	(cd src && make $@)
	${RM} *.o *.so *.bak

tidy:
	${RM} bin/se bin/test-se bin/test-csim bin/csim

count:
	wc -l src/base/*.c src/pipe/*.c src/cache/*.c | tail -n 1
	wc -l include/base/*.h include/pipe/*.h include/cache/*.h | tail -n 1

