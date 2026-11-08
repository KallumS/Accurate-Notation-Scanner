/*
    PdfReader - the one place that talks to MuPDF.

    It runs each page through a MuPDF "device" that, instead of painting,
    writes down what would have been painted: every character with its font,
    glyph and position, every path, every picture. Characters are named as
    music symbols here (Glyphs.h); paths become segments and curves
    (Shapes.h). Text the PDF marks invisible - the hidden layer a scanner's
    OCR adds - is ignored.
*/

#pragma once

#include "Ink.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ans
{

struct PdfContents
{
    std::vector<PageInk> pages;
    std::string error;     // set when the file could not be read at all
    std::string title;     // the document's own title, if it has one
};

PdfContents readPdf (const std::string& path);

// A page drawn into 8-bit RGB, for showing it. scale 1 = 72 dpi.
struct PageImage
{
    int width = 0, height = 0;
    std::vector<std::uint8_t> rgb;   // width * height * 3
};

PageImage renderPage (const std::string& path, int pageIndex, double scale, std::string* error = nullptr);

int pageCount (const std::string& path);

} // namespace ans
