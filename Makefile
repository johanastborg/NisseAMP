CC ?= clang
CFLAGS ?= -O2 -Wall -Wextra -Wno-unused-parameter -I/opt/homebrew/opt/glfw/include -Isrc
LDFLAGS ?= -L/opt/homebrew/opt/glfw/lib -lglfw -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -framework CoreAudio -framework AudioToolbox -framework CoreFoundation

all: src/nisseamp_core.o

src/nisseamp_core.o: src/nisseamp_core.c src/nisseamp_core.h
	$(CC) $(CFLAGS) -c src/nisseamp_core.c -o src/nisseamp_core.o

clean:
	rm -rf src/nisseamp_core.o _Inline lib/*.dylib

run: all
	perl nisseamp.pl

.PHONY: all clean run
