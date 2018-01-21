//
// Raster file viewer for CUPS.
//
// Copyright 1997-2018 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
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
