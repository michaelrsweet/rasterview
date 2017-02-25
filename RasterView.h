//
// "$Id: RasterView.h 79 2006-05-13 20:38:27Z mike $"
//
// CUPS raster file viewer application window header file.
//
// Copyright 2002-2006 by Michael Sweet.
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

#ifndef RasterView_h
#  define RasterView_h


//
// Include necessary headers...
//

#  include "RasterDisplay.h"
#  include <FL/Fl_Double_Window.H>
#  include <FL/Fl_Button.H>
#  include <FL/Fl_Sys_Menu_Bar.H>
#  include <FL/Fl_Text_Display.H>
#  include <FL/Fl_Help_Dialog.H>


//
// RasterView application class...
//

class RasterView : public Fl_Double_Window
{
  RasterView		*next_;		// Next window in list
  char			*filename_;	// Filename
  char			*title_;	// Window title
  int			loading_;	// Non-zero if we are loading a page
  char			pixel_[1024];	// Current pixel value
  Fl_Sys_Menu_Bar	*menubar_;	// Menubar
  RasterDisplay		*display_;	// Display widget
  Fl_Group		*buttons_;	// Button bar
  Fl_Button		*next_button_,	// Next page button
			*attrs_button_;	// Toggle attributes button
  Fl_Text_Display	*attributes_;	// Attributes
  Fl_Text_Buffer	*attr_buffer_;	// Attribute buffer

  static RasterView	*first_;	// First window in list
  static Fl_Help_Dialog	*help_;		// Help dialog


#  ifdef __APPLE__
  static void	apple_open_cb(const char *f);
#  endif // __APPLE__
  static void	attrs_cb(Fl_Widget *widget);
  static void	close_cb(Fl_Widget *widget);
  static void	color_cb(RasterDisplay *display);
  static void	help_cb();
  void		init();
  void		load_attrs();
  static void	next_cb(Fl_Widget *widget);
  static void	open_cb();
  static void	quit_cb();
  static void	reopen_cb(Fl_Widget *widget);
  void		set_filename(const char *f);

  public:

  RasterView(int X, int Y, int W, int H, const char *L = 0);
  RasterView(int W, int H, const char *L = 0);
  ~RasterView();

  int			handle(int event);
  static RasterView	*open_file(const char *f);
  void			resize(int X, int Y, int W, int H);
};


#endif // !RasterView_h

//
// End of "$Id: RasterView.h 79 2006-05-13 20:38:27Z mike $".
//
