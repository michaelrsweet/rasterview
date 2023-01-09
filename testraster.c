//
// Program to create raster files for testing.
//
// Usage:
//
//   ./testraster [--pwg] [--urf] [WIDTH] [HEIGHT] >FILENAME
//
// Copyright © 2023 by Michael R Sweet
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include <stdio.h>
#include <string.h>
#include "raster.h"


//
// Local functions...
//

static void	usage(FILE *out);
static void	write_page(cups_raster_t *ras, unsigned width, unsigned height, cups_cspace_t cspace, unsigned bpp);


//
// 'main()' - Main entry.
//

int
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  int			i;		// Looping var
  cups_raster_mode_t	mode = CUPS_RASTER_WRITE;
					// Output mode
  unsigned		width = 0,	// Output width
			height = 0;	// Output height
  cups_raster_t		*ras;		// Raster stream


  // Parse command-line
  for (i = 1; i < argc; i ++)
  {
    if (!strcmp(argv[i], "--help"))
    {
      usage(stdout);
      return (0);
    }
    else if (!strcmp(argv[i], "--pwg"))
    {
      mode = CUPS_RASTER_WRITE_PWG;
    }
    else if (!strcmp(argv[i], "--urf"))
    {
      mode = CUPS_RASTER_WRITE_APPLE;
    }
    else if (argv[i][0] < '1' || argv[i][0] > '9' || (width && height))
    {
      fprintf(stderr, "testraster: Unknown option '%s'.\n", argv[i]);
      usage(stderr);
      return (1);
    }
    else if (!width)
    {
      width = strtoul(argv[i], NULL, 10);
    }
    else
    {
      height = strtoul(argv[i], NULL, 10);
    }
  }

  // Get output size...
  if (!width)
    width = 850;

  if (!height)
    height = 1100 * width / 850;

  // Open the output stream...
  if ((ras = cupsRasterOpen(1, mode)) == NULL)
  {
    fprintf(stderr, "testraster: Unable to open raster stream: %s\n", cupsRasterErrorString());
    return (1);
  }

  // Write a test page in the usual color spaces...
  write_page(ras, width, height, CUPS_CSPACE_W, 8);
  write_page(ras, width, height, CUPS_CSPACE_RGB, 24);
  write_page(ras, width, height, CUPS_CSPACE_K, 1);
  write_page(ras, width, height, CUPS_CSPACE_K, 8);
  write_page(ras, width, height, CUPS_CSPACE_CMY, 24);
  write_page(ras, width, height, CUPS_CSPACE_CMYK, 32);
  write_page(ras, width, height, CUPS_CSPACE_SW, 8);
  write_page(ras, width, height, CUPS_CSPACE_SRGB, 24);
  write_page(ras, width, height, CUPS_CSPACE_ADOBERGB, 24);

  // Close the raster stream and return...
  cupsRasterClose(ras);

  return (0);
}


//
// 'usage()' - Show program usage.
//

static void
usage(FILE *out)			// I - Output file
{
  fputs("Usage: ./testraster [OPTIONS] [WIDTH [HEIGHT]] >FILENAME\n", out);
  fputs("Options:\n", out);
  fputs("  --help     Show program usage.\n", out);
  fputs("  --pwg      Output PWG raster instead of CUPS raster.\n", out);
  fputs("  --urf      Output Apple raster instead of CUPS raster.\n", out);
}


//
// 'write_page()' - Write a single page.
//

static void
write_page(cups_raster_t *ras,		// I - Raster stream
           unsigned      width,		// I - Width in columns
           unsigned      height,	// I - Height in lines
           cups_cspace_t cspace,	// I - Color space
           unsigned      bpp)		// I - Bits per pixel (1, 8, 24, or 32)
{
  cups_page_header_t	header;		// Page header
  unsigned		x, y;		// Position in page
  unsigned char		*line,		// Line
			*lineptr;	// Pointer into line


  // Initialize the page header and allocate memory for the line...
  memset(&header, 0, sizeof(header));
  header.cupsWidth        = width;
  header.cupsHeight       = height;
  header.cupsColorSpace   = cspace;
  header.cupsColorOrder   = CUPS_ORDER_CHUNKED;
  header.cupsBitsPerPixel = bpp;
  header.cupsBitsPerColor = bpp == 1 ? 1 : 8;
  header.cupsNumColors    = (bpp + 7) / 8;
  header.cupsBytesPerLine = (width * bpp + 7) / 8;

  if ((line = malloc(header.cupsBytesPerLine)) == NULL)
  {
    perror("testraster: Unable to allocate memory for line");
    return;
  }

  cupsRasterWriteHeader(ras, &header);

  width -= 4;
  height -= 4;

  // Write the first two lines (top black line and side borders)
  if (cspace == CUPS_CSPACE_K || cspace == CUPS_CSPACE_CMY)
  {
    memset(line, 255, header.cupsBytesPerLine);
    cupsRasterWritePixels(ras, line, header.cupsBytesPerLine);

    if (bpp == 1)
    {
      // 1-bit black, clear all but the first and last bits...
      if (header.cupsBytesPerLine > 2)
	memset(line + 1, 0, header.cupsBytesPerLine - 2);

      line[0] = 0x80;
      line[header.cupsBytesPerLine - 1] = 128 >> (width & 7);
    }
    else
    {
      // 8-bit black or 24-bit CMY,  clear all but the first and last pixels
      memset(line + bpp / 8, 0, header.cupsBytesPerLine - bpp / 4);
    }
  }
  else if (bpp == 32)
  {
    // 32-bit CMYK
    memset(line, 0, header.cupsBytesPerLine);
    for (x = 3; x < header.cupsBytesPerLine; x += 4)
      line[x] = 255;
    cupsRasterWritePixels(ras, line, header.cupsBytesPerLine);

    memset(line + bpp / 8, 0, header.cupsBytesPerLine - bpp / 4);
  }
  else
  {
    memset(line, 0, header.cupsBytesPerLine);
    cupsRasterWritePixels(ras, line, header.cupsBytesPerLine);

    memset(line + bpp / 8, 255, header.cupsBytesPerLine - bpp / 4);
  }

  cupsRasterWritePixels(ras, line, header.cupsBytesPerLine);

  // Write the interior lines...
  for (y = 0; y < height; y ++)
  {
    unsigned char	mask, bit;	// Mask and current bit
    unsigned		color;		// Current color

    switch (bpp)
    {
      case 1 :
	  if (header.cupsBytesPerLine > 2)
	    memset(line + 1, 0, header.cupsBytesPerLine - 2);

	  line[0] = 0x80;
	  line[header.cupsBytesPerLine - 1] = 128 >> (width & 7);

          for (x = 0, bit = 128 >> ((x + 2) & 7), lineptr = line; x < width; x ++)
          {
            static unsigned char masks[4][2] =
            {
              { 0x55, 0x00 },
              { 0x55, 0xaa },
              { 0xff, 0xaa },
              { 0xff, 0xff }
            };

            color = ((x / 8) * (y / 8)) & 3;
            mask  = masks[color][y & 1];

            if (mask & bit)
              *lineptr |= bit;

            if (bit > 1)
            {
              bit /= 2;
            }
            else
            {
              bit = 128;
              lineptr ++;
            }
          }
          break;

      case 8 :
          for (x = 0, lineptr = line + 2; x < width; x ++)
          {
            color = (((x / 8) * (y / 8)) % 15) + 1;

	    if (cspace == CUPS_CSPACE_K)
	      *lineptr++ = color * 0x11;
	    else
	      *lineptr++ = 255 - color * 0x11;
          }
          break;

      case 24 :
          for (x = 0, lineptr = line + 6; x < width; x ++)
          {
            color = (((x / 8) * (y / 8)) % 27) + 1;

	    if (cspace == CUPS_CSPACE_CMY)
	    {
	      *lineptr++ = 127 * ((color / 9) % 3);
	      *lineptr++ = 127 * ((color / 3) % 3);
	      *lineptr++ = 127 * (color % 3);
	    }
	    else
	    {
	      *lineptr++ = 255 - 127 * ((color / 9) % 3);
	      *lineptr++ = 255 - 127 * ((color / 3) % 3);
	      *lineptr++ = 255 - 127 * (color % 3);
	    }
          }
          break;

      case 32 :
          for (x = 0, lineptr = line + 8; x < width; x ++)
          {
            color = (((x / 8) * (y / 8)) % 27) + 1;

            if (color < 27)
            {
	      *lineptr++ = 127 * ((color / 9) % 3);
	      *lineptr++ = 127 * ((color / 3) % 3);
	      *lineptr++ = 127 * (color % 3);
	      *lineptr++ = 0;
	    }
	    else
	    {
	      *lineptr++ = 0;
	      *lineptr++ = 0;
	      *lineptr++ = 0;
	      *lineptr++ = 255;
	    }
          }
          break;
    }

    cupsRasterWritePixels(ras, line, header.cupsBytesPerLine);
  }

  // Write the last two lines (side borders and bottom black line)
  if (cspace == CUPS_CSPACE_K || cspace == CUPS_CSPACE_CMY)
  {
    if (bpp == 1)
    {
      // 1-bit black, clear all but the first and last bits...
      if (header.cupsBytesPerLine > 2)
	memset(line + 1, 0, header.cupsBytesPerLine - 2);

      line[0] = 0x80;
      line[header.cupsBytesPerLine - 1] = 128 >> (width & 7);
    }
    else
    {
      // 8-bit black or 24-bit CMY,  clear all but the first and last pixels
      memset(line, 255, bpp / 8);
      memset(line + bpp / 8, 0, header.cupsBytesPerLine - bpp / 4);
      memset(line + header.cupsBytesPerLine - bpp / 8, 255, bpp / 8);
    }

    cupsRasterWritePixels(ras, line, header.cupsBytesPerLine);

    memset(line, 255, header.cupsBytesPerLine);
  }
  else if (bpp == 32)
  {
    // 32-bit CMYK
    memset(line, 0, header.cupsBytesPerLine);
    line[3] = 255;
    line[header.cupsBytesPerLine - 1] = 255;

    cupsRasterWritePixels(ras, line, header.cupsBytesPerLine);

    for (x = 7; x < header.cupsBytesPerLine; x += 4)
      line[x] = 255;
  }
  else
  {
    memset(line, 0, bpp / 8);
    memset(line + bpp / 8, 255, header.cupsBytesPerLine - bpp / 4);
    memset(line + header.cupsBytesPerLine - bpp / 8, 0, bpp / 8);
    cupsRasterWritePixels(ras, line, header.cupsBytesPerLine);

    memset(line, 0, header.cupsBytesPerLine);
  }

  cupsRasterWritePixels(ras, line, header.cupsBytesPerLine);
}
