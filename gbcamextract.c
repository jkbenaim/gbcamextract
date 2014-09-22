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


#include <stdlib.h>     // malloc
#include <stdio.h>      // printf, fopen, fclose, fread
#include <string.h>     // strerror
#include <errno.h>      // errno
#include <png.h>
#include <zlib.h>

const int WIDTH = 160;
const int HEIGHT = 144;

int picNum2BaseAddress( int picNum );
void printPicInfo( char buf[static 128*1024], int picNum );
void convert( char frameBuffer[static 2*16384], char saveBuffer[static 128*1024], char pixelBuffer[static WIDTH*HEIGHT], int picNum );
void writeImageFile( char pixelBuffer[static WIDTH*HEIGHT], int picNum );
void drawSpan( char pixelBuffer[static WIDTH*HEIGHT], char lowBits, char highBits, int x, int y );

int main( int argc, char *argv[] )
{ 
  // Argument count check
  if( argc < 3 )
  {
    fprintf( stderr, "%s: usage: gbcamextract camerarom.gb save.sav\n", argv[0] );
    return 1;
  }
  
  
  // Open rom file
  FILE* file = fopen( argv[1], "r" );
  if( file == NULL )
  {
    // Failure while attempting to open the file.
    fprintf( stderr, "%s: cannot open `%s': %s\n", argv[0], argv[1], strerror(errno) );
    return 2;
  }
  
  // Read the rom file into memory.
  // This is used to extract the frame data.
  char frameBuffer[2*16384];   // two banks
  size_t blocksRead = 0;
  if( !fseek( file, 0xd0000, SEEK_SET ) )       // beginning of bank 34h
    blocksRead = fread( frameBuffer, 2*16384, 1, file );
  if( blocksRead != 1 )
  {
    // We read the wrong amount of bytes. Bail.
    fprintf( stderr, "%s: cannot open `%s': Wrong file size.\n", argv[0], argv[1] );
    return 3;
  }
  
  // Close rom file
  fclose( file );
  
  
  // Open save file
  file = fopen( argv[2], "r" );
  if( file == NULL )
  {
    // Failure while attempting to open the file.
    fprintf( stderr, "%s: cannot open `%s': %s\n", argv[0], argv[2], strerror(errno) );
    return 2;
  }
  
  // Read the save file into memory
  char saveBuffer[128*1024];   // 128k son. this is as good as it gets on gameboy
  blocksRead = fread( saveBuffer, 128*1024, 1, file );
  if( blocksRead != 1 )
  {
    // We read the wrong amount of bytes. Bail.
    fprintf( stderr, "%s: cannot open `%s': Wrong file size.\n", argv[0], argv[2] );
    return 3;
  }
  
  // Close save file
  fclose( file );
  
  // convert
  int picNum;
  char pixelBuffer[WIDTH*HEIGHT];
  for( picNum = 1; picNum <= 30; picNum++ )
  {
    memset( pixelBuffer, 0x80, WIDTH*HEIGHT*sizeof(pixelBuffer[0]) );
    convert( frameBuffer, saveBuffer, pixelBuffer, picNum );
    writeImageFile( pixelBuffer, picNum );
  }
  
  
  
  
  // Return
  return 0;
}

int picNum2BaseAddress( int picNum )
{
  // Picture 1 is at 0x2000, picture 2 is at 0x3000, etc.
  return (picNum + 1) * 0x1000;
}

void convert( char frameBuffer[static 2*16384], char saveBuffer[static 128*1024], char pixelBuffer[static WIDTH*HEIGHT], int picNum )
{
  int baseAddress = picNum2BaseAddress( picNum );
  int xTile, yTile, tileAddress, spanAddress, spanNum, tileNum, x, y;
  char lowBits, highBits;
  for( yTile = 0; yTile < 14; yTile++ ) for( xTile = 0; xTile < 16; xTile++ )
  {
    // for each tile
    tileNum = xTile + yTile * 16;
    tileAddress = baseAddress + tileNum * 16;
//     printf("\t%x\n", tileAddress );
    for(spanNum=0; spanNum<8; spanNum++)
    {
      // for each span
      spanAddress = tileAddress + spanNum*2;
      lowBits  = saveBuffer[ spanAddress   ];
      highBits = saveBuffer[ spanAddress+1 ];
      x = 16 + xTile*8;
      y = 16 + yTile*8 + spanNum;
//       printf( "tile: %d, x: %d, y: %d\n", tileNum, x, y );
      drawSpan( pixelBuffer, lowBits, highBits, x, y );
    }
  }
  
  // next, apply the frame
  // grab frame number
  char frameNumber = saveBuffer[baseAddress + 0xfb0];
  
  // validate the frame number.
  // return if it's not valid.
  if( frameNumber < 0 || frameNumber >= 18 )
    return;
  
  // calculate the border address.
  // it can be in one of two banks
  int frameAddress = 0;
  if( frameNumber < 9 )
    frameAddress = frameNumber * 0x688;
  else if( frameNumber < 18 )
    frameAddress = 0x4000 + (frameNumber - 9) * 0x688;
  
  // apply the top of the frame
  // iterate over each tile in the tilemap
  // top row
  for( xTile=0; xTile<20; ++xTile )
  {
    tileNum = frameBuffer[frameAddress + 0x600 + xTile];
    tileAddress = frameAddress + tileNum*16;
    // draw the tile
    for( spanNum = 0; spanNum<8; spanNum++ )
    {
      spanAddress = tileAddress + 2*spanNum;
      lowBits = frameBuffer[spanAddress];
      highBits = frameBuffer[spanAddress+1];
      x = xTile*8;
      y = 0 + spanNum;
      drawSpan( pixelBuffer, lowBits, highBits, x, y );
    }
  }
  // second row
  for( xTile=0; xTile<20; ++xTile )
  {
    tileNum = frameBuffer[frameAddress + 0x614 + xTile];
    tileAddress = frameAddress + tileNum*16;
    // draw the tile
    for( spanNum = 0; spanNum<8; spanNum++ )
    {
      spanAddress = tileAddress + 2*spanNum;
      lowBits = frameBuffer[spanAddress];
      highBits = frameBuffer[spanAddress+1];
      x = xTile*8;
      y = 8 + spanNum;
      drawSpan( pixelBuffer, lowBits, highBits, x, y );
    }
  }
  // second-from-bottom row
  for( xTile=0; xTile<20; ++xTile )
  {
    tileNum = frameBuffer[frameAddress + 0x628 + xTile];
    tileAddress = frameAddress + tileNum*16;
    // draw the tile
    for( spanNum = 0; spanNum<8; spanNum++ )
    {
      spanAddress = tileAddress + 2*spanNum;
      lowBits = frameBuffer[spanAddress];
      highBits = frameBuffer[spanAddress+1];
      x = xTile*8;
      y = 128 + spanNum;
      drawSpan( pixelBuffer, lowBits, highBits, x, y );
    }
  }
  // bottom row
  for( xTile=0; xTile<20; ++xTile )
  {
    tileNum = frameBuffer[frameAddress + 0x63c + xTile];
    tileAddress = frameAddress + tileNum*16;
    // draw the tile
    for( spanNum = 0; spanNum<8; spanNum++ )
    {
      spanAddress = tileAddress + 2*spanNum;
      lowBits = frameBuffer[spanAddress];
      highBits = frameBuffer[spanAddress+1];
      x = xTile*8;
      y = 136 + spanNum;
      drawSpan( pixelBuffer, lowBits, highBits, x, y );
    }
  }
  
  // sides
  for( yTile = 0; yTile<14; yTile++ )
  {
    // leftmost tile
    tileNum = frameBuffer[frameAddress + 0x650 + yTile*4 + 0];
    tileAddress = frameAddress + tileNum*16;
    // draw the tile
    for( spanNum = 0; spanNum<8; spanNum++ )
    {
      spanAddress = tileAddress + 2*spanNum;
      lowBits = frameBuffer[spanAddress];
      highBits = frameBuffer[spanAddress+1];
      x = 0;
      y = 16 + yTile*8 + spanNum;
      drawSpan( pixelBuffer, lowBits, highBits, x, y );
    }
    // second-from-leftmost tile
    tileNum = frameBuffer[frameAddress + 0x650 + yTile*4 + 1];
    tileAddress = frameAddress + tileNum*16;
    // draw the tile
    for( spanNum = 0; spanNum<8; spanNum++ )
    {
      spanAddress = tileAddress + 2*spanNum;
      lowBits = frameBuffer[spanAddress];
      highBits = frameBuffer[spanAddress+1];
      x = 8;
      y = 16 + yTile*8 + spanNum;
      drawSpan( pixelBuffer, lowBits, highBits, x, y );
    }
    // second-from-rightmost tile
    tileNum = frameBuffer[frameAddress + 0x650 + yTile*4 + 2];
    tileAddress = frameAddress + tileNum*16;
    // draw the tile
    for( spanNum = 0; spanNum<8; spanNum++ )
    {
      spanAddress = tileAddress + 2*spanNum;
      lowBits = frameBuffer[spanAddress];
      highBits = frameBuffer[spanAddress+1];
      x = 144;
      y = 16 + yTile*8 + spanNum;
      drawSpan( pixelBuffer, lowBits, highBits, x, y );
    }
    // rightmost tile
    tileNum = frameBuffer[frameAddress + 0x650 + yTile*4 + 3];
    tileAddress = frameAddress + tileNum*16;
    // draw the tile
    for( spanNum = 0; spanNum<8; spanNum++ )
    {
      spanAddress = tileAddress + 2*spanNum;
      lowBits = frameBuffer[spanAddress];
      highBits = frameBuffer[spanAddress+1];
      x = 152;
      y = 16 + yTile*8 + spanNum;
      drawSpan( pixelBuffer, lowBits, highBits, x, y );
    }
  }
}

void drawSpan( char pixelBuffer[static WIDTH*HEIGHT], char lowBits, char highBits, int x, int y )
{
  int pixelNum;
  char spanColors[8];
  int grays[4] = {0xFF, 0xAA, 0x55, 0x00};
  for(pixelNum=0; pixelNum<8; pixelNum++)
  {
    int color = 0;
    int mask = 1<<pixelNum;
    if( lowBits & mask )
      color = 1;
    if( highBits & mask )
      color += 2;
    spanColors[7-pixelNum] = grays[ color ];
  }
  for(pixelNum=0; pixelNum<8; pixelNum++)
  {
    // copy span to pixel buffer
    int pixelAddress = x + 160 * y +pixelNum;
    pixelBuffer[pixelAddress] = spanColors[pixelNum];
  }
}

void writeImageFile( char pixelBuffer[static WIDTH*HEIGHT], int picNum )
{
  // open file
  char name[50];
  sprintf(name, "%d.png", picNum );
  FILE *fp = fopen( name, "wb" );
  
  png_structp png_ptr = png_create_write_struct
    (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

  if (!png_ptr)
      return;

  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr)
  {
      png_destroy_write_struct(&png_ptr,
          (png_infopp)NULL);
      return;
  }
  
    // init output
  png_init_io( png_ptr, fp );
  
  // set header info
  png_set_IHDR( png_ptr, info_ptr, WIDTH, HEIGHT, 8, 
                PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT );
    
  // setup row pointers
  png_bytep * row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * HEIGHT);
  int y;
  for(y=0; y<HEIGHT; ++y)
    row_pointers[y] = (png_byte *)(pixelBuffer + WIDTH * y);
  
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
