CC?=gcc
SDL2FLAGS=$(shell pkg-config sdl2 --cflags --libs)
CFLAGS?=-std=c11 -pedantic -Werror -Wshadow -Wstrict-aliasing -Wstrict-overflow

.PHONY: all msg clean fullclean

all: msg main

msg:
	@echo '--- C11 ---'

main: src/main.c
	${CC} ${CFLAGS} -O2 -o $@ $< ${SDL2FLAGS}

small: src/main.c
	${CC} ${CFLAGS} -Os -o main $< ${SDL2FLAGS}
	-strip main
	-sstrip main

debug: src/main.c
	${CC} ${CFLAGS} -O1 -g -o main $< ${SDL2FLAGS}

asm: main.asm

main.asm: src/main.c
	${CC} ${CFLAGS} -S -o main.asm $< ${SDL2FLAGS}

run: msg main
	./main

clean:
	rm -f main *.o main.asm

fullclean: clean
