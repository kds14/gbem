TARGET = ./build/cpu
SOURCES = src\*.c
CC = cl
FLAGS = /Wall
HEADERS = src\*.h

all: $(TARGET)

$(TARGET):$(SOURCES)
	-@ if NOT EXIST "build" mkdir "build"
	$(CC) $(flags) /Fe$@ $** 
