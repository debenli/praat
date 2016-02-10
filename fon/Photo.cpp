/* Photo.cpp
 *
 * Copyright (C) 2013,2014,2015 Paul Boersma
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "Photo.h"
#include "NUM2.h"
#include "Formula.h"
#if defined (_WIN32)
	#include <GraphicsP.h>
#elif defined (macintosh)
	#include "macport_on.h"
	#include <Cocoa/Cocoa.h>
	#include "macport_off.h"
#elif defined (linux)
	#include <cairo/cairo.h>
#endif

#include "oo_DESTROY.h"
#include "Photo_def.h"
#include "oo_COPY.h"
#include "Photo_def.h"
#include "oo_EQUAL.h"
#include "Photo_def.h"
#include "oo_CAN_WRITE_AS_ENCODING.h"
#include "Photo_def.h"
#include "oo_WRITE_TEXT.h"
#include "Photo_def.h"
#include "oo_READ_TEXT.h"
#include "Photo_def.h"
#include "oo_WRITE_BINARY.h"
#include "Photo_def.h"
#include "oo_READ_BINARY.h"
#include "Photo_def.h"
#include "oo_DESCRIPTION.h"
#include "Photo_def.h"

Thing_implement (Photo, SampledXY, 0);

void structPhoto :: v_info () {
	our structDaata :: v_info ();
	MelderInfo_writeLine (U"xmin: ", our xmin);
	MelderInfo_writeLine (U"xmax: ", our xmax);
	MelderInfo_writeLine (U"Number of columns: ", our nx);
	MelderInfo_writeLine (U"dx: ", our dx, U" (-> sampling rate ", 1.0 / our dx, U" )");
	MelderInfo_writeLine (U"x1: ", our x1);
	MelderInfo_writeLine (U"ymin: ", our ymin);
	MelderInfo_writeLine (U"ymax: ", our ymax);
	MelderInfo_writeLine (U"Number of rows: ", our ny);
	MelderInfo_writeLine (U"dy: ", our dy, U" (-> sampling rate ", 1.0 / our dy, U" )");
	MelderInfo_writeLine (U"y1: ", our y1);
}

void Photo_init (Photo me,
	double xmin, double xmax, long nx, double dx, double x1,
	double ymin, double ymax, long ny, double dy, double y1)
{
	SampledXY_init (me, xmin, xmax, nx, dx, x1, ymin, ymax, ny, dy, y1);
	my d_red =          Matrix_create (xmin, xmax, nx, dx, x1, ymin, ymax, ny, dy, y1);
	my d_green =        Matrix_create (xmin, xmax, nx, dx, x1, ymin, ymax, ny, dy, y1);
	my d_blue =         Matrix_create (xmin, xmax, nx, dx, x1, ymin, ymax, ny, dy, y1);
	my d_transparency = Matrix_create (xmin, xmax, nx, dx, x1, ymin, ymax, ny, dy, y1);
}

autoPhoto Photo_create
	(double xmin, double xmax, long nx, double dx, double x1,
	 double ymin, double ymax, long ny, double dy, double y1)
{
	try {
		autoPhoto me = Thing_new (Photo);
		Photo_init (me.peek(), xmin, xmax, nx, dx, x1, ymin, ymax, ny, dy, y1);
		return me;
	} catch (MelderError) {
		Melder_throw (U"Photo object not created.");
	}
}

autoPhoto Photo_createSimple (long numberOfRows, long numberOfColumns) {
	try {
		autoPhoto me = Thing_new (Photo);
		Photo_init (me.peek(), 0.5, numberOfColumns + 0.5, numberOfColumns, 1, 1,
		                       0.5, numberOfRows    + 0.5, numberOfRows,    1, 1);
		return me;
	} catch (MelderError) {
		Melder_throw (U"Photo object not created.");
	}
}

autoPhoto Photo_readFromImageFile (MelderFile file) {
	try {
		#if defined (linux)
			cairo_surface_t *surface = cairo_image_surface_create_from_png (Melder_peek32to8 (file -> path));
			//if (cairo_surface_status)
			//	Melder_throw (U"Error opening PNG file.");
			long width = cairo_image_surface_get_width (surface);
			long height = cairo_image_surface_get_height (surface);
			if (width == 0 || height == 0) {
				cairo_surface_destroy (surface);
				Melder_throw (U"Error reading PNG file.");
			}
			unsigned char *imageData = cairo_image_surface_get_data (surface);
			long bytesPerRow = cairo_image_surface_get_stride (surface);
			cairo_format_t format = cairo_image_surface_get_format (surface);
			autoPhoto me = Photo_createSimple (height, width);
			if (format == CAIRO_FORMAT_ARGB32) {
				for (long irow = 1; irow <= height; irow ++) {
					uint8_t *rowAddress = imageData + bytesPerRow * (height - irow);
					for (long icol = 1; icol <= width; icol ++) {
						my d_blue  -> z [irow] [icol] = (* rowAddress ++) / 255.0;
						my d_green -> z [irow] [icol] = (* rowAddress ++) / 255.0;
						my d_red   -> z [irow] [icol] = (* rowAddress ++) / 255.0;
						my d_transparency -> z [irow] [icol] = 1.0 - (* rowAddress ++) / 255.0;
					}
				}
			} else if (format == CAIRO_FORMAT_RGB24) {
				for (long irow = 1; irow <= height; irow ++) {
					uint8_t *rowAddress = imageData + bytesPerRow * (height - irow);
					for (long icol = 1; icol <= width; icol ++) {
						my d_blue  -> z [irow] [icol] = (* rowAddress ++) / 255.0;
						my d_green -> z [irow] [icol] = (* rowAddress ++) / 255.0;
						my d_red   -> z [irow] [icol] = (* rowAddress ++) / 255.0;
						my d_transparency -> z [irow] [icol] = 0.0; rowAddress ++;
					}
				}
			} else {
				cairo_surface_destroy (surface);
				Melder_throw (U"Unsupported PNG format ", format, U".");
			}
			cairo_surface_destroy (surface);
			return me;
		#elif defined (_WIN32)
			Gdiplus::Bitmap gdiplusBitmap (Melder_peek32toW (file -> path));
			long width = gdiplusBitmap. GetWidth ();
			long height = gdiplusBitmap. GetHeight ();
			if (width == 0 || height == 0)
				Melder_throw (U"Error reading PNG file.");
			autoPhoto me = Photo_createSimple (height, width);
			for (long irow = 1; irow <= height; irow ++) {
				for (long icol = 1; icol <= width; icol ++) {
					Gdiplus::Color gdiplusColour;
					gdiplusBitmap. GetPixel (icol - 1, height - irow, & gdiplusColour);
					my d_red -> z [irow] [icol] = (gdiplusColour. GetRed ()) / 255.0;
					my d_green -> z [irow] [icol] = (gdiplusColour. GetGreen ()) / 255.0;
					my d_blue -> z [irow] [icol] = (gdiplusColour. GetBlue ()) / 255.0;
					my d_transparency -> z [irow] [icol] = 1.0 - (gdiplusColour. GetAlpha ()) / 255.0;
				}
			}
			return me;
		#elif defined (macintosh)
			autoPhoto me;
			char utf8 [500];
			Melder_str32To8bitFileRepresentation_inline (file -> path, utf8);
			CFStringRef path = CFStringCreateWithCString (nullptr, utf8, kCFStringEncodingUTF8);
			CFURLRef url = CFURLCreateWithFileSystemPath (nullptr, path, kCFURLPOSIXPathStyle, false);
			CFRelease (path);
			CGImageSourceRef imageSource = CGImageSourceCreateWithURL (url, nullptr);
			CFRelease (url);
			if (! imageSource)
				Melder_throw (U"Cannot open picture file ", file, U".");
			CGImageRef image = CGImageSourceCreateImageAtIndex (imageSource, 0, nullptr);
			CFRelease (imageSource);
			if (image) {
				long width = CGImageGetWidth (image);
				long height = CGImageGetHeight (image);
				me = Photo_createSimple (height, width);
				long bitsPerPixel = CGImageGetBitsPerPixel (image);
				long bitsPerComponent = CGImageGetBitsPerComponent (image);
				long bytesPerRow = CGImageGetBytesPerRow (image);
				trace (
					bitsPerPixel, U" bits per pixel, ",
					bitsPerComponent, U" bits per component, ",
					bytesPerRow, U" bytes per row"
				);
				/*
				 * Now we probably need to use:
				 * CGColorSpaceRef CGImageGetColorSpace (CGImageRef image);
				 * CGImageAlphaInfo CGImageGetAlphaInfo (CGImageRef image);
				 */
				CGDataProviderRef dataProvider = CGImageGetDataProvider (image);   // not retained, so don't release
				CFDataRef data = CGDataProviderCopyData (dataProvider);
				uint8_t *pixelData = (uint8_t *) CFDataGetBytePtr (data);
				for (long irow = 1; irow <= height; irow ++) {
					uint8_t *rowAddress = pixelData + bytesPerRow * (height - irow);
					for (long icol = 1; icol <= width; icol ++) {
						my d_red   -> z [irow] [icol] = (*rowAddress ++) / 255.0;
						my d_green -> z [irow] [icol] = (*rowAddress ++) / 255.0;
						my d_blue  -> z [irow] [icol] = (*rowAddress ++) / 255.0;
						my d_transparency -> z [irow] [icol] = 1.0 - (*rowAddress ++) / 255.0;
					}
				}
				CFRelease (data);
				CGImageRelease (image);
			}
			return me;
		#else
			return autoPhoto();
		#endif
	} catch (MelderError) {
		Melder_throw (U"Picture file ", file, U" not opened as Photo.");
	}
}

#if defined (macintosh)
	#include <time.h>
	#include "macport_on.h"
	static void _mac_releaseDataCallback (void * /* info */, const void *data, size_t /* size */) {
		Melder_free (data);
	}
#endif

#ifdef linux
	static void _lin_saveAsImageFile (Photo me, MelderFile file, const char32 *which) {
		cairo_format_t format = CAIRO_FORMAT_ARGB32;
		long bytesPerRow = cairo_format_stride_for_width (format, my nx);   // likely to be my nx * 4
		long numberOfRows = my ny;
		uint8 *imageData = Melder_malloc_f (uint8, bytesPerRow * numberOfRows);
		for (long irow = 1; irow <= my ny; irow ++) {
			uint8 *rowAddress = imageData + bytesPerRow * (my ny - irow);
			for (long icol = 1; icol <= my nx; icol ++) {
				* rowAddress ++ = round (my d_blue         -> z [irow] [icol] * 255.0);
				* rowAddress ++ = round (my d_green        -> z [irow] [icol] * 255.0);
				* rowAddress ++ = round (my d_red          -> z [irow] [icol] * 255.0);
				* rowAddress ++ = 255 - round (my d_transparency -> z [irow] [icol] * 255.0);
			}
		}
		cairo_surface_t *surface = cairo_image_surface_create_for_data (imageData,
			format, my nx, my ny, bytesPerRow);
		cairo_surface_write_to_png (surface, Melder_peek32to8 (file -> path));
		cairo_surface_destroy (surface);
	}
#endif

#ifdef _WIN32
	static void _win_saveAsImageFile (Photo me, MelderFile file, const char32 *mimeType) {
		Gdiplus::Bitmap gdiplusBitmap (my nx, my ny, PixelFormat32bppARGB);
		for (long irow = 1; irow <= my ny; irow ++) {
			for (long icol = 1; icol <= my nx; icol ++) {
				Gdiplus::Color gdiplusColour (
					255 - round (my d_transparency -> z [irow] [icol] * 255.0),
					round (my d_red   -> z [irow] [icol] * 255.0),
					round (my d_green -> z [irow] [icol] * 255.0),
					round (my d_blue  -> z [irow] [icol] * 255.0));
				gdiplusBitmap. SetPixel (icol - 1, my ny - irow, gdiplusColour);
			}
		}
		/*
		 * The 'mimeType' parameter specifies a "class encoder". Look it up.
		 */
		UINT numberOfImageEncoders, sizeOfImageEncoderArray;
		Gdiplus::GetImageEncodersSize (& numberOfImageEncoders, & sizeOfImageEncoderArray);
		if (sizeOfImageEncoderArray == 0)
			Melder_throw (U"Cannot find image encoders.");
		Gdiplus::ImageCodecInfo *imageEncoderInfos = Melder_malloc (Gdiplus::ImageCodecInfo, sizeOfImageEncoderArray);
		Gdiplus::GetImageEncoders (numberOfImageEncoders, sizeOfImageEncoderArray, imageEncoderInfos);
		for (int iencoder = 0; iencoder < numberOfImageEncoders; iencoder ++) {
			trace (U"Supported MIME type: ", Melder_peekWto32 (imageEncoderInfos [iencoder]. MimeType));
			if (str32equ (Melder_peekWto32 (imageEncoderInfos [iencoder]. MimeType), mimeType)) {
				Gdiplus::EncoderParameters *p = nullptr;
				Gdiplus::EncoderParameters encoderParameters;
				if (str32equ (mimeType, U"image/jpeg")) {
					encoderParameters. Count = 1;
					GUID guid = { 0x1D5BE4B5, 0xFA4A, 0x452D, { 0x9C, 0xDD, 0x5D, 0xB3, 0x51, 0x05, 0xE7, 0xEB }};  // EncoderQuality
					encoderParameters. Parameter [0]. Guid = guid;
					encoderParameters. Parameter [0]. Type = Gdiplus::EncoderParameterValueTypeLong;
					encoderParameters. Parameter [0]. NumberOfValues = 1;
					ULONG quality = 100;
					encoderParameters. Parameter [0]. Value = & quality;
					p = & encoderParameters;
				}
				gdiplusBitmap. Save (Melder_peek32toW (file -> path), & imageEncoderInfos [iencoder]. Clsid, p);
				Melder_free (imageEncoderInfos);
				return;
			}
		}
		Melder_throw (U"Unknown MIME type ", mimeType, U".");
	}
#endif

#ifdef macintosh
	static void _mac_saveAsImageFile (Photo me, MelderFile file, const void *which) {
		long bytesPerRow = my nx * 4;
		long numberOfRows = my ny;
		unsigned char *imageData = Melder_malloc_f (unsigned char, bytesPerRow * numberOfRows);
		for (long irow = 1; irow <= my ny; irow ++) {
			uint8_t *rowAddress = imageData + bytesPerRow * (my ny - irow);
			for (long icol = 1; icol <= my nx; icol ++) {
				* rowAddress ++ = (uint8) lround (my d_red          -> z [irow] [icol] * 255.0);   // BUG: should be tested for speed
				* rowAddress ++ = (uint8) lround (my d_green        -> z [irow] [icol] * 255.0);
				* rowAddress ++ = (uint8) lround (my d_blue         -> z [irow] [icol] * 255.0);
				* rowAddress ++ = 255 - (uint8) lround (my d_transparency -> z [irow] [icol] * 255.0);
			}
		}
		static CGColorSpaceRef colourSpace = nullptr;
		if (! colourSpace) {
			colourSpace = CGColorSpaceCreateWithName (kCGColorSpaceGenericRGB);   // used to be kCGColorSpaceUserRGB
			Melder_assert (colourSpace);
		}
		CGDataProviderRef dataProvider = CGDataProviderCreateWithData (nullptr,
			imageData,
			bytesPerRow * numberOfRows,
			_mac_releaseDataCallback   // needed?
		);
		Melder_assert (dataProvider);
		CGImageRef image = CGImageCreate (my nx, numberOfRows,
			8, 32, bytesPerRow, colourSpace, kCGImageAlphaNone, dataProvider, nullptr, false, kCGRenderingIntentDefault);
		CGDataProviderRelease (dataProvider);
		Melder_assert (image);
		NSString *path = (NSString *) Melder_peek32toCfstring (Melder_fileToPath (file));
		CFURLRef url = (CFURLRef) [NSURL   fileURLWithPath: path   isDirectory: NO];
		CGImageDestinationRef destination = CGImageDestinationCreateWithURL (url, (CFStringRef) which, 1, nullptr);
		CGImageDestinationAddImage (destination, image, nil);

		if (! CGImageDestinationFinalize (destination)) {
			//Melder_throw;
		}

		CFRelease (destination);
		CGColorSpaceRelease (colourSpace);
		CGImageRelease (image);
	}
#endif

void Photo_saveAsPNG (Photo me, MelderFile file) {
	#if defined (_WIN32)
		_win_saveAsImageFile (me, file, U"image/png");
	#elif defined (macintosh)
		_mac_saveAsImageFile (me, file, kUTTypePNG);
	#elif defined (linux)
		_lin_saveAsImageFile (me, file, U"image/png");
	#endif
}

void Photo_saveAsTIFF (Photo me, MelderFile file) {
	#if defined (_WIN32)
		_win_saveAsImageFile (me, file, U"image/tiff");
	#elif defined (macintosh)
		_mac_saveAsImageFile (me, file, kUTTypeTIFF);
	#else
		(void) me;
		(void) file;
	#endif
}

void Photo_saveAsGIF (Photo me, MelderFile file) {
	#if defined (_WIN32)
		_win_saveAsImageFile (me, file, U"image/gif");
	#elif defined (macintosh)
		_mac_saveAsImageFile (me, file, kUTTypeGIF);
	#else
		(void) me;
		(void) file;
	#endif
}

void Photo_saveAsWindowsBitmapFile (Photo me, MelderFile file) {
	#if defined (_WIN32)
		_win_saveAsImageFile (me, file, U"image/bmp");
	#elif defined (macintosh)
		_mac_saveAsImageFile (me, file, kUTTypeBMP);
	#else
		(void) me;
		(void) file;
	#endif
}

void Photo_saveAsJPEG (Photo me, MelderFile file) {
	#if defined (_WIN32)
		_win_saveAsImageFile (me, file, U"image/jpeg");
	#elif defined (macintosh)
		_mac_saveAsImageFile (me, file, kUTTypeJPEG);
	#else
		(void) me;
		(void) file;
	#endif
}

void Photo_saveAsJPEG2000 (Photo me, MelderFile file) {
	#if defined (_WIN32)
		_win_saveAsImageFile (me, file, U"image/jpeg2000");
	#elif defined (macintosh)
		_mac_saveAsImageFile (me, file, kUTTypeJPEG2000);
	#else
		(void) me;
		(void) file;
	#endif
}

void Photo_saveAsAppleIconFile (Photo me, MelderFile file) {
	#if defined (_WIN32)
		_win_saveAsImageFile (me, file, U"image/ICNS");
	#elif defined (macintosh)
		_mac_saveAsImageFile (me, file, kUTTypeAppleICNS);
	#else
		(void) me;
		(void) file;
	#endif
}

void Photo_saveAsWindowsIconFile (Photo me, MelderFile file) {
	#if defined (_WIN32)
		_win_saveAsImageFile (me, file, U"image/icon");
	#elif defined (macintosh)
		_mac_saveAsImageFile (me, file, kUTTypeICO);
	#else
		(void) me;
		(void) file;
	#endif
}

void Photo_replaceRed (Photo me, Matrix red) {
	my d_red = Data_copy (red);
}

void Photo_replaceGreen (Photo me, Matrix green) {
	my d_green = Data_copy (green);
}

void Photo_replaceBlue (Photo me, Matrix blue) {
	my d_blue = Data_copy (blue);
}

void Photo_replaceTransparency (Photo me, Matrix transparency) {
	my d_transparency = Data_copy (transparency);
}

static void _Photo_cellArrayOrImage (Photo me, Graphics g, double xmin, double xmax, double ymin, double ymax, bool interpolate) {
	if (xmax <= xmin) { xmin = my xmin; xmax = my xmax; }
	if (ymax <= ymin) { ymin = my ymin; ymax = my ymax; }
	long ixmin, ixmax, iymin, iymax;
	Sampled_getWindowSamples    (me, xmin - 0.49999 * my dx, xmax + 0.49999 * my dx, & ixmin, & ixmax);
	SampledXY_getWindowSamplesY (me, ymin - 0.49999 * my dy, ymax + 0.49999 * my dy, & iymin, & iymax);
	if (ixmin > ixmax || iymin > iymax) {
		Melder_fatal (U"ixmin ", ixmin, U" ixmax ", ixmax, U" iymin ", iymin, U" iymax ", iymax);
		return;
	}
	Graphics_setInner (g);
	Graphics_setWindow (g, xmin, xmax, ymin, ymax);
	autoNUMmatrix <double_rgbt> z (iymin, iymax, ixmin, ixmax);
	for (long iy = iymin; iy <= iymax; iy ++) {
		for (long ix = ixmin; ix <= ixmax; ix ++) {
			z [iy] [ix]. red          = my d_red          -> z [iy] [ix];
			z [iy] [ix]. green        = my d_green        -> z [iy] [ix];
			z [iy] [ix]. blue         = my d_blue         -> z [iy] [ix];
			z [iy] [ix]. transparency = my d_transparency -> z [iy] [ix];
		}
	}
	if (interpolate)
		Graphics_image_colour (g, z.peek(),
			ixmin, ixmax, Sampled_indexToX   (me, ixmin - 0.5), Sampled_indexToX   (me, ixmax + 0.5),
			iymin, iymax, SampledXY_indexToY (me, iymin - 0.5), SampledXY_indexToY (me, iymax + 0.5), 0.0, 1.0);
	else
		Graphics_cellArray_colour (g, z.peek(),
			ixmin, ixmax, Sampled_indexToX   (me, ixmin - 0.5), Sampled_indexToX   (me, ixmax + 0.5),
			iymin, iymax, SampledXY_indexToY (me, iymin - 0.5), SampledXY_indexToY (me, iymax + 0.5), 0.0, 1.0);
	//Graphics_rectangle (g, xmin, xmax, ymin, ymax);
	Graphics_unsetInner (g);
}

void Photo_paintImage (Photo me, Graphics g, double xmin, double xmax, double ymin, double ymax) {
	_Photo_cellArrayOrImage (me, g, xmin, xmax, ymin, ymax, true);
}

void Photo_paintCells (Photo me, Graphics g, double xmin, double xmax, double ymin, double ymax) {
	_Photo_cellArrayOrImage (me, g, xmin, xmax, ymin, ymax, false);
}

/* End of file Photo.cpp */
