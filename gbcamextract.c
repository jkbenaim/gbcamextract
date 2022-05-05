/*
 * Copyright (c) 2013-2020 jkbenaim et al.
 *
 * This program is free software; you may redistribute and/or modify it under
 * the terms of the expat license (also known as the "MIT license").
 *
 * This program is distributed in the hope that it will be useful, but without
 * any warranty; without even the implied warranty of merchantability or
 * firness for a particular purpose.
 *
 * For the full license text, see LICENSE.
 *
 ******************************************************************************
 *
 * This file implements main() and all of the Game Boy-specific functions.
 *
 */


#include <err.h>
#include <errno.h>      // errno
#include <png.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>      // printf, fopen, fclose, fread
#include <stdlib.h>     // malloc, EXIT_SUCCESS, EXIT_FAILURE, NULL
#include <stdnoreturn.h>
#include <string.h>     // strerror
#include <zlib.h>
#include "mapfile.h"
#include "sram.h"

const int HELLO_KITTY_FRAME_OFFSETS[25][2] = {{0xC6C70, 0xCF5D0}, {0xC3B80, 0xCF548}, {0xCBEC0, 0xCF4C0}, {0xC5F10, 0xCF658}, {0xCF210, 0xCF7F0}, {0xC73A0, 0xCF768}, {0xB7420, 0xCF6E0}, {0xBE3E0, 0xCF438}, {0xB3CD0, 0xC7EF0}, {0xB2B80, 0xCF3B0}, {0x8FD50, 0xC7F78}, {0xC3800, 0xD7800}, {0xBDC00, 0xD3F70}, {0xD7F70, 0xD7888}, {0xC5C00, 0xD7998}, {0xB7C20, 0xD7910}, {0xC3ED0, 0xD3D50}, {0x33F80, 0xD3CC8}, {0xDB800, 0xD3DD8}, {0xB2200, 0xD3EE8}, {0xB34D0, 0xD3E60}, {0xB3030, 0xD7A20}, {0x93E00, 0xD7D50}, {0x77FE0, 0xCFCB8}, {0x77FF0, 0xCFDC4}};
#define ROM_TITLE_OFFSET 0x134
#define ROM_TITLE_LENGTH 0xF

#define BANK_SIZE 0x4000
#define BANK(bank) BANK_SIZE * bank

const int FILE_ERROR = 2;
const int FILE_SIZE_ERROR = 3;

const int WIDTH = 160;
const int ROW_SIZE = 40; // WIDTH/4: 2 bits per pixel means 4 pixels per byte
const int HEIGHT = 144;
const int SAVEGAME_SIZE = 128*1024;
const int ROM_BUFFER_SIZE = 1024*1024;

static inline int picNum2BaseAddress(int picNum);
uint8_t *getFrame(uint8_t rom[], int frameNumber);
static inline unsigned int interleaveBytes(uint8_t low, uint8_t high);
void convert(uint8_t framesBuffer[], uint8_t saveBuffer[], uint8_t pixelBuffer[], int picNum);
void writeImageFile(uint8_t pixelBuffer[], const char *filename);
void drawSpan(uint8_t pixelBuffer[], uint8_t *buffer, int x, int y);
void readData(uint8_t *fileName, uint8_t *buffer, int offset);
bool isGbRom(const uint8_t data[0x150]);
bool isHkRom(const uint8_t rom[0x150]);
static void noreturn usage(void);

bool isGbRom(const uint8_t data[0x150])
{
	const uint8_t sig[] = {0xce, 0xed, 0x66, 0x66};
	if (!memcmp(data + 0x104, sig, 4))
		return true;
	else
		return false;
}

int getPicNumForSlotNum(const uint8_t *save, int slotNum)
{
	int picNum;
	struct firstslot_s *firstslot = (struct firstslot_s *)save;
	if ((slotNum < 1) || (slotNum > 30))
		return -1;
	picNum = firstslot->vec[slotNum-1];
	switch (picNum) {
	case 0 ... 29:
		picNum++;
		break;
	case 255:
	default:
		picNum = -1;
		break;
	}

	return picNum;
}

int main(int argc, char *argv[])
{
	char *filename_save = NULL;
	char *filename_rom = NULL;
	int rc;
	struct MappedFile_s mSave = {0};
	struct MappedFile_s mRom = {0};
	uint8_t pixelBuffer[ROW_SIZE*HEIGHT];

	while ((rc = getopt(argc, argv, "s:r:")) != -1)
		switch (rc) {
		case 's':
			if (filename_save)
				usage();
			filename_save = optarg;
			break;
		case 'r':
			if (filename_rom)
				usage();
			filename_rom = optarg;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;
	if (*argv != NULL)
		usage();

	if (!filename_save)
		usage();

	// Open the save file.
	mSave = MappedFile_Open(filename_save, false);
	if (!mSave.data)
		err(1, "couldn't open save for reading");

	// Check the savegame's size.
	if (mSave.size != 131072)
		errx(1, "savegame has weird size");
	
	// Check if the save that we read is actually a rom.
	if (isGbRom(mSave.data))
		errx(1, "save expected, but rom was given");

	// If a rom was given, open it.
	if (filename_rom) {
		mRom = MappedFile_Open(filename_rom, false);
		if (!mRom.data)
			err(1, "couldn't open rom for reading");
		if (mRom.size != 1048576)
			errx(1, "rom has weird size");
		if (!isGbRom(mRom.data))
			errx(1, "rom given doesn't look like a real rom");
	}

	
	// convert
	memset(pixelBuffer, 0, ROW_SIZE*HEIGHT);    // set pixelBuffer to all black

	for (int slotNum = 1; slotNum <= 30; ++slotNum)
	{
		int picNum = getPicNumForSlotNum(mSave.data, slotNum);
		char filename[16] = {0};
		convert(mRom.data, mSave.data, pixelBuffer, slotNum);
		if (picNum != -1)
			snprintf(filename, 15, "IMG_%02d.png", picNum);
		else
			snprintf(filename, 15, "DEL_%02d.png", slotNum);
		filename[15] = '\0';
		writeImageFile(pixelBuffer, filename);
	}

	// Return
	return EXIT_SUCCESS;
}

bool isHkRom(const uint8_t rom[0x150])
{
	if (!memcmp(rom + ROM_TITLE_OFFSET, "POCKETCAMERA_SN", ROM_TITLE_LENGTH))
		return true;
	else
		return false;
}

uint8_t *getFrame(uint8_t rom[], int frameNumber)
{
	int frameAddress;

	if (!rom) {
		return NULL;
	}

	if (isHkRom(rom))
	{
		// validate the frame number, hello kitty version has 25 frames
		if( frameNumber < 0 || frameNumber >= 25 )
			frameNumber = 24;

		// retrieve the border address
		frameAddress = HELLO_KITTY_FRAME_OFFSETS[frameNumber][0];
	}
	else
	{
		// validate the frame number
		if (frameNumber < 0 || frameNumber >= 18)
			frameNumber = 13;

		// calculate the border address.
		// it can be in one of two banks.
		if(frameNumber < 9)
			frameAddress = BANK(0x34) + frameNumber * 0x688;
		else
			frameAddress = BANK(0x35) + (frameNumber - 9) * 0x688;
	}

	return &rom[frameAddress];
}

static inline int picNum2BaseAddress(int picNum)
{
	// Picture 1 is at 0x2000, picture 2 is at 0x3000, etc.
	return (picNum + 1) * 0x1000;
}

void convert(uint8_t rom[], uint8_t saveBuffer[], uint8_t pixelBuffer[], int picNum)
{
	int baseAddress = picNum2BaseAddress(picNum);
	int frameNumber = saveBuffer[baseAddress + 0xfb0];
	uint8_t *frameAddress = getFrame(rom, frameNumber);
	int xTile, yTile, tileNum, x, y, z;
	uint8_t *tile;

	for (yTile = 0; yTile < 14; ++yTile)
	{
		y = 16 + yTile*8;
		x = 16;
		tile = saveBuffer + baseAddress + yTile*256;
		for (x = 16; x <= 8*17; tile+=16, x+=8)
		{
			drawSpan(pixelBuffer, tile, x, y);
		}

		if (rom) {
			// Draw the sides of the frame
			y = 16 + yTile*8;
			for (z=0; z<4; ++z)
			{
				if (isHkRom(rom)) {
					tileNum = rom[HELLO_KITTY_FRAME_OFFSETS[frameNumber][1] + 0x50 + yTile*4 + z];
				} else {
					tileNum = getFrame(rom, frameNumber)[0x650 + yTile*4 + z];
				}
				tile = getFrame(rom, frameNumber) +  tileNum*16;
				x = ((z&1)?8:0) + ((z&2)?HEIGHT:0);
				drawSpan(pixelBuffer, tile, x, y);
			}
		}
	}

	if (rom) {
		// Draw the top and bottom of the frame
		for (xTile=0; xTile<20; ++xTile) for (z=0; z<4; ++z)
		{
			if (isHkRom(rom)) {
				tileNum = rom[HELLO_KITTY_FRAME_OFFSETS[frameNumber][1] + xTile + 0x14*z];
			} else {
				tileNum = getFrame(rom, frameNumber)[0x600 + xTile + 0x14*z];
			}

			tile = frameAddress + tileNum*16;
			x = xTile*8;
			y = ((z&1)?8:0) + ((z&2)?128:0);
			drawSpan(pixelBuffer, tile, x, y);
		}
	}
}

static inline unsigned int interleaveBytes(uint8_t low, uint8_t high)
{
	int result;
	// We recieve two vars, each 8 bits in length
	// We return one int, 16 bits in length, that contains the two vars interleaved
	// example:
	//    low = 00000000
	//   high = 11111111
	// result = 10101010 10101010

	result  = low & 1;
	result |= (high & 1) << 1;
	result |= (low & 2) << 1;
	result |= (high & 2) << 2;
	result |= (low & 4) << 2;
	result |= (high & 4) << 3;
	result |= (low & 8) << 3;
	result |= (high & 8) << 4;

	result |= (low & 16) << 4;
	result |= (high & 16) << 5;
	result |= (low & 32) << 5;
	result |= (high & 32) << 6;
	result |= (low & 64) << 6;
	result |= (high & 64) << 7;
	result |= (low & 128) << 7;
	result |= (high & 128) << 8;

	return result;
}

void drawSpan(uint8_t pixelBuffer[], uint8_t *buffer, int x, int y) {
	unsigned int interleaved;
	uint8_t lowBits, highBits;
	uint8_t *p, *q;
	p = pixelBuffer + (x/4) + y * ROW_SIZE;
	for (q = p + ROW_SIZE * 8; p<q; p+=ROW_SIZE)
	{
		lowBits = ~*buffer++;
		highBits = ~*buffer++;
		interleaved = interleaveBytes(lowBits, highBits);
		p[1] = (uint8_t)(interleaved);
		p[0] = (uint8_t)(interleaved >> 8);
	}
}

void writeImageFile(uint8_t pixelBuffer[], const char *filename)
{
	int y;
	// open file
	FILE *fp = fopen(filename, "wb");

	png_structp png_ptr = png_create_write_struct
		(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	if (!png_ptr)
		exit(EXIT_FAILURE);

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		png_destroy_write_struct(&png_ptr, 
				(png_infopp)NULL);
		exit(EXIT_FAILURE);
	}

	// init output
	png_init_io( png_ptr, fp );
	png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);
	png_set_compression_mem_level(png_ptr, 9);

	// set header info
	png_set_IHDR(png_ptr, info_ptr, WIDTH, HEIGHT, 2, 
			PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	// setup row pointers
	png_bytep *row_pointers = malloc(sizeof(png_bytep) * HEIGHT);
	if (!row_pointers) err(1, "malloc failure");
	for(y=0; y<HEIGHT; ++y)
		row_pointers[y] = (png_byte *)(pixelBuffer + ROW_SIZE * y);

	// set rows
	png_set_rows(png_ptr, info_ptr, row_pointers);

	// write png
	// png_write_png( png_ptr, info_ptr, 0, NULL );
	png_write_info(png_ptr, info_ptr);
	png_write_flush(png_ptr);
	png_write_image(png_ptr, row_pointers);

	png_text source_text;
	source_text.compression = PNG_TEXT_COMPRESSION_NONE;
	source_text.key = "Source";
	source_text.text = "Nintendo Gameboy Camera";
	png_set_text(png_ptr, info_ptr, &source_text, 1);

	source_text.key = "Software";
	source_text.text = "gbcamextract";
	png_set_text(png_ptr, info_ptr, &source_text, 1);

	png_write_end(png_ptr, info_ptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	if (row_pointers) free(row_pointers);

	fclose(fp);
}

static void noreturn usage(void)
{
	fprintf(stderr, "usage: %s [-r rom.gb] -s save.sav\n",
		__progname
	);
	exit(EXIT_FAILURE);
}
