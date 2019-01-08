# game boy emulator

![](https://i.imgur.com/KLglfpV.gif)

Currently runs many of the early games such as Pac-Man, Tetris, Dr. Mario, Super Mario Land, and Metroid 2. Pokemon runs without some minor visual glitches and no clock support.

Supports ROM only and MBC1 cartridges. No audio yet. Passes blargg's cpu_instrs tests.

SDL is setup like this https://wiki.libsdl.org/Installation#Linux.2FUnix

Usage:

gbem [options] \-c cartridge_file


Options:

	-b bs_file 	enables bootstrap ROM startup with given bs_file
	-s 4		sets scale factor of the display to 4, defaults to 2
