TARGET=./build/gbem
SOURCES=./src/*.c
CC=gcc
FLAGS=-g -Wall -Werror

all: $(TARGET)

$(TARGET):$(SOURCES)
	mkdir -p build
	$(CC) $(FLAGS) -o $@ $^ `sdl2-config --cflags --libs`

clean:
	rm -rf ./build
