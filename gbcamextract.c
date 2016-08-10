/*
 * gbcamextract.c, extracts photos from Game Boy Camera save files.
 *
 * Copyright (c) 2013 jkbenaim. MIT license.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */


#include <stdlib.h>     // malloc, EXIT_SUCCESS, EXIT_FAILURE, NULL
#include <stdio.h>      // printf, fopen, fclose, fread
#include <string.h>     // strerror
#include <errno.h>      // errno
#include <png.h>
#include <zlib.h>

#define BANK_SIZE 0x4000
#define BANK(bank) BANK_SIZE * bank

const int FILE_ERROR = 2;
const int FILE_SIZE_ERROR = 3;

const int WIDTH = 160;
const int ROW_SIZE = 40; // WIDTH/4: 2 bits per pixel means 4 pixels per byte
const int HEIGHT = 144;
const int BUFFER_SIZE = 128*1024;

static inline int picNum2BaseAddress( int picNum );
static inline int getFrameAddress( int frameNumber );
static inline unsigned int interleaveBytes( unsigned char low, unsigned char high );
void convert( char frameBuffer[], char saveBuffer[], char pixelBuffer[], int picNum );
void writeImageFile( char pixelBuffer[], int picNum );
void drawSpan( char pixelBuffer[], char *buffer, int x, int y );
void readData( char *fileName, char *buffer, int offset );

int main( int argc, char *argv[] )
{
  FILE* file;
  size_t blocksRead;
  // Argument count check
  if( argc < 3 )
  {
    fprintf( stderr, "%s: usage: gbcamextract camerarom.gb save.sav\n", argv[0] );
    return EXIT_FAILURE;
  }


  // Open rom file
  file = fopen( argv[1], "r" );
  if( file == NULL )
  {
    // Failure while attempting to open the file.
    fprintf( stderr, "%s: cannot open `%s': %s\n", argv[0], argv[1], strerror(errno) );
    return FILE_ERROR;
  }

  // Read the rom file into memory.
  // This is used to extract the frame data.
  char frameBuffer[BANK_SIZE * 2];   // two banks
  blocksRead = 0;
  if( !fseek( file, BANK(0x34), SEEK_SET ) )       // beginning of bank 34h
    blocksRead = fread( frameBuffer, BANK_SIZE * 2, 1, file );
  if( blocksRead != 1 )
  {
    // We read the wrong amount of bytes. Bail.
    fprintf( stderr, "%s: cannot open `%s': Wrong file size.\n", argv[0], argv[1] );
    return FILE_SIZE_ERROR;
  }

  // Close rom file
  fclose( file );


  // Open save file
  file = fopen( argv[2], "r" );
  if( file == NULL )
  {
    // Failure while attempting to open the file.
    fprintf( stderr, "%s: cannot open `%s': %s\n", argv[0], argv[2], strerror(errno) );
    return FILE_ERROR;
  }

  // Read the save file into memory
  char saveBuffer[BUFFER_SIZE];   // 128k son. this is as good as it gets on gameboy
  blocksRead = fread( saveBuffer, BUFFER_SIZE, 1, file );
  if( blocksRead != 1 )
  {
    // We read the wrong amount of bytes. Bail.
    fprintf( stderr, "%s: cannot open `%s': Wrong file size.\n", argv[0], argv[2] );
    return FILE_SIZE_ERROR;
  }

  // Close save file
  fclose( file );

  // convert
  int picNum;
  char pixelBuffer[ROW_SIZE*HEIGHT];
  for( picNum = 1; picNum <= 30; ++picNum )
  {
    convert( frameBuffer, saveBuffer, pixelBuffer, picNum );
    writeImageFile( pixelBuffer, picNum );
  }

  // Return
  return EXIT_SUCCESS;
}

static inline int getFrameAddress( int frameNumber ) {
  int frameAddress;

  // validate the frame number
  if( frameNumber < 0 || frameNumber >= 18 )
    frameNumber = 13;

  // calculate the border address.
  // it can be in one of two banks
  if( frameNumber < 9 )
    frameAddress = BANK(0) + frameNumber * 0x688;
  else
    frameAddress = BANK(1) + (frameNumber - 9) * 0x688;

  return frameAddress;
}

static inline int picNum2BaseAddress( int picNum )
{
  // Picture 1 is at 0x2000, picture 2 is at 0x3000, etc.
  return (picNum + 1) * 0x1000;
}

void convert( char frameBuffer[], char saveBuffer[], char pixelBuffer[], int picNum )
{
  int baseAddress = picNum2BaseAddress( picNum );
  int frameNumber = saveBuffer[baseAddress + 0xfb0];
  int frameAddress = getFrameAddress( frameNumber );
  int xTile, yTile, tileAddress, tileNum, x, y, z;
  char *tile;
  for( yTile = 0; yTile < 14; ++yTile )
  {
    y = 16 + yTile*8;
    x = 16;
    tile = saveBuffer + baseAddress + yTile*256;
    for( x = 16; x <= 8*17; tile+=16, x+=8 )
    {
      drawSpan( pixelBuffer, tile, x, y );
    }

    // Draw the sides of the frame
    y = 16 + yTile*8;
    for ( z=0; z<4; ++z )
    {
      tileNum = frameBuffer[frameAddress + 0x650 + yTile*4 + z];
      tile = frameBuffer + frameAddress + tileNum*16;
      x = ((z&1)?8:0) + ((z&2)?HEIGHT:0);
      drawSpan( pixelBuffer, tile, x, y );
    }
  }

  // Draw the top and bottom of the frame
  for( xTile=0; xTile<20; ++xTile ) for ( z=0; z<4; ++z )
  {
    tileNum = frameBuffer[frameAddress + 0x600 + xTile + 0x14*z];
    tileAddress = frameAddress + tileNum*16;
    tile = frameBuffer + tileAddress;
    x = xTile*8;
    y = ((z&1)?8:0) + ((z&2)?128:0);
    drawSpan( pixelBuffer, tile, x, y );
  }
}

static inline unsigned int interleaveBytes( unsigned char low, unsigned char high )
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

void drawSpan( char pixelBuffer[], char *buffer, int x, int y ) {
  unsigned int interleaved;
  unsigned char lowBits, highBits;
  char *p, *q;
  p = pixelBuffer + (x/4) + y * ROW_SIZE;
  for( q = p + ROW_SIZE * 8; p<q; p+=ROW_SIZE )
  {
    lowBits = ~*buffer++;
    highBits = ~*buffer++;
    interleaved = interleaveBytes(lowBits, highBits);
    p[1] = (unsigned char)(interleaved);
    p[0] = (unsigned char)(interleaved >> 8);
//  *(uint16_t *)p = htons((uint16_t)interleaved);
  }
}

void writeImageFile( char pixelBuffer[], int picNum )
{
  int y;
  // open file
  char name[7];
  sprintf(name, "%d.png", picNum );
  FILE *fp = fopen( name, "wb" );

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
  png_set_IHDR( png_ptr, info_ptr, WIDTH, HEIGHT, 2,
                PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT );

  // setup row pointers
  png_bytep * row_pointers = malloc(sizeof(png_bytep) * HEIGHT);
  for(y=0; y<HEIGHT; ++y)
    row_pointers[y] = (png_byte *)(pixelBuffer + ROW_SIZE * y);

  // set rows
  png_set_rows( png_ptr, info_ptr, row_pointers );

  // write png
//  png_write_png( png_ptr, info_ptr, 0, NULL );
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

  fclose(fp);
}
