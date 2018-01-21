//
// CUPS raster file viewer application window header file.
//
// Copyright 2002-2015 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef RasterView_h
#  define RasterView_h


//
// Include necessary headers...
//

#  include "RasterDisplay.h"
#  include <FL/Fl_Double_Window.H>
#  include <FL/Fl_Button.H>
#  include <FL/Fl_Check_Button.H>
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
  Fl_Group		*attributes_;	// Attributes pane
  Fl_Button		*colors_[15];	// Color buttons
  Fl_Text_Display	*header_;	// Page header
  Fl_Text_Buffer	*header_buffer_;// Attribute buffer

  static RasterView	*first_;	// First window in list
  static Fl_Help_Dialog	*help_;		// Help dialog


#  ifdef __APPLE__
  static void	apple_open_cb(const char *f);
#  endif // __APPLE__
  static void	attrs_cb(Fl_Widget *widget);
  static void	close_cb(Fl_Widget *widget);
  static void	color_cb(RasterDisplay *display);
  static void	device_cb(Fl_Widget *widget);
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
