TARGET = ./build/cpu
DIR = ./build/
SOURCES = src/*.c
CC = cl
FLAGS = /Wall
HEADERS = src/*.h
LIBPATH = ./sdl2/lib
INC = ./sdl2/include

all: $(TARGET)

$(TARGET):$(SOURCES)
	-@ if NOT EXIST "build" mkdir "build"
	$(CC) $(flags) /Fe$@ $** /Fo$(DIR) /I $(INC) /link /LIBPATH:$(LIBPATH) SDL2.lib SDL2main.lib
