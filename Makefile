TARGET = ./build/cpu
SOURCES = src\*.c
CC = cl
FLAGS = /Wall

all: $(TARGET)

$(TARGET):$(SOURCES)
	-@ if NOT EXIST "build" mkdir "build"
	$(CC) $(flags) /Fe$@ /Fo$@ $** 
