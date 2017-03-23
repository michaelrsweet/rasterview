# RasterView v1.5

RasterView is a CUPS, PWG, and Apple raster file viewing application. It
basically allows you to look at the raster data produced by any of the standard
CUPS RIP filters (cgpdftoraster, imagetoraster, pdftoraster, and pstoraster) and
is normally used to either test those filters or look at the data that is being
sent to your raster printer driver.

RasterView is provided as free software under version 2 of the GNU General
Public license.

More information can be found at:

    https://michaelrsweet.github.io/rasterview


## Requirements

If you plan on (re)compiling it, you'll need FLTK 1.3.x (http://www.fltk.org/)
and a C++ compiler.


## How to Compile

Run the following commands:

    ./configure
    make


## How to Use

The program is called "rasterview" on UNIX/Linux and "RasterView.app" on macOS.
Run the program and then open a raster file, or pass the filename on the
command-line.  You can view multiple files simultaneously.

The "test" subdirectory includes a script for generating raster data using the
standard RIP filters.  Run the following command for help:

    tools/maketestfiles.sh help


## Legal Stuff

RasterView is Copyright 2002-2017 by Michael R Sweet.

This program is free software; you can redistribute it and/or modify it under
the terms of version 2 of the GNU General Public License as published by the
Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.


## Changes in v1.5 - 2017-03-22

- Added support for Apple raster files.


## Changes in v1.4.1 - 2015-08-27

- Fixed the dependency on strlcpy.


## Changes in v1.4 - 2015-08-26

- Added support for Device-N raster files.
- Colorants can now be changed for Device-N, K, CMY, and CMYK raster
  files.
- Attributes for PWG Raster files are now reported using the PWG 5102.4
  naming and contents.


## Changes in v1.3 - 2011-05-18

- Added support for PWG Raster files (requires CUPS 1.5 or higher)


## Changes in v1.2.2 - 2007-06-21

- Added range checks to the page reader so that pages larger than 64MB
  or with invalid dimensions will not cause the program to crash.


## Changes in v1.2.1 - 2006-09-28

- Changed the default scheme to gtk+, which is available in FLTK 1.1.8
  and higher.
- Added a --enable-static configure option to use the static CUPS
  libraries.


## Changes in v1.2 - 2006-05-13

- First public release.
