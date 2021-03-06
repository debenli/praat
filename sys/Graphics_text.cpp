/* Graphics_text.cpp
 *
 * Copyright (C) 1992-2011,2012,2013,2014,2015,2016,2017 Paul Boersma, 2013 Tom Naughton, 2017 David Weenink
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This code is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this work. If not, see <http://www.gnu.org/licenses/>.
 */

#include <ctype.h>
#include "UnicodeData.h"
#include "GraphicsP.h"
#include "longchar.h"
#include "Printer.h"

extern const char * ipaSerifRegularPS [];

/*
 * When computing the width of a text by adding the widths of the separate characters,
 * we will make a correction for systems that make slanted characters overlap the character box to their right.
 * The effect is especially strong on Mac (older versions).
 * The slant correction is taken relative to the font size.
 */
#define POSTSCRIPT_SLANT_CORRECTION  0.1
#define SLANT_CORRECTION  POSTSCRIPT_SLANT_CORRECTION

#define HAS_FI_AND_FL_LIGATURES  ( my postScript == true )

#if cairo
	#if USE_PANGO
		PangoFontMap *thePangoFontMap;
		PangoContext *thePangoContext;
	#endif
	static bool hasTimes, hasHelvetica, hasCourier, hasSymbol, hasPalatino, hasDoulos, hasCharis, hasIpaSerif;
#elif gdi
	#define win_MAXIMUM_FONT_SIZE  500
	static HFONT fonts [1 + kGraphics_resolution_MAX] [1 + kGraphics_font_JAPANESE] [1+win_MAXIMUM_FONT_SIZE] [1 + Graphics_BOLD_ITALIC];
	static int win_size2isize (int size) { return size > win_MAXIMUM_FONT_SIZE ? win_MAXIMUM_FONT_SIZE : size; }
	static int win_isize2size (int isize) { return isize; }
#elif quartz
	static bool hasTimes, hasHelvetica, hasCourier, hasSymbol, hasPalatino, hasDoulos, hasCharis, hasIpaSerif;
	#define mac_MAXIMUM_FONT_SIZE  500
	static CTFontRef theScreenFonts [1 + kGraphics_font_DINGBATS] [1+mac_MAXIMUM_FONT_SIZE] [1 + Graphics_BOLD_ITALIC];
	static RGBColor theWhiteColour = { 0xFFFF, 0xFFFF, 0xFFFF }, theBlueColour = { 0, 0, 0xFFFF };
#endif

#if gdi
	#ifdef __CYGWIN__
		#define FONT_TYPE_TYPE  unsigned int
	#else
		#define FONT_TYPE_TYPE  unsigned long int
	#endif
	static bool charisAvailable = false, doulosAvailable = false;
	static int CALLBACK fontFuncEx_charis (const LOGFONTW *oldLogFont, const TEXTMETRICW *oldTextMetric, FONT_TYPE_TYPE fontType, LPARAM lparam) {
		const LPENUMLOGFONTW logFont = (LPENUMLOGFONTW) oldLogFont; (void) oldTextMetric; (void) fontType; (void) lparam;
		charisAvailable = true;
		return 1;
	}
	static int CALLBACK fontFuncEx_doulos (const LOGFONTW *oldLogFont, const TEXTMETRICW *oldTextMetric, FONT_TYPE_TYPE fontType, LPARAM lparam) {
		const LPENUMLOGFONTW logFont = (LPENUMLOGFONTW) oldLogFont; (void) oldTextMetric; (void) fontType; (void) lparam;
		doulosAvailable = true;
		return 1;
	}
	static HFONT loadFont (GraphicsScreen me, int font, int size, int style) {
		LOGFONTW spec;
		static int ipaInited;
		if (my printer || my metafile) {
			spec. lfHeight = - win_isize2size (size) * my resolution / 72.0;
		} else {
			spec. lfHeight = - win_isize2size (size) * my resolution / 72.0;
		}
		spec. lfWidth = 0;
		spec. lfEscapement = spec. lfOrientation = 0;
		spec. lfWeight = style & Graphics_BOLD ? FW_BOLD : 0;
		spec. lfItalic = style & Graphics_ITALIC ? 1 : 0;
		spec. lfUnderline = spec. lfStrikeOut = 0;
		spec. lfCharSet =
			font == kGraphics_font_SYMBOL ? SYMBOL_CHARSET :
			font == kGraphics_font_CHINESE ? DEFAULT_CHARSET :
			font == kGraphics_font_JAPANESE ? DEFAULT_CHARSET :
			font >= kGraphics_font_IPATIMES ? DEFAULT_CHARSET :
			ANSI_CHARSET;
		spec. lfOutPrecision = spec. lfClipPrecision = spec. lfQuality = 0;
		spec. lfPitchAndFamily =
			( font == kGraphics_font_COURIER ? FIXED_PITCH : font == kGraphics_font_IPATIMES ? DEFAULT_PITCH : VARIABLE_PITCH ) |
			( font == kGraphics_font_HELVETICA ? FF_SWISS : font == kGraphics_font_COURIER ? FF_MODERN :
			  font == kGraphics_font_CHINESE ? FF_DONTCARE :
			  font == kGraphics_font_JAPANESE ? FF_DONTCARE :
			  font >= kGraphics_font_IPATIMES ? FF_DONTCARE : FF_ROMAN );
		if (font == kGraphics_font_IPATIMES && ! ipaInited && Melder_debug != 15) {
			LOGFONTW logFont;
			logFont. lfCharSet = DEFAULT_CHARSET;
			logFont. lfPitchAndFamily = 0;
			wcscpy (logFont. lfFaceName, L"Charis SIL");
			EnumFontFamiliesExW (my d_gdiGraphicsContext, & logFont, fontFuncEx_charis, 0, 0);
			wcscpy (logFont. lfFaceName, L"Doulos SIL");
			EnumFontFamiliesExW (my d_gdiGraphicsContext, & logFont, fontFuncEx_doulos, 0, 0);
			ipaInited = true;
			if (! charisAvailable && ! doulosAvailable) {
				/* BUG: The next warning may cause reentry of drawing (on window exposure) and lead to crash. Some code must be non-reentrant !! */
				Melder_warning (U"The phonetic font is not available.\nSeveral characters may not look correct.\nSee www.praat.org");
			}
		}
		wcscpy (spec. lfFaceName,
			font == kGraphics_font_HELVETICA ? L"Arial" :
			font == kGraphics_font_TIMES     ? L"Times New Roman" :
			font == kGraphics_font_COURIER   ? L"Courier New" :
			font == kGraphics_font_PALATINO  ? L"Book Antiqua" :
			font == kGraphics_font_SYMBOL    ? L"Symbol" :
			font == kGraphics_font_IPATIMES  ? ( doulosAvailable && style == 0 ? L"Doulos SIL" : charisAvailable ? L"Charis SIL" : L"Times New Roman" ) :
			font == kGraphics_font_DINGBATS  ? L"Wingdings" :
			font == kGraphics_font_CHINESE   ? L"SimSun" :
			font == kGraphics_font_JAPANESE  ? L"MS UI Gothic" :
			L"");
		return CreateFontIndirectW (& spec);
	}
#endif

#if cairo && USE_PANGO
	static PangoFontDescription *PangoFontDescription_create (int font, _Graphics_widechar *lc) {
		static PangoFontDescription *fontDescriptions [1 + kGraphics_font_DINGBATS];
		Melder_assert (font >= 0 && font <= kGraphics_font_DINGBATS);
		if (! fontDescriptions [font]) {
			const char *fontFace =
				font == kGraphics_font_HELVETICA ? "Helvetica" :
				font == kGraphics_font_TIMES ? "Times" :
				font == kGraphics_font_COURIER ? "Courier" : 
				font == kGraphics_font_PALATINO ? "Palatino" : 
				font == kGraphics_font_IPATIMES ? "Doulos SIL" :
				font == kGraphics_font_IPAPALATINO ? "Charis SIL" :
				font == kGraphics_font_DINGBATS ? "Dingbats" : "Serif";
			fontDescriptions [font] = pango_font_description_from_string (fontFace);
		}

		PangoStyle slant = (lc -> style & Graphics_ITALIC ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
		pango_font_description_set_style (fontDescriptions [font], slant);
						
		PangoWeight weight = (lc -> style & Graphics_BOLD ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL);
		pango_font_description_set_weight (fontDescriptions [font], weight);
		pango_font_description_set_absolute_size (fontDescriptions [font], (int) (lc -> size * PANGO_SCALE));
		return fontDescriptions [font];
	}
#endif

#if quartz || cairo
	inline static int chooseFont (Graphics me, Longchar_Info info, _Graphics_widechar *lc) {
		int font =
			info -> alphabet == Longchar_SYMBOL || // ? kGraphics_font_SYMBOL :
			info -> alphabet == Longchar_PHONETIC ?
				( my font == kGraphics_font_TIMES ?
					( hasDoulos ?
						( lc -> style == 0 ?
							kGraphics_font_IPATIMES :
						  hasCharis ?
							kGraphics_font_IPAPALATINO :   // other styles in Charis, because Doulos has no bold or italic
							kGraphics_font_TIMES
						) :
					  hasCharis ?
						kGraphics_font_IPAPALATINO :
						kGraphics_font_TIMES   // on newer systems, Times and Times New Roman have a lot of phonetic characters
					) :
				  my font == kGraphics_font_HELVETICA || my font == kGraphics_font_COURIER ?
					my font :   // sans serif or wide, so fall back on Lucida Grande for phonetic characters
				  /* my font must be kGraphics_font_PALATINO */
				  hasCharis && Melder_debug != 900 ?
					kGraphics_font_IPAPALATINO :
				  hasDoulos && Melder_debug != 900 ?
					( lc -> style == 0 ?
						kGraphics_font_IPATIMES :
						kGraphics_font_TIMES
					) :
					kGraphics_font_PALATINO
				) :
			lc -> kar == '/' ?
				kGraphics_font_PALATINO :   // override Courier
			info -> alphabet == Longchar_DINGBATS ?
				kGraphics_font_DINGBATS :
			lc -> font.integer == kGraphics_font_COURIER ?
				kGraphics_font_COURIER :
			my font == kGraphics_font_TIMES ?
				( hasDoulos ?
					( lc -> style == 0 ?
						kGraphics_font_IPATIMES :
					  lc -> style == Graphics_ITALIC ?
						kGraphics_font_TIMES :
					  hasCharis ?
						kGraphics_font_IPAPALATINO :
						kGraphics_font_TIMES 
					) :
					kGraphics_font_TIMES
				) :   // needed for correct placement of diacritics
			my font == kGraphics_font_HELVETICA ?
				kGraphics_font_HELVETICA :
			my font == kGraphics_font_PALATINO ?
				( hasCharis && Melder_debug != 900 ?
					kGraphics_font_IPAPALATINO :
					kGraphics_font_PALATINO
				) :
			my font;   // why not lc -> font.integer?
		Melder_assert (font >= 0 && font <= kGraphics_font_DINGBATS);
		return font;
	}
#endif

inline static bool isDiacritic (Longchar_Info info, int font) {
	if (info -> isDiacritic == 0) return false;
	if (info -> isDiacritic == 1) return true;
	Melder_assert (info -> isDiacritic == 2);   // corner
	if (font == kGraphics_font_IPATIMES || font == kGraphics_font_IPAPALATINO) return false;   // Doulos or Charis
	return true;   // e.g. Times substitutes a zero-width corner
}

static void charSize (void *void_me, _Graphics_widechar *lc) {
	iam (Graphics);
	if (my screen) {
		iam (GraphicsScreen);
		#if cairo
			if (my duringXor) {
				Longchar_Info info = Longchar_getInfoFromNative (lc -> kar);
				int normalSize = my fontSize * my resolution / 72.0;
				int smallSize = (3 * normalSize + 2) / 4;
				int size = lc -> size < 100 ? smallSize : normalSize;
				lc -> width = 10;
				lc -> baseline *= my fontSize * 0.01;
				lc -> code = lc -> kar;
				lc -> font.string = nullptr;
				lc -> font.integer = 0;
				lc -> size = size;
			} else {
			#if USE_PANGO
				if (! my d_cairoGraphicsContext) return;
				Longchar_Info info = Longchar_getInfoFromNative (lc -> kar);
				double normalSize = my fontSize * my resolution / 72.0;
				double smallSize = (3 * normalSize + 2) / 4;
				double size = lc -> size < 100 ? smallSize : normalSize;
				char32 buffer [2] = { lc -> kar, 0 };
				int font = chooseFont (me, info, lc);

				lc -> size = (int) size;   // an approximation, but needed for testing equality
				lc -> size_real = size;   // the accurate measurement

				PangoFontDescription *font_description = PangoFontDescription_create (font, lc);

				PangoAttribute *pango_attribute = pango_attr_font_desc_new (font_description);
				PangoAttrList *pango_attr_list = pango_attr_list_new ();
				pango_attr_list_insert (pango_attr_list, pango_attribute); // list is owner of attribute
				PangoAttrIterator *pango_attr_iterator = pango_attr_list_get_iterator (pango_attr_list);
				int length = strlen (Melder_peek32to8 (buffer));
				GList *pango_glist = pango_itemize (thePangoContext, Melder_peek32to8 (buffer), 0, length, pango_attr_list, pango_attr_iterator);
				PangoAnalysis pango_analysis = ((PangoItem *) pango_glist -> data) -> analysis;
				PangoGlyphString *pango_glyph_string = pango_glyph_string_new ();
				pango_shape (Melder_peek32to8 (buffer), length, & pango_analysis, pango_glyph_string);
				
				lc -> width = isDiacritic (info, font) ? 0 :
					pango_glyph_string_get_width (pango_glyph_string) / PANGO_SCALE;
				trace (U"width ", lc -> width);
				lc -> code = lc -> kar;
				lc -> baseline *= my fontSize * 0.01;
				lc -> font.string = nullptr;
				lc -> font.integer = font;
				pango_glyph_string_free (pango_glyph_string);
				g_list_free_full (pango_glist, (GDestroyNotify) pango_item_free);
				//g_list_free (pango_glist);
				pango_attr_iterator_destroy (pango_attr_iterator);
				pango_attr_list_unref (pango_attr_list);
				//pango_attribute_destroy (pango_attribute); // list is owner
			#else
				if (! my d_cairoGraphicsContext) return;
				Longchar_Info info = Longchar_getInfoFromNative (lc -> kar);
				int font, size, style;
				int normalSize = my fontSize * my resolution / 72.0;
				int smallSize = (3 * normalSize + 2) / 4;
				size = lc -> size < 100 ? smallSize : normalSize;
				cairo_text_extents_t extents;

				enum _cairo_font_slant slant   = (lc -> style & Graphics_ITALIC ? CAIRO_FONT_SLANT_ITALIC : CAIRO_FONT_SLANT_NORMAL);
				enum _cairo_font_weight weight = (lc -> style & Graphics_BOLD   ? CAIRO_FONT_WEIGHT_BOLD  : CAIRO_FONT_WEIGHT_NORMAL);

				cairo_set_font_size (my d_cairoGraphicsContext, size);
				
				font = info -> alphabet == Longchar_SYMBOL ? kGraphics_font_SYMBOL :
					   info -> alphabet == Longchar_PHONETIC ? kGraphics_font_IPATIMES :
					   info -> alphabet == Longchar_DINGBATS ? kGraphics_font_DINGBATS : lc -> font.integer;

				switch (font) {
					case kGraphics_font_HELVETICA: cairo_select_font_face (my d_cairoGraphicsContext, "Helvetica", slant, weight); break;
					case kGraphics_font_TIMES:     cairo_select_font_face (my d_cairoGraphicsContext, "Times New Roman", slant, weight); break;
					case kGraphics_font_COURIER:   cairo_select_font_face (my d_cairoGraphicsContext, "Courier", slant, weight); break;
					case kGraphics_font_PALATINO:  cairo_select_font_face (my d_cairoGraphicsContext, "Palatino", slant, weight); break;
					case kGraphics_font_SYMBOL:    cairo_select_font_face (my d_cairoGraphicsContext, "Symbol", slant, weight); break;
					case kGraphics_font_IPATIMES:  cairo_select_font_face (my d_cairoGraphicsContext, "Doulos SIL", slant, weight); break;
					case kGraphics_font_DINGBATS:  cairo_select_font_face (my d_cairoGraphicsContext, "Dingbats", slant, weight); break;
					default:                       cairo_select_font_face (my d_cairoGraphicsContext, "Sans", slant, weight); break;
				}
				char32 buffer [2] = { lc -> kar, 0 };
				cairo_text_extents (my d_cairoGraphicsContext, Melder_peek32to8 (buffer), & extents);
				lc -> width = extents.x_advance;
				trace (U"width ", lc -> width);
				lc -> baseline *= my fontSize * 0.01;
				lc -> code = lc -> kar;
				lc -> font.string = nullptr;
				lc -> font.integer = font;
				lc -> size = size;
			#endif
			}
		#elif gdi
			Longchar_Info info = Longchar_getInfoFromNative (lc -> kar);
			int font, size, style;
			HFONT fontInfo;
			int normalSize = win_size2isize (my fontSize);
			int smallSize = (3 * normalSize + 2) / 4;
			font = info -> alphabet == Longchar_SYMBOL ? kGraphics_font_SYMBOL :
			       info -> alphabet == Longchar_PHONETIC ? kGraphics_font_IPATIMES :
			       info -> alphabet == Longchar_DINGBATS ? kGraphics_font_DINGBATS : lc -> font.integer;
			if ((unsigned int) lc -> kar >= 0x2E80 && (unsigned int) lc -> kar <= 0x9FFF)
				font = ( theGraphicsCjkFontStyle == kGraphics_cjkFontStyle_CHINESE ? kGraphics_font_CHINESE : kGraphics_font_JAPANESE );
			size = lc -> size < 100 ? smallSize : normalSize;
			style = lc -> style & (Graphics_ITALIC | Graphics_BOLD);   // take out Graphics_CODE
			fontInfo = fonts [my resolutionNumber] [font] [size] [style];
			if (! fontInfo) {
				fontInfo = loadFont (me, font, size, style);
				if (! fontInfo) return;
				fonts [my resolutionNumber] [font] [size] [style] = fontInfo;
			}
			SIZE extent;
			lc -> code =
				font == kGraphics_font_IPATIMES ||
				font == kGraphics_font_TIMES ||
				font == kGraphics_font_HELVETICA ||
				font == kGraphics_font_CHINESE ||
				font == kGraphics_font_JAPANESE ||
				font == kGraphics_font_COURIER ? lc -> kar :
				info -> winEncoding;
			if (lc -> code == 0) {
				_Graphics_widechar *lc2;
				if (lc -> kar == UNICODE_LATIN_SMALL_LETTER_SCHWA_WITH_HOOK) {
					info = Longchar_getInfo ('s', 'w');
					lc -> kar = info -> unicode;
					lc -> code = info -> winEncoding;
					for (lc2 = lc + 1; lc2 -> kar != U'\0'; lc2 ++) { }
					lc2 [1]. kar = U'\0';
					while (lc2 - lc > 0) { lc2 [0] = lc2 [-1]; lc2 --; }
					lc [1]. kar = UNICODE_MODIFIER_LETTER_RHOTIC_HOOK;
				} else if (lc -> kar == UNICODE_LATIN_SMALL_LETTER_L_WITH_MIDDLE_TILDE) {
					info = Longchar_getInfo ('l', ' ');
					lc -> kar = info -> unicode;
					lc -> code = info -> winEncoding;
					for (lc2 = lc + 1; lc2 -> kar != U'\0'; lc2 ++) { }
					lc2 [1]. kar = U'\0';
					while (lc2 - lc > 0) { lc2 [0] = lc2 [-1]; lc2 --; }
					lc [1]. kar = UNICODE_COMBINING_TILDE_OVERLAY;
				}
			}
			SelectFont (my d_gdiGraphicsContext, fontInfo);
			if (lc -> code <= 0x00FFFF) {
				char16 code = (char16) lc -> code;
				GetTextExtentPoint32W (my d_gdiGraphicsContext, (WCHAR *) & code, 1, & extent);
			} else {
				char32 code [2] { lc -> code, U'\0' };
				GetTextExtentPoint32W (my d_gdiGraphicsContext, Melder_peek32toW (code), 2, & extent);
			}
			lc -> width = extent. cx;
			lc -> baseline *= my fontSize * 0.01 * my resolution / 72.0;
			lc -> font.string = nullptr;
			lc -> font.integer = font;   // kGraphics_font_HELVETICA .. kGraphics_font_DINGBATS
			lc -> size = size;   // 0..4 instead of 10..24
			lc -> style = style;   // without Graphics_CODE
		#elif quartz
		#endif
	} else if (my postScript) {
		iam (GraphicsPostscript);
		int normalSize = (int) ((double) my fontSize * (double) my resolution / 72.0);
		Longchar_Info info = Longchar_getInfoFromNative (lc -> kar);
		int font = info -> alphabet == Longchar_SYMBOL ? kGraphics_font_SYMBOL :
				info -> alphabet == Longchar_PHONETIC ? kGraphics_font_IPATIMES :
				info -> alphabet == Longchar_DINGBATS ? kGraphics_font_DINGBATS : lc -> font.integer;
		int style = lc -> style == Graphics_ITALIC ? Graphics_ITALIC :
			lc -> style == Graphics_BOLD || lc -> link ? Graphics_BOLD :
			lc -> style == Graphics_BOLD_ITALIC ? Graphics_BOLD_ITALIC : 0;
		if (! my fontInfos [font] [style]) {
			const char *fontInfo, *secondaryFontInfo = nullptr, *tertiaryFontInfo = nullptr;
			if (font == kGraphics_font_COURIER) {
				fontInfo = style == Graphics_BOLD ? "Courier-Bold" :
					style == Graphics_ITALIC ? "Courier-Oblique" :
					style == Graphics_BOLD_ITALIC ? "Courier-BoldOblique" : "Courier";
				secondaryFontInfo = style == Graphics_BOLD ? "CourierNewPS-BoldMT" :
					style == Graphics_ITALIC ? "CourierNewPS-ItalicMT" :
					style == Graphics_BOLD_ITALIC ? "CourierNewPS-BoldItalicMT" : "CourierNewPSMT";
				tertiaryFontInfo = style == Graphics_BOLD ? "CourierNew-Bold" :
					style == Graphics_ITALIC ? "CourierNew-Italic" :
					style == Graphics_BOLD_ITALIC ? "CourierNew-BoldItalic" : "CourierNew";
			} else if (font == kGraphics_font_TIMES) {
				fontInfo = style == Graphics_BOLD ? "Times-Bold" :
					style == Graphics_ITALIC ? "Times-Italic" :
					style == Graphics_BOLD_ITALIC ? "Times-BoldItalic" : "Times-Roman";
				secondaryFontInfo = style == Graphics_BOLD ? "TimesNewRomanPS-BoldMT" :
					style == Graphics_ITALIC ? "TimesNewRomanPS-ItalicMT" :
					style == Graphics_BOLD_ITALIC ? "TimesNewRomanPS-BoldItalicMT" : "TimesNewRomanPSMT";
				tertiaryFontInfo = style == Graphics_BOLD ? "TimesNewRoman-Bold" :
					style == Graphics_ITALIC ? "TimesNewRoman-Italic" :
					style == Graphics_BOLD_ITALIC ? "TimesNewRoman-BoldItalic" : "TimesNewRoman";
			} else if (font == kGraphics_font_PALATINO) {
				fontInfo = style == Graphics_BOLD ? "Palatino-Bold" :
					style == Graphics_ITALIC ? "Palatino-Italic" :
					style == Graphics_BOLD_ITALIC ? "Palatino-BoldItalic" : "Palatino-Roman";
				secondaryFontInfo = style == Graphics_BOLD ? "BookAntiquaPS-BoldMT" :
					style == Graphics_ITALIC ? "BookAntiquaPS-ItalicMT" :
					style == Graphics_BOLD_ITALIC ? "BookAntiquaPS-BoldItalicMT" : "BookAntiquaPSMT";
				tertiaryFontInfo = style == Graphics_BOLD ? "BookAntiqua-Bold" :
					style == Graphics_ITALIC ? "BookAntiqua-Italic" :
					style == Graphics_BOLD_ITALIC ? "BookAntiqua-BoldItalic" : "BookAntiqua";
			} else if (font == kGraphics_font_IPATIMES) {
				if (my includeFonts && ! my loadedXipa) {
					const char **p;
					for (p = & ipaSerifRegularPS [0]; *p; p ++)
						my d_printf (my d_file, "%s", *p);
					my loadedXipa = true;
				}
				fontInfo = my useSilipaPS ?
					(style == Graphics_BOLD || style == Graphics_BOLD_ITALIC ? "SILDoulosIPA93Bold" : "SILDoulosIPA93Regular") :
					"TeX-xipa10-Praat-Regular";
			} else if (font == kGraphics_font_SYMBOL) {
				fontInfo = "Symbol";
			} else if (font == kGraphics_font_DINGBATS) {
				fontInfo = "ZapfDingbats";
			} else {
				fontInfo = style == Graphics_BOLD ? "Helvetica-Bold" :
					style == Graphics_ITALIC ? "Helvetica-Oblique" :
					style == Graphics_BOLD_ITALIC ? "Helvetica-BoldOblique" : "Helvetica";
				secondaryFontInfo = style == Graphics_BOLD ? "Arial-BoldMT" :
					style == Graphics_ITALIC ? "Arial-ItalicMT" :
					style == Graphics_BOLD_ITALIC ? "Arial-BoldItalicMT" : "ArialMT";
				tertiaryFontInfo = style == Graphics_BOLD ? "Arial-Bold" :
					style == Graphics_ITALIC ? "Arial-Italic" :
					style == Graphics_BOLD_ITALIC ? "Arial-BoldItalic" : "Arial";
			}
			my fontInfos [font] [style] = Melder_malloc_f (char, 100);
			if (font == kGraphics_font_IPATIMES || font == kGraphics_font_SYMBOL || font == kGraphics_font_DINGBATS) {
				strcpy (my fontInfos [font] [style], fontInfo);
			} else {
				sprintf (my fontInfos [font] [style], "%s-Praat", fontInfo);
				if (thePrinter. fontChoiceStrategy == kGraphicsPostscript_fontChoiceStrategy_LINOTYPE) {
					my d_printf (my d_file, "/%s /%s-Praat PraatEncode\n", fontInfo, fontInfo);
				} else if (thePrinter. fontChoiceStrategy == kGraphicsPostscript_fontChoiceStrategy_MONOTYPE) {
					my d_printf (my d_file, "/%s /%s-Praat PraatEncode\n", tertiaryFontInfo, fontInfo);
				} else if (thePrinter. fontChoiceStrategy == kGraphicsPostscript_fontChoiceStrategy_PS_MONOTYPE) {
					my d_printf (my d_file, "/%s /%s-Praat PraatEncode\n", secondaryFontInfo, fontInfo);
				} else {
					/* Automatic font choice strategy. */
					if (secondaryFontInfo) {
						my d_printf (my d_file,
							"/%s /Font resourcestatus\n"
							"{ pop pop /%s /%s-Praat PraatEncode }\n"
							"{ /%s /%s-Praat PraatEncode }\n"
							"ifelse\n",
							secondaryFontInfo, secondaryFontInfo, fontInfo, fontInfo, fontInfo);
					} else {
						my d_printf (my d_file, "/%s /%s-Praat PraatEncode\n", fontInfo, fontInfo);
					}
				}
			}
		}
		lc -> font.integer = 0;
		lc -> font.string = my fontInfos [font] [style];

		/*
		 * Convert size and baseline information to device coordinates.
		 */
		lc -> size *= normalSize * 0.01;
		lc -> baseline *= normalSize * 0.01;

		if (font == kGraphics_font_COURIER) {
			lc -> width = 600;   // Courier
		} else if (style == 0) {
			if (font == kGraphics_font_TIMES) lc -> width = info -> ps.times;
			else if (font == kGraphics_font_HELVETICA) lc -> width = info -> ps.helvetica;
			else if (font == kGraphics_font_PALATINO) lc -> width = info -> ps.palatino;
			else if (font == kGraphics_font_SYMBOL) lc -> width = info -> ps.times;
			else if (my useSilipaPS) lc -> width = info -> ps.timesItalic;
			else lc -> width = info -> ps.times;   // XIPA
		} else if (style == Graphics_BOLD) {
			if (font == kGraphics_font_TIMES) lc -> width = info -> ps.timesBold;
			else if (font == kGraphics_font_HELVETICA) lc -> width = info -> ps.helveticaBold;
			else if (font == kGraphics_font_PALATINO) lc -> width = info -> ps.palatinoBold;
			else if (font == kGraphics_font_SYMBOL) lc -> width = info -> ps.times;
			else if (my useSilipaPS) lc -> width = info -> ps.timesBoldItalic;
			else lc -> width = info -> ps.times;   // Symbol, IPA
		} else if (style == Graphics_ITALIC) {
			if (font == kGraphics_font_TIMES) lc -> width = info -> ps.timesItalic;
			else if (font == kGraphics_font_HELVETICA) lc -> width = info -> ps.helvetica;
			else if (font == kGraphics_font_PALATINO) lc -> width = info -> ps.palatinoItalic;
			else if (font == kGraphics_font_SYMBOL) lc -> width = info -> ps.times;
			else if (my useSilipaPS) lc -> width = info -> ps.timesItalic;
			else lc -> width = info -> ps.times;   // Symbol, IPA
		} else if (style == Graphics_BOLD_ITALIC) {
			if (font == kGraphics_font_TIMES) lc -> width = info -> ps.timesBoldItalic;
			else if (font == kGraphics_font_HELVETICA) lc -> width = info -> ps.helveticaBold;
			else if (font == kGraphics_font_PALATINO) lc -> width = info -> ps.palatinoBoldItalic;
			else if (font == kGraphics_font_SYMBOL) lc -> width = info -> ps.times;
			else if (my useSilipaPS) lc -> width = info -> ps.timesBoldItalic;
			else lc -> width = info -> ps.times;   // Symbol, IPA
		}
		lc -> width *= lc -> size / 1000.0;
		lc -> code = font == kGraphics_font_IPATIMES && my useSilipaPS ? info -> macEncoding : info -> psEncoding;
		if (lc -> code == 0) {
			_Graphics_widechar *lc2;
			if (lc -> kar == UNICODE_LATIN_SMALL_LETTER_SCHWA_WITH_HOOK) {
				info = Longchar_getInfo ('s', 'w');
				lc -> kar = info -> unicode;
				lc -> code = info -> macEncoding;
				lc -> width = info -> ps.timesItalic * lc -> size / 1000.0;
				for (lc2 = lc + 1; lc2 -> kar != U'\0'; lc2 ++) { }
				lc2 [1]. kar = U'\0';
				while (lc2 - lc > 0) { lc2 [0] = lc2 [-1]; lc2 --; }
				lc [1]. kar = UNICODE_MODIFIER_LETTER_RHOTIC_HOOK;
			} else if (lc -> kar == UNICODE_LATIN_SMALL_LETTER_L_WITH_MIDDLE_TILDE) {
				info = Longchar_getInfo ('l', ' ');
				lc -> code = info -> macEncoding;
				lc -> kar = info -> unicode;
				lc -> width = info -> ps.timesItalic * lc -> size / 1000.0;
				for (lc2 = lc + 1; lc2 -> kar != U'\0'; lc2 ++) { }
				lc2 [1]. kar = U'\0';
				while (lc2 - lc > 0) { lc2 [0] = lc2 [-1]; lc2 --; }
				lc [1]. kar = UNICODE_COMBINING_TILDE_OVERLAY;
			}
		}
	}
}

static void charDraw (void *void_me, int xDC, int yDC, _Graphics_widechar *lc,
	const char32 *codes, int nchars, int width)
{
	iam (Graphics);
	//Melder_casual (U"nchars ", nchars, U" first ", (int) lc->kar, U" ", (char32) lc -> kar, U" rightToLeft ", lc->rightToLeft);
	if (my postScript) {
		iam (GraphicsPostscript);
		bool onlyRegular = lc -> font.string [0] == 'S' ||
			(lc -> font.string [0] == 'T' && lc -> font.string [1] == 'e');   // Symbol & SILDoulos !
		int slant = (lc -> style & Graphics_ITALIC) && onlyRegular;
		int thick = (lc -> style & Graphics_BOLD) && onlyRegular;
		if (lc -> font.string != my lastFid || lc -> size != my lastSize)
			my d_printf (my d_file, my languageLevel == 1 ? "/%s %d FONT\n" : "/%s %d selectfont\n",
				my lastFid = lc -> font.string, my lastSize = lc -> size);
		if (lc -> link) my d_printf (my d_file, "0 0 1 setrgbcolor\n");
		for (int i = -3; i <= 3; i ++) {
			if (i != 0 && ! thick) continue;
			my d_printf (my d_file, "%d %d M ", xDC + i, yDC);
			if (my textRotation != 0.0 || slant) {
				my d_printf (my d_file, "gsave currentpoint translate ");
				if (my textRotation != 0.0)
					my d_printf (my d_file, "%.6g rotate 0 0 M\n", (double) my textRotation);
				if (slant)
					my d_printf (my d_file, "[1 0 0.25 1 0 0] concat 0 0 M\n");
			}
			my d_printf (my d_file, "(");
			const char32 *p = codes;
			while (*p) {
				if (*p == U'(' || *p == U')' || *p == U'\\') {
					my d_printf (my d_file, "\\%c", (unsigned char) *p);
				} else if (*p >= 32 && *p <= 126) {
					my d_printf (my d_file, "%c", (unsigned char) *p);
				} else {
					my d_printf (my d_file, "\\%d%d%d", (unsigned char) *p / 64,
						((unsigned char) *p % 64) / 8, (unsigned char) *p % 8);
				}
				p ++;
			}
			my d_printf (my d_file, ") show\n");
			if (my textRotation != 0.0 || slant)
				my d_printf (my d_file, "grestore\n");
		}
		if (lc -> link) my d_printf (my d_file, "0 0 0 setrgbcolor\n");
	} else if (my screen) {
		iam (GraphicsScreen);
		#if cairo
			if (my duringXor) {
			} else {
				if (! my d_cairoGraphicsContext) return;
				// TODO!
			}
			int font = lc -> font.integer;
		#elif quartz
			/*
			 * Determine the font family.
			 */
			int font = lc -> font.integer;   // the font of the first character

			/*
			 * Determine the style.
			 */
			int style = lc -> style;   // the style of the first character

			/*
			 * Determine the font-style combination.
			 */
			CTFontRef ctFont = theScreenFonts [font] [lc -> size] [style];
			if (! ctFont) {
				CTFontSymbolicTraits ctStyle = ( style & Graphics_BOLD ? kCTFontBoldTrait : 0 ) | ( lc -> style & Graphics_ITALIC ? kCTFontItalicTrait : 0 );
			#if 1
				CFStringRef key = kCTFontSymbolicTrait;
				CFNumberRef value = CFNumberCreate (nullptr, kCFNumberIntType, & ctStyle);
				CFIndex numberOfValues = 1;
				CFDictionaryRef styleDict = CFDictionaryCreate (nullptr, (const void **) & key, (const void **) & value, numberOfValues,
					& kCFTypeDictionaryKeyCallBacks, & kCFTypeDictionaryValueCallBacks);
				CFRelease (value);
				CFStringRef keys [2];
				keys [0] = kCTFontTraitsAttribute;
				keys [1] = kCTFontNameAttribute;
				CFStringRef cfFont;
				switch (font) {
					case kGraphics_font_TIMES:       { cfFont = (CFStringRef) Melder_peek32toCfstring (U"Times New Roman"); } break;
					case kGraphics_font_HELVETICA:   { cfFont = (CFStringRef) Melder_peek32toCfstring (U"Arial"          ); } break;
					case kGraphics_font_COURIER:     { cfFont = (CFStringRef) Melder_peek32toCfstring (U"Courier New"    ); } break;
					case kGraphics_font_PALATINO:    { if (Melder_debug == 900)
															cfFont = (CFStringRef) Melder_peek32toCfstring (U"DG Meta Serif Science");
													   else
														    cfFont = (CFStringRef) Melder_peek32toCfstring (U"Palatino");
													 } break;
					case kGraphics_font_SYMBOL:      { cfFont = (CFStringRef) Melder_peek32toCfstring (U"Symbol"         ); } break;
					case kGraphics_font_IPATIMES:    { cfFont = (CFStringRef) Melder_peek32toCfstring (U"Doulos SIL"     ); } break;
					case kGraphics_font_IPAPALATINO: { cfFont = (CFStringRef) Melder_peek32toCfstring (U"Charis SIL"     ); } break;
					case kGraphics_font_DINGBATS:    { cfFont = (CFStringRef) Melder_peek32toCfstring (U"Zapf Dingbats"  ); } break;
				}
				void *values [2] = { (void *) styleDict, (void *) cfFont };
				CFDictionaryRef attributes = CFDictionaryCreate (nullptr, (const void **) & keys, (const void **) & values, 2,
					& kCFTypeDictionaryKeyCallBacks, & kCFTypeDictionaryValueCallBacks);
				CFRelease (styleDict);
				CTFontDescriptorRef ctFontDescriptor = CTFontDescriptorCreateWithAttributes (attributes);
				CFRelease (attributes);
			#else   /* Preparing for the time to come when Apple deprecates Core Foundation. */
				NSMutableDictionary *styleDict = [[NSMutableDictionary alloc] initWithCapacity: 1];
				[styleDict   setObject: [NSNumber numberWithUnsignedInt: ctStyle]   forKey: (id) kCTFontSymbolicTrait];
				NSMutableDictionary *attributes = [[NSMutableDictionary alloc] initWithCapacity: 2];
				[attributes   setObject: styleDict   forKey: (id) kCTFontTraitsAttribute];
				switch (font) {
					case kGraphics_font_TIMES:       { [attributes   setObject: @"Times New Roman"   forKey: (id) kCTFontNameAttribute]; } break;
					case kGraphics_font_HELVETICA:   { [attributes   setObject: @"Arial"             forKey: (id) kCTFontNameAttribute]; } break;
					case kGraphics_font_COURIER:     { [attributes   setObject: @"Courier New"       forKey: (id) kCTFontNameAttribute]; } break;
					case kGraphics_font_PALATINO:    { if (Melder_debug == 900)
															[attributes   setObject: @"DG Meta Serif Science" forKey: (id) kCTFontNameAttribute];
													   else
														    [attributes   setObject: @"Palatino"              forKey: (id) kCTFontNameAttribute];
													 } break;
					case kGraphics_font_SYMBOL:      { [attributes   setObject: @"Symbol"            forKey: (id) kCTFontNameAttribute]; } break;
					case kGraphics_font_IPATIMES:    { [attributes   setObject: @"Doulos SIL"        forKey: (id) kCTFontNameAttribute]; } break;
					case kGraphics_font_IPAPALATINO: { [attributes   setObject: @"Charis SIL"        forKey: (id) kCTFontNameAttribute]; } break;
					case kGraphics_font_DINGBATS:    { [attributes   setObject: @"Zapf Dingbats"     forKey: (id) kCTFontNameAttribute]; } break;
				}
				CTFontDescriptorRef ctFontDescriptor = CTFontDescriptorCreateWithAttributes ((CFMutableDictionaryRef) attributes);
				[styleDict release];
				[attributes release];
			#endif
				ctFont = CTFontCreateWithFontDescriptor (ctFontDescriptor, lc -> size, nullptr);
				CFRelease (ctFontDescriptor);
				theScreenFonts [font] [lc -> size] [style] = ctFont;
			}

			const char16 *codes16 = Melder_peek32to16 (codes);
			#if 1
				CFStringRef s = CFStringCreateWithBytes (nullptr,
					(const UInt8 *) codes16, str16len (codes16) * 2,
					kCFStringEncodingUTF16LE, false);
				int length = CFStringGetLength (s);
			#else
				NSString *s = [[NSString alloc]   initWithBytes: codes16   length: str16len (codes16) * 2   encoding: NSUTF16LittleEndianStringEncoding];
				int length = [s length];
			#endif

			CGFloat descent = CTFontGetDescent (ctFont);

            CFMutableAttributedStringRef string = CFAttributedStringCreateMutable (kCFAllocatorDefault, length);
            CFAttributedStringReplaceString (string, CFRangeMake (0, 0), (CFStringRef) s);
            CFRange textRange = CFRangeMake (0, length);
            CFAttributedStringSetAttribute (string, textRange, kCTFontAttributeName, ctFont);

			static CFNumberRef cfKerning;
			if (! cfKerning) {
				double kerning = 0.0;
				cfKerning = CFNumberCreate (kCFAllocatorDefault, kCFNumberDoubleType, & kerning);
			}
            CFAttributedStringSetAttribute (string, textRange, kCTKernAttributeName, cfKerning);

			static CTParagraphStyleRef paragraphStyle;
			if (! paragraphStyle) {
				CTTextAlignment textAlignment = kCTLeftTextAlignment;
				CTParagraphStyleSetting paragraphSettings [1] = { { kCTParagraphStyleSpecifierAlignment, sizeof (CTTextAlignment), & textAlignment } };
				paragraphStyle = CTParagraphStyleCreate (paragraphSettings, 1);
				Melder_assert (paragraphStyle != nullptr);
			}
            CFAttributedStringSetAttribute (string, textRange, kCTParagraphStyleAttributeName, paragraphStyle);

            RGBColor *macColor = lc -> link ? & theBlueColour : my duringXor ? & theWhiteColour : & my d_macColour;
            CGColorRef color = CGColorCreateGenericRGB (macColor->red / 65536.0, macColor->green / 65536.0, macColor->blue / 65536.0, 1.0);
			Melder_assert (color != nullptr);
            CFAttributedStringSetAttribute (string, textRange, kCTForegroundColorAttributeName, color);

            /*
             * Draw.
             */
    
            CGContextSetTextMatrix (my d_macGraphicsContext, CGAffineTransformIdentity);   // this could set the "current context" for CoreText
            CFRelease (color);

			if (my d_macView) {
				[my d_macView   lockFocus];
				my d_macGraphicsContext = (CGContextRef) [[NSGraphicsContext currentContext] graphicsPort];
			}
            CGContextSaveGState (my d_macGraphicsContext);
            CGContextTranslateCTM (my d_macGraphicsContext, xDC, yDC);
            if (my yIsZeroAtTheTop) CGContextScaleCTM (my d_macGraphicsContext, 1.0, -1.0);
            CGContextRotateCTM (my d_macGraphicsContext, my textRotation * NUMpi / 180.0);

			CTLineRef line = CTLineCreateWithAttributedString (string);
            if (my duringXor) {
                CGContextSetBlendMode (my d_macGraphicsContext, kCGBlendModeDifference);
                CGContextSetAllowsAntialiasing (my d_macGraphicsContext, false);
				CTLineDraw (line, my d_macGraphicsContext);
                CGContextSetBlendMode (my d_macGraphicsContext, kCGBlendModeNormal);
                CGContextSetAllowsAntialiasing (my d_macGraphicsContext, true);
            } else {
				CTLineDraw (line, my d_macGraphicsContext);
            }
			//CGContextFlush (my d_macGraphicsContext);
			CFRelease (line);
            CGContextRestoreGState (my d_macGraphicsContext);

            // Clean up
            CFRelease (string);
			CFRelease (s);
			//CFRelease (ctFont);
			if (my d_macView) {
				[my d_macView   unlockFocus];
				if (! my duringXor) {
					//[my d_macView   setNeedsDisplay: YES];   // otherwise, CoreText text may not be drawn
				}
			}
			return;
		#elif gdi
			int font = lc -> font.integer;
		#endif
		/*
		 * First handle the most common case: text without rotation.
		 */
		if (my textRotation == 0.0) {
			/*
			 * Unrotated text could be a link. If so, it will be blue.
			 */
			#if cairo
				if (my duringXor) {
				} else {
					if (lc -> link) _Graphics_setColour (me, Graphics_BLUE);
				}
			#elif gdi
			#endif
			/*
			 * The most common case: a native font.
			 */
			#if cairo
				if (my duringXor) {
					#if ALLOW_GDK_DRAWING
						static GdkFont *font = nullptr;
						if (! font) {
							font = gdk_font_load ("-*-courier-medium-r-normal--*-120-*-*-*-*-iso8859-1");
							if (! font) {
								font = gdk_font_load ("-*-courier 10 pitch-medium-r-normal--*-120-*-*-*-*-iso8859-1");
							}
						}
						if (font) {
							gdk_draw_text_wc (my d_window, font, my d_gdkGraphicsContext, xDC, yDC, (const GdkWChar *) codes, nchars);
						}
						gdk_flush ();
					#endif
				} else {
					Melder_assert (my d_cairoGraphicsContext);
				#if USE_PANGO
					const char *codes8 = Melder_peek32to8 (codes);
					#if 1
						PangoFontDescription *font_description = PangoFontDescription_create (font, lc);
						PangoLayout *layout = pango_cairo_create_layout (my d_cairoGraphicsContext);
						pango_layout_set_font_description (layout, font_description);
						pango_layout_set_text (layout, codes8, -1);
						cairo_move_to (my d_cairoGraphicsContext, xDC, yDC);
						// instead of pango_cairo_show_layout we use pango_cairo_show_layout_line to
						// get the same text origin as cairo_show_text, i.e. baseline left, instead of Pango's top left!
						pango_cairo_show_layout_line (my d_cairoGraphicsContext, pango_layout_get_line_readonly (layout, 0));
						g_object_unref (layout);
					#else
						PangoFontDescription *fontDescription = PangoFontDescription_create (font, lc);
						PangoAttribute *pangoAttribute = pango_attr_font_desc_new (fontDescription);
						PangoAttrList *pangoAttrList = pango_attr_list_new ();
						pango_attr_list_insert (pangoAttrList, pangoAttribute);   // list is owner of attribute
						PangoAttrIterator *pangoAttrIterator = pango_attr_list_get_iterator (pangoAttrList);
						int length = strlen (codes8);
						GList *pangoList = pango_itemize (thePangoContext, codes8, 0, length, pangoAttrList, pangoAttrIterator);
						PangoAnalysis pangoAnalysis = ((PangoItem *) pangoList -> data) -> analysis;
						PangoGlyphString *pangoGlyphString = pango_glyph_string_new ();
						pango_shape (codes8, length, & pangoAnalysis, pangoGlyphString);
						PangoFont *pangoFont = pango_font_map_load_font (thePangoFontMap, thePangoContext, fontDescription);
						cairo_move_to (my d_cairoGraphicsContext, xDC, yDC);
						/* This does no font substitution: */
						pango_cairo_show_glyph_string (my d_cairoGraphicsContext, pangoFont, pangoGlyphString);
						pango_glyph_string_free (pangoGlyphString);
						g_list_free_full (pangoList, (GDestroyNotify) pango_item_free);
						//g_list_free (pangoList);
						pango_attr_iterator_destroy (pangoAttrIterator);
						pango_attr_list_unref (pangoAttrList);
					#endif
				#else
					enum _cairo_font_slant slant   = (lc -> style & Graphics_ITALIC ? CAIRO_FONT_SLANT_ITALIC : CAIRO_FONT_SLANT_NORMAL);
					enum _cairo_font_weight weight = (lc -> style & Graphics_BOLD   ? CAIRO_FONT_WEIGHT_BOLD  : CAIRO_FONT_WEIGHT_NORMAL);
					cairo_set_font_size (my d_cairoGraphicsContext, lc -> size);
					switch (font) {
						case kGraphics_font_HELVETICA: cairo_select_font_face (my d_cairoGraphicsContext, "Helvetica", slant, weight); break;
						case kGraphics_font_TIMES:     cairo_select_font_face (my d_cairoGraphicsContext, "Times New Roman", slant, weight); break;
						case kGraphics_font_COURIER:   cairo_select_font_face (my d_cairoGraphicsContext, "Courier", slant, weight); break;
						case kGraphics_font_PALATINO:  cairo_select_font_face (my d_cairoGraphicsContext, "Palatino", slant, weight); break;
						case kGraphics_font_SYMBOL:    cairo_select_font_face (my d_cairoGraphicsContext, "Symbol", slant, weight); break;
						case kGraphics_font_IPATIMES:  cairo_select_font_face (my d_cairoGraphicsContext, "Doulos SIL", slant, weight); break;
						case kGraphics_font_DINGBATS:  cairo_select_font_face (my d_cairoGraphicsContext, "Dingbats", slant, weight); break;
						default:                       cairo_select_font_face (my d_cairoGraphicsContext, "Sans", slant, weight); break;
					}
					cairo_move_to (my d_cairoGraphicsContext, xDC, yDC);
					cairo_show_text (my d_cairoGraphicsContext, Melder_peek32to8 (codes));
				#endif
				}
			#elif gdi
				if (my duringXor) {
					int descent = (1.0/216) * my fontSize * my resolution;
					int ascent = (1.0/72) * my fontSize * my resolution;
					int maxWidth = 800, maxHeight = 200;
					int baseline = 100, top = baseline - ascent - 1, bottom = baseline + descent + 1;
					static int inited = 0;
					static HDC dc;
					static HBITMAP bitmap;
					if (! inited) {
						dc = CreateCompatibleDC (my d_gdiGraphicsContext);
						bitmap = CreateCompatibleBitmap (my d_gdiGraphicsContext, maxWidth, maxHeight);
						SelectBitmap (dc, bitmap);
						SetBkMode (dc, TRANSPARENT);   // not the default!
						SelectPen (dc, GetStockPen (BLACK_PEN));
						SelectBrush (dc, GetStockBrush (BLACK_BRUSH));
						SetTextAlign (dc, TA_LEFT | TA_BASELINE | TA_NOUPDATECP);   // baseline is not the default!
						inited = 1;
					}
					width += 4;   // for slant
					Rectangle (dc, 0, top, width, bottom);
					SelectFont (dc, fonts [my resolutionNumber] [font] [lc -> size] [lc -> style]);
					SetTextColor (dc, my d_winForegroundColour);
					WCHAR *codesW = Melder_peek32toW (codes);
					TextOutW (dc, 0, baseline, codesW, str16len ((const char16 *) codesW));
					BitBlt (my d_gdiGraphicsContext, xDC, yDC - ascent, width, bottom - top, dc, 0, top, SRCINVERT);
				} else {
					SelectPen (my d_gdiGraphicsContext, my d_winPen), SelectBrush (my d_gdiGraphicsContext, my d_winBrush);
					if (lc -> link) SetTextColor (my d_gdiGraphicsContext, RGB (0, 0, 255)); else SetTextColor (my d_gdiGraphicsContext, my d_winForegroundColour);
					SelectFont (my d_gdiGraphicsContext, fonts [my resolutionNumber] [font] [lc -> size] [lc -> style]);
					WCHAR *codesW = Melder_peek32toW (codes);
					TextOutW (my d_gdiGraphicsContext, xDC, yDC, codesW, str16len ((const char16 *) codesW));
					if (lc -> link) SetTextColor (my d_gdiGraphicsContext, my d_winForegroundColour);
					SelectPen (my d_gdiGraphicsContext, GetStockPen (BLACK_PEN)), SelectBrush (my d_gdiGraphicsContext, GetStockBrush (NULL_BRUSH));
				}
			#endif
			/*
			 * Back to normal colour.
			 */

			#if cairo
				if (my duringXor) {
				} else {
					if (lc -> link) _Graphics_setColour (me, my colour);
				}
			#elif gdi
			#endif
		} else {
			/*
			 * Rotated text.
			 */
			#if cairo
				#if USE_PANGO
					cairo_save (my d_cairoGraphicsContext);
					cairo_translate (my d_cairoGraphicsContext, xDC, yDC);
					//cairo_scale (my d_cairoGraphicsContext, 1, -1);
					cairo_rotate (my d_cairoGraphicsContext, - my textRotation * NUMpi / 180.0);
					cairo_move_to (my d_cairoGraphicsContext, 0, 0);
					
					PangoFontDescription *font_description = PangoFontDescription_create (font, lc);
					PangoLayout *layout = pango_cairo_create_layout (my d_cairoGraphicsContext);
					pango_layout_set_font_description (layout, font_description);
					pango_layout_set_text (layout, Melder_peek32to8 (codes), -1);
					// instead of pango_cairo_show_layout we use pango_cairo_show_layout_line to
					// get the same text origin as cairo_show_text, i.e. baseline left, instead of Pango's top left!
					pango_cairo_show_layout_line (my d_cairoGraphicsContext, pango_layout_get_line_readonly (layout, 0));

					g_object_unref (layout);
					//pango_font_description_free (font_description);
					cairo_restore (my d_cairoGraphicsContext);
				#else
					Melder_assert (my d_cairoGraphicsContext);
					enum _cairo_font_slant  slant  = (lc -> style & Graphics_ITALIC ? CAIRO_FONT_SLANT_ITALIC : CAIRO_FONT_SLANT_NORMAL);
					enum _cairo_font_weight weight = (lc -> style & Graphics_BOLD   ? CAIRO_FONT_WEIGHT_BOLD  : CAIRO_FONT_WEIGHT_NORMAL);
					cairo_set_font_size (my d_cairoGraphicsContext, lc -> size);
					switch (font) {
						case kGraphics_font_HELVETICA: cairo_select_font_face (my d_cairoGraphicsContext, "Helvetica", slant, weight); break;
						case kGraphics_font_TIMES:     cairo_select_font_face (my d_cairoGraphicsContext, "Times"    , slant, weight); break;
						case kGraphics_font_COURIER:   cairo_select_font_face (my d_cairoGraphicsContext, "Courier"  , slant, weight); break;
						case kGraphics_font_PALATINO:  cairo_select_font_face (my d_cairoGraphicsContext, "Palatino" , slant, weight); break;
						case kGraphics_font_SYMBOL:    cairo_select_font_face (my d_cairoGraphicsContext, "Symbol"   , slant, weight); break;
						case kGraphics_font_IPATIMES:  cairo_select_font_face (my d_cairoGraphicsContext, "IPA Times", slant, weight); break;
						case kGraphics_font_DINGBATS:  cairo_select_font_face (my d_cairoGraphicsContext, "Dingbats" , slant, weight); break;
						default:                       cairo_select_font_face (my d_cairoGraphicsContext, "Sans"     , slant, weight); break;
					}
					cairo_save (my d_cairoGraphicsContext);
					cairo_translate (my d_cairoGraphicsContext, xDC, yDC);
					//cairo_scale (my d_cairoGraphicsContext, 1, -1);
					cairo_rotate (my d_cairoGraphicsContext, - my textRotation * NUMpi / 180.0);
					cairo_move_to (my d_cairoGraphicsContext, 0, 0);
					cairo_show_text (my d_cairoGraphicsContext, Melder_peek32to8 (codes));
					cairo_restore (my d_cairoGraphicsContext);
					return;
				#endif
			#elif gdi
				if (1) {
					SelectPen (my d_gdiGraphicsContext, my d_winPen), SelectBrush (my d_gdiGraphicsContext, my d_winBrush);
					if (lc -> link) SetTextColor (my d_gdiGraphicsContext, RGB (0, 0, 255)); else SetTextColor (my d_gdiGraphicsContext, my d_winForegroundColour);
					SelectFont (my d_gdiGraphicsContext, fonts [my resolutionNumber] [font] [lc -> size] [lc -> style]);
					int restore = SaveDC (my d_gdiGraphicsContext);
					SetGraphicsMode (my d_gdiGraphicsContext, GM_ADVANCED);
					double a = my textRotation * NUMpi / 180.0, cosa = cos (a), sina = sin (a);
					XFORM rotate = { (float) cosa, (float) - sina, (float) sina, (float) cosa, 0.0, 0.0 };
					ModifyWorldTransform (my d_gdiGraphicsContext, & rotate, MWT_RIGHTMULTIPLY);
					XFORM translate = { 1.0, 0.0, 0.0, 1.0, (float) xDC, (float) yDC };
					ModifyWorldTransform (my d_gdiGraphicsContext, & translate, MWT_RIGHTMULTIPLY);
					WCHAR *codesW = Melder_peek32toW (codes);
					TextOutW (my d_gdiGraphicsContext, 0 /*xDC*/, 0 /*yDC*/, codesW, str16len ((const char16 *) codesW));
					RestoreDC (my d_gdiGraphicsContext, restore);
					if (lc -> link) SetTextColor (my d_gdiGraphicsContext, my d_winForegroundColour);
					SelectPen (my d_gdiGraphicsContext, GetStockPen (BLACK_PEN)), SelectBrush (my d_gdiGraphicsContext, GetStockBrush (NULL_BRUSH));
					return;
				}
			#endif
			int ascent = (1.0/72) * my fontSize * my resolution;
			int descent = (1.0/216) * my fontSize * my resolution;
			int ix, iy /*, baseline = 1 + ascent * 2*/;
			double cosa, sina;
			#if gdi
				int maxWidth = 1000, maxHeight = 600;   // BUG: printer???
				int baseline = maxHeight / 4, top = baseline - ascent - 1, bottom = baseline + descent + 1;
				static int inited = 0;
				static HDC dc;
				static HBITMAP bitmap;
				if (! inited) {
					dc = CreateCompatibleDC (my d_gdiGraphicsContext);
					bitmap = CreateBitmap (/*my d_gdiGraphicsContext,*/ maxWidth, maxHeight, 1, 1, nullptr);
					SelectBitmap (dc, bitmap);
					inited = 1;
				}
			#endif
			width += 4;   // leave room for slant
			#if gdi
				SelectPen (dc, GetStockPen (WHITE_PEN));
				SelectBrush (dc, GetStockBrush (WHITE_BRUSH));
				SetTextAlign (dc, TA_LEFT | TA_BASELINE | TA_NOUPDATECP);   // baseline is not the default!
				Rectangle (dc, 0, top, maxWidth, bottom + 1);
				//Rectangle (dc, 0, 0, maxWidth, maxHeight);
				SelectPen (dc, GetStockPen (BLACK_PEN));
				SelectBrush (dc, GetStockBrush (NULL_BRUSH));
				SelectFont (dc, fonts [my resolutionNumber] [font] [lc -> size] [lc -> style]);
				WCHAR *codesW = Melder_peek32toW (codes);
				TextOutW (dc, 0, baseline, codesW, str16len ((const char16 *) codesW));
			#endif
			if (my textRotation == 90.0) { cosa = 0.0; sina = 1.0; }
			else if (my textRotation == 270.0) { cosa = 0.0; sina = -1.0; }
			else { double a = my textRotation * NUMpi / 180.0; cosa = cos (a); sina = sin (a); }
			for (ix = 0; ix < width; ix ++) {
				double dx1 = ix;
				#if gdi
					for (iy = top; iy <= bottom; iy ++) {
						if (GetPixel (dc, ix, iy) == RGB (0, 0, 0)) {   // black?
							int dy1 = iy - baseline;   // translate, rotate, translate
							int xp = xDC + (int) (cosa * dx1 + sina * dy1);
							int yp = yDC - (int) (sina * dx1 - cosa * dy1);
							SetPixel (my d_gdiGraphicsContext, xp, yp, my d_winForegroundColour);
						}
					}
				#endif
			}
		}
	}
}

static void initText (void *void_me) {
	iam (Graphics);
	if (my screen) {
		iam (GraphicsScreen);
		(void) me;
	}
}

static void exitText (void *void_me) {
	iam (Graphics);
	if (my screen) {
		iam (GraphicsScreen);
		(void) me;
	}
}

#define MAX_LINK_LENGTH  300

static long bufferSize;
static _Graphics_widechar *theWidechar;
static char32 *charCodes;
static int initBuffer (const char32 *txt) {
	try {
		long sizeNeeded = str32len (txt) + 1;
		if (sizeNeeded > bufferSize) {
			sizeNeeded += sizeNeeded / 2 + 100;
			Melder_free (theWidechar);
			Melder_free (charCodes);
			theWidechar = Melder_calloc (_Graphics_widechar, sizeNeeded);
			charCodes = Melder_calloc (char32, sizeNeeded);
			bufferSize = sizeNeeded;
		}
		return 1;
	} catch (MelderError) {
		bufferSize = 0;
		Melder_flushError ();
		return 0;
	}
}

static int numberOfLinks = 0;
static Graphics_Link links [100];    // a maximum of 100 links per string

static void charSizes (Graphics me, _Graphics_widechar string [], bool measureEachCharacterSeparately) {
	if (my postScript) {
		for (_Graphics_widechar *character = string; character -> kar > U'\t'; character ++)
			charSize (me, character);
	} else {
	/*
	 * Measure the size of each character.
	 */
	_Graphics_widechar *character;
	#if quartz || (cairo && USE_PANGO && 1)
		#if cairo && USE_PANGO
			if (! ((GraphicsScreen) me) -> d_cairoGraphicsContext) return;
		#endif
		int numberOfDiacritics = 0;
		for (_Graphics_widechar *lc = string; lc -> kar > U'\t'; lc ++) {
			/*
			 * Determine the font family.
			 */
			Longchar_Info info = Longchar_getInfoFromNative (lc -> kar);
			int font = chooseFont (me, info, lc);
			lc -> font.string = nullptr;   // this erases font.integer!

			/*
			 * Determine the style.
			 */
			int style = lc -> style;
			Melder_assert (style >= 0 && style <= Graphics_BOLD_ITALIC);

			#if quartz
				/*
				 * Determine and store the font-style combination.
				 */
				CTFontRef ctFont = theScreenFonts [font] [100] [style];
				if (! ctFont) {
					CTFontSymbolicTraits ctStyle = ( style & Graphics_BOLD ? kCTFontBoldTrait : 0 ) | ( lc -> style & Graphics_ITALIC ? kCTFontItalicTrait : 0 );
					NSMutableDictionary *styleDict = [[NSMutableDictionary alloc] initWithCapacity: 1];
					[styleDict   setObject: [NSNumber numberWithUnsignedInt: ctStyle]   forKey: (id) kCTFontSymbolicTrait];
					NSMutableDictionary *attributes = [[NSMutableDictionary alloc] initWithCapacity: 2];
					[attributes   setObject: styleDict   forKey: (id) kCTFontTraitsAttribute];
					switch (font) {
						case kGraphics_font_TIMES:       { [attributes   setObject: @"Times"           forKey: (id) kCTFontNameAttribute]; } break;
						case kGraphics_font_HELVETICA:   { [attributes   setObject: @"Arial"           forKey: (id) kCTFontNameAttribute]; } break;
						case kGraphics_font_COURIER:     { [attributes   setObject: @"Courier New"     forKey: (id) kCTFontNameAttribute]; } break;
						case kGraphics_font_PALATINO:    { if (Melder_debug == 900)
																[attributes   setObject: @"DG Meta Serif Science" forKey: (id) kCTFontNameAttribute];
														   else
																[attributes   setObject: @"Palatino"              forKey: (id) kCTFontNameAttribute];
														 } break;
						case kGraphics_font_SYMBOL:      { [attributes   setObject: @"Symbol"          forKey: (id) kCTFontNameAttribute]; } break;
						case kGraphics_font_IPATIMES:    { [attributes   setObject: @"Doulos SIL"      forKey: (id) kCTFontNameAttribute]; } break;
						case kGraphics_font_IPAPALATINO: { [attributes   setObject: @"Charis SIL"      forKey: (id) kCTFontNameAttribute]; } break;
						case kGraphics_font_DINGBATS:    { [attributes   setObject: @"Zapf Dingbats"   forKey: (id) kCTFontNameAttribute]; } break;
					}
					CTFontDescriptorRef ctFontDescriptor = CTFontDescriptorCreateWithAttributes ((CFMutableDictionaryRef) attributes);
					[styleDict release];
					[attributes release];
					ctFont = CTFontCreateWithFontDescriptor (ctFontDescriptor, 100.0, nullptr);
					CFRelease (ctFontDescriptor);
					theScreenFonts [font] [100] [style] = ctFont;
				}
			#endif

			int normalSize = my fontSize * my resolution / 72.0;
			int smallSize = (3 * normalSize + 2) / 4;
			int size = lc -> size < 100 ? smallSize : normalSize;
			lc -> size = size;
			lc -> baseline *= 0.01 * normalSize;
			lc -> code = lc -> kar;
			lc -> font.integer = font;
			if (Longchar_Info_isDiacritic (info)) {
				numberOfDiacritics ++;
			}
		}
		int nchars = 0;
		for (_Graphics_widechar *lc = string; lc -> kar > U'\t'; lc ++) {
			charCodes [nchars ++] = lc -> code;
			_Graphics_widechar *next = lc + 1;
			lc -> width = 0;
			if (measureEachCharacterSeparately ||
				next->kar <= U' ' || next->style != lc->style ||
				next->baseline != lc->baseline || next->size != lc->size || next->link != lc->link ||
				next->font.integer != lc->font.integer || next->font.string != lc->font.string ||
				next->rightToLeft != lc->rightToLeft ||
				(my textRotation != 0.0 && my screen && my resolution > 150))
			{
				charCodes [nchars] = U'\0';
				#if cairo && USE_PANGO
					const char *codes8 = Melder_peek32to8 (charCodes);
					int length = strlen (codes8);
					PangoFontDescription *fontDescription = PangoFontDescription_create (lc -> font.integer, lc);

					#if 1
						PangoLayout *layout = pango_cairo_create_layout (((GraphicsScreen) me) -> d_cairoGraphicsContext);
						pango_layout_set_font_description (layout, fontDescription);
						pango_layout_set_text (layout, codes8, -1);
						PangoRectangle inkRect, logicalRect;
						pango_layout_get_extents (layout, & inkRect, & logicalRect);
						lc -> width = logicalRect. width / PANGO_SCALE;
						g_object_unref (layout);
					#else
						PangoAttribute *pangoAttribute = pango_attr_font_desc_new (fontDescription);
						PangoAttrList *pangoAttrList = pango_attr_list_new ();
						pango_attr_list_insert (pangoAttrList, pangoAttribute);   // list is owner of attribute
						PangoAttrIterator *pangoAttrIterator = pango_attr_list_get_iterator (pangoAttrList);
						GList *pangoList = pango_itemize (thePangoContext, codes8, 0, length, pangoAttrList, pangoAttrIterator);
						PangoAnalysis pangoAnalysis = ((PangoItem *) pangoList -> data) -> analysis;
						PangoGlyphString *pangoGlyphString = pango_glyph_string_new ();
						pango_shape (codes8, length, & pangoAnalysis, pangoGlyphString);

						/*
							The following attempts to compute the width of a longer glyph string both fail,
							because neither `pango_glyph_string_get_width()` nor `pango_glyph_string_extents()`
							handle font substitution correctly: they seem to compute the width solely on the
							basis of the (perhaps substituted) font of the *first* glyph. In Praat you can
							see this when drawing the string "fdfgasdf\as\as\ct\ctfgdsghj" or the string
							"fdfgasdf\al\al\be\befgdsghj" with right alignment.

							Hence our use of `charSize()` instead of `charSizes()`, despite `charSize`'s problems
							with the widths of diacritics.
						*/
						#if 0
							lc -> width = pango_glyph_string_get_width (pangoGlyphString) / PANGO_SCALE;
						#else
							PangoFont *pangoFont = pango_font_map_load_font (thePangoFontMap, thePangoContext, fontDescription);
							PangoRectangle inkRect, logicalRect;
							pango_glyph_string_extents (pangoGlyphString, pangoFont, & inkRect, & logicalRect);
							lc -> width = logicalRect. width / PANGO_SCALE;
						#endif
						pango_glyph_string_free (pangoGlyphString);
						g_list_free_full (pangoList, (GDestroyNotify) pango_item_free);
						//g_list_free (pangoList);
						pango_attr_iterator_destroy (pangoAttrIterator);
						pango_attr_list_unref (pangoAttrList);
						//pango_attribute_destroy (pangoAttribute);   // list is owner
					#endif
				#elif quartz
					const char16 *codes16 = Melder_peek32to16 (charCodes);
					int64 length = str16len (codes16);

					NSString *s = [[NSString alloc]
						initWithBytes: codes16
						length: (NSUInteger) (length * 2)
						encoding: NSUTF16LittleEndianStringEncoding   // BUG: should be NSUTF16NativeStringEncoding, except that that doesn't exist
						];

					CFRange textRange = CFRangeMake (0, (CFIndex) [s length]);

					CFMutableAttributedStringRef cfstring =
						CFAttributedStringCreateMutable (kCFAllocatorDefault, (CFIndex) [s length]);
					CFAttributedStringReplaceString (cfstring, CFRangeMake (0, 0), (CFStringRef) s);
					CFAttributedStringSetAttribute (cfstring, textRange, kCTFontAttributeName, theScreenFonts [lc -> font.integer] [100] [lc -> style]);

					/*
					 * Measure.
					 */

					// Create a path to render text in
					CGMutablePathRef path = CGPathCreateMutable ();
					NSRect measureRect = NSMakeRect (0, 0, CGFLOAT_MAX, CGFLOAT_MAX);
					CGPathAddRect (path, nullptr, (CGRect) measureRect);
				
					CTFramesetterRef framesetter = CTFramesetterCreateWithAttributedString ((CFAttributedStringRef) cfstring);
					CFRange fitRange;
					CGSize targetSize = CGSizeMake (lc -> width, CGFLOAT_MAX);
					CGSize frameSize = CTFramesetterSuggestFrameSizeWithConstraints (framesetter, textRange, nullptr, targetSize, & fitRange);
					CFRelease (framesetter);
					CFRelease (cfstring);
					[s release];
					CFRelease (path);
					//Longchar_Info info = Longchar_getInfoFromNative (lc -> kar);
					//bool isDiacritic = info -> ps.times == 0;
					//lc -> width = isDiacritic ? 0.0 : frameSize.width * lc -> size / 100.0;
					lc -> width = frameSize.width * lc -> size / 100.0;
					if (Melder_systemVersion >= 101100) {
						/*
						 * If the text ends in a space, CTFramesetterSuggestFrameSizeWithConstraints() ignores the space.
						 * we correct for this.
						 */
						if (codes16 [length - 1] == u' ') {
							lc -> width += 25.0 * lc -> size / 100.0;
						}
					}
				#endif
				nchars = 0;
			}
		}
	#else
		for (character = string; character -> kar > U'\t'; character ++)
			charSize (me, character);
	#endif
	}
	/*
	 * Each character has been garnished with information about the character's width.
	 * Make a correction for systems that make slanted characters overlap the character box to their right.
	 * We must do this after the previous loop, because we query the size of the *next* character.
	 *
	 * Keep this in SYNC with psTextWidth.
	 */
	for (_Graphics_widechar *character = string; character -> kar > U'\t'; character ++) {
		if ((character -> style & Graphics_ITALIC) != 0) {
			_Graphics_widechar *nextCharacter = character + 1;
			if (nextCharacter -> kar <= U'\t') {
				character -> width += SLANT_CORRECTION / 72 * my fontSize * my resolution;
			} else if (((nextCharacter -> style & Graphics_ITALIC) == 0 && nextCharacter -> baseline >= character -> baseline)
				|| (character -> baseline == 0 && nextCharacter -> baseline > 0))
			{
				if (nextCharacter -> kar == U'.' || nextCharacter -> kar == U',')
					character -> width += SLANT_CORRECTION / 144 * my fontSize * my resolution;
				else
					character -> width += SLANT_CORRECTION / 72 * my fontSize * my resolution;
			}
		}
	}
}

/*
 * The routine textWidth determines the fractional width of a text, in device coordinates.
 */
static double textWidth (_Graphics_widechar string []) {
	_Graphics_widechar *character;
	double width = 0;
	for (character = string; character -> kar > U'\t'; character ++)
		width += character -> width;
	return width;
}

static void drawOneCell (Graphics me, int xDC, int yDC, _Graphics_widechar lc []) {
	int nchars = 0;
	double width = textWidth (lc), dx, dy;
	/*
	 * We must continue even if width is zero (for adjusting textY).
	 */
	_Graphics_widechar *plc, *lastlc;
	bool inLink = false;
	switch (my horizontalTextAlignment) {
		case Graphics_LEFT:      dx = 1 + (0.1/72) * my fontSize * my resolution; break;
		case Graphics_CENTRE:    dx = - width / 2; break;
		case Graphics_RIGHT:     dx = width != 0.0 ? - width - (0.1/72) * my fontSize * my resolution : 0; break;   // if width is zero, do not step left
		default:                 dx = 1 + (0.1/72) * my fontSize * my resolution; break;
	}
	switch (my verticalTextAlignment) {
		case Graphics_BOTTOM:    dy = (0.4/72) * my fontSize * my resolution; break;
		case Graphics_HALF:      dy = (-0.3/72) * my fontSize * my resolution; break;
		case Graphics_TOP:       dy = (-1.0/72) * my fontSize * my resolution; break;
		case Graphics_BASELINE:  dy = 0; break;
		default:                 dy = 0; break;
	}
	#if quartz
		if (my screen) {
			GraphicsQuartz_initDraw ((GraphicsScreen) me);
		}
	#endif
	if (my textRotation != 0.0) {
		double xbegin = dx, x = xbegin, cosa, sina;
		if (my textRotation == 90.0f) { cosa = 0.0; sina = 1.0; }
		else if (my textRotation == 270.0f) { cosa = 0.0; sina = -1.0; }
		else { double a = my textRotation * NUMpi / 180.0; cosa = cos (a); sina = sin (a); }
		for (plc = lc; plc -> kar > U'\t'; plc ++) {
			_Graphics_widechar *next = plc + 1;
			charCodes [nchars ++] = plc -> code;   // buffer...
			x += plc -> width;
			/*
			 * We can draw stretches of characters:
			 * they have different styles, baselines, sizes, or fonts,
			 * or if there is a break between them,
			 * or if we cannot rotate multiple characters,
			 * which is the case on bitmap printers.
			 */
			if (next->kar < U' ' || next->style != plc->style ||
				next->baseline != plc->baseline || next->size != plc->size ||
				next->font.integer != plc->font.integer || next->font.string != plc->font.string ||
				next->rightToLeft != plc->rightToLeft ||
				(my screen && my resolution > 150))
			{
				double dy2 = dy + plc -> baseline;
				double xr = cosa * xbegin - sina * dy2;
				double yr = sina * xbegin + cosa * dy2;
				charCodes [nchars] = U'\0';   // ...and flush
				charDraw (me, xDC + xr, my yIsZeroAtTheTop ? yDC - yr : yDC + yr,
					plc, charCodes, nchars, x - xbegin);
				nchars = 0;
				xbegin = x;
			}
		}
	} else {
		double xbegin = xDC + dx, x = xbegin, y = my yIsZeroAtTheTop ? yDC - dy : yDC + dy;
		lastlc = lc;
		if (my wrapWidth != 0.0) {
			/*
			 * Replace some spaces with new-line symbols.
			 */
			int xmax = xDC + my wrapWidth * my scaleX;
			for (plc = lc; plc -> kar >= U' '; plc ++) {
				x += plc -> width;
				if (x > xmax) {   // wrap (if wrapWidth is too small, each word will be on a separate line)
					while (plc >= lastlc) {
						if (plc -> kar == U' ' && ! plc -> link)   // keep links contiguous
							break;
						plc --;
					}
					if (plc <= lastlc) break;   // hopeless situation: no spaces; get over it
					lastlc = plc;
					plc -> kar = U'\n';   // replace space with newline
					#if quartz
						_Graphics_widechar *next = plc + 1;
						if (next->style != plc->style ||
							next->baseline != plc->baseline || next->size != plc->size || next->link != plc->link ||
							next->font.integer != plc->font.integer || next->font.string != plc->font.string ||
							next->rightToLeft != plc->rightToLeft)
						{
							// nothing
						} else {
							next -> width -= 0.3 * my fontSize * my resolution / 72.0;   // subtract the width of one space
						}
					#endif
					x = xDC + dx + my secondIndent * my scaleX;
				}
			}
			xbegin = x = xDC + dx;   // re-initialize for second pass
		}
		for (plc = lc; plc -> kar > U'\t'; plc ++) {
			_Graphics_widechar *next = plc + 1;
			if (plc -> link) {
				if (! inLink) {
					double descent = ( my yIsZeroAtTheTop ? -(0.3/72) : (0.3/72) ) * my fontSize * my resolution;
					links [++ numberOfLinks]. x1 = x;
					links [numberOfLinks]. y1 = y - descent;
					links [numberOfLinks]. y2 = y + 3 * descent;
					inLink = true;
				}
			} else if (inLink) {
				links [numberOfLinks]. x2 = x;
				inLink = false;
			}
			if (plc -> kar == U'\n') {
				xbegin = x = xDC + dx + my secondIndent * my scaleX;
				y = my yIsZeroAtTheTop ? y + (1.2/72) * my fontSize * my resolution : y - (1.2/72) * my fontSize * my resolution;
			} else {
				charCodes [nchars ++] = plc -> code;   // buffer...
				x += plc -> width;
				if (next->kar < U' ' || next->style != plc->style ||
					next->baseline != plc->baseline || next->size != plc->size || next->link != plc->link ||
					next->font.integer != plc->font.integer || next->font.string != plc->font.string ||
					next->rightToLeft != plc->rightToLeft)
				{
					charCodes [nchars] = U'\0';   // ...and flush
					charDraw (me, xbegin, my yIsZeroAtTheTop ? y - plc -> baseline : y + plc -> baseline,
						plc, charCodes, nchars, x - xbegin);
					nchars = 0;
					xbegin = x;
				}
			}
		}
		if (inLink) {
			links [numberOfLinks]. x2 = x;
			inLink = false;
		}
		my textX = (x - my deltaX) / my scaleX;
		my textY = (( my yIsZeroAtTheTop ? y + dy : y - dy ) - my deltaY) / my scaleY;
	}
	#if quartz
		if (my screen) {
			GraphicsQuartz_exitDraw ((GraphicsScreen) me);
		}
	#endif
}

static struct { double width; short alignment; } tabs [1 + 20] = { { 0, Graphics_CENTRE },
	{ 1, Graphics_CENTRE }, { 1, Graphics_CENTRE }, { 1, Graphics_CENTRE }, { 1, Graphics_CENTRE },
	{ 1, Graphics_CENTRE }, { 1, Graphics_CENTRE }, { 1, Graphics_CENTRE }, { 1, Graphics_CENTRE } };

/*
 * The routine 'drawCells' handles table and layout.
 */
static void drawCells (Graphics me, double xWC, double yWC, _Graphics_widechar lc []) {
	_Graphics_widechar *plc;
	int itab = 0, saveTextAlignment = my horizontalTextAlignment;
	double saveWrapWidth = my wrapWidth;
	numberOfLinks = 0;
	for (plc = lc; /* No stop condition. */ ; plc ++) {
		charSizes (me, plc, false);
		drawOneCell (me, xWC * my scaleX + my deltaX, yWC * my scaleY + my deltaY, plc);
		while (plc -> kar != U'\0' && plc -> kar != U'\t') plc ++;   // find end of cell
		if (plc -> kar == U'\0') break;   // end of text?
		if (plc -> kar == U'\t') {   // go to next cell
			xWC += ( tabs [itab]. alignment == Graphics_LEFT ? tabs [itab]. width :
			       tabs [itab]. alignment == Graphics_CENTRE ? 0.5 * tabs [itab]. width : 0 ) * my fontSize / 12.0;
			itab ++;
			xWC += ( tabs [itab]. alignment == Graphics_LEFT ? 0 :
			       tabs [itab]. alignment == Graphics_CENTRE ? 0.5 * tabs [itab]. width : tabs [itab]. width ) * my fontSize / 12.0;
			my horizontalTextAlignment = tabs [itab]. alignment;
			my wrapWidth = tabs [itab]. width * my fontSize / 12.0;
		}
	}
	my horizontalTextAlignment = saveTextAlignment;
	my wrapWidth = saveWrapWidth;
}

static void parseTextIntoCellsLinesRuns (Graphics me, const char32 *txt /* cattable */, _Graphics_widechar a_widechar []) {
	char32 kar;
	const char32 *in = txt;
	int nquote = 0;
	_Graphics_widechar *out = & a_widechar [0];
	unsigned int charSuperscript = 0, charSubscript = 0, charItalic = 0, charBold = 0;
	unsigned int wordItalic = 0, wordBold = 0, wordCode = 0, wordLink = 0;
	unsigned int globalSuperscript = 0, globalSubscript = 0, globalItalic = 0, globalBold = 0, globalCode = 0, globalLink = 0;
	unsigned int globalSmall = 0;
	numberOfLinks = 0;
	while ((kar = *in++) != U'\0') {
		if (kar == U'^' && my circumflexIsSuperscript) {
			if (globalSuperscript) globalSuperscript = 0;
			else if (in [0] == '^') { globalSuperscript = 1; in ++; }
			else charSuperscript = 1;
			wordItalic = wordBold = wordCode = 0;
			continue;
		} else if (kar == U'_' && my underscoreIsSubscript) {
			if (globalSubscript) { globalSubscript = 0; wordItalic = wordBold = wordCode = 0; continue; }
			else if (in [0] == U'_') { globalSubscript = 1; in ++; wordItalic = wordBold = wordCode = 0; continue; }
			else if (! my dollarSignIsCode) { charSubscript = 1; wordItalic = wordBold = wordCode = 0; continue; }   // not in manuals
			else
				;   // a normal underscore in manuals
		} else if (kar == U'%' && my percentSignIsItalic) {
			if (globalItalic) globalItalic = 0;
			else if (in [0] == U'%') { globalItalic = 1; in ++; }
			else if (my dollarSignIsCode) wordItalic = 1;   // in manuals
			else charItalic = 1;
			continue;
		} else if (kar == U'#' && my numberSignIsBold) {
			if (globalBold) globalBold = 0;
			else if (in [0] == U'#') { globalBold = 1; in ++; }
			else if (my dollarSignIsCode) wordBold = 1;   // in manuals
			else charBold = 1;
			continue;
		} else if (kar == U'$' && my dollarSignIsCode) {
			if (globalCode) globalCode = 0;
			else if (in [0] == U'$') { globalCode = 1; in ++; }
			else wordCode = 1;
			continue;
		} else if (kar == U'@' && my atSignIsLink   // recognize links
		           && my textRotation == 0.0)   // no links allowed in rotated text, because links are identified by 2-point rectangles
		{
			char32 *to, *max;
			/*
			 * We will distinguish:
			 * 1. The link text: the text shown to the user, drawn in blue.
			 * 2. The link info: the information saved in the Graphics object when the user clicks the link;
			 *    this may be a page title in a manual or any other information.
			 * The link info is equal to the link text in the following cases:
			 * 1. A single-word link: "this is a @Link that consists of one word".
			 * 2. Longer links without '|' in them: "@@Link with spaces@".
			 * The link info is unequal to the link text in the following case:
			 * 3. Longer links with '|' in them: "@@Page linked to|Text shown in blue@"
			 */
			if (globalLink) {
				/*
				 * Detected the third '@' in strings like "@@Link with spaces@".
				 * This closes the link text (which will be shown in blue).
				 */
				globalLink = 0;   // close the drawn link text (the normal colour will take over)
				continue;   // the '@' must not be drawn
			} else if (in [0] == U'@') {
				/*
				 * Detected the second '@' in strings like "@@Link with spaces@".
				 * A format like "@@Page linked to|Text shown in blue@" is permitted.
				 * First step: collect the page text (the link information);
				 * it is everything between "@@" and "|" or "@" or end of string.
				 */
				const char32 *from = in + 1;   // start with first character after "@@"
				if (! links [++ numberOfLinks]. name)   // make room for saving link info
					links [numberOfLinks]. name = Melder_calloc_f (char32, MAX_LINK_LENGTH + 1);
				to = links [numberOfLinks]. name, max = to + MAX_LINK_LENGTH;
				while (*from && *from != U'@' && *from != U'|' && to < max)   // until end-of-string or '@' or '|'...
					* to ++ = * from ++;   // ... copy one character
				*to = U'\0';   // close saved link info
				/*
				 * Second step: collect the link text that is to be drawn.
				 * Its characters will be collected during the normal cycles of the loop.
				 * If the link info is equal to the link text, no action is needed.
				 * If, on the other hand, there is a separate link info, this will have to be skipped.
				 */
				if (*from == U'|')
					in += to - links [numberOfLinks]. name + 1;   // skip link info + '|'
				/*
				 * We are entering the link-text-collection mode.
				 */
				globalLink = 1;
				/*
				 * Both '@' must be skipped and must not be drawn.
				 */
				in ++;   // skip second '@'
				continue;   // do not draw
			} else {
				/*
				 * Detected a single-word link, like in "this is a @Link that consists of one word".
				 * First step: collect the page text: letters, digits, and underscores.
				 */
				const char32 *from = in;   // start with first character after "@"
				if (! links [++ numberOfLinks]. name)   // make room for saving link info
					links [numberOfLinks]. name = Melder_calloc_f (char32, MAX_LINK_LENGTH + 1);
				to = links [numberOfLinks]. name, max = to + MAX_LINK_LENGTH;
				while (*from && (isalnum ((int) *from) || *from == U'_') && to < max)   // until end-of-word...
					*to ++ = *from++;   // ... copy one character
				*to = '\0';   // close saved link info
				/*
				 * Second step: collect the link text that is to be drawn.
				 * Its characters will be collected during the normal cycles of the loop.
				 * The link info is equal to the link text, so no skipping is needed.
				 */
				wordLink = 1;   // enter the single-word link-text-collection mode
			}
			continue;
		} else if (kar == U'\\') {
			/*
			 * Detected backslash sequence: backslash + kar1 + kar2...
			 */
			char32 kar1, kar2;
			/*
			 * ... except if kar1 or kar2 is null: in that case, draw the backslash.
			 */
			if (! (kar1 = in [0]) || ! (kar2 = in [1])) {
				;   // normal backslash symbol
			/*
			 * Catch "\s{", which means: small characters until corresponding '}'.
			 */
			} else if (kar2 == U'{') {
				if (kar1 == U's') globalSmall = 1;
				in += 2;
				continue;
			/*
			 * Default action: translate the backslash sequence into the long character 'kar1,kar2'.
			 */
			} else {
				kar = Longchar_getInfo (kar1, kar2) -> unicode;
				in += 2;
			}
		} else if (kar == U'\"') {
			if (! (my font == kGraphics_font_COURIER || my fontStyle == Graphics_CODE || wordCode || globalCode))
				kar = ++nquote & 1 ? UNICODE_LEFT_DOUBLE_QUOTATION_MARK : UNICODE_RIGHT_DOUBLE_QUOTATION_MARK;
		} else if (kar == U'\'') {
			kar = UNICODE_RIGHT_SINGLE_QUOTATION_MARK;
		} else if (kar == U'`') {
			kar = UNICODE_LEFT_SINGLE_QUOTATION_MARK;
		} else if (kar >= 32 && kar <= 126) {
			if (kar == U'f') {
				if (in [0] == U'i' && HAS_FI_AND_FL_LIGATURES && ! (my font == kGraphics_font_COURIER || my fontStyle == Graphics_CODE || wordCode || globalCode)) {
					kar = UNICODE_LATIN_SMALL_LIGATURE_FI;
					in ++;
				} else if (in [0] == U'l' && HAS_FI_AND_FL_LIGATURES && ! (my font == kGraphics_font_COURIER || my fontStyle == Graphics_CODE || wordCode || globalCode)) {
					kar = UNICODE_LATIN_SMALL_LIGATURE_FL;
					in ++;
				}
			} else if (kar == U'}') {
				if (globalSmall) { globalSmall = 0; continue; }
			}
		} else if (kar == U'\t') {
			out -> kar = U'\t';
			out -> rightToLeft = false;
			wordItalic = wordBold = wordCode = wordLink = 0;
			globalSubscript = globalSuperscript = globalItalic = globalBold = globalCode = globalLink = globalSmall = 0;
			charItalic = charBold = charSuperscript = charSubscript = 0;
			out ++;
			continue;   // do not draw
		} else if (kar == '\n') {
			kar = ' ';
		}
		if (wordItalic | wordBold | wordCode | wordLink) {
			if (! isalnum ((int) kar) && kar != U'_')   // FIXME: this test could be more precise.
				wordItalic = wordBold = wordCode = wordLink = 0;
		}
		out -> style =
			(wordLink | globalLink) && my fontStyle != Graphics_CODE ? Graphics_BOLD :
			((my fontStyle & Graphics_ITALIC) | charItalic | wordItalic | globalItalic ? Graphics_ITALIC : 0) +
			((my fontStyle & Graphics_BOLD) | charBold | wordBold | globalBold ? Graphics_BOLD : 0);
		out -> font.string = nullptr;
		out -> font.integer = my fontStyle == Graphics_CODE || wordCode || globalCode ||
			kar == U'/' || kar == U'|' ? kGraphics_font_COURIER : my font;
		out -> link = wordLink | globalLink;
		out -> baseline = charSuperscript | globalSuperscript ? 34 : charSubscript | globalSubscript ? -25 : 0;
		out -> size = globalSmall || out -> baseline != 0 ? 80 : 100;
		if (kar == U'/') {
			out -> baseline -= out -> size / 12;
			out -> size += out -> size / 10;
		}
		out -> code = U'?';   // does this have any meaning?
		out -> kar = kar;
		out -> rightToLeft =
			(kar >= 0x0590 && kar <= 0x06FF) ||
			(kar >= 0xFE70 && kar <= 0xFEFF) ||
			(kar >= 0xFB1E && kar <= 0xFDFF);
		charItalic = charBold = charSuperscript = charSubscript = 0;
		out ++;
	}
	out -> kar = U'\0';   // end of text
	out -> rightToLeft = false;
}

double Graphics_textWidth (Graphics me, const char32 *txt) {
	if (! initBuffer (txt)) return 0.0;
	initText (me);
	parseTextIntoCellsLinesRuns (me, txt, theWidechar);
	charSizes (me, theWidechar, false);
	double width = textWidth (theWidechar);
	exitText (me);
	return width / my scaleX;
}

void Graphics_textRect (Graphics me, double x1, double x2, double y1, double y2, const char32 *txt) {
	_Graphics_widechar *plc, *startOfLine;
	double width = 0.0, lineHeight = (1.1 / 72) * my fontSize * my resolution;
	long x1DC = x1 * my scaleX + my deltaX + 2, x2DC = x2 * my scaleX + my deltaX - 2;
	long y1DC = y1 * my scaleY + my deltaY, y2DC = y2 * my scaleY + my deltaY;
	int availableHeight = my yIsZeroAtTheTop ? y1DC - y2DC : y2DC - y1DC, availableWidth = x2DC - x1DC;
	int linesAvailable = availableHeight / lineHeight, linesNeeded = 1, lines, iline;
	if (linesAvailable <= 0) linesAvailable = 1;
	if (availableWidth <= 0) return;
	if (! initBuffer (txt)) return;
	initText (me);
	parseTextIntoCellsLinesRuns (me, txt, theWidechar);
	charSizes (me, theWidechar, true);
	for (plc = theWidechar; plc -> kar > U'\t'; plc ++) {
		width += plc -> width;
		if (width > availableWidth) {
			if (++ linesNeeded > linesAvailable) break;
			width = 0.0;
		}	
	}
	lines = linesNeeded > linesAvailable ? linesAvailable : linesNeeded;
	startOfLine = theWidechar;
	for (iline = 1; iline <= lines; iline ++) {
		width = 0.0;
		for (plc = startOfLine; plc -> kar > U'\t'; plc ++) {
			bool flush = false;
			width += plc -> width;
			if (width > availableWidth) flush = true;
			/*
			 * Trick for incorporating end-of-text.
			 */
			if (! flush && plc [1]. kar <= U'\t') {
				Melder_assert (iline == lines);
				plc ++;   // brr
				flush = true;
			}
			if (flush) {
				char32 saveKar = plc -> kar;
				int direction = my yIsZeroAtTheTop ? -1 : 1;
				int x = my horizontalTextAlignment == Graphics_LEFT ? x1DC :
					my horizontalTextAlignment == Graphics_RIGHT ? x2DC :
					0.5 * (x1 + x2) * my scaleX + my deltaX;
				int y = my verticalTextAlignment == Graphics_BOTTOM ?
					y1DC + direction * (lines - iline) * lineHeight :
					my verticalTextAlignment == Graphics_TOP ?
					y2DC - direction * (iline - 1) * lineHeight :
					0.5 * (y1 + y2) * my scaleY + my deltaY + 0.5 * direction * (lines - iline*2 + 1) * lineHeight;
				plc -> kar = U'\0';
				drawOneCell (me, x, y, startOfLine);
				plc -> kar = saveKar;
				startOfLine = plc;
				break;
			}
		}
	}
	exitText (me);
}

static void _Graphics_text (Graphics me, double xWC, double yWC, const char32 *txt) {
	if (my wrapWidth == 0.0 && str32chr (txt, U'\n') && my textRotation == 0.0) {
		double lineSpacingWC = (1.2/72.0) * my fontSize * my resolution / fabs (my scaleY);
		long numberOfLines = 1;
		for (const char32 *p = & txt [0]; *p != U'\0'; p ++) {
			if (*p == U'\n') {
				numberOfLines ++;
			}
		}
		yWC +=
			my verticalTextAlignment == Graphics_TOP ? 0.0 :
			my verticalTextAlignment == Graphics_HALF ? 0.5 * (numberOfLines - 1) * lineSpacingWC:
			(numberOfLines - 1) * lineSpacingWC;
		autostring32 linesToDraw = Melder_dup_f (txt);
		char32 *p = & linesToDraw [0];
		for (;;) {
			char32 *newline = str32chr (p, U'\n');
			if (newline) *newline = U'\0';
			Graphics_text (me, xWC, yWC, p);
			yWC -= lineSpacingWC;
			if (newline) {
				p = newline + 1;
			} else {
				break;
			}
		}
		return;
	}
	if (! initBuffer (txt)) return;
	initText (me);
	parseTextIntoCellsLinesRuns (me, txt, theWidechar);
	drawCells (me, xWC, yWC, theWidechar);
	exitText (me);
	if (my recording) {
		char *txt_utf8 = Melder_peek32to8 (txt);
		int length = strlen (txt_utf8) / sizeof (double) + 1;
		op (TEXT, 3 + length); put (xWC); put (yWC); sput (txt_utf8, length)
	}
}

static MelderString theGraphicsTextBuffer { 0 };
void Graphics_text (Graphics me, double x, double y, Melder_1_ARG) {
	MelderString_copy (& theGraphicsTextBuffer, Melder_1_ARG_CALL);   // even in the one-argument case, make a copy because s1 may be a temporary string (Melder_integer or so)
	_Graphics_text (me, x, y, theGraphicsTextBuffer.string);
}
void Graphics_text (Graphics me, double x, double y, Melder_2_ARGS) {
	MelderString_copy (& theGraphicsTextBuffer, Melder_2_ARGS_CALL);
	_Graphics_text (me, x, y, theGraphicsTextBuffer.string);
}
void Graphics_text (Graphics me, double x, double y, Melder_3_ARGS) {
	MelderString_copy (& theGraphicsTextBuffer, Melder_3_ARGS_CALL);
	_Graphics_text (me, x, y, theGraphicsTextBuffer.string);
}
void Graphics_text (Graphics me, double x, double y, Melder_4_ARGS) {
	MelderString_copy (& theGraphicsTextBuffer, Melder_4_ARGS_CALL);
	_Graphics_text (me, x, y, theGraphicsTextBuffer.string);
}
void Graphics_text (Graphics me, double x, double y, Melder_5_ARGS) {
	MelderString_copy (& theGraphicsTextBuffer, Melder_5_ARGS_CALL);
	_Graphics_text (me, x, y, theGraphicsTextBuffer.string);
}
void Graphics_text (Graphics me, double x, double y, Melder_6_ARGS) {
	MelderString_copy (& theGraphicsTextBuffer, Melder_6_ARGS_CALL);
	_Graphics_text (me, x, y, theGraphicsTextBuffer.string);
}
void Graphics_text (Graphics me, double x, double y, Melder_7_ARGS) {
	MelderString_copy (& theGraphicsTextBuffer, Melder_7_ARGS_CALL);
	_Graphics_text (me, x, y, theGraphicsTextBuffer.string);
}
void Graphics_text (Graphics me, double x, double y, Melder_8_ARGS) {
	MelderString_copy (& theGraphicsTextBuffer, Melder_8_ARGS_CALL);
	_Graphics_text (me, x, y, theGraphicsTextBuffer.string);
}
void Graphics_text (Graphics me, double x, double y, Melder_9_ARGS) {
	MelderString_copy (& theGraphicsTextBuffer, Melder_9_ARGS_CALL);
	_Graphics_text (me, x, y, theGraphicsTextBuffer.string);
}
void Graphics_text (Graphics me, double x, double y, Melder_10_ARGS) {
	MelderString_copy (& theGraphicsTextBuffer, Melder_10_ARGS_CALL);
	_Graphics_text (me, x, y, theGraphicsTextBuffer.string);
}
void Graphics_text (Graphics me, double x, double y, Melder_11_ARGS) {
	MelderString_copy (& theGraphicsTextBuffer, Melder_11_ARGS_CALL);
	_Graphics_text (me, x, y, theGraphicsTextBuffer.string);
}
void Graphics_text (Graphics me, double x, double y, Melder_13_ARGS) {
	MelderString_copy (& theGraphicsTextBuffer, Melder_13_ARGS_CALL);
	_Graphics_text (me, x, y, theGraphicsTextBuffer.string);
}
void Graphics_text (Graphics me, double x, double y, Melder_15_ARGS) {
	MelderString_copy (& theGraphicsTextBuffer, Melder_15_ARGS_CALL);
	_Graphics_text (me, x, y, theGraphicsTextBuffer.string);
}
void Graphics_text (Graphics me, double x, double y, Melder_19_ARGS) {
	MelderString_copy (& theGraphicsTextBuffer, Melder_19_ARGS_CALL);
	_Graphics_text (me, x, y, theGraphicsTextBuffer.string);
}

double Graphics_inqTextX (Graphics me) { return my textX; }
double Graphics_inqTextY (Graphics me) { return my textY; }

int Graphics_getLinks (Graphics_Link **plinks) { *plinks = & links [0]; return numberOfLinks; }

static double psTextWidth (_Graphics_widechar string [], bool useSilipaPS) {
	_Graphics_widechar *character;
	/*
	 * The following has to be kept IN SYNC with GraphicsPostscript::charSize.
	 */
	double textWidth = 0;
	for (character = string; character -> kar > U'\t'; character ++) {
		Longchar_Info info = Longchar_getInfoFromNative (character -> kar);
		int font = info -> alphabet == Longchar_SYMBOL ? kGraphics_font_SYMBOL :
				info -> alphabet == Longchar_PHONETIC ? kGraphics_font_IPATIMES :
				info -> alphabet == Longchar_DINGBATS ? kGraphics_font_DINGBATS : character -> font.integer;
		int style = character -> style == Graphics_ITALIC ? Graphics_ITALIC :
			character -> style == Graphics_BOLD || character -> link ? Graphics_BOLD :
			character -> style == Graphics_BOLD_ITALIC ? Graphics_BOLD_ITALIC : 0;
		double size = character -> size * 0.01;
		double charWidth = 600;   // Courier
		if (font == kGraphics_font_COURIER) {
			charWidth = 600;
		} else if (style == 0) {
			if (font == kGraphics_font_TIMES) charWidth = info -> ps.times;
			else if (font == kGraphics_font_HELVETICA) charWidth = info -> ps.helvetica;
			else if (font == kGraphics_font_PALATINO) charWidth = info -> ps.palatino;
			else if (font == kGraphics_font_IPATIMES && useSilipaPS) charWidth = info -> ps.timesItalic;
			else charWidth = info -> ps.times;   // Symbol, IPA
		} else if (style == Graphics_BOLD) {
			if (font == kGraphics_font_TIMES) charWidth = info -> ps.timesBold;
			else if (font == kGraphics_font_HELVETICA) charWidth = info -> ps.helveticaBold;
			else if (font == kGraphics_font_PALATINO) charWidth = info -> ps.palatinoBold;
			else if (font == kGraphics_font_IPATIMES && useSilipaPS) charWidth = info -> ps.timesBoldItalic;
			else charWidth = info -> ps.times;
		} else if (style == Graphics_ITALIC) {
			if (font == kGraphics_font_TIMES) charWidth = info -> ps.timesItalic;
			else if (font == kGraphics_font_HELVETICA) charWidth = info -> ps.helvetica;
			else if (font == kGraphics_font_PALATINO) charWidth = info -> ps.palatinoItalic;
			else if (font == kGraphics_font_IPATIMES && useSilipaPS) charWidth = info -> ps.timesItalic;
			else charWidth = info -> ps.times;
		} else if (style == Graphics_BOLD_ITALIC) {
			if (font == kGraphics_font_TIMES) charWidth = info -> ps.timesBoldItalic;
			else if (font == kGraphics_font_HELVETICA) charWidth = info -> ps.helveticaBold;
			else if (font == kGraphics_font_PALATINO) charWidth = info -> ps.palatinoBoldItalic;
			else if (font == kGraphics_font_IPATIMES && useSilipaPS) charWidth = info -> ps.timesBoldItalic;
			else charWidth = info -> ps.times;
		}
		charWidth *= size / 1000.0;
		textWidth += charWidth;
	}
	/*
	 * The following has to be kept IN SYNC with charSizes ().
	 */
	for (character = string; character -> kar > U'\t'; character ++) {
		if ((character -> style & Graphics_ITALIC) != 0) {
			_Graphics_widechar *nextCharacter = character + 1;
			if (nextCharacter -> kar <= '\t') {
				textWidth += POSTSCRIPT_SLANT_CORRECTION;
			} else if (((nextCharacter -> style & Graphics_ITALIC) == 0 && nextCharacter -> baseline >= character -> baseline)
				|| (character -> baseline == 0 && nextCharacter -> baseline > 0))
			{
				if (nextCharacter -> kar == U'.' || nextCharacter -> kar == U',')
					textWidth += 0.5 * POSTSCRIPT_SLANT_CORRECTION;
				else
					textWidth += POSTSCRIPT_SLANT_CORRECTION;
			}
		}
	}
	return textWidth;
}

double Graphics_textWidth_ps_mm (Graphics me, const char32 *txt, bool useSilipaPS) {
	if (! initBuffer (txt)) return 0.0;
	parseTextIntoCellsLinesRuns (me, txt, theWidechar);
	return psTextWidth (theWidechar, useSilipaPS) * (double) my fontSize * (25.4 / 72.0);
}

double Graphics_textWidth_ps (Graphics me, const char32 *txt, bool useSilipaPS) {
	return Graphics_dxMMtoWC (me, Graphics_textWidth_ps_mm (me, txt, useSilipaPS));
}

#if quartz
	bool _GraphicsMac_tryToInitializeFonts () {
		static bool inited = false;
		if (inited) return true;
		NSArray *fontNames = [[NSFontManager sharedFontManager] availableFontFamilies];
		hasTimes = [fontNames containsObject: @"Times"];
		if (! hasTimes) hasTimes = [fontNames containsObject: @"Times New Roman"];
		hasHelvetica = [fontNames containsObject: @"Helvetica"];
		if (! hasHelvetica) hasHelvetica = [fontNames containsObject: @"Arial"];
		hasCourier = [fontNames containsObject: @"Courier"];
		if (! hasCourier) hasCourier = [fontNames containsObject: @"Courier New"];
		hasSymbol = [fontNames containsObject: @"Symbol"];
		hasPalatino = [fontNames containsObject: @"Palatino"];
		if (! hasPalatino) hasPalatino = [fontNames containsObject: @"Book Antiqua"];
		hasDoulos = [fontNames containsObject: @"Doulos SIL"];
		hasCharis = [fontNames containsObject: @"Charis SIL"];
		hasIpaSerif = hasDoulos || hasCharis;
		inited = true;
		return true;
	}
#endif

#if cairo
	#if USE_PANGO
		static const char *testFont (const char *fontName) {
			PangoFontDescription *pangoFontDescription, *pangoFontDescription2;
			PangoFont *pangoFont;
			pangoFontDescription = pango_font_description_from_string (fontName);
			pangoFont = pango_font_map_load_font (thePangoFontMap, thePangoContext, pangoFontDescription);
			pangoFontDescription2 = pango_font_describe (pangoFont);
			return pango_font_description_get_family (pangoFontDescription2);
		}
	#endif
	bool _GraphicsLin_tryToInitializeFonts () {
		static bool inited = false;
		if (inited) return true;
		#if USE_PANGO
			thePangoFontMap = pango_cairo_font_map_get_default ();
			thePangoContext = pango_font_map_create_context (thePangoFontMap);
			#if 0   /* For debugging: list all fonts. */
				PangoFontFamily **families;
				int numberOfFamilies;
				pango_font_map_list_families (thePangoFontMap, & families, & numberOfFamilies);
				for (int i = 0; i < numberOfFamilies; i ++) {
					fprintf (stderr, "%d %s\n", i, pango_font_family_get_name (families [i]));
				}
				g_free (families);
			#endif
			const char *trueName;
			trueName = testFont ("Times");
			hasTimes = !! strstr (trueName, "Times") || !! strstr (trueName, "Roman") || !! strstr (trueName, "Serif");
			trueName = testFont ("Helvetica");
			hasHelvetica = !! strstr (trueName, "Helvetica") || !! strstr (trueName, "Arial") || !! strstr (trueName, "Sans");
			trueName = testFont ("Courier");
			hasCourier = !! strstr (trueName, "Courier") || !! strstr (trueName, "Mono");
			trueName = testFont ("Palatino");
			hasPalatino = !! strstr (trueName, "Palatino") || !! strstr (trueName, "Palladio");
			trueName = testFont ("Doulos SIL");
			hasDoulos = !! strstr (trueName, "Doulos");
			trueName = testFont ("Charis SIL");
			hasCharis = !! strstr (trueName, "Charis");
			hasIpaSerif = hasDoulos || hasCharis;
			testFont ("Symbol");
			testFont ("Dingbats");
			#if 0   /* For debugging: list font availability. */
				fprintf (stderr, "times %d helvetica %d courier %d palatino %d doulos %d charis %d\n",
					hasTimes, hasHelvetica, hasCourier, hasPalatino, hasDoulos, hasCharis);
			#endif
		#endif
		inited = true;
		return true;
	}
#endif

void _GraphicsScreen_text_init (GraphicsScreen me) {   // BUG: should be done as late as possible
	#if cairo
        (void) me;
		Melder_assert (_GraphicsLin_tryToInitializeFonts ());
	#elif gdi
		int font, size, style;
		if (my printer || my metafile)
			for (font = kGraphics_font_MIN; font <= kGraphics_font_DINGBATS; font ++)
				for (size = 0; size <= 4; size ++)
					for (style = 0; style <= Graphics_BOLD_ITALIC; style ++)
						if (fonts [my resolutionNumber] [font] [size] [style]) {
							//DeleteObject (fonts [my resolutionNumber] [font] [size] [style]);
							//fonts [my resolutionNumber] [font] [size] [style] = 0;
						}
	#elif quartz
        (void) me;
        Melder_assert (_GraphicsMac_tryToInitializeFonts ());   // should have been handled when setting my useQuartz to true
	#endif
}

/* Output attributes. */

void Graphics_setTextAlignment (Graphics me, int hor, int vert) {
	if (hor != Graphics_NOCHANGE) my horizontalTextAlignment = hor;
	if (vert != Graphics_NOCHANGE) my verticalTextAlignment = vert;
	if (my recording) { op (SET_TEXT_ALIGNMENT, 2); put (hor); put (vert); }
}

void Graphics_setFont (Graphics me, enum kGraphics_font font) {
	my font = font;
	if (my recording) { op (SET_FONT, 1); put (font); }
}

void Graphics_setFontSize (Graphics me, int size) {
	my fontSize = size;
	if (my recording) { op (SET_FONT_SIZE, 1); put (size); }
}

void Graphics_setFontStyle (Graphics me, int style) {
	my fontStyle = style;
	if (my recording) { op (SET_FONT_STYLE, 1); put (style); }
}

void Graphics_setItalic (Graphics me, bool onoff) {
	if (onoff) my fontStyle |= Graphics_ITALIC; else my fontStyle &= ~ Graphics_ITALIC;
}

void Graphics_setBold (Graphics me, bool onoff) {
	if (onoff) my fontStyle |= Graphics_BOLD; else my fontStyle &= ~ Graphics_BOLD;
}

void Graphics_setCode (Graphics me, bool onoff) {
	if (onoff) my fontStyle |= Graphics_CODE; else my fontStyle &= ~ Graphics_CODE;
}

void Graphics_setTextRotation (Graphics me, double angle) {
	my textRotation = angle;
	if (my recording) { op (SET_TEXT_ROTATION, 1); put (angle); }
}

void Graphics_setWrapWidth (Graphics me, double wrapWidth) {
	my wrapWidth = wrapWidth;
	if (my recording) { op (SET_WRAP_WIDTH, 1); put (wrapWidth); }
}

void Graphics_setSecondIndent (Graphics me, double indent) {
	my secondIndent = indent;
	if (my recording) { op (SET_SECOND_INDENT, 1); put (indent); }
}

void Graphics_setPercentSignIsItalic (Graphics me, bool isItalic) {
	my percentSignIsItalic = isItalic;
	if (my recording) { op (SET_PERCENT_SIGN_IS_ITALIC, 1); put (isItalic); }
}

void Graphics_setNumberSignIsBold (Graphics me, bool isBold) {
	my numberSignIsBold = isBold;
	if (my recording) { op (SET_NUMBER_SIGN_IS_BOLD, 1); put (isBold); }
}

void Graphics_setCircumflexIsSuperscript (Graphics me, bool isSuperscript) {
	my circumflexIsSuperscript = isSuperscript;
	if (my recording) { op (SET_CIRCUMFLEX_IS_SUPERSCRIPT, 1); put (isSuperscript); }
}

void Graphics_setUnderscoreIsSubscript (Graphics me, bool isSubscript) {
	my underscoreIsSubscript = isSubscript;
	if (my recording) { op (SET_UNDERSCORE_IS_SUBSCRIPT, 1); put (isSubscript); }
}

void Graphics_setDollarSignIsCode (Graphics me, bool isCode) {
	my dollarSignIsCode = isCode;
	if (my recording) { op (SET_DOLLAR_SIGN_IS_CODE, 1); put (isCode); }
}

void Graphics_setAtSignIsLink (Graphics me, bool isLink) {
	my atSignIsLink = isLink;
	if (my recording) { op (SET_AT_SIGN_IS_LINK, 1); put (isLink); }
}

/* Inquiries. */

enum kGraphics_font Graphics_inqFont (Graphics me) { return my font; }
int Graphics_inqFontSize (Graphics me) { return my fontSize; }
int Graphics_inqFontStyle (Graphics me) { return my fontStyle; }

/* End of file Graphics_text.cpp */
