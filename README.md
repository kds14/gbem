# game boy emulator

![](https://i.imgur.com/KLglfpV.gif)

Currently runs many of the early games such as Pac-Man, Tetris, Dr. Mario, Super Mario Land, and Metroid 2. Pokemon runs without some minor visual glitches and no clock support.

Supports ROM only and MBC1 cartridges. No audio yet. Passes blargg's cpu_instrs tests.

Linux build steps:
1. Install SDL like this this https://wiki.libsdl.org/Installation#Linux.2FUnix
2. Run make

Windows build steps:
1. Get SDL2 VC++ version from https://libsdl.org/download-2.0.php
2. Edit Makefile to use SDL2 include and lib folders
3. Using Developer Command Prompt for VS 2019 run nmake
4. Put SDL2.dll in the build folder

Usage:

gbem [options] \-c cartridge_file


Options:

	-b bs_file 	enables bootstrap ROM startup with given bs_file
	-s 4		sets scale factor of the display to 4, defaults to 2
