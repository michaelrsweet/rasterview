//
// CUPS raster file display widget header file.
//
// Copyright 2002-2018 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef RasterDisplay_h
#  define RasterDisplay_h


//
// Include necessary headers...
//

#  include "raster.h"
#  include <FL/Fl.H>
#  include <FL/Fl_Group.H>
#  include <FL/Fl_Scrollbar.H>
#  include <zlib.h>


//
// Constants...
//

#  define RASTER_MAX_PAGES	1000	// Maximum pages
#  define SBWIDTH		17	// Scrollbar width


//
// Display control modes...
//

enum
{
  RASTER_MODE_PAN,
  RASTER_MODE_ZOOM_IN,
  RASTER_MODE_ZOOM_OUT,
  RASTER_MODE_CLICK
};


//
// RasterDisplay widget...
//

class RasterDisplay : public Fl_Group
{
  cups_raster_t		*ras_;		// Raster stream
  const char		*filename_;	// Filename
  gzFile		fp_;		// File pointer
  int			page_,		// Current page number
			num_pages_;	// Number of pages
  z_off_t		pages_[RASTER_MAX_PAGES];
					// Page offsets
  cups_page_header2_t	header_;	// Page header for current page
  int			bpc_,		// Bytes per color
			bpp_;		// Bytes per pixel
  uchar			*pixels_;	// Pixel buffer
  long			alloc_pixels_;	// Number of bytes allocated
  uchar			*colors_;	// Color data buffer
  long			alloc_colors_;	// Numebr of colors allocated
  float			factor_;	// Zoom factor
  int			xsize_;		// Bresenheim variables
  int			xstep_;		// ...
  int			xmod_;		// ...
  int			ysize_;		// ...

  Fl_Scrollbar		xscrollbar_;	// Horizontal scrollbar
  Fl_Scrollbar		yscrollbar_;	// Vertical scrollbar

  int			mode_;		// Viewing mode
  int			start_ex_,	// Start position (mouse coords)
			start_ey_;
  int			start_x_,	// Start position (image coords)
			start_y_;
  int			mouse_x_,	// Current position (image coords)
			mouse_y_;
  int			last_x_,	// Previous position (image coords)
			last_y_;

  uchar			device_colors_[15][3];
					// CMY device colors

  static void	image_cb(void *p, int X, int Y, int W, uchar *D);
  void		load_colors();
  void		save_colors();
  static void	scrollbar_cb(Fl_Widget *w, void *d);
  void		update_mouse_xy();
  void		update_scrollbars();
  int		visible_h() { return (h() - Fl::box_dh(box()) - (yscrollbar_.visible() ? SBWIDTH : 0)); }
  int		visible_w() { return (w() - Fl::box_dw(box()) - (xscrollbar_.visible() ? SBWIDTH : 0)); }

  protected:

  void		draw();

  public:

  RasterDisplay(int X, int Y, int W, int H, const char *L = 0);
  ~RasterDisplay();

  int			bytes_per_color() const { return bpc_; }
  int			bytes_per_pixel() const { return bpp_; }
  int			close_file();
  void			device_color(int n, Fl_Color c) { uchar r,g,b; Fl::get_color(c, r, g, b); device_colors_[n][0] = 255-r; device_colors_[n][1] = 255-g; device_colors_[n][2] = 255-b; save_colors();}
  Fl_Color		device_color(int n) { return (fl_rgb_color(255-device_colors_[n][0], 255-device_colors_[n][1], 255-device_colors_[n][2])); }
  uchar			*get_color(int X, int Y);
  uchar			*get_pixel(int X, int Y);
  int			handle(int event);
  cups_page_header2_t	*header() { return &header_; }
  int			is_subtractive();
  int			load_page();
  void			mode(int m) { mode_ = m; }
  int			mode() const { return mode_; }
  int			mouse_x() const { return mouse_x_; }
  int			mouse_y() const { return mouse_y_; }
  int			num_pages() const { return num_pages_; }
  int			page(void);
  void			page(int number);
  void			position(int X, int Y);
  void			resize(int X, int Y, int W, int H);
  void			scale(float factor);
  float			scale() const { return factor_; }
  int			start_x() const { return start_x_; }
  int			start_y() const { return start_y_; }
  int			open_file(const char *filename);
  int			xposition() { return xscrollbar_.value(); }
  int			yposition() { return yscrollbar_.value(); }
};


#endif // !RasterDisplay_h
