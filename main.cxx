//
// Raster file viewer for CUPS.
//
// Copyright 1997-2017 by Michael R Sweet.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//

//
// Include necessary headers...
//

#include "RasterView.h"
#include <FL/Fl.H>
#include <string.h>


//
// 'main()' - Main entry and processing...
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  int		i;			// Looping var
  RasterView	*view;			// View window


  Fl::scheme("gtk+");

  for (i = 1, view = 0; i < argc; i ++)
    if (!strncmp(argv[i], "-psn", 4))
      break;
    else
      view = RasterView::open_file(argv[i]);

  if (!view)
  {
    view = new RasterView(600, 800);
    view->show(1, argv);
  }

  Fl::run();

  return (0);
}
