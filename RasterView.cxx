//
// CUPS raster file viewer application window code.
//
// Copyright 2002-2018 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#define __OPENTRANSPORTPROVIDERS__	// For macOS...
#define FL_INTERNAL
#include "RasterView.h"
#include <FL/Fl.H>
#include <FL/Fl_Color_Chooser.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/x.H>


//
// Constants...
//

#ifdef __APPLE__
#  define MENU_OFFSET	0
#else
#  define MENU_OFFSET	25
#endif // __APPLE__
#define ATTRS_WIDTH	310
#define DEVICEN_HEIGHT	30

#define HELP_HTML \
"<HTML>\n" \
"<HEAD>\n" \
"<TITLE>RasterView Help</TITLE>\n" \
"</HEAD>\n" \
"<BODY>\n" \
"<H1>RasterView Help</H1>\n" \
"<H2>The Basics</H2>\n" \
"<P>RasterView starts in <var>zoom in</var> mode (<CODE>Z</CODE>, " \
"which allows you to click on the page or drag a zoom box on the " \
"page to zoom in. You can also use <var>pan</var> mode " \
"(<CODE>P</CODE>) to drag/pan the page in the window, <var>zoom " \
"out</var> mode (<CODE>SHIFT + Z</CODE>) to click on the page to " \
"zoom out, or <var>color viewing</var> mode (<CODE>C</CODE>) to " \
"click or drag the mouse and view the raw colors on the page.</P>\n" \
"<H2>Keyboard Shortcuts</H2>\n" \
"<UL>\n" \
"<LI><CODE>0</CODE>: Zoom to fit</LI>\n" \
"<LI><CODE>1</CODE>: Zoom 100%</LI>\n" \
"<LI><CODE>2</CODE>: Zoom 200%</LI>\n" \
"<LI><CODE>3</CODE>: Zoom 300%</LI>\n" \
"<LI><CODE>4</CODE>: Zoom 400%</LI>\n" \
"<LI><CODE>-</CODE>: Zoom out</LI>\n" \
"<LI><CODE>=</CODE>: Zoom in</LI>\n" \
"<LI><CODE>C</CODE>: Click or drag mouse to view colors</LI>\n" \
"<LI><CODE>P</CODE>: Drag mouse to pan</LI>\n" \
"<LI><CODE>Z</CODE>: Click or drag mouse to zoom in</LI>\n" \
"<LI><CODE>SHIFT + Z</CODE>: Click to zoom out</LI>\n" \
"<LI><CODE>CTRL/CMD + A</CODE>: Show/hide the page attributes</LI>\n" \
"<LI><CODE>CTRL/CMD + O</CODE>: Open a raster file</LI>\n" \
"<LI><CODE>CTRL/CMD + Q</CODE>: Quit RasterView</LI>\n" \
"<LI><CODE>CTRL/CMD + R</CODE>: Reload the raster file</LI>\n" \
"</UL>\n" \
"</BODY>\n" \
"</HTML>\n"


//
// Class globals...
//

RasterView	*RasterView::first_ = 0;
Fl_Help_Dialog	*RasterView::help_ = 0;


#ifdef __APPLE__
//
// 'RasterView::apple_open_cb()' - Open a file.
//

void
RasterView::apple_open_cb(const char *f)// I - File to open
{
#  ifdef DEBUG
  fprintf(stderr, "Opening \"%s\" via Apple event.\n", f);
#  endif // DEBUG

  open_file(f);
}
#endif // __APPLE__


//
// 'RasterView::attrs_cb()' - Toggle attributes.
//

void
RasterView::attrs_cb(Fl_Widget *widget)	// I - Button
{
  RasterView	*view;			// I - Window


  view = (RasterView *)(widget->window());

  if (view->attributes_->visible())
  {
    view->attrs_button_->label("Show Attributes @>");
    view->attributes_->hide();
    view->resize(view->x(), view->y(), view->w() - ATTRS_WIDTH, view->h());
  }
  else
  {
    view->attrs_button_->label("Hide Attributes @<");
    view->attributes_->show();
    view->resize(view->x(), view->y(), view->w() + ATTRS_WIDTH, view->h());
  }
}


//
// 'RasterView::close_cb()' - Close this window.
//

void
RasterView::close_cb(Fl_Widget  *widget)// I - Menu or window
{
  RasterView	*view;			// I - Window


  if (widget->window())
    view = (RasterView *)(widget->window());
  else
    view = (RasterView *)widget;

  if (view->loading_)
    return;

  view->hide();
  view->display_->close_file();
  view->set_filename(NULL);
}


//
// 'RasterView::color_cb()' - Show the color at the current position.
//

void
RasterView::color_cb(
    RasterDisplay *display)		// O - Display widget
{
  RasterView		*view;		// I - Window
  int			i;		// Looping var
  uchar			*cpixel,	// Current pixel value
			*ccolor;	// Current pixel color
  char			*ptr;		// Pointer into label
  cups_page_header2_t	*header;	// Page header


  ccolor = display->get_color(display->mouse_x(), display->mouse_y());
  cpixel = display->get_pixel(display->mouse_x(), display->mouse_y());
  view   = (RasterView *)(display->window());
  header = display->header();

  if (!ccolor || !cpixel)
    strcpy(view->pixel_, "        -/=/0/1/2/3/4 to zoom");
  else
  {
    strcpy(view->pixel_, "       ");

    for (i = 0, ptr = view->pixel_ + 7; i < display->bytes_per_pixel(); i ++)
    {
      snprintf(ptr, sizeof(view->pixel_) - (ptr - view->pixel_), " %d",
               *cpixel++);
      ptr += strlen(ptr);
    }

    strcpy(ptr, " :");
    ptr += 2;

    int banding = (header->cupsBitsPerPixel + 7) / 8;

    for (i = 0; i < display->bytes_per_color(); i ++)
    {
      if ((i % banding) == 0)
        *ptr++ = ' ';

      snprintf(ptr, sizeof(view->pixel_) - (ptr - view->pixel_), "%02X",
               ccolor[i]);
      ptr += 2;
    }

    if (header->cupsColorSpace == CUPS_CSPACE_CIEXYZ)
    {
      float	xyz[3];			/* XYZ value */


      if (header->cupsBitsPerColor == 16)
      {
        unsigned short	*c16 = (unsigned short *)ccolor;


        xyz[0] = c16[0] / 59577.2727f;
        xyz[1] = c16[1] / 59577.2727f;
        xyz[2] = c16[2] / 59577.2727f;
      }
      else
      {
        xyz[0] = ccolor[0] / 231.8181f;
        xyz[1] = ccolor[1] / 231.8181f;
        xyz[2] = ccolor[2] / 231.8181f;
      }

      snprintf(ptr, sizeof(view->pixel_) - (ptr - view->pixel_),
               " (%.3f %.3f %.3f)", xyz[0], xyz[1], xyz[2]);
    }
    else if (header->cupsColorSpace == CUPS_CSPACE_CIELab ||
             header->cupsColorSpace >= CUPS_CSPACE_ICC1)
    {
      float	lab[3];			/* Lab value */


      if (header->cupsBitsPerColor == 16)
      {
        unsigned short	*c16 = (unsigned short *)ccolor;


        lab[0] = c16[0] / 655.35f;
        lab[1] = c16[1] / 256.0f - 128.0f;
        lab[2] = c16[2] / 256.0f - 128.0f;
      }
      else
      {
        lab[0] = ccolor[0] / 2.55f;
        lab[1] = ccolor[1] - 128.0f;
        lab[2] = ccolor[2] - 128.0f;
      }

      snprintf(ptr, sizeof(view->pixel_) - (ptr - view->pixel_),
               " (%.3f %.3f %.3f)", lab[0], lab[1], lab[2]);
    }
  }

  view->buttons_->label(view->pixel_);
}


//
// 'RasterView::device_cb()' - Handle device color changes.
//

void
RasterView::device_cb(
    Fl_Widget *widget)			// I - Widget
{
  RasterView		*view;		// I - Window
  int			i;		// Looping var


  view = (RasterView *)(widget->window());
  for (i = 0; i < 15; i ++)
    if (widget == view->colors_[i])
    {
      uchar r, g, b;

      Fl::get_color(view->display_->device_color(i), r, g, b);

      if (fl_color_chooser(widget->label(), r, g, b))
      {
        view->display_->device_color(i, fl_rgb_color(r, g, b));
        reopen_cb(widget);
      }
      break;
    }
}


//
// 'RasterView::handle()' - Handle global shortcuts...
//

int					// O - 1 if handled, 0 otherwise
RasterView::handle(int event)		// I - Event
{
#ifdef __APPLE__
  if (event == FL_SHORTCUT && Fl::event_key() == 'w' &&
      Fl::event_state(FL_COMMAND))
  {
    close_cb(this);
    return (1);
  }
#endif // __APPLE__

  return (Fl_Double_Window::handle(event));
}


//
// 'RasterView::help_cb()' - Show the on-line help.
//

void
RasterView::help_cb()
{
  if (!help_)
  {
    help_ = new Fl_Help_Dialog();
    help_->value(HELP_HTML);
  }

  help_->show();
}


//
// 'RasterView::init()' - Initialize the window.
//

void
RasterView::init()
{
  static Fl_Menu_Item	items[] =	// Menu items
  {
    {"&File", 0, 0, 0, FL_SUBMENU },
      {"&Open...", FL_COMMAND + 'o', (Fl_Callback *)open_cb },
      {"&Re-open", FL_COMMAND + 'r', (Fl_Callback *)reopen_cb, },
      {"&Close", FL_COMMAND + 'w', (Fl_Callback *)close_cb },
#ifndef __APPLE__
      {"&Quit", FL_COMMAND + 'q', (Fl_Callback *)quit_cb },
#endif // !__APPLE__
      {0},
    {"&Help", 0, 0, 0, FL_SUBMENU },
      {"&About RasterView...", 0, (Fl_Callback *)help_cb },
      {0},
    {0}
  };
  static const char * const color_labels[] =
  {
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
    "9",
    "10",
    "11",
    "12",
    "13",
    "14",
    "15"
  };


  menubar_ = new Fl_Sys_Menu_Bar(0, 0, w(), 25);
  menubar_->menu(items);

  display_ = new RasterDisplay(0, MENU_OFFSET, w(), h() - MENU_OFFSET - 25);
  display_->callback((Fl_Callback *)color_cb);

  buttons_ = new Fl_Group(0, h() - 25, w(), 25, "        -/=/0/1/2/3/4 to zoom");
  buttons_->align((Fl_Align)(FL_ALIGN_LEFT | FL_ALIGN_INSIDE));
    next_button_ = new Fl_Button(0, h() - 25, 25, 25, "@>");
    next_button_->callback((Fl_Callback *)next_cb);
    next_button_->deactivate();
    next_button_->shortcut(' ');

    attrs_button_ = new Fl_Button(25, h() - 25, w() - 25, 25, "Show Attributes @>");
    attrs_button_->align((Fl_Align)(FL_ALIGN_INSIDE | FL_ALIGN_RIGHT));
    attrs_button_->box(FL_NO_BOX);
    attrs_button_->callback((Fl_Callback *)attrs_cb);
    attrs_button_->shortcut(FL_COMMAND + 'a');
  buttons_->resizable(attrs_button_);
  buttons_->end();

  attributes_ = new Fl_Group(w(), 0, ATTRS_WIDTH, h());
    header_ = new Fl_Text_Display(w(), 0, ATTRS_WIDTH, h() - DEVICEN_HEIGHT);
    header_buffer_ = new Fl_Text_Buffer(65536);
    header_->buffer(header_buffer_);
    header_->textfont(FL_COURIER);
    header_->textsize(12);
    header_->box(FL_DOWN_BOX);

    for (int i = 0; i < 15; i ++)
    {
      colors_[i] = new Fl_Button(w() + 5 + 20 * i, h() - DEVICEN_HEIGHT + 5, 20, 20, color_labels[i]);
      colors_[i]->callback(device_cb);
    }
  attributes_->resizable(header_);
  attributes_->end();
  attributes_->hide();

  end();
  resizable(display_);
  callback((Fl_Callback *)close_cb, this);

  // Initialize the titlebar...
  filename_ = NULL;
  title_    = NULL;

  set_filename(NULL);

  // Add this window to the list of windows...
#ifdef __APPLE__
  if (!first_)
    fl_open_callback(apple_open_cb);
#endif // __APPLE__

  loading_ = 0;
  next_    = first_;
  first_   = this;
}


//
// 'RasterView::load_attrs()' - Load attributes into the display.
//

void
RasterView::load_attrs()
{
  int			i;		// Looping var
  char			s[1024];	// Line buffer
  cups_page_header2_t	*header;	// Header data
  static const char * const cspaces[] =	// Colorspace strings
  {
    "CUPS_CSPACE_W",
    "CUPS_CSPACE_RGB",
    "CUPS_CSPACE_RGBA",
    "CUPS_CSPACE_K",
    "CUPS_CSPACE_CMY",
    "CUPS_CSPACE_YMC",
    "CUPS_CSPACE_CMYK",
    "CUPS_CSPACE_YMCK",
    "CUPS_CSPACE_KCMY",
    "CUPS_CSPACE_KCMYcm",
    "CUPS_CSPACE_GMCK",
    "CUPS_CSPACE_GMCS",
    "CUPS_CSPACE_WHITE",
    "CUPS_CSPACE_GOLD",
    "CUPS_CSPACE_SILVER",
    "CUPS_CSPACE_CIEXYZ",
    "CUPS_CSPACE_CIELab",
    "CUPS_CSPACE_RGBW",
    "CUPS_CSPACE_SW",
    "CUPS_CSPACE_SRGB",
    "CUPS_CSPACE_ADOBERGB",
    "UNKNOWN21",
    "UNKNOWN22",
    "UNKNOWN23",
    "UNKNOWN24",
    "UNKNOWN25",
    "UNKNOWN26",
    "UNKNOWN27",
    "UNKNOWN28",
    "UNKNOWN29",
    "UNKNOWN30",
    "UNKNOWN31",
    "CUPS_CSPACE_ICC1",
    "CUPS_CSPACE_ICC2",
    "CUPS_CSPACE_ICC3",
    "CUPS_CSPACE_ICC4",
    "CUPS_CSPACE_ICC5",
    "CUPS_CSPACE_ICC6",
    "CUPS_CSPACE_ICC7",
    "CUPS_CSPACE_ICC8",
    "CUPS_CSPACE_ICC9",
    "CUPS_CSPACE_ICCA",
    "CUPS_CSPACE_ICCB",
    "CUPS_CSPACE_ICCC",
    "CUPS_CSPACE_ICCD",
    "CUPS_CSPACE_ICCE",
    "CUPS_CSPACE_ICCF",
    "UNKNOWN47",
    "CUPS_CSPACE_DEVICE1",
    "CUPS_CSPACE_DEVICE2",
    "CUPS_CSPACE_DEVICE3",
    "CUPS_CSPACE_DEVICE4",
    "CUPS_CSPACE_DEVICE5",
    "CUPS_CSPACE_DEVICE6",
    "CUPS_CSPACE_DEVICE7",
    "CUPS_CSPACE_DEVICE8",
    "CUPS_CSPACE_DEVICE9",
    "CUPS_CSPACE_DEVICEA",
    "CUPS_CSPACE_DEVICEB",
    "CUPS_CSPACE_DEVICEC",
    "CUPS_CSPACE_DEVICED",
    "CUPS_CSPACE_DEVICEE",
    "CUPS_CSPACE_DEVICEF"
  };


  header = display_->header();

  if (!strcmp(header->MediaClass, "PwgRaster"))
  {
    header_buffer_->text("PWG Raster Page Attributes:\n\n");

    snprintf(s, sizeof(s), "MediaColor = \"%s\"\n", header->MediaColor);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "MediaType = \"%s\"\n", header->MediaType);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "PrintContentOptimize = \"%s\"\n", header->OutputType);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "CutMedia = %d\n", header->CutMedia);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "Duplex = %d\n", header->Duplex);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "HWResolution = [ %d %d ]\n",
	     header->HWResolution[0], header->HWResolution[1]);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "InsertSheet = %d\n", header->InsertSheet);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "Jog = %d\n", header->Jog);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "LeadingEdge = %d\n", header->LeadingEdge);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "MediaPosition = %d\n", header->MediaPosition);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "MediaWeightMetric = %d\n", header->MediaWeight);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "NumCopies = %d\n", header->NumCopies);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "Orientation = %d\n", header->Orientation);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "PageSize = [ %d %d ]\n", header->PageSize[0],
	     header->PageSize[1]);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "Tumble = %d\n", header->Tumble);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "Width = %d\n", header->cupsWidth);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "Height = %d\n", header->cupsHeight);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "BitsPerColor = %d\n", header->cupsBitsPerColor);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "BitsPerPixel = %d\n", header->cupsBitsPerPixel);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "BytesPerLine = %d\n", header->cupsBytesPerLine);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "ColorOrder = %s\n",
	     header->cupsColorOrder == CUPS_ORDER_CHUNKED ? "CUPS_ORDER_CHUNKED" :
		 header->cupsColorOrder == CUPS_ORDER_BANDED ? "CUPS_ORDER_BANDED" :
		 header->cupsColorOrder == CUPS_ORDER_PLANAR ? "CUPS_ORDER_PLANAR" :
		 "UNKNOWN");
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "ColorSpace = %s\n",
	     header->cupsColorSpace < (int)(sizeof(cspaces) / sizeof(cspaces[0])) ?
		 cspaces[header->cupsColorSpace] : "UNKNOWN");
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "NumColors = %d\n", header->cupsNumColors);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "TotalPageCount = %u\n", header->cupsInteger[0]);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "CrossFeedTransform = %d\n", header->cupsInteger[1]);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "FeedTransform = %d\n", header->cupsInteger[2]);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "ImageBoxLeft = %u\n", header->cupsInteger[3]);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "ImageBoxTop = %u\n", header->cupsInteger[4]);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "ImageBoxRight = %u\n", header->cupsInteger[5]);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "ImageBoxBottom = %u\n", header->cupsInteger[6]);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "AlternatePrimary = %06x (%u, %u, %u)\n", header->cupsInteger[7], (header->cupsInteger[7] >> 16) & 255, (header->cupsInteger[7] >> 8) & 255, header->cupsInteger[7] & 255);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "PrintQuality = %u\n", header->cupsInteger[8]);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "VendorIdentifier = %u\n", header->cupsInteger[14]);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "VendorLength = %u\n", header->cupsInteger[15]);
    header_buffer_->append(s);

    unsigned char	*data = (unsigned char *)header->cupsReal;
    unsigned		dataidx, datalen = header->cupsInteger[15];

    header_buffer_->append("VendorData =");

    for (dataidx = 0; dataidx < datalen; dataidx ++)
    {
      if ((dataidx & 7) == 0)
	header_buffer_->append("\n   ");

      snprintf(s, sizeof(s), " %02X", *data++);
      header_buffer_->append(s);
    }

    header_buffer_->append("\n");

    snprintf(s, sizeof(s), "RenderingIntent = \"%s\"\n",
	     header->cupsRenderingIntent);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "PageSizeName = \"%s\"\n",
	     header->cupsPageSizeName);
    header_buffer_->append(s);
  }
  else
  {
    header_buffer_->text("CUPS Raster Page Attributes:\n\n");

    snprintf(s, sizeof(s), "MediaClass = \"%s\"\n", header->MediaClass);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "MediaColor = \"%s\"\n", header->MediaColor);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "MediaType = \"%s\"\n", header->MediaType);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "OutputType = \"%s\"\n", header->OutputType);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "AdvanceDistance = %d\n", header->AdvanceDistance);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "AdvanceMedia = %d\n", header->AdvanceMedia);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "Collate = %d\n", header->Collate);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "CutMedia = %d\n", header->CutMedia);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "Duplex = %d\n", header->Duplex);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "HWResolution = [ %d %d ]\n",
	     header->HWResolution[0], header->HWResolution[1]);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "ImagingBoundingBox = [ %d %d %d %d ]\n",
	     header->ImagingBoundingBox[0],
	     header->ImagingBoundingBox[1],
	     header->ImagingBoundingBox[2],
	     header->ImagingBoundingBox[3]);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "InsertSheet = %d\n", header->InsertSheet);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "Jog = %d\n", header->Jog);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "LeadingEdge = %d\n", header->LeadingEdge);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "Margins = [ %d %d ]\n",
	     header->Margins[0], header->Margins[1]);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "ManualFeed = %d\n", header->ManualFeed);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "MediaPosition = %d\n", header->MediaPosition);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "MediaWeight = %d\n", header->MediaWeight);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "MirrorPrint = %d\n", header->MirrorPrint);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "NegativePrint = %d\n", header->NegativePrint);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "NumCopies = %d\n", header->NumCopies);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "Orientation = %d\n", header->Orientation);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "OutputFaceUp = %d\n", header->OutputFaceUp);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "PageSize = [ %d %d ]\n", header->PageSize[0],
	     header->PageSize[1]);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "Separations = %d\n", header->Separations);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "TraySwitch = %d\n", header->TraySwitch);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "Tumble = %d\n", header->Tumble);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "cupsWidth = %d\n", header->cupsWidth);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "cupsHeight = %d\n", header->cupsHeight);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "cupsMediaType = %d\n", header->cupsMediaType);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "cupsBitsPerColor = %d\n", header->cupsBitsPerColor);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "cupsBitsPerPixel = %d\n", header->cupsBitsPerPixel);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "cupsBytesPerLine = %d\n", header->cupsBytesPerLine);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "cupsColorOrder = %s\n",
	     header->cupsColorOrder == CUPS_ORDER_CHUNKED ? "CUPS_ORDER_CHUNKED" :
		 header->cupsColorOrder == CUPS_ORDER_BANDED ? "CUPS_ORDER_BANDED" :
		 header->cupsColorOrder == CUPS_ORDER_PLANAR ? "CUPS_ORDER_PLANAR" :
		 "UNKNOWN");
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "cupsColorSpace = %s\n",
	     header->cupsColorSpace < (int)(sizeof(cspaces) / sizeof(cspaces[0])) ?
		 cspaces[header->cupsColorSpace] : "UNKNOWN");
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "cupsCompression = %d\n", header->cupsCompression);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "cupsRowCount = %d\n", header->cupsRowCount);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "cupsRowFeed = %d\n", header->cupsRowFeed);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "cupsRowStep = %d\n", header->cupsRowStep);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "cupsNumColors = %d\n", header->cupsNumColors);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "cupsBorderlessScalingFactor = %f\n",
	     header->cupsBorderlessScalingFactor);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "cupsPageSize = [ %f %f ]\n",
	     header->cupsPageSize[0], header->cupsPageSize[1]);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "cupsImagingBBox = [ %f %f %f %f ]\n",
	     header->cupsImagingBBox[0], header->cupsImagingBBox[1],
	     header->cupsImagingBBox[2], header->cupsImagingBBox[3]);
    header_buffer_->append(s);

    for (i = 0; i < 16; i ++)
    {
      snprintf(s, sizeof(s), "cupsInteger%d = %d\n", i + 1,
	       header->cupsInteger[i]);
      header_buffer_->append(s);
    }

    for (i = 0; i < 16; i ++)
    {
      snprintf(s, sizeof(s), "cupsReal%d = %f\n", i + 1, header->cupsReal[i]);
      header_buffer_->append(s);
    }

    for (i = 0; i < 16; i ++)
    {
      snprintf(s, sizeof(s), "cupsString%d = \"%s\"\n", i + 1,
	       header->cupsString[i]);
      header_buffer_->append(s);
    }

    snprintf(s, sizeof(s), "cupsMarkerType = \"%s\"\n",
	     header->cupsMarkerType);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "cupsRenderingIntent = \"%s\"\n",
	     header->cupsRenderingIntent);
    header_buffer_->append(s);

    snprintf(s, sizeof(s), "cupsPageSizeName = \"%s\"\n",
	     header->cupsPageSizeName);
    header_buffer_->append(s);
  }

 /*
  * Set device colors for the given color space...
  */

  for (i = 0; i < header->cupsNumColors; i ++)
  {
    Fl_Color c = display_->device_color(i);

    colors_[i]->show();
    colors_[i]->color(c);
    colors_[i]->labelcolor(fl_contrast(FL_BLACK, c));
    colors_[i]->redraw();
  }
  for (; i < 15; i ++)
    colors_[i]->hide();

  if (display_->is_subtractive() && header->cupsBitsPerColor >= 8)
  {
    for (i = 0; i < 15; i ++)
      colors_[i]->activate();
  }
  else
  {
    for (i = 0; i < 15; i ++)
      colors_[i]->deactivate();
  }
  header_->redraw();
}


//
// 'RasterView::next_cb()' - Show the next page.
//

void
RasterView::next_cb(Fl_Widget *widget)	// I - Next button
{
  RasterView	*view;			// I - Window


  view = (RasterView *)(widget->window());

  if (view->loading_)
    return;

  view->loading_ = 1;
    view->display_->load_page();
    view->load_attrs();
  view->loading_ = 0;

  if (view->display_->eof())
    view->next_button_->deactivate();
  else
    view->next_button_->activate();
}


//
// 'RasterView::open_file()' - Open a raster file in a new window.
//

RasterView *				// O - New window
RasterView::open_file(const char *f)	// I - File to open
{
  RasterView	*view;			// New window
  char		*argv[1];		// Argument


  for (view = first_; view; view = view->next_)
    if (view->filename_ && !strcmp(f, view->filename_))
      break;

  if (!view)
  {
    for (view = first_; view; view = view->next_)
      if (!view->filename_)
	break;
  }

  if (!view)
    view = new RasterView(600, 800);

  view->set_filename(f);

  argv[0] = (char *)"rasterview";
  view->show(1, argv);

  view->reopen_cb(view);

  return (view);
}


//
// 'RasterView::open_cb()' - Open a new file.
//

void
RasterView::open_cb()
{
  Fl_Native_File_Chooser fc;


  fc.title("Open?");
  fc.type(Fl_Native_File_Chooser::BROWSE_FILE);
  fc.filter("Raster Files\t*.{apple,pwg,ras}{,.gz}\n");

  if (!fc.show())
    open_file(fc.filename());
}


//
// 'RasterView::quit_cb()' - Quit the application.
//

void
RasterView::quit_cb()
{
  exit(0);
}


//
// 'RasterView::reopen_cb()' - Re-open the current file.
//

void
RasterView::reopen_cb(Fl_Widget *widget)// I - Menu or window
{
  RasterView	*view;			// I - Window


  if (widget->window())
    view = (RasterView *)(widget->window());
  else
    view = (RasterView *)widget;

  if (view->loading_)
    return;

  view->loading_ = 1;
    view->header_buffer_->text("Loading...");
    view->display_->open_file(view->filename_);
    view->load_attrs();
  view->loading_ = 0;

  if (view->display_->eof())
    view->next_button_->deactivate();
  else
    view->next_button_->activate();
}


//
// 'RasterView::resize()' - Resize the window.
//

void
RasterView::resize(int X,		// I - New X position
                   int Y,		// I - New Y position
		   int W,		// I - New width
		   int H)		// I - New height
{
  int	base_W;				// Base width


  Fl_Double_Window::resize(X, Y, W, H);

  if (attributes_->visible())
  {
    base_W = W - ATTRS_WIDTH;
    attributes_->resize(base_W, 0, ATTRS_WIDTH, H);
  }
  else
    base_W = W;

  menubar_->resize(0, 0, base_W, 25);
  display_->resize(0, MENU_OFFSET, base_W, H - MENU_OFFSET - 25);
  buttons_->resize(0, H - 25, base_W, 25);
}


//
// 'RasterView::set_filename()' - Set the filename and window title.
//

void
RasterView::set_filename(const char *f)	// I - New filename
{
  char		s[1024];		// String
  const char	*base;			// Basename


  if (filename_)
    free(filename_);

  if (f)
    filename_ = strdup(f);
  else
    filename_ = NULL;

  if (title_)
    free(title_);

  if (f)
  {
    if ((base = strrchr(f, '/')) != NULL)
      base ++;
    else
      base = f;

    snprintf(s, sizeof(s), "%s - " VERSION, base);
    title_ = strdup(s);
  }
  else
    title_ = strdup(VERSION);

  label(title_);
}


//
// 'RasterView::RasterView()' - Create a new window.
//

RasterView::RasterView(int X, int Y, int W, int H, const char *L)
  : Fl_Double_Window(X, Y, W, H, L)
{
  init();
}


//
// 'RasterView::RasterView()' - Create a new window.
//

RasterView::RasterView(int W, int H, const char *L)
  : Fl_Double_Window(Fl::x() + (Fl::w() - W) / 2,
                     Fl::y() + (Fl::h() - H) / 2,
                     W, H, L)
{
  init();
}


//
// 'RasterView::~RasterView()' - Destroy a window.
//

RasterView::~RasterView()
{
  RasterView	*current, *prev;


  for (prev = NULL, current = first_;
       current;
       prev = current, current = current->next_)
    if (current == this)
      break;

  if (current)
  {
    if (prev)
      prev->next_ = current->next_;
    else
      first_ = current->next_;
  }
}
