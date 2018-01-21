//
// "$Id: RasterDisplay.cxx 514 2015-08-26 21:39:41Z msweet $"
//
// CUPS/PWG Raster display widget methods.
//
// Copyright 2002-2018 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "RasterDisplay.h"
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Preferences.H>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>


//
// Absolute value macro...
//

#define abs(a) ((a) < 0 ? -(a) : (a))


//
// Constants...
//

#define D65_X	(0.412453 + 0.357580 + 0.180423)
#define D65_Y	(0.212671 + 0.715160 + 0.072169)
#define D65_Z	(0.019334 + 0.119193 + 0.950227)
#define SBWIDTH	17			// Scrollbar width


//
// Local globals...
//

static int		endian_offset = -1;
static Fl_Preferences	*prefs = NULL;



//
// Local functions...
//

static void	convert_cmy(cups_page_header2_t *header, uchar *line,
		            uchar *colors, uchar *pixels);
static void	convert_cmyk(cups_page_header2_t *header, uchar *line,
		             uchar *colors, uchar *pixels);
static void	convert_device(cups_page_header2_t *header, uchar *line, uchar *colors, uchar *pixels, uchar device_colors[][3]);
static void	convert_k(cups_page_header2_t *header, uchar *line,
		          uchar *colors, uchar *pixels);
static void	convert_kcmy(cups_page_header2_t *header, uchar *line,
		             uchar *colors, uchar *pixels);
static void	convert_kcmycm(cups_page_header2_t *header, uchar *line,
		               uchar *colors, uchar *pixels);
static void	convert_lab(cups_page_header2_t *header, uchar *line,
		            uchar *colors, uchar *pixels);
static void	convert_rgb(cups_page_header2_t *header, uchar *line,
		            uchar *colors, uchar *pixels);
static void	convert_rgba(cups_page_header2_t *header, int y, uchar *line,
		             uchar *colors, uchar *pixels);
static void	convert_rgbw(cups_page_header2_t *header, uchar *line,
		             uchar *colors, uchar *pixels);
static void	convert_w(cups_page_header2_t *header, uchar *line,
		          uchar *colors, uchar *pixels);
static void	convert_xyz(cups_page_header2_t *header, uchar *line,
		            uchar *colors, uchar *pixels);
static void	convert_ymc(cups_page_header2_t *header, uchar *line,
		            uchar *colors, uchar *pixels);
static void	convert_ymck(cups_page_header2_t *header, uchar *line,
		             uchar *colors, uchar *pixels);
static ssize_t	raster_cb(gzFile ctx, unsigned char *buffer, size_t length);


//
// 'RasterDisplay::RasterDisplay()' - Create a new raster display widget.
//

RasterDisplay::RasterDisplay(
    int        X,			// I - X position
    int        Y,			// I - Y position
    int        W,			// I - Width
    int        H,			// I - Height
    const char *L)			// I - Label string
  : Fl_Group(X, Y, W, H, L),
    xscrollbar_(X, Y + H - SBWIDTH, W - SBWIDTH, SBWIDTH),
    yscrollbar_(X + W - SBWIDTH, Y, SBWIDTH, H - SBWIDTH)
{
  end();

  box(FL_DOWN_BOX);

  memset(&header_, 0, sizeof(header_));
  memset(&next_header_, 0, sizeof(next_header_));

  fp_           = NULL;
  ras_          = NULL;
  ras_eof_      = 1;
  pixels_       = NULL;
  alloc_pixels_ = 0;
  colors_       = NULL;
  alloc_colors_ = 0;
  factor_       = 0.0;
  mode_         = RASTER_MODE_ZOOM_IN;
  mouse_x_      = 0;
  mouse_y_      = 0;

  xscrollbar_.type(FL_HORIZONTAL);
  xscrollbar_.callback(scrollbar_cb, this);

  yscrollbar_.type(FL_VERTICAL);
  yscrollbar_.callback(scrollbar_cb, this);

  resize(X, Y, W, H);
}


//
// 'RasterDisplay::~RasterDisplay()' - Destroy a raster display widget.
//

RasterDisplay::~RasterDisplay()
{
  close_file();
}


//
// 'RasterDisplay::close_file()' - Close an opened raster file.
//

int					// O - 1 on success, 0 on failure
RasterDisplay::close_file()
{
  if (ras_)
  {
    cupsRasterClose(ras_);
    ras_     = NULL;
    ras_eof_ = 1;
  }

  if (fp_)
  {
    gzclose(fp_);
    fp_ = NULL;
  }

  if (pixels_)
  {
    delete[] pixels_;
    pixels_       = NULL;
    alloc_pixels_ = 0;
  }

  if (colors_)
  {
    delete[] colors_;
    colors_       = NULL;
    alloc_colors_ = 0;
  }

  memset(&header_, 0, sizeof(header_));

  return (1);
}


//
// 'RasterDisplay::draw()' - Draw the raster display widget.
//

void
RasterDisplay::draw()
{
  int	xoff, yoff;			// Offset of image
  int	X, Y, W, H;			// Interior of widget


#ifdef DEBUG
  puts("RasterDisplay::draw()");
#endif // DEBUG

  X = x() + Fl::box_dx(box());
  Y = y() + Fl::box_dy(box());
  W = w() - Fl::box_dw(box());
  H = h() - Fl::box_dh(box());

  if (factor_)
  {
    xscrollbar_.show();
    yscrollbar_.show();

    W -= SBWIDTH;
    H -= SBWIDTH;
  }
  else
  {
    xscrollbar_.hide();
    yscrollbar_.hide();
  }

  if (damage() & FL_DAMAGE_SCROLL)
    fl_push_clip(X, Y, W, H);

  if (factor_)
    draw_box(box(), x(), y(), w() - SBWIDTH, h() - SBWIDTH, color());
  else
    draw_box();

  if (damage() & FL_DAMAGE_SCROLL)
    fl_pop_clip();
  else if (factor_)
  {
    fl_color(FL_GRAY);
    fl_rectf(x() + w() - SBWIDTH, y() + h() - SBWIDTH, SBWIDTH, SBWIDTH);
  }

  if (ras_ && pixels_ && header_.cupsWidth && header_.cupsHeight)
  {
#ifdef DEBUG
    printf("    pixels_=%p, cupsWidth=%d, cupsHeight=%d\n", pixels_,
           header_.cupsWidth, header_.cupsHeight);
#endif // DEBUG

    fl_push_clip(X, Y, W, H);

    if (xsize_ <= W)
      xoff = (W - xsize_) / 2;
    else
      xoff = 0;

    if (ysize_ <= H)
      yoff = (H - ysize_) / 2;
    else
      yoff = 0;

    xoff += X;
    yoff += Y;

    xstep_ = header_.cupsWidth / xsize_;
    xmod_  = header_.cupsWidth % xsize_;

#ifdef DEBUG
    printf("    xoff=%d, yoff=%d, xsize_=%d, ysize_=%d, xstep_=%d, xmod_=%d\n",
           xoff, yoff, xsize_, ysize_, xstep_, xmod_);
#endif // DEBUG

    fl_draw_image(image_cb, this, xoff, yoff,
                  xsize_ > W ? W : xsize_,
		  ysize_ > H ? H : ysize_, bpp_);

    fl_pop_clip();
  }

  draw_label(X, Y, W, H - 2 * labelsize());

  if (factor_)
  {
    if (damage() & FL_DAMAGE_SCROLL)
    {
      update_child(xscrollbar_);
      update_child(yscrollbar_);
    }
    else
    {
      draw_child(xscrollbar_);
      draw_child(yscrollbar_);
    }
  }
}


//
// 'RasterDisplay::get_color()' - Return the original color value for a coordinate.
//

uchar *					// O - Color values or NULL
RasterDisplay::get_color(int X,		// I - X position in image
                         int Y)		// I - Y position in image
{
  if (!colors_ || X < 0 || X >= (int)header_.cupsWidth ||
      Y < 0 || Y >= (int)header_.cupsHeight)
    return (NULL);
  else
    return (colors_ + (Y * header_.cupsWidth + X) * bpc_);
}


//
// 'RasterDisplay::get_pixel()' - Return the displayed color value for a coordinate.
//

uchar *					// O - RGB color value or NULL
RasterDisplay::get_pixel(int X,		// I - X position in image
                         int Y)		// I - Y position in image
{
  if (!pixels_ || X < 0 || X >= (int)header_.cupsWidth ||
      Y < 0 || Y >= (int)header_.cupsHeight)
    return (NULL);
  else
    return (pixels_ + (Y * header_.cupsWidth + X) * bpp_);
}


//
// 'RasterDisplay::handle()' - Handle events in the widget.
//

int					// O - 1 if handled, 0 otherwise
RasterDisplay::handle(int event)	// I - Event to handle
{
  if (header_.cupsWidth && header_.cupsHeight)
  {
    switch (event)
    {
      case FL_ENTER :
      case FL_MOVE :
          if ((!yscrollbar_.visible() || Fl::event_x() < yscrollbar_.x()) &&
	      (!xscrollbar_.visible() || Fl::event_y() < xscrollbar_.y()))
            switch (mode_)
	    {
	      case RASTER_MODE_PAN :
	          fl_cursor(FL_CURSOR_MOVE);
		  break;

	      case RASTER_MODE_ZOOM_IN :
	      case RASTER_MODE_ZOOM_OUT :
	          fl_cursor(FL_CURSOR_DEFAULT);
		  break;

	      default :
	      case RASTER_MODE_CLICK :
	          fl_cursor(FL_CURSOR_CROSS);
		  break;
            }
	  else
	    fl_cursor(FL_CURSOR_DEFAULT);
          return (1);

      case FL_LEAVE :
          fl_cursor(FL_CURSOR_DEFAULT);
          return (1);

      case FL_SHORTCUT :
	  switch (Fl::event_key())
	  {
	    case '-' : // Zoom out
        	if (factor_)
        	  scale(factor_ * 0.8f);
		else
		  scale((float)xsize_ / (float)header_.cupsWidth * 0.8f);

		return (1);

	    case '=' : // Zoom in
        	if (factor_)
        	  scale(factor_ * 1.25f);
		else
		  scale((float)xsize_ / (float)header_.cupsWidth * 1.25f);

		return (1);

            case '0' : // Fit
	        scale(0.0f);
		return (1);

            case '1' : // 100%
	        scale(1.0f);
		return (1);

            case '2' : // 200%
	        scale(2.0f);
		return (1);

            case '3' : // 300%
	        scale(3.0f);
		return (1);

            case '4' : // 400%
	        scale(4.0f);
		return (1);

            case 'p' : // Pan
	        mode(RASTER_MODE_PAN);
		fl_cursor(FL_CURSOR_MOVE);
		return (1);

            case 'z' : // Zoom in/out
	        if (Fl::event_state(FL_SHIFT))
	          mode(RASTER_MODE_ZOOM_OUT);
		else
	          mode(RASTER_MODE_ZOOM_IN);

		fl_cursor(FL_CURSOR_DEFAULT);
		return (1);

            case 'c' : // Click/color
	        mode(RASTER_MODE_CLICK);
		fl_cursor(FL_CURSOR_CROSS);
		return (1);
	  }
	  break;

      case FL_PUSH :
          if ((!yscrollbar_.visible() || Fl::event_x() < yscrollbar_.x()) &&
	      (!xscrollbar_.visible() || Fl::event_y() < xscrollbar_.y()))
	  {
	    update_mouse_xy();

	    last_x_   = Fl::event_x_root();
	    last_y_   = Fl::event_y_root();

	    start_x_  = mouse_x_;
	    start_y_  = mouse_y_;

	    start_ex_ = Fl::event_x();
	    start_ey_ = Fl::event_y();

            if (mode_ == RASTER_MODE_CLICK)
	      do_callback();

	    return (1);
	  }
	  break;

      case FL_DRAG :
          switch (mode_)
	  {
	    case RASTER_MODE_PAN :
		position(xscrollbar_.value() + last_x_ - Fl::event_x_root(),
	        	 yscrollbar_.value() + last_y_ - Fl::event_y_root());
                break;
	    case RASTER_MODE_ZOOM_IN :
		if ((Fl::event_x() < start_ex_ && Fl::event_y() < start_ey_) ||
	            (Fl::event_x() > start_ex_ && Fl::event_y() > start_ey_))
		  fl_cursor(FL_CURSOR_NWSE);
		else
		  fl_cursor(FL_CURSOR_NESW);

		window()->make_current();

		fl_overlay_rect(start_ex_, start_ey_,
	                	Fl::event_x() - start_ex_,
				Fl::event_y() - start_ey_);
	        break;
	    case RASTER_MODE_CLICK :
		do_callback();
                break;
          }

	  last_x_ = Fl::event_x_root();
	  last_y_ = Fl::event_y_root();
	  update_mouse_xy();
	  return (1);

      case FL_RELEASE :
	  update_mouse_xy();

          switch (mode_)
	  {
	    case RASTER_MODE_ZOOM_IN :
		window()->make_current();
		fl_cursor(FL_CURSOR_DEFAULT);
                fl_overlay_clear();

                if (Fl::event_button() == FL_LEFT_MOUSE)
		{
		  int W, H;


                  W = w() - SBWIDTH - Fl::box_dw(box());
                  H = h() - SBWIDTH - Fl::box_dh(box());

                  if (abs(start_ex_ - Fl::event_x()) > 4 ||
		      abs(start_ey_ - Fl::event_y()) > 4)
		  {
		    // Zoom to box...
		    float xfactor, yfactor;

		    xfactor = (float)W / (float)abs(mouse_x_ - start_x_);
		    yfactor = (float)H / (float)abs(mouse_y_ - start_y_);

//                    printf("start_x_=%d, start_y_=%d, mouse_x_=%d, mouse_y_=%d\n",
//		           start_x_, start_y_, mouse_x_, mouse_y_);
//		    printf("W=%d, H=%d, dx=%d, dy=%d\n", W, H,
//		           abs(mouse_x_ - start_x_), abs(mouse_x_ - start_x_));
//                    printf("xfactor=%g, yfactor=%g\n", xfactor, yfactor);

		    scale(xfactor < yfactor ? xfactor : yfactor);
		    position((int)((mouse_x_ < start_x_ ? mouse_x_ : start_x_) * scale()),
		             (int)((mouse_y_ < start_y_ ? mouse_y_ : start_y_) * scale()));
		  }
		  else
		  {
		    if (factor_)
        	      scale(factor_ * 1.25f);
		    else
		      scale((float)xsize_ / (float)header_.cupsWidth * 1.25f);

		    position((int)((mouse_x_ < start_x_ ? mouse_x_ : start_x_) * scale()) - W / 2,
		             (int)((mouse_y_ < start_y_ ? mouse_y_ : start_y_) * scale()) - H / 2);
                  }
		  break;
		}

	    case RASTER_MODE_ZOOM_OUT :
        	if (factor_)
        	  scale(factor_ * 0.8f);
		else
		  scale((float)xsize_ / (float)header_.cupsWidth * 0.8f);
		break;

	    case RASTER_MODE_CLICK :
		do_callback();
		break;
          }
	  return (1);
    }
  }

  return (Fl_Group::handle(event));
}


//
// 'RasterDisplay::image_cb()' - Provide a single line of an image.
//

void
RasterDisplay::image_cb(void  *p,	// I - Raster display widget
                	int   X,	// I - X offset
			int   Y,	// I - Y offset
			int   W,	// I - Width of image row
			uchar *D)	// O - Image data
{
  RasterDisplay		*display;	// Display widget
  const uchar		*inptr;		// Pointer into image
  int			bpp,		// Bytes per pixel value
			xerr,		// Bresenheim values
			xstep,		// ...
			xmod,		// ...
			xsize;		// ...


  display = (RasterDisplay *)p;
  bpp     = display->bpp_;
  xstep   = display->xstep_ * bpp;
  xmod    = display->xmod_;
  xsize   = display->xsize_;
  xerr    = (X * xmod) % xsize;

  if (xsize > (display->w() - Fl::box_dw(display->box()) - SBWIDTH))
    X = (X + display->xscrollbar_.value()) *
        (display->header_.cupsWidth - 1) / (xsize - 1);
  else
    X = X * (display->header_.cupsWidth - 1) / (xsize - 1);

  if (display->ysize_ > (display->h() - Fl::box_dh(display->box()) - SBWIDTH))
    Y = (Y + display->yscrollbar_.value()) *
        (display->header_.cupsHeight - 1) / (display->ysize_ - 1);
  else
    Y = Y * (display->header_.cupsHeight - 1) / (display->ysize_ - 1);

  inptr = display->pixels_ + (Y * display->header_.cupsWidth + X) * bpp;

  if (xstep == bpp && xmod == 0)
    memcpy(D, inptr, W * bpp);
  else if (bpp == 1)
  {
    for (; W > 0; W --)
    {
      *D++ = *inptr;

      inptr += xstep;
      xerr  += xmod;

      if (xerr >= xsize)
      {
	xerr  -= xsize;
	inptr += bpp;
      }
    }
  }
  else
  {
    for (; W > 0; W --)
    {
      *D++ = inptr[0];
      *D++ = inptr[1];
      *D++ = inptr[2];

      inptr += xstep;
      xerr  += xmod;

      if (xerr >= xsize)
      {
	xerr  -= xsize;
	inptr += bpp;
      }
    }
  }
}


//
// 'RasterDisplay::is_subtractive()' - Is the color space subtractive?
//

int
RasterDisplay::is_subtractive()
{
  return ((header_.cupsColorSpace >= CUPS_CSPACE_K && header_.cupsColorSpace <= CUPS_CSPACE_SILVER) ||
          (header_.cupsColorSpace >= CUPS_CSPACE_DEVICE1 && header_.cupsColorSpace <= CUPS_CSPACE_DEVICEF));
}


//
// 'RasterDisplay::load_colors()' - Load device colors.
//

void
RasterDisplay::load_colors()
{
  int		i;			// Looping var
  char		key[256],		// Key string
		*value;			// Value
  unsigned	c, m ,y;		// Colors


  if (!is_subtractive() || header_.cupsBitsPerColor < 8)
    return;

  if (!prefs)
    prefs = new Fl_Preferences(Fl_Preferences::USER, "msweet.org", "rasterview");

  for (i = 0; i < header_.cupsNumColors; i ++)
  {
    snprintf(key, sizeof(key), "cs%dc%d", header_.cupsColorSpace, i);

    if (prefs->get(key, value, "") && sscanf(value, "%u %u %u", &c, &m, &y) == 3)
    {
      device_colors_[i][0] = c;
      device_colors_[i][1] = m;
      device_colors_[i][2] = y;
    }
  }
}


//
// 'RasterDisplay::load_page()' - Load the next page from a raster stream.
//

int					// O - 1 on success, 0 on failure
RasterDisplay::load_page()
{
  union
  {
    unsigned char	bytes[sizeof(int)];
    int			integer;
  }	endian_test;			// Endian test variable


  if (!ras_ || ras_eof_)
    return (0);

  if (next_header_.cupsColorOrder == CUPS_ORDER_PLANAR)
  {
    fl_alert("Sorry, we don't support planar raster data at this time!");
    close_file();
    return (0);
  }

  // Copy the next page header to the current one and allocate memory
  // for the page data...
  memcpy(&header_, &next_header_, sizeof(header_));

#ifdef DEBUG
  fprintf(stderr, "DEBUG: sizeof(cups_page_header_t) = %d\n", (int)sizeof(cups_page_header_t));
  fprintf(stderr, "DEBUG: sizeof(cups_page_header2_t) = %d\n", (int)sizeof(cups_page_header2_t));
  fprintf(stderr, "DEBUG: MediaClass = \"%s\"\n", header_.MediaClass);
  fprintf(stderr, "DEBUG: MediaColor = \"%s\"\n", header_.MediaColor);
  fprintf(stderr, "DEBUG: MediaType = \"%s\"\n", header_.MediaType);
  fprintf(stderr, "DEBUG: OutputType = \"%s\"\n", header_.OutputType);

  fprintf(stderr, "DEBUG: AdvanceDistance = %d(%x)\n", header_.AdvanceDistance,
          header_.AdvanceDistance);
  fprintf(stderr, "DEBUG: AdvanceMedia = %d(%x)\n", header_.AdvanceMedia,
          header_.AdvanceMedia);
  fprintf(stderr, "DEBUG: Collate = %d(%x)\n", header_.Collate,
          header_.Collate);
  fprintf(stderr, "DEBUG: CutMedia = %d(%x)\n", header_.CutMedia,
          header_.CutMedia);
  fprintf(stderr, "DEBUG: Duplex = %d(%x)\n", header_.Duplex, header_.Duplex);
  fprintf(stderr, "DEBUG: HWResolution = [ %d(%x) %d(%x) ]\n",
          header_.HWResolution[0], header_.HWResolution[0],
          header_.HWResolution[1], header_.HWResolution[1]);
  fprintf(stderr,
          "DEBUG: ImagingBoundingBox = [ %d(%x) %d(%x) %d(%x) %d(%x) ]\n",
          header_.ImagingBoundingBox[0], header_.ImagingBoundingBox[0],
	  header_.ImagingBoundingBox[1], header_.ImagingBoundingBox[1],
	  header_.ImagingBoundingBox[2], header_.ImagingBoundingBox[2],
          header_.ImagingBoundingBox[3], header_.ImagingBoundingBox[3]);
  fprintf(stderr, "DEBUG: InsertSheet = %d(%x)\n", header_.InsertSheet,
          header_.InsertSheet);
  fprintf(stderr, "DEBUG: Jog = %d(%x)\n", header_.Jog, header_.Jog);
  fprintf(stderr, "DEBUG: LeadingEdge = %d(%x)\n", header_.LeadingEdge,
          header_.LeadingEdge);
  fprintf(stderr, "DEBUG: Margins = [ %d(%x) %d(%x) ]\n",
          header_.Margins[0], header_.Margins[0],
          header_.Margins[1], header_.Margins[1]);
  fprintf(stderr, "DEBUG: ManualFeed = %d(%x)\n", header_.ManualFeed,
          header_.ManualFeed);
  fprintf(stderr, "DEBUG: MediaPosition = %d(%x)\n", header_.MediaPosition,
          header_.MediaPosition);
  fprintf(stderr, "DEBUG: MediaWeight = %d(%x)\n", header_.MediaWeight,
          header_.MediaWeight);
  fprintf(stderr, "DEBUG: MirrorPrint = %d(%x)\n", header_.MirrorPrint,
          header_.MirrorPrint);
  fprintf(stderr, "DEBUG: NegativePrint = %d(%x)\n", header_.NegativePrint,
          header_.NegativePrint);
  fprintf(stderr, "DEBUG: NumCopies = %d(%x)\n", header_.NumCopies,
          header_.NumCopies);
  fprintf(stderr, "DEBUG: Orientation = %d(%x)\n", header_.Orientation,
          header_.Orientation);
  fprintf(stderr, "DEBUG: OutputFaceUp = %d(%x)\n", header_.OutputFaceUp,
          header_.OutputFaceUp);
  fprintf(stderr, "DEBUG: PageSize = [ %d(%x) %d(%x) ]\n",
          header_.PageSize[0], header_.PageSize[0],
          header_.PageSize[1], header_.PageSize[1]);
  fprintf(stderr, "DEBUG: Separations = %d(%x)\n", header_.Separations,
          header_.Separations);
  fprintf(stderr, "DEBUG: TraySwitch = %d(%x)\n", header_.TraySwitch,
          header_.TraySwitch);
  fprintf(stderr, "DEBUG: Tumble = %d(%x)\n", header_.Tumble, header_.Tumble);
  fprintf(stderr, "DEBUG: cupsWidth = %d(%x)\n", header_.cupsWidth,
          header_.cupsWidth);
  fprintf(stderr, "DEBUG: cupsHeight = %d(%x)\n", header_.cupsHeight,
          header_.cupsHeight);
  fprintf(stderr, "DEBUG: cupsMediaType = %d(%x)\n", header_.cupsMediaType,
          header_.cupsMediaType);
  fprintf(stderr, "DEBUG: cupsBitsPerColor = %d(%x)\n",
          header_.cupsBitsPerColor, header_.cupsBitsPerColor);
  fprintf(stderr, "DEBUG: cupsBitsPerPixel = %d(%x)\n",
          header_.cupsBitsPerPixel, header_.cupsBitsPerPixel);
  fprintf(stderr, "DEBUG: cupsBytesPerLine = %d(%x)\n",
          header_.cupsBytesPerLine, header_.cupsBytesPerLine);
  fprintf(stderr, "DEBUG: cupsColorOrder = %d(%x)\n", header_.cupsColorOrder,
          header_.cupsColorOrder);
  fprintf(stderr, "DEBUG: cupsColorSpace = %d(%x)\n", header_.cupsColorSpace,
          header_.cupsColorSpace);
  fprintf(stderr, "DEBUG: cupsCompression = %d(%x)\n", header_.cupsCompression,
          header_.cupsCompression);
  fprintf(stderr, "DEBUG: cupsNumColors = %d(%x)\n", header_.cupsNumColors,
          header_.cupsNumColors);
#endif // DEBUG

  bpp_ = ((header_.cupsBitsPerColor < 8 || !is_subtractive()) && header_.cupsNumColors == 1) ? 1 : 3;

  if (header_.cupsWidth == 0 || header_.cupsWidth > 1000000 ||
      header_.cupsHeight == 0 || header_.cupsHeight > 1000000)
  {
    fl_alert("Sorry, image dimensions are out of range (%ux%u)!",
             header_.cupsWidth, header_.cupsHeight);
    close_file();
    return (0);
  }

  int	pixelsize = header_.cupsWidth * bpp_;

  long bytes = pixelsize * header_.cupsHeight;

  if (bytes > (256 * 1024 * 1024))
  {
    fl_alert("Sorry, image dimensions are out of range (%ux%u)!",
             header_.cupsWidth, header_.cupsHeight);
    close_file();
    return (0);
  }

  if (bytes > alloc_pixels_)
  {
    if (pixels_)
      delete[] pixels_;

    pixels_       = new uchar[bytes];
    alloc_pixels_ = bytes;

    if (!pixels_)
    {
      fl_alert("Unable to allocate %ld bytes for page data!", bytes);
      return (0);
    }
  }

  bpc_ = (header_.cupsBitsPerPixel + 7) / 8;

  if (header_.cupsColorOrder != CUPS_ORDER_CHUNKED)
    bpc_ *= header_.cupsNumColors;

  int colorsize = header_.cupsWidth * bpc_;

  bytes = colorsize * header_.cupsHeight;

  if (bytes > alloc_colors_)
  {
    if (colors_)
      delete[] colors_;

    colors_       = new uchar[bytes];
    alloc_colors_ = bytes;

    if (!colors_)
    {
      fl_alert("Unable to allocate %ld bytes for page data!", bytes);
      return (0);
    }
  }

  // Update the page dimensions/scaling...
  resize(x(), y(), w(), h());

  memset(colors_, 0, alloc_colors_);
  memset(pixels_, 255, alloc_pixels_);

  uchar *line = new uchar[header_.cupsBytesPerLine];

  if (!line)
  {
    fl_alert("Unable to allocate %d bytes for raster data!",
             header_.cupsBytesPerLine);
    return (0);
  }

  // See what word order we need to use...
  if (endian_offset < 0)
  {
    memset(&endian_test, 0, sizeof(endian_test));
    endian_test.bytes[sizeof(endian_test.bytes) - 1] = 1;
    if (endian_test.integer == 1)
      endian_offset = 0;		// Big endian
    else
      endian_offset = 1;		// Little endian
  }

  // Set device colors...
  memset(device_colors_, 255, sizeof(device_colors_));

  switch (header_.cupsColorSpace)
  {
    case CUPS_CSPACE_DEVICE3 :
    case CUPS_CSPACE_DEVICE4 :
    case CUPS_CSPACE_CMY :
    case CUPS_CSPACE_CMYK :
        device_colors_[0][1] = device_colors_[0][2] = 0;
        device_colors_[1][0] = device_colors_[1][2] = 0;
        device_colors_[2][0] = device_colors_[2][1] = 0;
	break;

    case CUPS_CSPACE_YMC :
    case CUPS_CSPACE_YMCK :
        device_colors_[0][0] = device_colors_[0][1] = 0;
        device_colors_[1][0] = device_colors_[1][2] = 0;
        device_colors_[2][1] = device_colors_[2][2] = 0;
        break;

    case CUPS_CSPACE_DEVICE6 :
        device_colors_[0][1] = device_colors_[0][2] = 0;
        device_colors_[1][0] = device_colors_[1][2] = 0;
        device_colors_[2][0] = device_colors_[2][1] = 0;
        device_colors_[4][0] = 127; device_colors_[4][1] = device_colors_[4][2] = 0;
        device_colors_[5][1] = 127; device_colors_[5][0] = device_colors_[5][2] = 0;
	break;

    case CUPS_CSPACE_W :
    case CUPS_CSPACE_SW :
        device_colors_[0][0] = device_colors_[0][1] = device_colors_[0][2] = 0;
        break;

    case CUPS_CSPACE_RGB :
    case CUPS_CSPACE_SRGB :
    case CUPS_CSPACE_ADOBERGB :
        device_colors_[0][0] = 0;
        device_colors_[1][1] = 0;
        device_colors_[2][2] = 0;
        break;

    default :
        break;
  }

  load_colors();

  // Read the raster data...
  uchar *pptr,				// Pointer into pixels_
	*cptr;				// Pointer into colors_
  int	py;				// Current position in page

  for (py = header_.cupsHeight, cptr = colors_, pptr = pixels_;
       py > 0;
       py --, cptr += colorsize, pptr += pixelsize)
  {
    if ((py % header_.HWResolution[1]) == 0)
    {
      // Update the screen to show progress...
      redraw();
      Fl::check();
    }

    if (!cupsRasterReadPixels(ras_, line, header_.cupsBytesPerLine))
    {
      fl_alert("Unable to read page data: %s", strerror(errno));
      delete[] line;
      ras_eof_ = 1;
      return (0);
    }

    switch (header_.cupsColorSpace)
    {
      case CUPS_CSPACE_DEVICE1 :
      case CUPS_CSPACE_DEVICE2 :
      case CUPS_CSPACE_DEVICE3 :
      case CUPS_CSPACE_DEVICE4 :
      case CUPS_CSPACE_DEVICE5 :
      case CUPS_CSPACE_DEVICE6 :
      case CUPS_CSPACE_DEVICE7 :
      case CUPS_CSPACE_DEVICE8 :
      case CUPS_CSPACE_DEVICE9 :
      case CUPS_CSPACE_DEVICEA :
      case CUPS_CSPACE_DEVICEB :
      case CUPS_CSPACE_DEVICEC :
      case CUPS_CSPACE_DEVICED :
      case CUPS_CSPACE_DEVICEE :
      case CUPS_CSPACE_DEVICEF :
          convert_device(&header_, line, cptr, pptr, device_colors_);
          break;

      case CUPS_CSPACE_W :
      case CUPS_CSPACE_SW :
          convert_w(&header_, line, cptr, pptr);
	  break;

      case CUPS_CSPACE_RGB :
      case CUPS_CSPACE_SRGB :
      case CUPS_CSPACE_ADOBERGB :
          convert_rgb(&header_, line, cptr, pptr);
	  break;

      case CUPS_CSPACE_RGBA :
          convert_rgba(&header_, py, line, cptr, pptr);
	  break;

      case CUPS_CSPACE_RGBW :
          convert_rgbw(&header_, line, cptr, pptr);
	  break;

      case CUPS_CSPACE_K :
      case CUPS_CSPACE_WHITE :
      case CUPS_CSPACE_GOLD :
      case CUPS_CSPACE_SILVER :
          if (header_.cupsBitsPerColor >= 8)
            convert_device(&header_, line, cptr, pptr, device_colors_);
          else
	    convert_k(&header_, line, cptr, pptr);
	  break;

      case CUPS_CSPACE_CMY :
          if (header_.cupsBitsPerColor >= 8)
            convert_device(&header_, line, cptr, pptr, device_colors_);
          else
	    convert_cmy(&header_, line, cptr, pptr);
	  break;

      case CUPS_CSPACE_YMC :
          if (header_.cupsBitsPerColor >= 8)
            convert_device(&header_, line, cptr, pptr, device_colors_);
          else
            convert_ymc(&header_, line, cptr, pptr);
	  break;

      case CUPS_CSPACE_KCMYcm :
          if (header_.cupsBitsPerColor == 1)
	  {
	    convert_kcmycm(&header_, line, cptr, pptr);
	    break;
	  }
      case CUPS_CSPACE_KCMY :
          if (header_.cupsBitsPerColor >= 8)
            convert_device(&header_, line, cptr, pptr, device_colors_);
          else
	    convert_kcmy(&header_, line, cptr, pptr);
	  break;

      case CUPS_CSPACE_CMYK :
          if (header_.cupsBitsPerColor >= 8)
            convert_device(&header_, line, cptr, pptr, device_colors_);
          else
	    convert_cmyk(&header_, line, cptr, pptr);
	  break;

      case CUPS_CSPACE_YMCK :
      case CUPS_CSPACE_GMCK :
      case CUPS_CSPACE_GMCS :
          if (header_.cupsBitsPerColor >= 8)
            convert_device(&header_, line, cptr, pptr, device_colors_);
          else
	    convert_ymck(&header_, line, cptr, pptr);
	  break;

      case CUPS_CSPACE_CIEXYZ :
          convert_xyz(&header_, line, cptr, pptr);
	  break;

      case CUPS_CSPACE_CIELab :
      case CUPS_CSPACE_ICC1 :
      case CUPS_CSPACE_ICC2 :
      case CUPS_CSPACE_ICC3 :
      case CUPS_CSPACE_ICC4 :
      case CUPS_CSPACE_ICC5 :
      case CUPS_CSPACE_ICC6 :
      case CUPS_CSPACE_ICC7 :
      case CUPS_CSPACE_ICC8 :
      case CUPS_CSPACE_ICC9 :
      case CUPS_CSPACE_ICCA :
      case CUPS_CSPACE_ICCB :
      case CUPS_CSPACE_ICCC :
      case CUPS_CSPACE_ICCD :
      case CUPS_CSPACE_ICCE :
      case CUPS_CSPACE_ICCF :
          convert_lab(&header_, line, cptr, pptr);
	  break;
    }
  }

  delete[] line;

  // Mark the page for redisplay...
  redraw();

  // Try reading the next page header...
  if (!cupsRasterReadHeader2(ras_, &next_header_))
    ras_eof_ = 1;

  // Return successfully...
  return (1);
}


//
// 'RasterDisplay::open_file()' - Open a raster file for viewing.
//

int					// O - 1 on success, 0 on failure
RasterDisplay::open_file(
    const char *filename)		// I - File to open
{
//  printf("RasterDisplay::open_file(filename=\"%s\")\n", filename);

  close_file();

  if ((fp_ = gzopen(filename, "r")) == NULL)
  {
    fl_alert("Unable to open file: %s", strerror(errno));
    return (0);
  }

  if ((ras_ = cupsRasterOpenIO((cups_raster_iocb_t)raster_cb, fp_, CUPS_RASTER_READ)) == NULL)
  {
    fl_alert("cupsRasterOpenIO() failed.");
    gzclose(fp_);
    fp_ = NULL;
    return (0);
  }

  if (!cupsRasterReadHeader2(ras_, &next_header_))
  {
    fl_alert("cupsRasterReadHeader() failed!");
    close_file();
    return (0);
  }

  ras_eof_ = 0;

  return (load_page());
}


//
// 'RasterDisplay::position()' - Reposition the image on the screen.
//

void
RasterDisplay::position(int X,		// I - New X offset
                        int Y)		// I - New Y offset
{
  int	W, H;				// Interior size


  W = w() - SBWIDTH;
  H = h() - SBWIDTH;

  if (X < 0)
    X = 0;
  else if (X > (xsize_ - W))
    X = xsize_ - W;

  if (Y < 0)
    Y = 0;
  else if (Y > (ysize_ - H))
    Y = ysize_ - H;

  xscrollbar_.value(X, W, 0, xsize_);
  yscrollbar_.value(Y, H, 0, ysize_);

  damage(FL_DAMAGE_SCROLL);
}


//
// 'RasterDisplay::resize()' - Resize the raster display widget.
//

void
RasterDisplay::resize(int X,		// I - New X position
                      int Y,		// I - New Y position
		      int W,		// I - New width
		      int H)		// I - New height
{
  Fl_Widget::resize(X, Y, W, H);

  xscrollbar_.resize(X, Y + H - SBWIDTH, W - SBWIDTH, SBWIDTH);
  yscrollbar_.resize(X + W - SBWIDTH, Y, SBWIDTH, H - SBWIDTH);

  W -= Fl::box_dw(box()) + SBWIDTH;
  H -= Fl::box_dh(box()) + SBWIDTH;

  if (factor_ == 0.0f && header_.cupsWidth && header_.cupsHeight)
  {
    xsize_ = W;

    if (xsize_ > (int)(header_.cupsWidth * 4))
      xsize_ = header_.cupsWidth * 4;

    ysize_ = xsize_ * header_.cupsHeight / header_.cupsWidth;

    if (ysize_ > H)
    {
      ysize_ = H;

      if (ysize_ > (int)(header_.cupsHeight * 4))
	ysize_ = header_.cupsHeight * 4;

      xsize_ = ysize_ * header_.cupsWidth / header_.cupsHeight;
    }
  }

  update_scrollbars();

  redraw();
}


//
// 'RasterDisplay::save_colors()' - Save device colors.
//

void
RasterDisplay::save_colors()
{
  int		i;			// Looping var
  char		key[256],		// Key string
		value[256];		// Value


  if (!is_subtractive() || header_.cupsBitsPerColor < 8)
    return;

  if (!prefs)
    prefs = new Fl_Preferences(Fl_Preferences::USER, "msweet.org", "rasterview");

  for (i = 0; i < header_.cupsNumColors; i ++)
  {
    snprintf(key, sizeof(key), "cs%dc%d", header_.cupsColorSpace, i);
    snprintf(value, sizeof(value), "%d %d %d", device_colors_[i][0], device_colors_[i][1], device_colors_[i][2]);

    prefs->set(key, value);
  }

  prefs->flush();
}


//
// 'RasterDisplay::scale()' - Scale the image.
//

void
RasterDisplay::scale(float factor)	// I - Scaling factor (0 = auto)
{
  int	X, Y, W, H;			// Interior of widget
  float	ratio;				// Scaling ratio


  if (factor > 10.0f)
    factor = 10.0f;

  // Make sure that the image doesn't get scaled to nothin'...
  if (header_.cupsWidth && header_.cupsHeight)
  {
    if (factor > 0.0f && (header_.cupsWidth * factor) < 32.0f && header_.cupsWidth > 32)
      factor = 32.0f / header_.cupsWidth;

    if (factor > 0.0f && (header_.cupsHeight * factor) < 32.0f && header_.cupsHeight > 32)
      factor = 32.0f / header_.cupsHeight;
  }

  if (factor_ == 0.0f)
    ratio = 0.0f;
  else
    ratio = factor / factor_;

  factor_ = factor;

  redraw();

  if (!header_.cupsWidth || !header_.cupsHeight)
    return;

  W = w() - SBWIDTH - Fl::box_dw(box());
  H = h() - SBWIDTH - Fl::box_dh(box());

  if (factor_ == 0.0f)
  {
    xsize_ = W;

    if (xsize_ > (int)(header_.cupsWidth * 4))
      xsize_ = header_.cupsWidth * 4;

    ysize_ = xsize_ * header_.cupsHeight / header_.cupsWidth;

    if (ysize_ > H)
    {
      ysize_ = H;

      if (ysize_ > (int)(header_.cupsHeight * 4))
	ysize_ = header_.cupsHeight * 4;

      xsize_ = ysize_ * header_.cupsWidth / header_.cupsHeight;
    }

    X = 0;
    Y = 0;
  }
  else
  {
    xsize_ = (int)((float)header_.cupsWidth * factor_ + 0.5f);
    ysize_ = (int)((float)header_.cupsHeight * factor_ + 0.5f);

    if (xsize_ <= W)
    {
      // The image will be centered...
      X = 0;
    }
    else if (ratio == 0.0)
    {
      // Previous zoom was auto-fit, center it...
      X = (xsize_ - W) / 2;
    }
    else
    {
      // Try to center on the previous location...
      X = (int)((xscrollbar_.value() + W / 2) * ratio) - W / 2;
    }

    if (ysize_ <= H)
    {
      // The image will be centered...
      Y = 0;
    }
    else if (ratio == 0.0)
    {
      // Previous zoom was auto-fit, center it...
      Y = (ysize_ - H) / 2;
    }
    else
    {
      // Try to center on the previous location...
      Y = (int)((yscrollbar_.value() + H / 2) * ratio) - H / 2;
    }
  }

  // Update the scrollbars...
  if (X < 0)
    X = 0;
  else if (X > (xsize_ - W))
    X = xsize_ - W;

  xscrollbar_.value(X, W, 0, xsize_);

  if (xsize_ <= W)
    xscrollbar_.deactivate();
  else
    xscrollbar_.activate();

  if (Y < 0)
    Y = 0;
  else if (Y > (ysize_ - H))
    Y = ysize_ - H;

  yscrollbar_.value(Y, H, 0, ysize_);

  if (ysize_ <= H)
    yscrollbar_.deactivate();
  else
    yscrollbar_.activate();
}


//
// 'RasterDisplay::scrollbar_cb()' - Update the display based on the scrollbar position.
//

void
RasterDisplay::scrollbar_cb(
    Fl_Widget *w,			// I - Widget
    void      *d)			// I - Raster display widget
{
  RasterDisplay	*img = (RasterDisplay *)d;


  img->damage(FL_DAMAGE_SCROLL);
}


//
// 'RasterDisplay::update_mouse_xy()' - Update the mouse X and Y values.
//

void
RasterDisplay::update_mouse_xy()
{
  int	X, Y;				// X,Y position
  int	W, H;				// Width and height


  X = Fl::event_x() - x() - Fl::box_dx(box());
  Y = Fl::event_y() - y() - Fl::box_dy(box());
  W = w() - SBWIDTH - Fl::box_dw(box());
  H = h() - SBWIDTH - Fl::box_dh(box());

  if (!ras_ || xsize_ <= 0 || ysize_ <= 0)
  {
    mouse_x_ = -1;
    mouse_y_ = -1;
  }

  if (xsize_ < W)
  {
    X -= (W - xsize_) / 2;

    if (X < 0)
      mouse_x_ = 0;
    else if (X >= xsize_)
      mouse_x_ = header_.cupsWidth;
    else
      mouse_x_ = X * header_.cupsWidth / xsize_;
  }
  else
    mouse_x_ = (xscrollbar_.value() + X) * header_.cupsWidth / xsize_;

  if (ysize_ < H)
  {
    Y -= (H - ysize_) / 2;

    if (Y < 0)
      mouse_y_ = 0;
    else if (Y >= ysize_)
      mouse_y_ = header_.cupsHeight;
    else
      mouse_y_ = Y * header_.cupsHeight / ysize_;
  }
  else
    mouse_y_ = (yscrollbar_.value() + Y) * header_.cupsHeight / ysize_;

  if (mouse_x_ < 0)
    mouse_x_ = 0;
  else if (mouse_x_ > (int)header_.cupsWidth)
    mouse_x_ = header_.cupsWidth;

  if (mouse_y_ < 0)
    mouse_y_ = 0;
  else if (mouse_y_ > (int)header_.cupsHeight)
    mouse_y_ = header_.cupsHeight;

//  printf("xscrollbar_=%d, yscrollbar_=%d\n", xscrollbar_.value(),
//         yscrollbar_.value());
//  printf("mouse_x_=%d, mouse_y_=%d\n", mouse_x_, mouse_y_);
}


//
// 'RasterDisplay::update_scrollbars()' - Update the scrollbars.
//

void
RasterDisplay::update_scrollbars()
{
  int	X, Y;				// X/Y offsets
  int	W, H;				// Interior size


  if (header_.cupsWidth && header_.cupsHeight)
  {
    W = w() - SBWIDTH - Fl::box_dw(box());
    H = h() - SBWIDTH - Fl::box_dh(box());

    X = xscrollbar_.value();
    if (X > (xsize_ - W))
      X = xsize_ - W;
    else if (X < 0)
      X = 0;

    xscrollbar_.value(X, W, 0, xsize_);

    if (xsize_ <= W)
      xscrollbar_.deactivate();
    else
      xscrollbar_.activate();

    Y = yscrollbar_.value();
    if (Y > (ysize_ - H))
      Y = ysize_ - H;
    else if (Y < 0)
      Y = 0;

    yscrollbar_.value(Y, H, 0, ysize_);

    if (ysize_ <= H)
      yscrollbar_.deactivate();
    else
      yscrollbar_.activate();
  }
  else
  {
    xscrollbar_.value(0, 1, 0, 1);
    yscrollbar_.value(0, 1, 0, 1);
  }
}


//
// 'convert_cmy()' - Convert CMY or YMC raster data.
//

static void
convert_cmy(
    cups_page_header2_t *header,	// I - Raster header */
    uchar               *line,		// I - Raster line
    uchar               *colors,	// O - Original pixels
    uchar               *pixels)	// O - RGB pixels
{
  int	x,				// X position in line
	w,				// Width of line
	val;				// Pixel value
  uchar	*cptr,				// Cyan pointer
	*mptr,				// Magenta pointer
	*yptr,				// Yellow pointer
	bit;				// Current bit


  w = header->cupsWidth;

  if (header->cupsColorOrder == CUPS_ORDER_CHUNKED)
  {
    // Chunky
    switch (header->cupsBitsPerColor)
    {
      case 1 :
          for (x = w; x > 0; x -= 2, pixels += 6)
	  {
	    bit       = *line++;
	    *colors++ = bit >> 4;

	    if (bit & 0x40)
	      pixels[0] = 0;
	    if (bit & 0x20)
	      pixels[1] = 0;
	    if (bit & 0x10)
	      pixels[2] = 0;

	    if (x > 1)
	    {
	      *colors++ = bit & 15;

	      if (bit & 0x04)
	        pixels[3] = 0;
	      if (bit & 0x02)
	        pixels[4] = 0;
	      if (bit & 0x01)
	        pixels[5] = 0;
	    }
          }
          break;
      case 2 :
          for (x = w; x > 0; x --)
	  {
	    bit = *line++;
	    *colors++ = bit;

	    *pixels++ = 255 - 85 * ((bit & 0x30) >> 4);
	    *pixels++ = 255 - 85 * ((bit & 0x0c) >> 2);
	    *pixels++ = 255 - 85 * (bit & 0x03);
          }
          break;
      case 4 :
          for (x = w; x > 0; x --)
	  {
	    *colors++ = bit = *line++;
	    *pixels++ = 255 - 17 * (bit & 0x0f);

	    *colors++ = bit = *line++;
	    *pixels++ = 255 - 17 * ((bit & 0xf0) >> 4);
	    *pixels++ = 255 - 17 * (bit & 0x0f);
          }
          break;
      case 8 :
          for (x = w; x > 0; x --)
	  {
	    *colors++ = val = *line++;
	    *pixels++ = 255 - val;

	    *colors++ = val = *line++;
	    *pixels++ = 255 - val;

	    *colors++ = val = *line++;
	    *pixels++ = 255 - val;
          }
          break;
      case 16 :
	  if (endian_offset)
	  {
            for (x = w; x > 0; x --)
	    {
	      *colors++ = *line++;
	      *colors++ = val = *line++;
	      *pixels++ = 255 - val;

	      *colors++ = *line++;
	      *colors++ = val = *line++;
	      *pixels++ = 255 - val;

	      *colors++ = *line++;
	      *colors++ = val = *line++;
	      *pixels++ = 255 - val;
            }
	  }
	  else
	  {
            for (x = w; x > 0; x --)
	    {
	      *colors++ = val = *line++;
	      *pixels++ = 255 - val;
	      *colors++ = *line++;

	      *colors++ = val = *line++;
	      *pixels++ = 255 - val;
	      *colors++ = *line++;

	      *colors++ = val = *line++;
	      *pixels++ = 255 - val;
	      *colors++ = *line++;
            }
	  }
          break;
    }
  }
  else
  {
    // Banded
    int bytespercolor = (header->cupsBitsPerColor * header->cupsWidth + 7) / 8;


    cptr = line;
    mptr = line + bytespercolor;
    yptr = line + 2 * bytespercolor;

    switch (header->cupsBitsPerColor)
    {
      case 1 :
          for (x = w, bit = 0x80; x > 0; x --, pixels += 3)
	  {
	    if (*cptr & bit)
	    {
	      *colors++ = 1;
	      pixels[0] = 0;
	    }
	    else
	      colors ++;

	    if (*mptr & bit)
	    {
	      *colors++ = 1;
	      pixels[1] = 0;
	    }
	    else
	      colors ++;

	    if (*yptr & bit)
	    {
	      *colors++ = 1;
	      pixels[2] = 0;
	    }
	    else
	      colors ++;

            if (bit > 1)
	      bit >>= 1;
	    else
	    {
	      bit = 0x80;
	      cptr ++;
	      mptr ++;
	      yptr ++;
	    }
          }
          break;
      case 2 :
          for (x = 0; x < w; x ++)
	  {
	    switch (x & 3)
	    {
	      case 0 :
	          *colors++ = val = (*cptr & 0xc0) >> 6;
		  *pixels++ = 255 - 85 * val;
	          *colors++ = val = (*mptr & 0xc0) >> 6;
		  *pixels++ = 255 - 85 * val;
	          *colors++ = val = (*yptr & 0xc0) >> 6;
		  *pixels++ = 255 - 85 * val;
                  break;
	      case 1 :
	          *colors++ = val = (*cptr & 0x30) >> 4;
		  *pixels++ = 255 - 85 * val;
	          *colors++ = val = (*mptr & 0x30) >> 4;
		  *pixels++ = 255 - 85 * val;
	          *colors++ = val = (*yptr & 0x30) >> 4;
		  *pixels++ = 255 - 85 * val;
                  break;
	      case 2 :
	          *colors++ = val = (*cptr & 0x0c) >> 2;
		  *pixels++ = 255 - 85 * val;
	          *colors++ = val = (*mptr & 0x0c) >> 2;
		  *pixels++ = 255 - 85 * val;
	          *colors++ = val = (*yptr & 0x0c) >> 2;
		  *pixels++ = 255 - 85 * val;
                  break;
	      case 3 :
	          *colors++ = val = *cptr & 0x03;
		  *pixels++ = 255 - 85 * val;
	          *colors++ = val = *mptr & 0x03;
		  *pixels++ = 255 - 85 * val;
	          *colors++ = val = *yptr & 0x03;
		  *pixels++ = 255 - 85 * val;

		  cptr ++;
		  mptr ++;
		  yptr ++;
                  break;
	    }
          }
          break;
      case 4 :
          for (x = 0; x < w; x ++)
	  {
	    switch (x & 1)
	    {
	      case 0 :
	          *colors++ = val = (*cptr & 0xf0) >> 4;
		  *pixels++ = 255 - 17 * val;
	          *colors++ = val = (*mptr & 0xf0) >> 4;
		  *pixels++ = 255 - 17 * val;
	          *colors++ = val = (*yptr & 0xf0) >> 4;
		  *pixels++ = 255 - 17 * val;
                  break;
	      case 1 :
	          *colors++ = val = *cptr & 0x0f;
		  *pixels++ = 255 - 17 * val;
	          *colors++ = val = *mptr & 0x0f;
		  *pixels++ = 255 - 17 * val;
	          *colors++ = val = *yptr & 0x0f;
		  *pixels++ = 255 - 17 * val;

		  cptr ++;
		  mptr ++;
		  yptr ++;
                  break;
	    }
          }
          break;
      case 8 :
          for (x = w; x > 0; x --)
	  {
	    *colors++ = val = *cptr++;
	    *pixels++ = 255 - val;

	    *colors++ = val = *mptr++;
	    *pixels++ = 255 - val;

	    *colors++ = val = *yptr++;
	    *pixels++ = 255 - val;
          }
          break;
      case 16 :
          if (endian_offset)
	  {
            for (x = w; x > 0; x --)
	    {
	      *colors++ = *cptr++;
	      *colors++ = val = *cptr++;
	      *pixels++ = 255 - val;

	      *colors++ = *mptr++;
	      *colors++ = val = *mptr++;
	      *pixels++ = 255 - val;

	      *colors++ = *yptr++;
	      *colors++ = val = *yptr++;
	      *pixels++ = 255 - val;
            }
          }
	  else
	  {
            for (x = w; x > 0; x --)
	    {
	      *colors++ = val = *cptr++;
	      *pixels++ = 255 - val;
	      *colors++ = *cptr++;

	      *colors++ = val = *mptr++;
	      *pixels++ = 255 - val;
	      *colors++ = *mptr++;

	      *colors++ = val = *yptr++;
	      *pixels++ = 255 - val;
	      *colors++ = *yptr++;
            }
          }
	  break;
    }
  }
}


//
// 'convert_cmyk()' - Convert CMYK raster data.
//

static void
convert_cmyk(
    cups_page_header2_t *header,	// I - Raster header */
    uchar               *line,		// I - Raster line
    uchar               *colors,	// O - Original pixels
    uchar               *pixels)	// O - RGB pixels
{
  int	x,				// X position in line
	w,				// Width of line
	val;				// Pixel value
  uchar	*cptr,				// Cyan pointer
	*mptr,				// Magenta pointer
	*yptr,				// Yellow pointer
	*kptr,				// Black pointer
	bit;				// Current bit
  int	r, g, b, k;			// Current RGB color + K


  w = header->cupsWidth;

  if (header->cupsColorOrder == CUPS_ORDER_CHUNKED)
  {
    // Chunky
    switch (header->cupsBitsPerColor)
    {
      case 1 :
          for (x = w; x > 0; x -= 2, pixels += 6)
	  {
	    bit       = *line++;
	    *colors++ = bit >> 4;

            if (bit & 0x10)
	    {
	      pixels[0] = 0;
	      pixels[1] = 0;
	      pixels[2] = 0;
	    }
	    else
	    {
	      if (bit & 0x80)
		pixels[0] = 0;
	      if (bit & 0x40)
		pixels[1] = 0;
	      if (bit & 0x20)
		pixels[2] = 0;
	    }

	    if (x > 1)
	    {
 	      *colors++ = bit & 0x0f;

	      if (bit & 0x01)
	      {
		pixels[3] = 0;
		pixels[4] = 0;
		pixels[5] = 0;
	      }
	      else
	      {
		if (bit & 0x08)
	          pixels[3] = 0;
		if (bit & 0x04)
	          pixels[4] = 0;
		if (bit & 0x02)
	          pixels[5] = 0;
	      }
	    }
          }
          break;
      case 2 :
          for (x = w; x > 0; x --)
	  {
	    *colors++ = bit = *line++;

	    k = 85 * (bit & 0x03);
	    r = 255 - 85 * ((bit & 0xc0) >> 6) - k;
	    g = 255 - 85 * ((bit & 0x30) >> 4) - k;
	    b = 255 - 85 * ((bit & 0x0c) >> 2) - k;

	    if (r <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = r;

	    if (g <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = g;

	    if (b <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = b;
          }
          break;
      case 4 :
          for (x = w; x > 0; x --)
	  {
	    *colors++ = bit = *line++;

	    r = 255 - 17 * ((bit & 0xf0) >> 4);
	    g = 255 - 17 * (bit & 0x0f);

	    *colors++ = bit = *line++;

	    b = 255 - 17 * ((bit & 0xf0) >> 4);
	    k = 17 * (bit & 0x0f);
	    r -= k;
	    g -= k;
	    b -= k;

	    if (r <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = r;

	    if (g <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = g;

	    if (b <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = b;
          }
          break;
      case 8 :
          for (x = w; x > 0; x --)
	  {
	    *colors++ = val = *line++;
	    r         = 255 - val;
	    *colors++ = val = *line++;
	    g         = 255 - val;
	    *colors++ = val = *line++;
	    b         = 255 - val;
	    *colors++ = k = *line++;

	    r -= k;
	    g -= k;
	    b -= k;

	    if (r <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = r;

	    if (g <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = g;

	    if (b <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = b;
          }
          break;
      case 16 :
          for (x = w; x > 0; x --)
	  {
	    if (endian_offset)
	    {
	      *colors++ = *line++;
	      *colors++ = val = *line++;
	      r         = 255 - val;

	      *colors++ = *line++;
	      *colors++ = val = *line++;
	      g         = 255 - val;

	      *colors++ = *line++;
	      *colors++ = val = *line++;
	      b         = 255 - val;

	      *colors++ = *line++;
	      *colors++ = k = *line++;
	    }
	    else
	    {
	      *colors++ = val = *line++;
	      r         = 255 - val;
	      *colors++ = *line++;

	      *colors++ = val = *line++;
	      g         = 255 - val;
	      *colors++ = *line++;

	      *colors++ = val = *line++;
	      b         = 255 - val;
	      *colors++ = *line++;

	      *colors++ = k = *line++;
	      *colors++ = *line++;
	    }

	    r -= k;
	    g -= k;
	    b -= k;

	    if (r <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = r;

	    if (g <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = g;

	    if (b <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = b;
          }
          break;
    }
  }
  else
  {
    // Banded
    int bytespercolor = (header->cupsBitsPerColor * header->cupsWidth + 7) / 8;


    cptr = line;
    mptr = line + bytespercolor;
    yptr = line + 2 * bytespercolor;
    kptr = line + 3 * bytespercolor;

    switch (header->cupsBitsPerColor)
    {
      case 1 :
          for (x = w, bit = 0x80; x > 0; x --, pixels += 3)
	  {
	    if (*cptr & bit)
	      *colors++ = 1;
	    else
	      colors ++;

	    if (*mptr & bit)
	      *colors++ = 1;
	    else
	      colors ++;

	    if (*yptr & bit)
	      *colors++ = 1;
	    else
	      colors ++;

	    if (*kptr & bit)
	    {
	      *colors++ = 1;

	      pixels[0] = 0;
	      pixels[1] = 0;
	      pixels[2] = 0;
	    }
	    else
	    {
	      colors ++;

	      if (*cptr & bit)
		pixels[0] = 0;
	      if (*mptr & bit)
		pixels[1] = 0;
	      if (*yptr & bit)
		pixels[2] = 0;
            }

            if (bit > 1)
	      bit >>= 1;
	    else
	    {
	      bit = 0x80;
	      cptr ++;
	      mptr ++;
	      yptr ++;
	      kptr ++;
	    }
          }
          break;
      case 2 :
          for (x = 0; x < w; x ++)
	  {
	    switch (x & 3)
	    {
	      default :
	      case 0 :
	          val       = (*kptr & 0xc0) >> 6;
		  k         = 85 * val;

	          *colors++ = val = (*cptr & 0xc0) >> 6;
		  r         = 255 - 85 * val - k;

	          *colors++ = val = (*mptr & 0xc0) >> 6;
		  g         = 255 - 85 * val - k;

	          *colors++ = val = (*yptr & 0xc0) >> 6;
		  b         = 255 - 85 * val - k;

	          *colors++ = val;
                  break;
	      case 1 :
	          val       = (*kptr & 0x30) >> 4;
		  k         = 85 * val;

	          *colors++ = val = (*cptr & 0x30) >> 4;
		  r         = 255 - 85 * val - k;

	          *colors++ = val = (*mptr & 0x30) >> 4;
		  g         = 255 - 85 * val - k;

	          *colors++ = val = (*yptr & 0x30) >> 4;
		  b         = 255 - 85 * val - k;

	          *colors++ = val;
                  break;
	      case 2 :
	          val       = (*kptr & 0x0c) >> 2;
		  k         = 85 * val;

	          *colors++ = val = (*cptr & 0x0c) >> 2;
		  r         = 255 - 85 * val - k;

	          *colors++ = val = (*mptr & 0x0c) >> 2;
		  g         = 255 - 85 * val - k;

	          *colors++ = val = (*yptr & 0x0c) >> 2;
		  b         = 255 - 85 * val - k;

	          *colors++ = val;
                  break;
	      case 3 :
	          val       = *kptr & 0x03;
		  k         = 85 * val;

	          *colors++ = val = *cptr & 0x03;
		  r         = 255 - 85 * val - k;

	          *colors++ = val = *mptr & 0x03;
		  g         = 255 - 85 * val - k;

	          *colors++ = val = *yptr & 0x03;
		  b         = 255 - 85 * val - k;

	          *colors++ = val;

		  cptr ++;
		  mptr ++;
		  yptr ++;
		  kptr ++;
                  break;
	    }

	    if (r <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = r;

	    if (g <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = g;

	    if (b <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = b;
          }
          break;
      case 4 :
          for (x = 0; x < w; x ++)
	  {
	    switch (x & 1)
	    {
	      default :
	      case 0 :
	          val       = (*kptr & 0xf0) >> 4;
		  k         = 17 * val;
	          *colors++ = val = (*cptr & 0xf0) >> 4;
		  r         = 255 - 17 * val - k;
	          *colors++ = val = (*mptr & 0xf0) >> 4;
		  g         = 255 - 17 * val - k;
	          *colors++ = val = (*yptr & 0xf0) >> 4;
		  b         = 255 - 17 * val - k;
	          *colors++ = val;
                  break;
	      case 1 :
	          val       = *kptr & 0x0f;
		  k         = 17 * val;
	          *colors++ = val = *cptr & 0x0f;
		  r         = 255 - 17 * val - k;
	          *colors++ = val = *mptr & 0x0f;
		  g         = 255 - 17 * val - k;
	          *colors++ = val = *yptr & 0x0f;
		  b         = 255 - 17 * val - k;
	          *colors++ = val;

		  cptr ++;
		  mptr ++;
		  yptr ++;
		  kptr ++;
                  break;
	    }

	    if (r <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = r;

	    if (g <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = g;

	    if (b <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = b;
          }
          break;
      case 8 :
          for (x = w; x > 0; x --)
	  {
	    k         = *kptr++;
	    *colors++ = val = *cptr++;
	    r         = 255 - val - k;
	    *colors++ = val = *mptr++;
	    g         = 255 - val - k;
	    *colors++ = val = *yptr++;
	    b         = 255 - val - k;
	    *colors++ = k;

	    if (r <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = r;

	    if (g <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = g;

	    if (b <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = b;
          }
          break;
      case 16 :
          for (x = w; x > 0; x --)
	  {
            if (endian_offset)
	    {
	      *colors++ = *cptr++;
	      *colors++ = val = *cptr++;
	      r         = 255 - val;

	      *colors++ = *mptr++;
	      *colors++ = val = *mptr++;
	      g         = 255 - val;

	      *colors++ = *yptr++;
	      *colors++ = val = *yptr++;
	      b         = 255 - val;

	      *colors++ = *kptr++;
	      *colors++ = k = *kptr++;
            }
	    else
            {
	      *colors++ = val = *cptr++;
	      r         = 255 - val;
	      *colors++ = *cptr++;

	      *colors++ = val = *mptr++;
	      g         = 255 - val;
	      *colors++ = *mptr++;

	      *colors++ = val = *yptr++;
	      b         = 255 - val;
	      *colors++ = *yptr++;

	      *colors++ = k = *kptr++;
	      *colors++ = *kptr++;
	    }

            r -= k;
	    g -= k;
	    b -= k;

	    if (r <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = r;

	    if (g <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = g;

	    if (b <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = b;
	  }
          break;
    }
  }
}


//
// 'convert_device()' - Convert Device-N raster data.
//

static void
convert_device(
    cups_page_header2_t *header,	// I - Raster header */
    uchar               *line,		// I - Raster line
    uchar               *colors,	// O - Original pixels
    uchar               *pixels,	// O - RGB pixels
    uchar               device_colors[][3])
    					// I - RGB values for each device color
{
  int	x,				// X position in line
	z,				// Color
	w,				// Width of line
	val;				// Pixel value
  uchar	*cptr,				// Cyan pointer
	*mptr,				// Magenta pointer
	*yptr,				// Yellow pointer
	*kptr,				// Black pointer
	bit;				// Current bit
  int	r, g, b;			// Current RGB color


  w = header->cupsWidth;

  if (header->cupsColorOrder != CUPS_ORDER_CHUNKED)
  {
    fputs("Error: Unsupported color order for Device-N...\n", stderr);
    return;
  }

  if (header->cupsBitsPerColor != 8 && header->cupsBitsPerColor != 16)
  {
    fputs("Error: Unsupported bit depth for Device-N...\n", stderr);
    return;
  }

  switch (header->cupsBitsPerColor)
  {
    case 8 :
	for (x = w; x > 0; x --)
	{
	  r = g = b = 255;
	  for (z = 0; z < header->cupsNumColors; z ++)
	  {
	    *colors++ = val = *line++;

	    r -= val * device_colors[z][0] / 255;
	    g -= val * device_colors[z][1] / 255;
	    b -= val * device_colors[z][2] / 255;
	  }

	  if (r <= 0)
	    *pixels++ = 0;
	  else
	    *pixels++ = r;

	  if (g <= 0)
	    *pixels++ = 0;
	  else
	    *pixels++ = g;

	  if (b <= 0)
	    *pixels++ = 0;
	  else
	    *pixels++ = b;
	}
	break;
    case 16 :
	for (x = w; x > 0; x --)
	{
	  r = g = b = 255;
	  for (z = 0; z < header->cupsNumColors; z ++)
	  {
	    if (endian_offset)
	    {
	      *colors++ = *line++;
	      *colors++ = val = *line++;
	    }
	    else
	    {
	      *colors++ = val = *line++;
	      *colors++ = *line++;
	    }

	    r -= val * device_colors[z][0] / 255;
	    g -= val * device_colors[z][1] / 255;
	    b -= val * device_colors[z][2] / 255;
	  }

	  if (r <= 0)
	    *pixels++ = 0;
	  else
	    *pixels++ = r;

	  if (g <= 0)
	    *pixels++ = 0;
	  else
	    *pixels++ = g;

	  if (b <= 0)
	    *pixels++ = 0;
	  else
	    *pixels++ = b;
	}
	break;
  }
}



//
// 'convert_k()' - Convert black raster data.
//

static void
convert_k(
    cups_page_header2_t *header,	// I - Raster header */
    uchar               *line,		// I - Raster line
    uchar               *colors,	// O - Original pixels
    uchar               *pixels)	// O - Grayscale pixels
{
  int	x,				// X position in line
	w,				// Width of line
	val;				// Pixel value
  uchar	bit,				// Current bit
	byte;				// Current byte


  w = header->cupsWidth;

  switch (header->cupsBitsPerColor)
  {
    case 1 :
        for (x = w, bit = 0x80, byte = *line++; x > 0; x --)
	{
	  if (byte & bit)
	  {
	    *colors++ = 1;
	    *pixels++ = 0;
	  }
	  else
	  {
	    colors ++;
	    pixels ++;
          }

          if (bit > 1)
	    bit >>= 1;
	  else
	  {
	    bit  = 0x80;
	    byte = *line++;
	  }
        }
        break;
    case 2 :
        for (x = w; x > 0; x -= 4)
	{
	  byte = *line++;

	  *colors++ = val = (byte & 0xc0) >> 6;
	  *pixels++ = ~(85 * val);

	  if (x > 1)
	  {
	    *colors++ = val = (byte & 0x30) >> 4;
	    *pixels++ = ~(85 * val);
	  }

	  if (x > 2)
	  {
	    *colors++ = val = (byte & 0x0c) >> 2;
	    *pixels++ = ~(85 * val);
	  }

	  if (x > 3)
	  {
	    *colors++ = val = byte & 0x03;
	    *pixels++ = ~(85 * val);
	  }
        }
        break;
    case 4 :
        for (x = w; x > 0; x -= 2)
	{
	  byte = *line++;

	  *colors++ = val = (byte & 0xf0) >> 4;
	  *pixels++ = ~(17 * val);

	  *colors++ = val = byte & 0x0f;
	  *pixels++ = ~(17 * val);
        }
        break;
    case 8 :
        for (x = w; x > 0; x --)
	{
	  *colors++ = val = *line++;
	  *pixels++ = ~val;
	}
        break;
    case 16 :
	if (endian_offset)
	{
          for (x = w; x > 0; x --)
	  {
	    *colors++ = *line++;
	    *colors++ = val = *line++;
	    *pixels++ = ~val;
          }
	}
	else
	{
          for (x = w; x > 0; x --)
	  {
	    *colors++ = val = *line++;
	    *pixels++ = ~val;
	    *colors++ = *line++;
          }
	}
        break;
  }
}


//
// 'convert_kcmy()' - Convert KCMY or KCMYcm (8-bit) raster data.
//

static void
convert_kcmy(
    cups_page_header2_t *header,	// I - Raster header */
    uchar               *line,		// I - Raster line
    uchar               *colors,	// O - Original pixels
    uchar               *pixels)	// O - RGB pixels
{
  int	x,				// X position in line
	w,				// Width of line
	val;				// Pixel value
  uchar	*cptr,				// Cyan pointer
	*mptr,				// Magenta pointer
	*yptr,				// Yellow pointer
	*kptr,				// Black pointer
	bit;				// Current bit
  int	r, g, b, k;			// Current RGB color + K


  w = header->cupsWidth;

  if (header->cupsColorOrder == CUPS_ORDER_CHUNKED)
  {
    // Chunky
    switch (header->cupsBitsPerColor)
    {
      case 1 :
          for (x = w; x > 0; x -= 2, pixels += 6)
	  {
	    bit       = *line++;
	    *colors++ = bit >> 4;

            if (bit & 0x80)
	    {
	      pixels[0] = 0;
	      pixels[1] = 0;
	      pixels[2] = 0;
	    }
	    else
	    {
	      if (bit & 0x40)
		pixels[0] = 0;
	      if (bit & 0x20)
		pixels[1] = 0;
	      if (bit & 0x10)
		pixels[2] = 0;
	    }

	    if (x > 1)
	    {
	      *colors++ = bit & 0x0f;

	      if (bit & 0x08)
	      {
		pixels[3] = 0;
		pixels[4] = 0;
		pixels[5] = 0;
	      }
	      else
	      {
		if (bit & 0x04)
	          pixels[3] = 0;
		if (bit & 0x02)
	          pixels[4] = 0;
		if (bit & 0x01)
	          pixels[5] = 0;
	      }
	    }
          }
          break;
      case 2 :
          for (x = w; x > 0; x --)
	  {
	    *colors++ = bit = *line++;

	    k = 85 * ((bit & 0xc0) >> 6);
	    r = 255 - 85 * ((bit & 0x30) >> 4) - k;
	    g = 255 - 85 * ((bit & 0x0c) >> 2) - k;
	    b = 255 - 85 * (bit & 0x03) - k;

	    if (r <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = r;

	    if (g <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = g;

	    if (b <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = b;
          }
          break;
      case 4 :
          for (x = w; x > 0; x --)
	  {
	    *colors++ = bit = *line++;

	    k = 17 * ((bit & 0xf0) >> 4);
	    r = 255 - 17 * (bit & 0x0f) - k;

	    *colors++ = bit = *line++;

	    g = 255 - 17 * ((bit & 0xf0) >> 4) - k;
	    b = 255 - 17 * (bit & 0x0f) - k;

	    if (r <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = r;

	    if (g <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = g;

	    if (b <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = b;
          }
          break;
      case 8 :
          for (x = w; x > 0; x --)
	  {
	    *colors++ = *line;
	    k         = *line++;
	    *colors++ = *line;
	    r         = 255 - *line++ - k;
	    *colors++ = *line;
	    g         = 255 - *line++ - k;
	    *colors++ = *line;
	    b         = 255 - *line++ - k;

	    if (r <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = r;

	    if (g <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = g;

	    if (b <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = b;
          }
          break;
      case 16 :
          for (x = w; x > 0; x --)
	  {
	    if (endian_offset)
	    {
	      *colors++ = *line++;
	      *colors++ = k = *line++;

	      *colors++ = *line++;
	      *colors++ = val = *line++;
	      r         = 255 - val - k;

	      *colors++ = *line++;
	      *colors++ = val = *line++;
	      g         = 255 - val - k;

	      *colors++ = *line++;
	      *colors++ = val = *line++;
	      b         = 255 - val - k;
            }
	    else
	    {
	      *colors++ = k = *line++;
	      *colors++ = *line++;

	      *colors++ = val = *line++;
	      r         = 255 - val - k;
	      *colors++ = *line++;

	      *colors++ = val = *line++;
	      g         = 255 - val - k;
	      *colors++ = *line++;

	      *colors++ = val = *line++;
	      b         = 255 - val - k;
	      *colors++ = *line++;
            }

	    if (r <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = r;

	    if (g <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = g;

	    if (b <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = b;
          }
          break;
    }
  }
  else
  {
    // Banded
    int bytespercolor = (header->cupsBitsPerColor * header->cupsWidth + 7) / 8;


    kptr = line;
    cptr = line + bytespercolor;
    mptr = line + 2 * bytespercolor;
    yptr = line + 3 * bytespercolor;

    switch (header->cupsBitsPerColor)
    {
      case 1 :
          for (x = w, bit = 0x80; x > 0; x --, pixels += 3)
	  {
	    if (*kptr & bit)
	    {
	      *colors++ = 1;

	      pixels[0] = 0;
	      pixels[1] = 0;
	      pixels[2] = 0;
	    }
	    else
	    {
	      colors ++;

	      if (*cptr & bit)
		pixels[0] = 0;
	      if (*mptr & bit)
		pixels[1] = 0;
	      if (*yptr & bit)
		pixels[2] = 0;
            }

	    if (*cptr & bit)
	      *colors++ = 1;
	    else
	      colors ++;

	    if (*mptr & bit)
	      *colors++ = 1;
	    else
	      colors ++;

	    if (*yptr & bit)
	      *colors++ = 1;
	    else
	      colors ++;

            if (bit > 1)
	      bit >>= 1;
	    else
	    {
	      bit = 0x80;
	      cptr ++;
	      mptr ++;
	      yptr ++;
	      kptr ++;
	    }
          }
          break;
      case 2 :
          for (x = 0; x < w; x ++)
	  {
	    switch (x & 3)
	    {
	      default :
	      case 0 :
	          *colors++ = val = (*kptr & 0xc0) >> 6;
		  k         = 85 * val;

	          *colors++ = val = (*cptr & 0xc0) >> 6;
		  r         = 255 - 85 * val - k;

	          *colors++ = val = (*mptr & 0xc0) >> 6;
		  g         = 255 - 85 * val - k;

	          *colors++ = val = (*yptr & 0xc0) >> 6;
		  b         = 255 - 85 * val - k;
                  break;
	      case 1 :
	          *colors++ = val = (*kptr & 0x30) >> 4;
		  k         = 85 * val;

	          *colors++ = val = (*cptr & 0x30) >> 4;
		  r         = 255 - 85 * val - k;

	          *colors++ = val = (*mptr & 0x30) >> 4;
		  g         = 255 - 85 * val - k;

	          *colors++ = val = (*yptr & 0x30) >> 4;
		  b         = 255 - 85 * val - k;
                  break;
	      case 2 :
	          *colors++ = val = (*kptr & 0x0c) >> 2;
		  k         = 85 * val;

	          *colors++ = val = (*cptr & 0x0c) >> 2;
		  r         = 255 - 85 * val - k;

	          *colors++ = val = (*mptr & 0x0c) >> 2;
		  g         = 255 - 85 * val - k;

	          *colors++ = val = (*yptr & 0x0c) >> 2;
		  b         = 255 - 85 * val - k;
                  break;
	      case 3 :
	          *colors++ = val = *kptr & 0x03;
		  k         = 85 * val;

	          *colors++ = val = *cptr & 0x03;
		  r         = 255 - 85 * val - k;

	          *colors++ = val = *mptr & 0x03;
		  g         = 255 - 85 * val - k;

	          *colors++ = val = *yptr & 0x03;
		  b         = 255 - 85 * val - k;

		  cptr ++;
		  mptr ++;
		  yptr ++;
		  kptr ++;
                  break;
	    }

	    if (r <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = r;

	    if (g <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = g;

	    if (b <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = b;
          }
          break;
      case 4 :
          for (x = 0; x < w; x ++)
	  {
	    switch (x & 1)
	    {
	      default :
	      case 0 :
	          *colors++ = val = (*kptr & 0xf0) >> 4;
		  k         = 17 * val;
	          *colors++ = val = (*cptr & 0xf0) >> 4;
		  r         = 255 - 17 * val - k;
	          *colors++ = val = (*mptr & 0xf0) >> 4;
		  g         = 255 - 17 * val - k;
	          *colors++ = val = (*yptr & 0xf0) >> 4;
		  b         = 255 - 17 * val - k;
                  break;
	      case 1 :
	          *colors++ = val = *kptr & 0x0f;
		  k         = 17 * val;
	          *colors++ = val = *cptr & 0x0f;
		  r         = 255 - 17 * val - k;
	          *colors++ = val = *mptr & 0x0f;
		  g         = 255 - 17 * val - k;
	          *colors++ = val = *yptr & 0x0f;
		  b         = 255 - 17 * val - k;

		  cptr ++;
		  mptr ++;
		  yptr ++;
		  kptr ++;
                  break;
	    }

	    if (r <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = r;

	    if (g <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = g;

	    if (b <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = b;
          }
          break;
      case 8 :
          for (x = w; x > 0; x --)
	  {
	    *colors++ = k = *kptr++;
	    *colors++ = val = *cptr++;
	    r         = 255 - val - k;
	    *colors++ = val = *mptr++;
	    g         = 255 - val - k;
	    *colors++ = val = *yptr++;
	    b         = 255 - val - k;

	    if (r <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = r;

	    if (g <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = g;

	    if (b <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = b;
          }
          break;
      case 16 :
          for (x = w; x > 0; x --)
	  {
            if (endian_offset)
	    {
	      *colors++ = *kptr++;
	      *colors++ = k = *kptr++;

	      *colors++ = *cptr++;
	      *colors++ = val = *cptr++;
	      r         = 255 - val - k;

	      *colors++ = *mptr++;
	      *colors++ = val = *mptr++;
	      g         = 255 - val - k;

	      *colors++ = *yptr++;
	      *colors++ = val = *yptr++;
	      b         = 255 - val - k;
            }
	    else
            {
	      *colors++ = k = *kptr++;
	      *colors++ = *kptr++;

	      *colors++ = val = *cptr++;
	      r         = 255 - val - k;
	      *colors++ = *cptr++;

	      *colors++ = val = *mptr++;
	      g         = 255 - val - k;
	      *colors++ = *mptr++;

	      *colors++ = val = *yptr++;
	      b         = 255 - val - k;
	      *colors++ = *yptr++;
	    }

	    if (r <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = r;

	    if (g <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = g;

	    if (b <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = b;
	  }
          break;
    }
  }
}


//
// 'convert_kcmycm()' - Convert KCMYcm (1-bit) raster data.
//

static void
convert_kcmycm(
    cups_page_header2_t *header,	// I - Raster header */
    uchar               *line,		// I - Raster line
    uchar               *colors,	// O - Original pixels
    uchar               *pixels)	// O - RGB pixels
{
  int	x,				// X position in line
	w;				// Width of line
  uchar	*cptr,				// Cyan pointer
	*mptr,				// Magenta pointer
	*yptr,				// Yellow pointer
	*kptr,				// Black pointer
	*lcptr,				// Light cyan pointer
	*lmptr,				// Light magenta pointer
	bit;				// Current bit
  int	r, g, b;			// Current RGB color


  w = header->cupsWidth;

  if (header->cupsColorOrder == CUPS_ORDER_CHUNKED)
  {
    // Chunky
    for (x = w; x > 0; x --)
    {
      bit       = *line++;
      *colors++ = bit;

      if (bit & 0x20)
      {
        *pixels++ = 0;
        *pixels++ = 0;
        *pixels++ = 0;
	continue;
      }

      r = g = 255;

      if (bit & 0x10)
	r -= 255;
      if (bit & 0x08)
	g -= 255;
      if (bit & 0x02)
	r -= 127;
      if (bit & 0x01)
	g -= 127;
      if (bit & 0x04)
	b = 0;
      else
        b = 255;

      if (r < 0)
	*pixels++ = 0;
      else
	*pixels++ = r;

      if (g < 0)
	*pixels++ = 0;
      else
	*pixels++ = g;

      // Blue is always in range...
      *pixels++ = b;
    }
  }
  else
  {
    // Banded
    int bytespercolor = header->cupsBytesPerLine / 6;


    kptr  = line;
    cptr  = line + bytespercolor;
    mptr  = line + 2 * bytespercolor;
    yptr  = line + 3 * bytespercolor;
    lcptr = line + 4 * bytespercolor;
    lmptr = line + 5 * bytespercolor;

    for (x = w, bit = 0x80; x > 0; x --)
    {
      if (*kptr & bit)
      {
        *colors++ = 1;
	*pixels++ = 0;
	*pixels++ = 0;
	*pixels++ = 0;
      }
      else
      {
        colors ++;
        r = g = 255;

	if (*cptr & bit)
	  r -= 255;
	if (*mptr & bit)
	  g -= 255;
	if (*lcptr & bit)
	  r -= 127;
	if (*lmptr & bit)
	  g -= 127;
	if (*yptr & bit)
	  b = 0;
	else
	  b = 255;

	if (r < 0)
	  *pixels++ = 0;
	else
	  *pixels++ = r;

	if (g < 0)
	  *pixels++ = 0;
	else
	  *pixels++ = g;

	// Blue is always in range...
	*pixels++ = b;
      }

      if (*cptr & bit)
        *colors++ = 1;
      else
        colors ++;

      if (*mptr & bit)
        *colors++ = 1;
      else
        colors ++;

      if (*yptr & bit)
        *colors++ = 1;
      else
        colors ++;

      if (*lcptr & bit)
        *colors++ = 1;
      else
        colors ++;

      if (*lmptr & bit)
        *colors++ = 1;
      else
        colors ++;

      if (bit > 1)
	bit >>= 1;
      else
      {
	bit = 0x80;
	cptr ++;
	mptr ++;
	yptr ++;
	kptr ++;
	lcptr ++;
	lmptr ++;
      }
    }
  }
}


//
// 'convert_lab()' - Convert CIE Lab or ICCn raster data.
//

static void
convert_lab(
    cups_page_header2_t *header,	// I - Raster header */
    uchar               *line,		// I - Raster line
    uchar               *colors,	// O - Original pixels
    uchar               *pixels)	// O - RGB pixels
{
  int			x,		// X position in line
			w,		// Width of line
			val;		// Pixel value
  unsigned short	*sline;		// 16-bit pixels
  float			lab[3],		// Lab color
			p,		//
			xyz[3],		// XYZ color
			rgb[3];		// RGB color


  w = header->cupsWidth;

  for (x = w, sline = (unsigned short *)line; x > 0; x --)
  {
    // Extract the Lab color value...
    if (header->cupsBitsPerColor == 8)
    {
      // 8-bit color values...
      *colors++ = val = *line++;
      lab[0]    = val / 2.55f;
      *colors++ = val = *line++;
      lab[1]    = val - 128.0f;
      *colors++ = val = *line++;
      lab[2]    = val - 128.0f;
    }
    else
    {
      // 16-bit color values...
      *colors++ = *line++;
      *colors++ = *line++;
      lab[0]    = *sline++ / 655.35f;
      *colors++ = *line++;
      *colors++ = *line++;
      lab[1]    = *sline++ / 256.0f - 128.0f;
      *colors++ = *line++;
      *colors++ = *line++;
      lab[2]    = *sline++ / 256.0f - 128.0f;
    }

    // Convert Lab to XYZ...
    if (lab[0] < 8)
      p = lab[0] / 903.3;
    else
      p = (lab[0] + 16.0f) / 116.0f;

    xyz[0] = D65_X * pow(p + lab[1] * 0.002, 3.0);
    xyz[1] = D65_Y * pow((double)p, 3.0);
    xyz[2] = D65_Z * pow(p - lab[2] * 0.005, 3.0);

    // Convert XYZ to sRGB...
    rgb[0] =  3.240479f * xyz[0] - 1.537150f * xyz[1] - 0.498535f * xyz[2];
    rgb[1] = -0.969256f * xyz[0] + 1.875992f * xyz[1] + 0.041556f * xyz[2];
    rgb[2] =  0.055648f * xyz[0] - 0.204043f * xyz[1] + 1.057311f * xyz[2];

    rgb[0] = rgb[0] <= 0.0 ? 0.0 : 1.055f * pow((double)rgb[0], 0.41666) - 0.055f;
    rgb[1] = rgb[1] <= 0.0 ? 0.0 : 1.055f * pow((double)rgb[1], 0.41666) - 0.055f;
    rgb[2] = rgb[2] <= 0.0 ? 0.0 : 1.055f * pow((double)rgb[2], 0.41666) - 0.055f;

    // Save it...
    if (rgb[0] <= 0.0f)
      *pixels++ = 0;
    else if (rgb[0] < 1.0f)
      *pixels++ = (int)(255.0f * rgb[0] + 0.5);
    else
      *pixels++ = 255;

    if (rgb[1] <= 0.0f)
      *pixels++ = 0;
    else if (rgb[1] < 1.0f)
      *pixels++ = (int)(255.0f * rgb[1] + 0.5);
    else
      *pixels++ = 255;

    if (rgb[2] <= 0.0f)
      *pixels++ = 0;
    else if (rgb[2] < 1.0f)
      *pixels++ = (int)(255.0f * rgb[2] + 0.5);
    else
      *pixels++ = 255;
  }
}


//
// 'convert_rgb()' - Convert RGB raster data.
//

static void
convert_rgb(
    cups_page_header2_t *header,	// I - Raster header */
    uchar               *line,		// I - Raster line
    uchar               *colors,	// O - Original pixels
    uchar               *pixels)	// O - RGB pixels
{
  int	x,				// X position in line
	w,				// Width of line
	val;				// Pixel value
  uchar	*rptr,				// Red pointer
	*gptr,				// Green pointer
	*bptr,				// Blue pointer
	bit;				// Current bit


  w = header->cupsWidth;

  if (header->cupsColorOrder == CUPS_ORDER_CHUNKED)
  {
    // Chunky
    switch (header->cupsBitsPerColor)
    {
      case 1 :
	  memset(pixels, 0, w * 3);

          for (x = w; x > 0; x -= 2, pixels += 6)
	  {
	    bit = *line++;
	    *colors++ = bit >> 4;

	    if (bit & 0x40)
	      pixels[0] = 255;
	    if (bit & 0x20)
	      pixels[1] = 255;
	    if (bit & 0x10)
	      pixels[2] = 255;

	    if (x > 1)
	    {
	      *colors++ = bit & 0x0f;

	      if (bit & 0x04)
	        pixels[3] = 255;
	      if (bit & 0x02)
	        pixels[4] = 255;
	      if (bit & 0x01)
	        pixels[5] = 255;
	    }
          }
          break;
      case 2 :
          for (x = w; x > 0; x --)
	  {
	    *colors++ = bit = *line++;

	    *pixels++ = 85 * ((bit & 0x30) >> 4);
	    *pixels++ = 85 * ((bit & 0x0c) >> 2);
	    *pixels++ = 85 * (bit & 0x03);
          }
          break;
      case 4 :
	  memset(pixels, 0, w * 3);

          for (x = w; x > 0; x --, pixels += 3)
	  {
	    bit = *line++;
	    *colors++ = bit;

	    if (bit & 0x0f)
	      pixels[0] += 17 * (bit & 0x0f);

	    bit = *line++;
	    *colors++ = bit;

	    if (bit & 0xf0)
	      pixels[1] += 17 * ((bit & 0xf0) >> 4);
	    if (bit & 0x0f)
	      pixels[2] += 17 * (bit & 0x0f);
          }
          break;
      case 8 :
	  memcpy(colors, line, w * 3);
          memcpy(pixels, line, w * 3);
          break;
      case 16 :
	  if (endian_offset)
	    *colors++ = *line++;

          for (x = w; x > 0; x --)
	  {
	    *colors++ = *pixels++ = *line++;
	    *colors++ = *line++;

	    *colors++ = *pixels++ = *line++;
	    *colors++ = *line++;

	    *colors++ = *pixels++ = *line++;
	    if (!endian_offset || x > 1)
	      *colors++ = *line++;
          }
          break;
    }
  }
  else
  {
    // Banded
    int bytespercolor = (header->cupsBitsPerColor * header->cupsWidth + 7) / 8;


    rptr = line;
    gptr = line + bytespercolor;
    bptr = line + 2 * bytespercolor;

    switch (header->cupsBitsPerColor)
    {
      case 1 :
	  memset(pixels, 0, w * 3);

          for (x = w, bit = 0x80; x > 0; x --, pixels += 3)
	  {
	    if (*rptr & bit)
	    {
	      *colors++ = 1;
	      pixels[0] = 255;
	    }
	    else
	      colors ++;

	    if (*gptr & bit)
	    {
	      *colors++ = 1;
	      pixels[1] = 255;
	    }
	    else
	      colors ++;

	    if (*bptr & bit)
	    {
	      *colors++ = 1;
	      pixels[2] = 255;
	    }
	    else
	      colors ++;

            if (bit > 1)
	      bit >>= 1;
	    else
	    {
	      bit = 0x80;
	      rptr ++;
	      gptr ++;
	      bptr ++;
	    }
          }
          break;
      case 2 :
          for (x = 0; x < w; x ++)
	  {
	    switch (x & 3)
	    {
	      case 0 :
	          *colors++ = val = (*rptr & 0xc0) >> 6;
		  *pixels++ = 85 * val;
	          *colors++ = val = (*gptr & 0xc0) >> 6;
		  *pixels++ = 85 * val;
	          *colors++ = val = (*bptr & 0xc0) >> 6;
		  *pixels++ = 85 * val;
                  break;
	      case 1 :
	          *colors++ = val = (*rptr & 0x30) >> 4;
		  *pixels++ = 85 * val;
	          *colors++ = val = (*gptr & 0x30) >> 4;
		  *pixels++ = 85 * val;
	          *colors++ = val = (*bptr & 0x30) >> 4;
		  *pixels++ = 85 * val;
                  break;
	      case 2 :
	          *colors++ = val = (*rptr & 0x0c) >> 2;
		  *pixels++ = 85 * val;
	          *colors++ = val = (*gptr & 0x0c) >> 2;
		  *pixels++ = 85 * val;
	          *colors++ = val = (*bptr & 0x0c) >> 2;
		  *pixels++ = 85 * val;
                  break;
	      case 3 :
	          *colors++ = val = *rptr & 0x03;
		  *pixels++ = 85 * val;
	          *colors++ = val = *gptr & 0x03;
		  *pixels++ = 85 * val;
	          *colors++ = val = *bptr & 0x03;
		  *pixels++ = 85 * val;

		  rptr ++;
		  gptr ++;
		  bptr ++;
                  break;
	    }
          }
          break;
      case 4 :
          for (x = 0; x < w; x ++)
	  {
	    switch (x & 1)
	    {
	      case 0 :
	          *colors++ = val = (*rptr & 0xf0) >> 4;
		  *pixels++ = 17 * val;
	          *colors++ = val = (*gptr & 0xf0) >> 4;
		  *pixels++ = 17 * val;
	          *colors++ = val = (*bptr & 0xf0) >> 4;
		  *pixels++ = 17 * val;
                  break;
	      case 1 :
	          *colors++ = val = *rptr & 0x0f;
		  *pixels++ = 17 * val;
	          *colors++ = val = *gptr & 0x0f;
		  *pixels++ = 17 * val;
	          *colors++ = val = *bptr & 0x0f;
		  *pixels++ = 17 * val;

		  rptr ++;
		  gptr ++;
		  bptr ++;
                  break;
	    }
          }
          break;
      case 8 :
          for (x = w; x > 0; x --)
	  {
	    *colors++ = *pixels++ = *rptr++;
	    *colors++ = *pixels++ = *gptr++;
	    *colors++ = *pixels++ = *bptr++;
          }
          break;
      case 16 :
          if (endian_offset)
	  {
            for (x = w; x > 0; x --)
	    {
	      *colors++ = *rptr++;
	      *colors++ = *pixels++ = *rptr++;
	      *colors++ = *gptr++;
	      *colors++ = *pixels++ = *gptr++;
	      *colors++ = *bptr++;
	      *colors++ = *pixels++ = *bptr++;
            }
	  }
	  else
	  {
            for (x = w; x > 0; x --)
	    {
	      *colors++ = *pixels++ = *rptr++;
	      *colors++ = *rptr++;
	      *colors++ = *pixels++ = *gptr++;
	      *colors++ = *gptr++;
	      *colors++ = *pixels++ = *bptr++;
	      *colors++ = *bptr++;
            }
	  }
          break;
    }
  }
}


//
// 'convert_rgba()' - Convert RGBA raster data.
//

static void
convert_rgba(
    cups_page_header2_t *header,	// I - Raster header */
    int                 y,		// I - Raster Y position
    uchar               *line,		// I - Raster line
    uchar               *colors,	// O - Original pixels
    uchar               *pixels)	// O - RGB pixels
{
  int	x,				// X position in line
	w,				// Width of line
	val;				// Pixel value
  uchar	*rptr,				// Red pointer
	*gptr,				// Green pointer
	*bptr,				// Blue pointer
	*aptr,				// Alpha pointer
	bit;				// Current bit
  int	r, g, b, a;			// Current color
  int	bg;				// Background to blend


  w = header->cupsWidth;
  y &= 128;

  if (header->cupsColorOrder == CUPS_ORDER_CHUNKED)
  {
    // Chunky
    switch (header->cupsBitsPerColor)
    {
      case 1 :
	  memset(pixels, 0, w * 3);

          for (x = w; x > 0; x -= 2, pixels += 6)
	  {
	    bit = *line++;
	    *colors++ = bit >> 4;

            if (bit & 0x10)
	    {
	      if (bit & 0x80)
		pixels[0] = 255;
	      if (bit & 0x40)
		pixels[1] = 255;
	      if (bit & 0x20)
		pixels[2] = 255;
            }
	    else if ((x & 128) ^ y)
	    {
	      pixels[0] = 128;
	      pixels[1] = 128;
	      pixels[2] = 128;
	    }
	    else
	    {
	      pixels[0] = 192;
	      pixels[1] = 192;
	      pixels[2] = 192;
	    }

	    if (x > 1)
	    {
	      *colors++ = bit & 0x0f;

              if (bit & 0x01)
	      {
		if (bit & 0x08)
	          pixels[3] = 255;
		if (bit & 0x04)
	          pixels[4] = 255;
		if (bit & 0x02)
	          pixels[5] = 255;
	      }
	      else if (((x - 1) & 128) ^ y)
	      {
		pixels[3] = 128;
		pixels[4] = 128;
		pixels[5] = 128;
	      }
	      else
	      {
		pixels[3] = 192;
		pixels[4] = 192;
		pixels[5] = 192;
	      }
	    }
          }
          break;
      case 2 :
          for (x = w; x > 0; x --)
	  {
	    *colors++ = bit = *line++;

	    r = 85 * ((bit & 0xc0) >> 6);
	    g = 85 * ((bit & 0x30) >> 4);
	    b = 85 * ((bit & 0x0c) >> 2);
	    a = 85 * (bit & 0x03);

	    if (a < 255)
	    {
	      if ((x & 128) ^ y)
	        bg = 128;
	      else
	        bg = 192;

              if (a == 0)
	        r = g = b = bg;
	      else
	      {
        	r = (a * r + (255 - a) * bg) / 255;
        	g = (a * g + (255 - a) * bg) / 255;
        	b = (a * b + (255 - a) * bg) / 255;
	      }
	    }

	    *pixels++ = r;
	    *pixels++ = g;
	    *pixels++ = b;
          }
          break;
      case 4 :
          for (x = w; x > 0; x --)
	  {
	    *colors++ = bit = *line++;

	    r = 17 * ((bit & 0xf0) >> 4);
	    g = 17 * (bit & 0x0f);

	    *colors++ = bit = *line++;

	    b = 17 * ((bit & 0xf0) >> 4);
	    a = 17 * (bit & 0x0f);

	    if (a < 255)
	    {
	      if ((x & 128) ^ y)
	        bg = 128;
	      else
	        bg = 192;

              if (a == 0)
	        r = g = b = bg;
	      else
	      {
        	r = (a * r + (255 - a) * bg) / 255;
        	g = (a * g + (255 - a) * bg) / 255;
        	b = (a * b + (255 - a) * bg) / 255;
	      }
	    }

	    *pixels++ = r;
	    *pixels++ = g;
	    *pixels++ = b;
          }
          break;
      case 8 :
          for (x = w; x > 0; x --)
	  {
	    *colors++ = r = *line++;
	    *colors++ = g = *line++;
	    *colors++ = b = *line++;
	    *colors++ = a = *line++;

	    if (a < 255)
	    {
	      if ((x & 128) ^ y)
	        bg = 128;
	      else
	        bg = 192;

              if (a == 0)
	        r = g = b = bg;
	      else
	      {
        	r = (a * r + (255 - a) * bg) / 255;
        	g = (a * g + (255 - a) * bg) / 255;
        	b = (a * b + (255 - a) * bg) / 255;
	      }
	    }

	    *pixels++ = r;
	    *pixels++ = g;
	    *pixels++ = b;
          }
          break;
      case 16 :
	  if (endian_offset)
	    *colors++ = *line++;

          for (x = w; x > 0; x --)
	  {
	    *colors++ = r = *line++;
	    *colors++ = *line++;
	    *colors++ = g = *line++;
	    *colors++ = *line++;
	    *colors++ = b = *line++;
	    *colors++ = *line++;
	    *colors++ = a = *line++;
	    if (!endian_offset || x > 1)
	      *colors++ = *line++;

	    if (a < 255)
	    {
	      if ((x & 128) ^ y)
	        bg = 128;
	      else
	        bg = 192;

              if (a == 0)
	        r = g = b = bg;
	      else
	      {
        	r = (a * r + (255 - a) * bg) / 255;
        	g = (a * g + (255 - a) * bg) / 255;
        	b = (a * b + (255 - a) * bg) / 255;
	      }
	    }

	    *pixels++ = r;
	    *pixels++ = g;
	    *pixels++ = b;
          }
          break;
    }
  }
  else
  {
    // Banded
    int bytespercolor = (header->cupsBitsPerColor * header->cupsWidth + 7) / 8;


    rptr = line;
    gptr = line + bytespercolor;
    bptr = line + 2 * bytespercolor;
    aptr = line + 3 * bytespercolor;

    switch (header->cupsBitsPerColor)
    {
      case 1 :
	  memset(pixels, 0, w * 3);

          for (x = w, bit = 0x80; x > 0; x --, pixels += 3)
	  {
	    if (*rptr & bit)
	    {
	      *colors++ = 1;
	      pixels[0] = 255;
	    }
	    else
	      colors ++;

	    if (*gptr & bit)
	    {
	      *colors++ = 1;
	      pixels[1] = 255;
	    }
	    else
	      colors ++;

	    if (*bptr & bit)
	    {
	      *colors++ = 1;
	      pixels[2] = 255;
	    }
	    else
	      colors ++;

	    if (*aptr & bit)
	      *colors++ = 1;
	    else
	    {
	      colors ++;

	      if ((x & 128) ^ y)
	      {
		pixels[0] = 128;
		pixels[1] = 128;
		pixels[2] = 128;
	      }
	      else
	      {
		pixels[0] = 192;
		pixels[1] = 192;
		pixels[2] = 192;
	      }
            }

            if (bit > 1)
	      bit >>= 1;
	    else
	    {
	      bit = 0x80;
	      rptr ++;
	      gptr ++;
	      bptr ++;
	      aptr ++;
	    }
          }
          break;
      case 2 :
          for (x = 0; x < w; x ++)
	  {
	    switch (x & 3)
	    {
	      default :
	      case 0 :
	          *colors++ = val = (*rptr & 0xc0) >> 6;
		  r         = 85 * val;

	          *colors++ = val = (*gptr & 0xc0) >> 6;
		  g         = 85 * val;

	          *colors++ = val = (*bptr & 0xc0) >> 6;
		  b         = 85 * val;

	          *colors++ = val = (*aptr & 0xc0) >> 6;
		  a         = 85 * val;
                  break;
	      case 1 :
	          *colors++ = val = (*rptr & 0x30) >> 4;
		  r         = 85 * val;

	          *colors++ = val = (*gptr & 0x30) >> 4;
		  g         = 85 * val;

	          *colors++ = val = (*bptr & 0x30) >> 4;
		  b         = 85 * val;

	          *colors++ = val = (*aptr & 0x30) >> 4;
		  a         = 85 * val;
                  break;
	      case 2 :
	          *colors++ = val = (*rptr & 0x0c) >> 2;
		  r         = 85 * val;

	          *colors++ = val = (*gptr & 0x0c) >> 2;
		  g         = 85 * val;

	          *colors++ = val = (*bptr & 0x0c) >> 2;
		  b         = 85 * val;

	          *colors++ = val = (*aptr & 0x0c) >> 2;
		  a         = 85 * val;
                  break;
	      case 3 :
	          *colors++ = val = *rptr & 0x03;
		  r         = 85 * val;

	          *colors++ = val = *gptr & 0x03;
		  g         = 85 * val;

	          *colors++ = val = *bptr & 0x03;
		  b         = 85 * val;

	          *colors++ = val = *aptr & 0x03;
		  a         = 85 * val;

		  rptr ++;
		  gptr ++;
		  bptr ++;
		  aptr ++;
                  break;
	    }

	    if (a < 255)
	    {
	      if ((x & 128) ^ y)
	        bg = 128;
	      else
	        bg = 192;

              if (a == 0)
	        r = g = b = bg;
	      else
	      {
        	r = (a * r + (255 - a) * bg) / 255;
        	g = (a * g + (255 - a) * bg) / 255;
        	b = (a * b + (255 - a) * bg) / 255;
	      }
	    }

	    *pixels++ = r;
	    *pixels++ = g;
	    *pixels++ = b;
          }
          break;
      case 4 :
          for (x = 0; x < w; x ++)
	  {
	    switch (x & 1)
	    {
	      default :
	      case 0 :
	          *colors++ = val = (*rptr & 0xf0) >> 4;
		  r         = 17 * val;

	          *colors++ = val = (*gptr & 0xf0) >> 4;
		  g         = 17 * val;

	          *colors++ = val = (*bptr & 0xf0) >> 4;
		  b         = 17 * val;

	          *colors++ = val = (*aptr & 0xf0) >> 4;
		  a         = 17 * val;
                  break;
	      case 1 :
	          *colors++ = val = *rptr & 0x0f;
		  r         = 17 * val;

	          *colors++ = val = *gptr & 0x0f;
		  g         = 17 * val;

	          *colors++ = val = *bptr & 0x0f;
		  b         = 17 * val;

	          *colors++ = val = *aptr & 0x0f;
		  a         = 17 * val;

		  rptr ++;
		  gptr ++;
		  bptr ++;
		  aptr ++;
                  break;
	    }

	    if (a < 255)
	    {
	      if ((x & 128) ^ y)
	        bg = 128;
	      else
	        bg = 192;

              if (a == 0)
	        r = g = b = bg;
	      else
	      {
        	r = (a * r + (255 - a) * bg) / 255;
        	g = (a * g + (255 - a) * bg) / 255;
        	b = (a * b + (255 - a) * bg) / 255;
	      }
	    }

	    *pixels++ = r;
	    *pixels++ = g;
	    *pixels++ = b;
          }
          break;
      case 8 :
          for (x = w; x > 0; x --)
	  {
	    *colors++ = r = *rptr++;
	    *colors++ = g = *gptr++;
	    *colors++ = b = *bptr++;
	    *colors++ = a = *aptr++;

	    if (a < 255)
	    {
	      if ((x & 128) ^ y)
	        bg = 128;
	      else
	        bg = 192;

              if (a == 0)
	        r = g = b = bg;
	      else
	      {
        	r = (a * r + (255 - a) * bg) / 255;
        	g = (a * g + (255 - a) * bg) / 255;
        	b = (a * b + (255 - a) * bg) / 255;
	      }
	    }

	    *pixels++ = r;
	    *pixels++ = g;
	    *pixels++ = b;
          }
          break;
      case 16 :
          for (x = w; x > 0; x --)
	  {
            if (endian_offset)
	    {
	      *colors++ = *rptr++;
	      *colors++ = r = *rptr++;
	      *colors++ = *gptr++;
	      *colors++ = g = *gptr++;
	      *colors++ = *bptr++;
	      *colors++ = b = *bptr++;
	      *colors++ = *aptr++;
	      *colors++ = a = *aptr++;
            }
	    else
	    {
	      *colors++ = r = *rptr++;
	      *colors++ = *rptr++;
	      *colors++ = g = *gptr++;
	      *colors++ = *gptr++;
	      *colors++ = b = *bptr++;
	      *colors++ = *bptr++;
	      *colors++ = a = *aptr++;
	      *colors++ = *aptr++;
            }

	    if (a < 255)
	    {
	      if ((x & 128) ^ y)
	        bg = 128;
	      else
	        bg = 192;

              if (a == 0)
	        r = g = b = bg;
	      else
	      {
        	r = (a * r + (255 - a) * bg) / 255;
        	g = (a * g + (255 - a) * bg) / 255;
        	b = (a * b + (255 - a) * bg) / 255;
	      }
	    }

	    *pixels++ = r;
	    *pixels++ = g;
	    *pixels++ = b;
	  }
          break;
    }
  }
}


//
// 'convert_rgbw()' - Convert RGBW raster data.
//

static void
convert_rgbw(
    cups_page_header2_t *header,	// I - Raster header */
    uchar               *line,		// I - Raster line
    uchar               *colors,	// O - Original pixels
    uchar               *pixels)	// O - RGB pixels
{
  int	x,				// X position in line
	w,				// Width of line
	val;				// Pixel value
  uchar	*rptr,				// Red pointer
	*gptr,				// Green pointer
	*bptr,				// Blue pointer
	*wptr,				// White pointer
	bit;				// Current bit
  int	r, g, b, white;			// Current RGBW color


  w = header->cupsWidth;

  if (header->cupsColorOrder == CUPS_ORDER_CHUNKED)
  {
    // Chunky
    switch (header->cupsBitsPerColor)
    {
      case 1 :
	  memset(pixels, 0, w * 3);

          for (x = w; x > 0; x -= 2, pixels += 6)
	  {
	    bit = *line++;
	    *colors++ = bit >> 4;

            if (bit & 0x10)
	    {
	      if (bit & 0x80)
		pixels[0] = 255;
	      if (bit & 0x40)
		pixels[1] = 255;
	      if (bit & 0x20)
		pixels[2] = 255;
            }

	    if (x > 1)
	    {
	      *colors++ = bit & 0x0f;

	      if (bit & 0x01)
	      {
		if (bit & 0x08)
	          pixels[3] = 255;
		if (bit & 0x04)
	          pixels[4] = 255;
		if (bit & 0x02)
	          pixels[5] = 255;
	      }
	    }
          }
          break;
      case 2 :
          for (x = w; x > 0; x --)
	  {
	    *colors++ = bit = *line++;

	    white = 85 * (bit & 0x03) - 255;
            r     = 85 * ((bit & 0xc0) >> 6) + white;
	    g     = 85 * ((bit & 0x30) >> 4) + white;
	    b     = 85 * ((bit & 0x0c) >> 2) + white;

            if (r <= 0)
	      *pixels++ = 0;
	    else if (r < 255)
	      *pixels++ = r;
	    else
	      *pixels++ = 255;

            if (g <= 0)
	      *pixels++ = 0;
	    else if (g < 255)
	      *pixels++ = g;
	    else
	      *pixels++ = 255;

            if (b <= 0)
	      *pixels++ = 0;
	    else if (b < 255)
	      *pixels++ = b;
	    else
	      *pixels++ = 255;
          }
          break;
      case 4 :
          for (x = w; x > 0; x --)
	  {
	    *colors++ = bit = *line++;

	    r = 17 * ((bit & 0xf0) >> 4);
	    g = 17 * (bit & 0x0f);

	    *colors++ = bit = *line++;

	    b     = 17 * ((bit & 0xf0) >> 4);
	    white = 17 * (bit & 0x0f) - 255;

	    r += white;
	    g += white;
	    b += white;

            if (r <= 0)
	      *pixels++ = 0;
	    else if (r < 255)
	      *pixels++ = r;
	    else
	      *pixels++ = 255;

            if (g <= 0)
	      *pixels++ = 0;
	    else if (g < 255)
	      *pixels++ = g;
	    else
	      *pixels++ = 255;

            if (b <= 0)
	      *pixels++ = 0;
	    else if (b < 255)
	      *pixels++ = b;
	    else
	      *pixels++ = 255;
          }
          break;
      case 8 :
          for (x = w; x > 0; x --)
	  {
	    *colors++ = r = *line++;
	    *colors++ = g = *line++;
	    *colors++ = b = *line++;
	    *colors++ = val = *line++;
	    white     = val - 255;

	    r += white;
	    g += white;
	    b += white;

            if (r <= 0)
	      *pixels++ = 0;
	    else if (r < 255)
	      *pixels++ = r;
	    else
	      *pixels++ = 255;

            if (g <= 0)
	      *pixels++ = 0;
	    else if (g < 255)
	      *pixels++ = g;
	    else
	      *pixels++ = 255;

            if (b <= 0)
	      *pixels++ = 0;
	    else if (b < 255)
	      *pixels++ = b;
	    else
	      *pixels++ = 255;
          }
          break;
      case 16 :
	  if (endian_offset)
	    *colors++ = *line++;

          for (x = w; x > 0; x --)
	  {
	    *colors++ = r = *line++;
	    *colors++ = *line++;
	    *colors++ = g = *line++;
	    *colors++ = *line++;
	    *colors++ = b = *line++;
	    *colors++ = *line++;
	    *colors++ = val = *line++;
	    white     = val - 255;
	    if (!endian_offset || x > 1)
	      *colors++ = *line++;

	    r += white;
	    g += white;
	    b += white;

            if (r <= 0)
	      *pixels++ = 0;
	    else if (r < 255)
	      *pixels++ = r;
	    else
	      *pixels++ = 255;

            if (g <= 0)
	      *pixels++ = 0;
	    else if (g < 255)
	      *pixels++ = g;
	    else
	      *pixels++ = 255;

            if (b <= 0)
	      *pixels++ = 0;
	    else if (b < 255)
	      *pixels++ = b;
	    else
	      *pixels++ = 255;
          }
          break;
    }
  }
  else
  {
    // Banded
    int bytespercolor = (header->cupsBitsPerColor * header->cupsWidth + 7) / 8;


    rptr = line;
    gptr = line + bytespercolor;
    bptr = line + 2 * bytespercolor;
    wptr = line + 3 * bytespercolor;

    switch (header->cupsBitsPerColor)
    {
      case 1 :
	  memset(pixels, 0, w * 3);

          for (x = w, bit = 0x80; x > 0; x --, pixels += 3)
	  {
	    if (*rptr & bit)
	      *colors++ = 1;
	    else
	      colors ++;

	    if (*gptr & bit)
	      *colors++ = 1;
	    else
	      colors ++;

	    if (*bptr & bit)
	      *colors++ = 1;
	    else
	      colors ++;

	    if (*wptr & bit)
	    {
	      *colors++ = 1;

	      if (*rptr & bit)
		pixels[0] = 255;
	      if (*gptr & bit)
		pixels[1] = 255;
	      if (*bptr & bit)
		pixels[2] = 255;
            }
	    else
	      colors ++;

            if (bit > 1)
	      bit >>= 1;
	    else
	    {
	      bit = 0x80;
	      rptr ++;
	      gptr ++;
	      bptr ++;
	      wptr ++;
	    }
          }
          break;
      case 2 :
          for (x = 0; x < w; x ++)
	  {
	    switch (x & 3)
	    {
	      default :
	      case 0 :
	          *colors++ = val = (*rptr & 0xc0) >> 6;
		  r         = 85 * val;

		  *colors++ = val = (*gptr & 0xc0) >> 6;
		  g         = 85 * val;

	          *colors++ = val = (*bptr & 0xc0) >> 6;
		  b         = 85 * val;

		  *colors++ = val = (*wptr & 0xc0) >> 6;
		  white     = 85 * val - 255;
                  break;
	      case 1 :
	          *colors++ = val = (*rptr & 0x30) >> 4;
		  r         = 85 * val;

		  *colors++ = val = (*gptr & 0x30) >> 4;
		  g         = 85 * val;

	          *colors++ = val = (*bptr & 0x30) >> 4;
		  b         = 85 * val;

		  *colors++ = val = (*wptr & 0x30) >> 4;
		  white     = 85 * val - 255;
                  break;
	      case 2 :
	          *colors++ = val = (*rptr & 0x0c) >> 2;
		  r         = 85 * val;

		  *colors++ = val = (*gptr & 0x0c) >> 2;
		  g         = 85 * val;

	          *colors++ = val = (*bptr & 0x0c) >> 2;
		  b         = 85 * val;

		  *colors++ = val = (*wptr & 0x0c) >> 2;
		  white     = 85 * val - 255;
                  break;
	      case 3 :
	          *colors++ = val = *rptr & 0x03;
		  r         = 85 * val;

		  *colors++ = val = *gptr & 0x03;
		  g         = 85 * val;

	          *colors++ = val = *bptr & 0x03;
		  b         = 85 * val;

		  *colors++ = val = *wptr & 0x03;
		  white     = 85 * val - 255;

		  rptr ++;
		  gptr ++;
		  bptr ++;
		  wptr ++;
                  break;
	    }

	    r += white;
	    g += white;
	    b += white;

            if (r <= 0)
	      *pixels++ = 0;
	    else if (r < 255)
	      *pixels++ = r;
	    else
	      *pixels++ = 255;

            if (g <= 0)
	      *pixels++ = 0;
	    else if (g < 255)
	      *pixels++ = g;
	    else
	      *pixels++ = 255;

            if (b <= 0)
	      *pixels++ = 0;
	    else if (b < 255)
	      *pixels++ = b;
	    else
	      *pixels++ = 255;
          }
          break;
      case 4 :
          for (x = 0; x < w; x ++)
	  {
	    switch (x & 1)
	    {
	      default :
	      case 0 :
	          *colors++ = val = (*rptr & 0xf0) >> 4;
		  r         = 17 * val;

	          *colors++ = val = (*gptr & 0xf0) >> 4;
		  g         = 17 * val;

	          *colors++ = val = (*bptr & 0xf0) >> 4;
		  b         = 17 * val;

		  *colors++ = val = (*wptr & 0xf0) >> 4;
		  white     = 17 * val - 255;
                  break;
	      case 1 :
	          *colors++ = val = *rptr & 0x0f;
		  r         = 17 * val;

	          *colors++ = val = *gptr & 0x0f;
		  g         = 17 * val;

	          *colors++ = val = *bptr & 0x0f;
		  b         = 17 * val;

		  *colors++ = val = *wptr & 0x0f;
		  white     = 17 * val - 255;

		  rptr ++;
		  gptr ++;
		  bptr ++;
		  wptr ++;
                  break;
	    }

	    r += white;
	    g += white;
	    b += white;

            if (r <= 0)
	      *pixels++ = 0;
	    else if (r < 255)
	      *pixels++ = r;
	    else
	      *pixels++ = 255;

            if (g <= 0)
	      *pixels++ = 0;
	    else if (g < 255)
	      *pixels++ = g;
	    else
	      *pixels++ = 255;

            if (b <= 0)
	      *pixels++ = 0;
	    else if (b < 255)
	      *pixels++ = b;
	    else
	      *pixels++ = 255;
          }
          break;
      case 8 :
          for (x = w; x > 0; x --)
	  {
	    *colors++ = r = *rptr++;
	    *colors++ = g = *gptr++;
	    *colors++ = b = *bptr++;
	    *colors++ = val = *wptr++;
	    white     = val - 255;

            r += white;
            g += white;
            b += white;

            if (r <= 0)
	      *pixels++ = 0;
	    else if (r < 255)
	      *pixels++ = r;
	    else
	      *pixels++ = 255;

            if (g <= 0)
	      *pixels++ = 0;
	    else if (g < 255)
	      *pixels++ = g;
	    else
	      *pixels++ = 255;

            if (b <= 0)
	      *pixels++ = 0;
	    else if (b < 255)
	      *pixels++ = b;
	    else
	      *pixels++ = 255;
          }
          break;
      case 16 :
          for (x = w; x > 0; x --)
	  {
            if (endian_offset)
	    {
	      *colors++ = *rptr++;
	      *colors++ = r = *rptr++;

	      *colors++ = *gptr++;
	      *colors++ = g = *gptr++;

	      *colors++ = *bptr++;
	      *colors++ = b = *bptr++;

	      *colors++ = *wptr++;
	      *colors++ = val = *wptr++;
	      white     = val - 255;
	    }
	    else
	    {
	      *colors++ = r = *rptr++;
	      *colors++ = *rptr++;

	      *colors++ = g = *gptr++;
	      *colors++ = *gptr++;

	      *colors++ = b = *bptr++;
	      *colors++ = *bptr++;

	      *colors++ = val = *wptr++;
	      white     = val - 255;
	      *colors++ = *wptr++;
	    }

            r += white;
            g += white;
            b += white;

            if (r <= 0)
	      *pixels++ = 0;
	    else if (r < 255)
	      *pixels++ = r;
	    else
	      *pixels++ = 255;

            if (g <= 0)
	      *pixels++ = 0;
	    else if (g < 255)
	      *pixels++ = g;
	    else
	      *pixels++ = 255;

            if (b <= 0)
	      *pixels++ = 0;
	    else if (b < 255)
	      *pixels++ = b;
	    else
	      *pixels++ = 255;
          }
          break;
    }
  }
}


//
// 'convert_w()' - Convert grayscale raster data.
//

static void
convert_w(
    cups_page_header2_t *header,	// I - Raster header */
    uchar               *line,		// I - Raster line
    uchar               *colors,	// O - Original pixels
    uchar               *pixels)	// O - Grayscale pixels
{
  int	x,				// X position in line
	w,				// Width of line
	val;				// Pixel value
  uchar	bit,				// Current bit
	byte;				// Current byte


  w = header->cupsWidth;

  switch (header->cupsBitsPerColor)
  {
    case 1 :
        for (x = w, bit = 0x80, byte = *line++; x > 0; x --)
	{
	  if (byte & bit)
	  {
	    *colors++ = 1;
	    pixels ++;
	  }
	  else
	  {
	    colors ++;
	    *pixels++ = 0;
	  }

          if (bit > 1)
	    bit >>= 1;
	  else
	  {
	    bit  = 0x80;
	    byte = *line++;
	  }
        }
        break;
    case 2 :
        for (x = w; x > 0; x -= 4)
	{
	  byte = *line++;

	  *colors++ = val = (byte & 0xc0) >> 6;
	  *pixels++ = 85 * val;

	  if (x > 1)
	  {
	    *colors++ = val = (byte & 0x30) >> 4;
	    *pixels++ = 85 * val;
	  }

	  if (x > 2)
	  {
	    *colors++ = val = (byte & 0x0c) >> 2;
	    *pixels++ = 85 * val;
	  }

	  if (x > 3)
	  {
	    *colors++ = val = byte & 0x03;
	    *pixels++ = 85 * val;
	  }
        }
        break;
    case 4 :
        for (x = w; x > 0; x -= 2)
	{
	  byte = *line++;

          *colors++ = val = (byte & 0xf0) >> 4;
	  *pixels++ = 17 * val;

	  if (x > 1)
	  {
            *colors++ = val = byte & 0x0f;
	    *pixels++ = 17 * val;
	  }
        }
        break;
    case 8 :
        memcpy(colors, line, w);
        memcpy(pixels, line, w);
        break;
    case 16 :
	if (endian_offset)
	{
          for (x = w; x > 0; x --)
	  {
	    *colors++ = *line++;
	    *colors++ = *pixels++ = *line++;
          }
	}
	else
	{
          for (x = w; x > 0; x --)
	  {
	    *colors++ = *pixels++ = *line++;
	    *colors++ = *line++;
          }
	}
        break;
  }
}


//
// 'convert_xyz()' - Convert CIE XYZ raster data.
//

static void
convert_xyz(
    cups_page_header2_t *header,	// I - Raster header */
    uchar               *line,		// I - Raster line
    uchar               *colors,	// O - Original pixels
    uchar               *pixels)	// O - RGB pixels
{
  int			x,		// X position in line
			w,		// Width of line
			val;		// Pixel value
  unsigned short	*sline;		// 16-bit pixels
  float			xyz[3],		// XYZ color
			rgb[3];		// RGB color


  w = header->cupsWidth;

  for (x = w, sline = (unsigned short *)line; x > 0; x --)
  {
    // Extract the Lab color value...
    if (header->cupsBitsPerColor == 8)
    {
      // 8-bit color values...
      *colors++ = val = *line++;
      xyz[0]    = val / 231.8181f;
      *colors++ = val = *line++;
      xyz[1]    = val / 231.8181f;
      *colors++ = val = *line++;
      xyz[2]    = val / 231.8181f;
    }
    else
    {
      // 16-bit color values...
      *colors++ = *line++;
      *colors++ = *line++;
      *colors++ = *line++;
      *colors++ = *line++;
      *colors++ = *line++;
      *colors++ = *line++;

      xyz[0] = *sline++ / 59577.2727f;
      xyz[1] = *sline++ / 59577.2727f;
      xyz[2] = *sline++ / 59577.2727f;
    }

    // Convert XYZ to sRGB...
    rgb[0] =  3.240479f * xyz[0] - 1.537150f * xyz[1] - 0.498535f * xyz[2];
    rgb[1] = -0.969256f * xyz[0] + 1.875992f * xyz[1] + 0.041556f * xyz[2];
    rgb[2] =  0.055648f * xyz[0] - 0.204043f * xyz[1] + 1.057311f * xyz[2];

    rgb[0] = rgb[0] <= 0.0 ? 0.0 : 1.055f * pow((double)rgb[0], 0.41666) - 0.055f;
    rgb[1] = rgb[1] <= 0.0 ? 0.0 : 1.055f * pow((double)rgb[1], 0.41666) - 0.055f;
    rgb[2] = rgb[2] <= 0.0 ? 0.0 : 1.055f * pow((double)rgb[2], 0.41666) - 0.055f;

    // Save it...
    if (rgb[0] <= 0.0f)
      *pixels++ = 0;
    else if (rgb[0] < 1.0f)
      *pixels++ = (int)(255.0f * rgb[0] + 0.5);
    else
      *pixels++ = 255;

    if (rgb[1] <= 0.0f)
      *pixels++ = 0;
    else if (rgb[1] < 1.0f)
      *pixels++ = (int)(255.0f * rgb[1] + 0.5);
    else
      *pixels++ = 255;

    if (rgb[2] <= 0.0f)
      *pixels++ = 0;
    else if (rgb[2] < 1.0f)
      *pixels++ = (int)(255.0f * rgb[2] + 0.5);
    else
      *pixels++ = 255;
  }
}


//
// 'convert_ymc()' - Convert YMC raster data.
//

static void
convert_ymc(
    cups_page_header2_t *header,	// I - Raster header */
    uchar               *line,		// I - Raster line
    uchar               *colors,	// O - Original pixels
    uchar               *pixels)	// O - RGB pixels
{
  int	x,				// X position in line
	w,				// Width of line
	val;				// Pixel value
  uchar	*cptr,				// Cyan pointer
	*mptr,				// Magenta pointer
	*yptr,				// Yellow pointer
	bit;				// Current bit


  w = header->cupsWidth;

  if (header->cupsColorOrder == CUPS_ORDER_CHUNKED)
  {
    // Chunky
    switch (header->cupsBitsPerColor)
    {
      case 1 :
          for (x = w; x > 0; x -= 2, pixels += 6)
	  {
	    bit       = *line++;
	    *colors++ = bit >> 4;

	    if (bit & 0x40)
	      pixels[2] = 0;
	    if (bit & 0x20)
	      pixels[1] = 0;
	    if (bit & 0x10)
	      pixels[0] = 0;

	    if (x > 1)
	    {
	      *colors++ = bit & 0x0f;

	      if (bit & 0x04)
		pixels[5] = 0;
	      if (bit & 0x02)
		pixels[4] = 0;
	      if (bit & 0x01)
		pixels[3] = 0;
	    }
          }
          break;
      case 2 :
          for (x = w; x > 0; x --, pixels += 3)
	  {
	    *colors++ = bit = *line++;

	    pixels[2] = 255 - 85 * ((bit & 0x30) >> 4);
	    pixels[1] = 255 - 85 * ((bit & 0x0c) >> 2);
	    pixels[0] = 255 - 85 * (bit & 0x03);
          }
          break;
      case 4 :
          for (x = w; x > 0; x --, pixels += 3)
	  {
	    *colors++ = bit = *line++;

	    pixels[2] = 255 - 17 * (bit & 0x0f);

	    *colors++ = bit = *line++;

	    pixels[1] = 255 - 17 * ((bit & 0xf0) >> 4);
	    pixels[0] = 255 - 17 * (bit & 0x0f);
          }
          break;
      case 8 :
          for (x = w; x > 0; x --, pixels += 3)
	  {
	    *colors++ = val = *line++;
	    pixels[2] = 255 - val;

	    *colors++ = val = *line++;
	    pixels[1] = 255 - val;

	    *colors++ = val = *line++;
	    pixels[0] = 255 - val;
          }
          break;
      case 16 :
	  if (endian_offset)
	  {
            for (x = w; x > 0; x --, pixels += 3)
	    {
	      *colors++ = *line++;
	      *colors++ = val = *line++;
	      pixels[2] = 255 - val;

	      *colors++ = *line++;
	      *colors++ = val = *line++;
	      pixels[1] = 255 - val;

	      *colors++ = *line++;
	      *colors++ = val = *line++;
	      pixels[0] = 255 - val;
	    }
          }
	  else
	  {
            for (x = w; x > 0; x --, pixels += 3)
	    {
	      *colors++ = val = *line++;
	      pixels[2] = 255 - val;
	      *colors++ = *line++;

	      *colors++ = val = *line++;
	      pixels[1] = 255 - val;
	      *colors++ = *line++;

	      *colors++ = val = *line++;
	      pixels[0] = 255 - val;
	      *colors++ = *line++;
	    }
	  }
          break;
    }
  }
  else
  {
    // Banded
    int bytespercolor = (header->cupsBitsPerColor * header->cupsWidth + 7) / 8;


    yptr = line;
    mptr = line + bytespercolor;
    cptr = line + 2 * bytespercolor;

    switch (header->cupsBitsPerColor)
    {
      case 1 :
          for (x = w, bit = 0x80; x > 0; x --, pixels += 3)
	  {
	    if (*yptr & bit)
	    {
	      *colors++ = 1;
	      pixels[2] = 0;
	    }
	    else
	      colors ++;

	    if (*mptr & bit)
	    {
	      *colors++ = 1;
	      pixels[1] = 0;
	    }
	    else
	      colors ++;

	    if (*cptr & bit)
	    {
	      *colors++ = 1;
	      pixels[0] = 0;
	    }
	    else
	      colors ++;

            if (bit > 1)
	      bit >>= 1;
	    else
	    {
	      bit = 0x80;
	      cptr ++;
	      mptr ++;
	      yptr ++;
	    }
          }
          break;
      case 2 :
          for (x = 0; x < w; x ++, pixels += 3)
	  {
	    switch (x & 3)
	    {
	      case 0 :
	          *colors++ = val = (*yptr & 0xc0) >> 6;
		  pixels[2] = 255 - 85 * val;
	          *colors++ = val = (*mptr & 0xc0) >> 6;
		  pixels[1] = 255 - 85 * val;
	          *colors++ = val = (*cptr & 0xc0) >> 6;
		  pixels[0] = 255 - 85 * val;
                  break;
	      case 1 :
	          *colors++ = val = (*yptr & 0x30) >> 4;
		  pixels[2] = 255 - 85 * val;
	          *colors++ = val = (*mptr & 0x30) >> 4;
		  pixels[1] = 255 - 85 * val;
	          *colors++ = val = (*cptr & 0x30) >> 4;
		  pixels[0] = 255 - 85 * val;
                  break;
	      case 2 :
	          *colors++ = val = (*yptr & 0x0c) >> 2;
		  pixels[2] = 255 - 85 * val;
	          *colors++ = val = (*mptr & 0x0c) >> 2;
		  pixels[1] = 255 - 85 * val;
	          *colors++ = val = (*cptr & 0x0c) >> 2;
		  pixels[0] = 255 - 85 * val;
                  break;
	      case 3 :
	          *colors++ = val = *yptr & 0x03;
		  pixels[2] = 255 - 85 * val;
	          *colors++ = val = *mptr & 0x03;
		  pixels[1] = 255 - 85 * val;
	          *colors++ = val = *cptr & 0x03;
		  pixels[0] = 255 - 85 * val;

		  cptr ++;
		  mptr ++;
		  yptr ++;
                  break;
	    }
          }
          break;
      case 4 :
          for (x = 0; x < w; x ++, pixels += 3)
	  {
	    switch (x & 1)
	    {
	      case 0 :
	          *colors++ = val = (*yptr & 0xf0) >> 4;
		  pixels[2] = 255 - 17 * val;
	          *colors++ = val = (*mptr & 0xf0) >> 4;
		  pixels[1] = 255 - 17 * val;
	          *colors++ = val = (*cptr & 0xf0) >> 4;
		  pixels[0] = 255 - 17 * val;
                  break;
	      case 1 :
	          *colors++ = val = *yptr & 0x0f;
		  pixels[2] = 255 - 17 * val;
	          *colors++ = val = *mptr & 0x0f;
		  pixels[1] = 255 - 17 * val;
	          *colors++ = val = *cptr & 0x0f;
		  pixels[0] = 255 - 17 * val;

		  cptr ++;
		  mptr ++;
		  yptr ++;
                  break;
	    }
          }
          break;
      case 8 :
          for (x = w; x > 0; x --, pixels += 3)
	  {
	    *colors++ = val = *yptr++;
	    pixels[2] = 255 - val;

	    *colors++ = val = *mptr++;
	    pixels[1] = 255 - val;

	    *colors++ = val = *cptr++;
	    pixels[0] = 255 - val;
          }
          break;
      case 16 :
          if (endian_offset)
	  {
            for (x = w; x > 0; x --, pixels += 3)
	    {
	      *colors++ = *yptr++;
	      *colors++ = val = *yptr++;
	      pixels[2] = 255 - val;

	      *colors++ = *mptr++;
	      *colors++ = val = *mptr++;
	      pixels[1] = 255 - val;

	      *colors++ = *cptr++;
	      *colors++ = val = *cptr++;
	      pixels[0] = 255 - val;
            }
          }
	  else
	  {
            for (x = w; x > 0; x --, pixels += 3)
	    {
	      *colors++ = val = *yptr++;
	      pixels[2] = 255 - val;
	      *colors++ = *yptr++;

	      *colors++ = val = *mptr++;
	      pixels[1] = 255 - val;
	      *colors++ = *mptr++;

	      *colors++ = val = *cptr++;
	      pixels[0] = 255 - val;
	      *colors++ = *cptr++;
            }
          }
	  break;
    }
  }
}


//
// 'convert_ymck()' - Convert YMCK, GMCK, or GMCS raster data.
//

static void
convert_ymck(
    cups_page_header2_t *header,	// I - Raster header */
    uchar               *line,		// I - Raster line
    uchar               *colors,	// O - Original pixels
    uchar               *pixels)	// O - RGB pixels
{
  int	x,				// X position in line
	w,				// Width of line
	val;				// Pixel value
  uchar	*cptr,				// Cyan pointer
	*mptr,				// Magenta pointer
	*yptr,				// Yellow pointer
	*kptr,				// Black pointer
	bit;				// Current bit
  int	r, g, b, k;			// Current RGB color + K


  w = header->cupsWidth;

  if (header->cupsColorOrder == CUPS_ORDER_CHUNKED)
  {
    // Chunky
    switch (header->cupsBitsPerColor)
    {
      case 1 :
          for (x = w; x > 0; x -= 2, pixels += 6)
	  {
	    bit       = *line++;
	    *colors++ = bit >> 4;

            if (bit & 0x10)
	    {
	      pixels[0] = 0;
	      pixels[1] = 0;
	      pixels[2] = 0;
	    }
	    else
	    {
	      if (bit & 0x80)
		pixels[2] = 0;
	      if (bit & 0x40)
		pixels[1] = 0;
	      if (bit & 0x20)
		pixels[0] = 0;
	    }

	    if (x > 1)
	    {
	      *colors++ = bit & 0x0f;

	      if (bit & 0x01)
	      {
		pixels[3] = 0;
		pixels[4] = 0;
		pixels[5] = 0;
	      }
	      else
	      {
		if (bit & 0x08)
	          pixels[5] = 0;
		if (bit & 0x04)
	          pixels[4] = 0;
		if (bit & 0x02)
	          pixels[3] = 0;
	      }
	    }
          }
          break;
      case 2 :
          for (x = w; x > 0; x --)
	  {
	    *colors++ = bit = *line++;

	    k = 85 * (bit & 0x03);
	    b = 255 - 85 * ((bit & 0xc0) >> 6) - k;
	    g = 255 - 85 * ((bit & 0x30) >> 4) - k;
	    r = 255 - 85 * ((bit & 0x0c) >> 2) - k;

	    if (r < 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = r;

	    if (g < 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = g;

	    if (b < 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = b;
          }
          break;
      case 4 :
          for (x = w; x > 0; x --)
	  {
	    *colors++ = bit = *line++;

	    b = 255 - 17 * ((bit & 0xf0) >> 4);
	    g = 255 - 17 * (bit & 0x0f);

	    *colors++ = bit = *line++;

	    r = 255 - 17 * ((bit & 0xf0) >> 4);
	    k = 17 * (bit & 0x0f);

	    r -= k;
	    g -= k;
	    b -= k;

	    if (r < 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = r;

	    if (g < 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = g;

	    if (b < 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = b;
          }
          break;
      case 8 :
          for (x = w; x > 0; x --)
	  {
	    *colors++ = val = *line++;
	    b         = 255 - val;

	    *colors++ = val = *line++;
	    g         = 255 - val;

	    *colors++ = val = *line++;
	    r         = 255 - val;

	    *colors++ = k = *line++;

	    r -= k;
	    g -= k;
	    b -= k;

	    if (r < 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = r;

	    if (g < 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = g;

	    if (b < 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = b;
          }
          break;
      case 16 :
          for (x = w; x > 0; x --)
	  {
	    if (endian_offset)
	    {
	      *colors++ = *line++;
	      *colors++ = val = *line++;
	      b         = 255 - val;

	      *colors++ = *line++;
	      *colors++ = val = *line++;
	      g         = 255 - val;

	      *colors++ = *line++;
	      *colors++ = val = *line++;
	      r         = 255 - val;

	      *colors++ = *line++;
	      *colors++ = k = *line++;
	    }
	    else
	    {
	      *colors++ = val = *line++;
	      b         = 255 - val;
	      *colors++ = *line++;

	      *colors++ = val = *line++;
	      g         = 255 - val;
	      *colors++ = *line++;

	      *colors++ = val = *line++;
	      r         = 255 - val;
	      *colors++ = *line++;

	      *colors++ = k = *line++;
	      *colors++ = *line++;
	    }

	    r -= k;
	    g -= k;
	    b -= k;

	    if (r < 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = r;

	    if (g < 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = g;

	    if (b < 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = b;
          }
          break;
    }
  }
  else
  {
    // Banded
    int bytespercolor = (header->cupsBitsPerColor * header->cupsWidth + 7) / 8;


    yptr = line;
    mptr = line + bytespercolor;
    cptr = line + 2 * bytespercolor;
    kptr = line + 3 * bytespercolor;

    switch (header->cupsBitsPerColor)
    {
      case 1 :
          for (x = w, bit = 0x80; x > 0; x --, pixels += 3)
	  {
	    if (*yptr & bit)
	      *colors++ = 1;
	    else
	      colors ++;

	    if (*mptr & bit)
	      *colors++ = 1;
	    else
	      colors ++;

	    if (*cptr & bit)
	      *colors++ = 1;
	    else
	      colors ++;

	    if (*kptr & bit)
	    {
	      *colors++ = 1;

	      pixels[0] = 0;
	      pixels[1] = 0;
	      pixels[2] = 0;
	    }
	    else
	    {
	      colors ++;

	      if (*cptr & bit)
		pixels[0] = 0;
	      if (*mptr & bit)
		pixels[1] = 0;
	      if (*yptr & bit)
		pixels[2] = 0;
            }

            if (bit > 1)
	      bit >>= 1;
	    else
	    {
	      bit = 0x80;
	      cptr ++;
	      mptr ++;
	      yptr ++;
	      kptr ++;
	    }
          }
          break;
      case 2 :
          for (x = 0; x < w; x ++)
	  {
	    switch (x & 3)
	    {
	      default :
	      case 0 :
	          val       = (*kptr & 0xc0) >> 6;
		  k         = 85 * val;

	          *colors++ = val = (*yptr & 0xc0) >> 6;
		  b         = 255 - 85 * val - k;

	          *colors++ = val = (*mptr & 0xc0) >> 6;
		  g         = 255 - 85 * val - k;

	          *colors++ = val = (*cptr & 0xc0) >> 6;
		  r         = 255 - 85 * val - k;

	          *colors++ = val;
                  break;
	      case 1 :
	          val       = (*kptr & 0x30) >> 4;
		  k         = 85 * val;

	          *colors++ = val = (*yptr & 0x30) >> 4;
		  b         = 255 - 85 * val - k;

	          *colors++ = val = (*mptr & 0x30) >> 4;
		  g         = 255 - 85 * val - k;

	          *colors++ = val = (*cptr & 0x30) >> 4;
		  r         = 255 - 85 * val - k;

	          *colors++ = val;
                  break;
	      case 2 :
	          val       = (*kptr & 0x0c) >> 2;
		  k         = 85 * val;

	          *colors++ = val = (*yptr & 0x0c) >> 2;
		  b         = 255 - 85 * val - k;

	          *colors++ = val = (*mptr & 0x0c) >> 2;
		  g         = 255 - 85 * val - k;

	          *colors++ = val = (*cptr & 0x0c) >> 2;
		  r         = 255 - 85 * val - k;

	          *colors++ = val;
                  break;
	      case 3 :
	          val       = *kptr & 0x03;
		  k         = 85 * val;

	          *colors++ = val = *yptr & 0x03;
		  b         = 255 - 85 * val - k;

	          *colors++ = val = *mptr & 0x03;
		  g         = 255 - 85 * val - k;

	          *colors++ = val = *cptr & 0x03;
		  r         = 255 - 85 * val - k;

	          *colors++ = val;

		  cptr ++;
		  mptr ++;
		  yptr ++;
		  kptr ++;
                  break;
	    }

	    if (r <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = r;

	    if (g <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = g;

	    if (b <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = b;
          }
          break;
      case 4 :
          for (x = 0; x < w; x ++)
	  {
	    switch (x & 1)
	    {
	      default :
	      case 0 :
	          val       = (*kptr & 0xf0) >> 4;
		  k         = 17 * val;
	          *colors++ = val = (*yptr & 0xf0) >> 4;
		  b         = 255 - 17 * val - k;
	          *colors++ = val = (*mptr & 0xf0) >> 4;
		  g         = 255 - 17 * val - k;
	          *colors++ = val = (*cptr & 0xf0) >> 4;
		  r         = 255 - 17 * val - k;
	          *colors++ = val;
                  break;
	      case 1 :
	          val       = *kptr & 0x0f;
		  k         = 17 * val;
	          *colors++ = val = *yptr & 0x0f;
		  b         = 255 - 17 * val - k;
	          *colors++ = val = *mptr & 0x0f;
		  g         = 255 - 17 * val - k;
	          *colors++ = val = *cptr & 0x0f;
		  r         = 255 - 17 * val - k;
	          *colors++ = val;

		  cptr ++;
		  mptr ++;
		  yptr ++;
		  kptr ++;
                  break;
	    }

	    if (r <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = r;

	    if (g <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = g;

	    if (b <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = b;
          }
          break;
      case 8 :
          for (x = w; x > 0; x --)
	  {
	    k         = *kptr++;
	    *colors++ = val = *yptr++;
	    b         = 255 - val - k;
	    *colors++ = val = *mptr++;
	    g         = 255 - val - k;
	    *colors++ = val = *cptr++;
	    r         = 255 - val - k;
	    *colors++ = k;

	    if (r <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = r;

	    if (g <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = g;

	    if (b <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = b;
          }
          break;
      case 16 :
          for (x = w; x > 0; x --)
	  {
            if (endian_offset)
	    {
	      *colors++ = *cptr++;
	      *colors++ = val = *cptr++;
	      r         = 255 - val;

	      *colors++ = *mptr++;
	      *colors++ = val = *mptr++;
	      g         = 255 - val;

	      *colors++ = *yptr++;
	      *colors++ = val = *yptr++;
	      b         = 255 - val;

	      *colors++ = *kptr++;
	      *colors++ = k = *kptr++;
            }
	    else
            {
	      *colors++ = val = *cptr++;
	      r         = 255 - val;
	      *colors++ = *cptr++;

	      *colors++ = val = *mptr++;
	      g         = 255 - val;
	      *colors++ = *mptr++;

	      *colors++ = val = *yptr++;
	      b         = 255 - val;
	      *colors++ = *yptr++;

	      *colors++ = k = *kptr++;
	      *colors++ = *kptr++;
	    }

            r -= k;
	    g -= k;
	    b -= k;

	    if (r <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = r;

	    if (g <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = g;

	    if (b <= 0)
	      *pixels++ = 0;
	    else
	      *pixels++ = b;
	  }
          break;
    }
  }
}


/*
 * 'raster_cb()' - Read data from a gzFile.
 */

static ssize_t				/* O - Bytes read or -1 on error */
raster_cb(gzFile        ctx,		/* I - File pointer */
          unsigned char *buffer,	/* I - Buffer */
          size_t        length)		/* I - Bytes to read */
{
  return ((ssize_t)gzread(ctx, buffer, (unsigned)length));
}
