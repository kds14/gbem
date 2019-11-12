BUILD=./build
TARGET=$(BUILD)/gbem
SOURCES=./src/*.c
CC=cl
SDL_LIB=..\SDL2\lib\x86\*.lib
SDL_INC=..\SDL2\include

all: $(TARGET)

$(TARGET):$(SOURCES)
	if not exist "$(BUILD)" mkdir build
	$(CC) /O2 /Fe$(TARGET) /Fo$(BUILD)/ $** /I$(SDL_INC) $(SDL_LIB)
