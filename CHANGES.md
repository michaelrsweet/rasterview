Changes in RasterView
=====================


Changes in v1.9.1 (YYYY-MM-DD)
------------------------------

- Fixed 16-bit viewing support for many color spaces (Issue #23)


Changes in v1.9.0 (2023-01-16)
------------------------------

- Fixed macOS 'Close' and 'Re-Open' menu items (Issue #20)
- Fixed color picking (Issue #21)
- Updated the maximum zoom to 20x
- Added Visual Studio project files for Windows release


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
