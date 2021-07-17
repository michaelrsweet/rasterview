RasterView
==========

RasterView is a CUPS, PWG, and Apple raster file viewing application. It
basically allows you to look at the raster data produced by any of the standard
CUPS RIP filters (cgpdftoraster, imagetoraster, pdftoraster, and pstoraster) and
is normally used to either test those filters or look at the data that is being
sent to your raster printer driver.

RasterView is licensed under the Apache License Version 2.0.

More information can be found at:

    https://www.msweet.org/rasterview


Requirements
------------

If you plan on (re)compiling it, you'll need FLTK (<http://www.fltk.org/>) 1.1.x
or later and a C++ compiler.


## How to Compile

Run the following commands:

    ./configure
    make


How to Use
----------

The program is called "rasterview" on UNIX/Linux and "RasterView.app" on macOS.
Run the program and then open a raster file, or pass the filename on the
command-line.  You can view multiple files simultaneously.

The "test" subdirectory includes a script for generating raster data using the
standard RIP filters.  Run the following command for help:

    tools/maketestfiles.sh help


Legal Stuff
-----------

RasterView is Copyright © 2002-2021 by Michael R Sweet.

RasterView is provided under the terms of the Apache License, Version 2.0.  A
copy of this license can be found in the file `LICENSE`.  Additional legal
information is provided in the file `NOTICE`.

Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied.  See the License for the
specific language governing permissions and limitations under the License.


Changes in v1.8 (2021-07-17)
----------------------------

- Fixed HiDPI support on Linux (Issue #11)
- Fixed macOS 11 (Big Sur) issues (Issue #15)
- Can now view page bitmaps up to 2GB in size (Issue #16)
- Color picker mode now copies the hex RGB color to the clipboard.
- Zoom gestures are now supported on macOS.
- Addressed a few warnings from LGTM.
- Fixed macOS bundle information and now provide fat binaries for Intel and
  Apple Silicon.


Changes in v1.7.1 (2018-07-02)
------------------------------

- Fixed a crash bug on macOS.


Changes in v1.7 (2018-06-03)
----------------------------

- Updated the page controls to allow navigation to the previous page and to
  selected pages.
- Added mode buttons for zoom, pan, and color (to show the current mode).


Changes in v1.6 (2018-01-21)
----------------------------

- Now licensed under the Apache License Version 2.0.
- Fixed support for 16-bit per color files.
- Added support for gzip'd raster files (Issue #7)


Changes in v1.5 (2017-03-22)
----------------------------

- Added support for Apple raster files.


Changes in v1.4.1 (2015-08-27)
------------------------------

- Fixed the dependency on strlcpy.


Changes in v1.4 (2015-08-26)
----------------------------

- Added support for Device-N raster files.
- Colorants can now be changed for Device-N, K, CMY, and CMYK raster files.
- Attributes for PWG Raster files are now reported using the PWG 5102.4
  naming and contents.


Changes in v1.3 (2011-05-18)
----------------------------

- Added support for PWG Raster files (requires CUPS 1.5 or higher)


Changes in v1.2.2 (2007-06-21)
------------------------------

- Added range checks to the page reader so that pages larger than 64MB or with
  invalid dimensions will not cause the program to crash.


Changes in v1.2.1 (2006-09-28)
------------------------------

- Changed the default scheme to gtk+, which is available in FLTK 1.1.8
  and higher.
- Added a --enable-static configure option to use the static CUPS
  libraries.


Changes in v1.2 (2006-05-13)
----------------------------
- First public release.
