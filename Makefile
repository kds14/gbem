TARGET=./build/cpu
SOURCES=./src/*.c
CC=gcc
FLAGS=-Wall -Werror
LIBPATH=./sdl2/lib
INC=./sdl2/include

all: $(TARGET)

$(TARGET):$(SOURCES)
	mkdir -p build
	$(CC) $(FLAGS) -o $@ $^ `sdl2-config --cflags --libs`

clean:
	rm -rf ./build
