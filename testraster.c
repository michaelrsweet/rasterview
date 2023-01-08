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
  write_page(ras, width, height, CUPS_CSPACE_RGB, 8);
  write_page(ras, width, height, CUPS_CSPACE_K, 1);
  write_page(ras, width, height, CUPS_CSPACE_K, 8);
  write_page(ras, width, height, CUPS_CSPACE_CMYK, 8);
  write_page(ras, width, height, CUPS_CSPACE_SW, 8);
  write_page(ras, width, height, CUPS_CSPACE_SRGB, 8);
  write_page(ras, width, height, CUPS_CSPACE_ADOBERGB, 8);

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


  //

}
