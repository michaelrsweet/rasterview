RasterView - PWG/Apple Raster Viewer
====================================

![Version](https://img.shields.io/github/v/release/michaelrsweet/rasterview?include_prereleases)
![Apache 2.0](https://img.shields.io/github/license/michaelrsweet/rasterview)

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

RasterView is Copyright © 2002-2023 by Michael R Sweet.

RasterView is provided under the terms of the Apache License, Version 2.0.  A
copy of this license can be found in the file `LICENSE`.  Additional legal
information is provided in the file `NOTICE`.

Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied.  See the License for the
specific language governing permissions and limitations under the License.
