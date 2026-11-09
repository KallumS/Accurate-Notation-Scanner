#include "PdfReader.h"

#include "Glyphs.h"
#include "Shapes.h"

#include <mupdf/fitz.h>

#include <cmath>
#include <cstring>
#include <map>

namespace ans
{

namespace
{
// ---------------------------------------------------------------------------
// Paths

struct PathBuilder
{
    DrawnPath* path;
    fz_matrix ctm;
    Point last, start;

    Point map (float x, float y) const
    {
        const fz_point p = fz_transform_point (fz_make_point (x, y), ctm);
        return { p.x, p.y };
    }
};

void walkMove (fz_context*, void* arg, float x, float y)
{
    auto* b = static_cast<PathBuilder*> (arg);
    PathCommand c; c.kind = PathCommand::Move; c.p[0] = b->map (x, y);
    b->path->commands.push_back (c);
    b->last = b->start = { x, y };
}

void walkLine (fz_context*, void* arg, float x, float y)
{
    auto* b = static_cast<PathBuilder*> (arg);
    PathCommand c; c.kind = PathCommand::Line; c.p[0] = b->map (x, y);
    b->path->commands.push_back (c);
    b->last = { x, y };
}

void walkCurve (fz_context*, void* arg, float x1, float y1, float x2, float y2, float x3, float y3)
{
    auto* b = static_cast<PathBuilder*> (arg);
    PathCommand c; c.kind = PathCommand::CurveTo;
    c.p[0] = b->map (x1, y1); c.p[1] = b->map (x2, y2); c.p[2] = b->map (x3, y3);
    b->path->commands.push_back (c);
    b->last = { x3, y3 };
}

void walkQuad (fz_context* ctx, void* arg, float x1, float y1, float x2, float y2)
{
    auto* b = static_cast<PathBuilder*> (arg);
    const auto l = b->last;
    walkCurve (ctx, arg,
               static_cast<float> (l.x + 2.0 / 3.0 * (x1 - l.x)), static_cast<float> (l.y + 2.0 / 3.0 * (y1 - l.y)),
               static_cast<float> (x2 + 2.0 / 3.0 * (x1 - x2)), static_cast<float> (y2 + 2.0 / 3.0 * (y1 - y2)), x2, y2);
}

void walkCurveV (fz_context* ctx, void* arg, float x2, float y2, float x3, float y3)
{
    auto* b = static_cast<PathBuilder*> (arg);
    walkCurve (ctx, arg, static_cast<float> (b->last.x), static_cast<float> (b->last.y), x2, y2, x3, y3);
}

void walkCurveY (fz_context* ctx, void* arg, float x1, float y1, float x3, float y3)
{
    walkCurve (ctx, arg, x1, y1, x3, y3, x3, y3);
}

void walkClose (fz_context*, void* arg)
{
    auto* b = static_cast<PathBuilder*> (arg);
    PathCommand c; c.kind = PathCommand::Close;
    b->path->commands.push_back (c);
    b->last = b->start;
}

void walkRect (fz_context* ctx, void* arg, float x1, float y1, float x2, float y2)
{
    walkMove (ctx, arg, x1, y1);
    walkLine (ctx, arg, x2, y1);
    walkLine (ctx, arg, x2, y2);
    walkLine (ctx, arg, x1, y2);
    walkClose (ctx, arg);
}

DrawnPath toDrawnPath (fz_context* ctx, const fz_path* path, fz_matrix ctm)
{
    DrawnPath out;
    PathBuilder b { &out, ctm, {}, {} };
    fz_path_walker w;
    std::memset (&w, 0, sizeof w);
    w.moveto = walkMove; w.lineto = walkLine; w.curveto = walkCurve; w.closepath = walkClose;
    w.quadto = walkQuad; w.curvetov = walkCurveV; w.curvetoy = walkCurveY; w.rectto = walkRect;
    fz_walk_path (ctx, path, &w, &b);
    return out;
}

// Ink nobody sees: white, or fully transparent.
bool invisible (fz_context* ctx, fz_colorspace* cs, const float* color, float alpha, fz_color_params params)
{
    if (alpha <= 0.01f) return true;
    if (cs == nullptr || color == nullptr) return false;
    float rgb[3] = { 0, 0, 0 };
    fz_convert_color (ctx, cs, color, fz_device_rgb (ctx), rgb, nullptr, params);
    return rgb[0] > 0.9f && rgb[1] > 0.9f && rgb[2] > 0.9f;
}

// ---------------------------------------------------------------------------
// The device

struct Recorder
{
    fz_device super;
    PageInk* page;
    int glyphsFacing[4] = { 0, 0, 0, 0 };   // music glyphs by which way they face, in quarter turns
};

PageInk& pageOf (fz_device* dev) { return *reinterpret_cast<Recorder*> (dev)->page; }

double expansion (fz_matrix m) { return std::sqrt (std::abs (m.a * m.d - m.b * m.c)); }

void recordFillPath (fz_context* ctx, fz_device* dev, const fz_path* path, int, fz_matrix ctm,
                     fz_colorspace* cs, const float* color, float alpha, fz_color_params params)
{
    if (invisible (ctx, cs, color, alpha, params)) return;
    auto drawn = toDrawnPath (ctx, path, ctm);
    drawn.filled = true;
    addPath (pageOf (dev), drawn);
}

void recordStrokePath (fz_context* ctx, fz_device* dev, const fz_path* path, const fz_stroke_state* stroke, fz_matrix ctm,
                       fz_colorspace* cs, const float* color, float alpha, fz_color_params params)
{
    if (invisible (ctx, cs, color, alpha, params)) return;
    auto drawn = toDrawnPath (ctx, path, ctm);
    drawn.stroked = true;
    drawn.lineWidth = (stroke != nullptr ? stroke->linewidth : 1.0f) * expansion (ctm);
    drawn.dashed = stroke != nullptr && stroke->dash_len > 0;
    addPath (pageOf (dev), drawn);
}

void appendUtf8 (std::string& out, int code)
{
    if (code < 0) return;
    const auto c = static_cast<unsigned> (code);
    if (c < 0x80) out += static_cast<char> (c);
    else if (c < 0x800) { out += static_cast<char> (0xc0 | (c >> 6)); out += static_cast<char> (0x80 | (c & 0x3f)); }
    else if (c < 0x10000) { out += static_cast<char> (0xe0 | (c >> 12)); out += static_cast<char> (0x80 | ((c >> 6) & 0x3f)); out += static_cast<char> (0x80 | (c & 0x3f)); }
    else { out += static_cast<char> (0xf0 | (c >> 18)); out += static_cast<char> (0x80 | ((c >> 12) & 0x3f)); out += static_cast<char> (0x80 | ((c >> 6) & 0x3f)); out += static_cast<char> (0x80 | (c & 0x3f)); }
}

// A font subset that lost its Unicode table still names its glyphs: LilyPond
// writes volta and fingering numbers this way ("one", "period").
unsigned letterFromGlyphName (const char* glyphName)
{
    // LilyPond's own digits: "fattened.one", "fattened.four.alt".
    std::string base (glyphName);
    if (base.rfind ("fattened.", 0) == 0) base = base.substr (9);
    if (const auto dot = base.find ('.'); dot != std::string::npos && dot > 0) base = base.substr (0, dot);
    const char* name = base.c_str();
    static const char* const digits[] = { "zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine" };
    for (unsigned d = 0; d < 10; ++d)
        if (std::strcmp (name, digits[d]) == 0) return '0' + d;
    struct { const char* n; unsigned c; } const marks[] = { { "period", '.' }, { "comma", ',' }, { "plus", '+' }, { "hyphen", '-' }, { "space", ' ' } };
    for (const auto& m : marks)
        if (std::strcmp (name, m.n) == 0) return m.c;
    return 0;
}

void recordText (fz_context* ctx, fz_device* dev, const fz_text* text, fz_matrix ctm,
                 fz_colorspace* cs, const float* color, float alpha, fz_color_params params)
{
    if (invisible (ctx, cs, color, alpha, params)) return;
    auto& page = pageOf (dev);

    for (const fz_text_span* span = text->head; span != nullptr; span = span->next)
    {
        fz_font* font = span->font;
        const std::string fontName = fz_font_name (ctx, font);
        const bool italic = fz_font_is_italic (ctx, font) != 0;
        const bool bold = fz_font_is_bold (ctx, font) != 0;

        Text run;
        auto flush = [&] {
            if (! run.text.empty()) page.texts.push_back (run);
            run = Text {};
        };

        for (int i = 0; i < span->len; ++i)
        {
            const fz_text_item& item = span->items[i];
            if (item.gid < 0)
            {
                if (! run.text.empty()) appendUtf8 (run.text, item.ucs);
                continue;
            }
            fz_matrix trm = span->trm;
            trm.e = item.x;
            trm.f = item.y;
            const fz_matrix m = fz_concat (trm, ctm);
            const double size = expansion (m);

            char glyphName[64] = { 0 };
            fz_get_glyph_name (ctx, font, item.gid, glyphName, sizeof glyphName);
            unsigned ucs = item.ucs > 0 && item.ucs != 0xFFFD ? static_cast<unsigned> (item.ucs) : 0u;
            if (ucs == 0) ucs = letterFromGlyphName (glyphName);
            const auto naming = glyphs::name (fontName, ucs, glyphName);

            const fz_rect r = fz_bound_glyph (ctx, font, item.gid, m);
            const Box box { r.x0, r.y0, r.x1, r.y1 };

            if (! naming.name.empty())
            {
                flush();
                const double angle = std::atan2 (m.b, m.a) * 180.0 / 3.14159265358979;
                ++reinterpret_cast<Recorder*> (dev)->glyphsFacing[((static_cast<int> (std::lround (-angle / 90.0)) % 4) + 4) % 4];
                Glyph g;
                g.name = naming.name;
                g.origin = { m.e, m.f };
                g.box = box.empty() ? Box { m.e, m.f, m.e, m.f } : box;
                g.size = size;
                g.advance = fz_advance_glyph (ctx, font, item.gid, span->wmode) * size;
                page.glyphs.push_back (std::move (g));
                continue;
            }
            if (naming.musicFontUnread)
            {
                flush();
                ++page.unnamedMusicGlyphs;
                const auto base = glyphs::baseFontName (fontName);
                bool seen = false;
                for (const auto& f : page.unknownMusicFonts) seen = seen || f == base;
                if (! seen) page.unknownMusicFonts.push_back (base);
                continue;
            }

            // Ordinary text. A space, or a jump, ends a word.
            if (ucs == ' ' || ucs == 0xa0)
            {
                flush();
                continue;
            }
            if (! run.text.empty() && (m.e - run.box.x1 > size * 0.25 || std::abs (m.f - run.box.y1) > size))
                flush();
            if (run.text.empty())
            {
                run.size = size; run.italic = italic; run.bold = bold; run.font = glyphs::baseFontName (fontName);
                run.box = box.empty() ? Box { m.e, m.f - size * 0.7, m.e + size * 0.5, m.f } : box;
            }
            else if (! box.empty())
                run.box.add (box);
            appendUtf8 (run.text, ucs != 0 ? static_cast<int> (ucs) : '?');
        }
        flush();
    }
}

void recordStrokeText (fz_context* ctx, fz_device* dev, const fz_text* text, const fz_stroke_state*, fz_matrix ctm,
                       fz_colorspace* cs, const float* color, float alpha, fz_color_params params)
{
    recordText (ctx, dev, text, ctm, cs, color, alpha, params);
}

void recordImage (fz_context*, fz_device* dev, fz_image*, fz_matrix ctm, float, fz_color_params)
{
    const fz_rect r = fz_transform_rect (fz_unit_rect, ctm);
    pageOf (dev).pictures.push_back ({ { r.x0, r.y0, r.x1, r.y1 } });
}

void recordImageMask (fz_context* ctx, fz_device* dev, fz_image* img, fz_matrix ctm, fz_colorspace*, const float*, float alpha, fz_color_params params)
{
    recordImage (ctx, dev, img, ctm, alpha, params);
}

struct Context
{
    fz_context* ctx = fz_new_context (nullptr, nullptr, FZ_STORE_DEFAULT);
    Context()
    {
        if (ctx == nullptr) return;
        fz_register_document_handlers (ctx);
        // MuPDF's notes about repairing damaged files are not for the user.
        fz_set_warning_callback (ctx, [] (void*, const char*) {}, nullptr);
        fz_set_error_callback (ctx, [] (void*, const char*) {}, nullptr);
    }
    ~Context() { if (ctx != nullptr) fz_drop_context (ctx); }
};

// Opens a document, trying an empty password (many PDFs are "encrypted"
// with no password at all, just to set permissions).
fz_document* openDocument (fz_context* ctx, const std::string& path, std::string& error)
{
    fz_document* doc = nullptr;
    fz_var (doc);
    fz_try (ctx)
        doc = fz_open_document (ctx, path.c_str());
    fz_catch (ctx)
    {
        error = std::string ("This file could not be opened as a PDF (") + fz_caught_message (ctx) + ").";
        return nullptr;
    }
    if (fz_needs_password (ctx, doc) && ! fz_authenticate_password (ctx, doc, ""))
    {
        fz_drop_document (ctx, doc);
        error = "This PDF is protected by a password.";
        return nullptr;
    }
    return doc;
}
// Turns a page's drawing a number of quarter turns clockwise, keeping it at 0,0.
fz_matrix turned (fz_rect bounds, int quarterTurns)
{
    const fz_matrix r = fz_rotate (static_cast<float> (90 * quarterTurns));
    const fz_rect b = fz_transform_rect (bounds, r);
    return fz_concat (r, fz_translate (-b.x0, -b.y0));
}

// Runs one page through the recording device. False if MuPDF gave up on it.
// A score drawn turned on its page (LilyPond's landscape setting draws it
// sideways on an upright page) is read again, turned back upright.
bool readPage (fz_context* ctx, fz_document* doc, int index, PageInk& ink, int quarterTurns = -1)
{
    fz_page* page = nullptr;
    fz_device* dev = nullptr;
    bool ok = true;
    fz_var (page);
    fz_var (dev);
    fz_var (ok);
    int again = 0;   // quarter turns to read it again with
    fz_var (again);
    fz_try (ctx)
    {
        page = fz_load_page (ctx, doc, index);
        const fz_rect bounds = fz_bound_page (ctx, page);
        const int turns = std::max (0, quarterTurns);
        const fz_rect shown = fz_transform_rect (bounds, turned (bounds, turns));
        ink.width = shown.x1 - shown.x0;
        ink.height = shown.y1 - shown.y0;
        ink.quarterTurns = turns;

        auto* rec = fz_new_derived_device (ctx, Recorder);
        rec->page = &ink;
        rec->super.fill_path = recordFillPath;
        rec->super.stroke_path = recordStrokePath;
        rec->super.fill_text = recordText;
        rec->super.stroke_text = recordStrokeText;
        rec->super.fill_image = recordImage;
        rec->super.fill_image_mask = recordImageMask;
        dev = &rec->super;

        fz_run_page (ctx, page, dev, turned (bounds, turns), nullptr);
        fz_close_device (ctx, dev);
        if (quarterTurns < 0)
        {
            const auto* facing = rec->glyphsFacing;
            int most = 0;
            for (int k = 1; k < 4; ++k) if (facing[k] > facing[most]) most = k;
            const int all = facing[0] + facing[1] + facing[2] + facing[3];
            if (most != 0 && facing[most] * 2 > all) again = most;   // read it again, turned
        }
    }
    fz_always (ctx)
    {
        fz_drop_device (ctx, dev);
        fz_drop_page (ctx, page);
    }
    fz_catch (ctx)
        ok = false;
    if (ok && again > 0)
    {
        const int number = ink.number;
        ink = PageInk {};
        ink.number = number;
        return readPage (ctx, doc, index, ink, again);
    }
    return ok;
}
} // namespace

PdfContents readPdf (const std::string& path)
{
    PdfContents out;
    Context c;
    fz_context* ctx = c.ctx;
    if (ctx == nullptr) { out.error = "Out of memory."; return out; }

    fz_document* doc = openDocument (ctx, path, out.error);
    if (doc == nullptr) return out;

    char title[512] = { 0 };
    if (fz_lookup_metadata (ctx, doc, FZ_META_INFO_TITLE, title, sizeof title) > 0) out.title = title;

    int pages = 0;
    fz_var (pages);
    fz_try (ctx)
        pages = fz_count_pages (ctx, doc);
    fz_catch (ctx)
        pages = 0;

    out.pages.resize (static_cast<size_t> (std::max (0, pages)));
    for (int i = 0; i < pages; ++i)
    {
        PageInk& ink = out.pages[static_cast<size_t> (i)];
        ink.number = i;
        if (! readPage (ctx, doc, i, ink) && out.error.empty())
            out.error = "Page " + std::to_string (i + 1) + " could not be read.";
        removeDuplicates (ink);
    }
    fz_drop_document (ctx, doc);
    return out;
}

int pageCount (const std::string& path)
{
    Context c;
    std::string error;
    fz_document* doc = openDocument (c.ctx, path, error);
    if (doc == nullptr) return 0;
    int n = 0;
    fz_var (n);
    fz_try (c.ctx)
        n = fz_count_pages (c.ctx, doc);
    fz_catch (c.ctx)
        n = 0;
    fz_drop_document (c.ctx, doc);
    return n;
}

PageImage renderPage (const std::string& path, int pageIndex, double scale, std::string* error, int quarterTurns)
{
    PageImage out;
    Context c;
    fz_context* ctx = c.ctx;
    std::string err;
    fz_document* doc = openDocument (ctx, path, err);
    if (doc == nullptr) { if (error != nullptr) *error = err; return out; }

    fz_pixmap* pix = nullptr;
    fz_var (pix);
    fz_try (ctx)
    {
        const auto s = static_cast<float> (scale);
        fz_matrix m = fz_scale (s, s);
        if (quarterTurns != 0)
        {
            fz_page* page = fz_load_page (ctx, doc, pageIndex);
            fz_try (ctx)
                m = fz_concat (turned (fz_bound_page (ctx, page), quarterTurns), m);
            fz_always (ctx)
                fz_drop_page (ctx, page);
            fz_catch (ctx)
                fz_rethrow (ctx);
        }
        pix = fz_new_pixmap_from_page_number (ctx, doc, pageIndex, m, fz_device_rgb (ctx), 0);
        out.width = fz_pixmap_width (ctx, pix);
        out.height = fz_pixmap_height (ctx, pix);
        const int stride = fz_pixmap_stride (ctx, pix);
        const int n = fz_pixmap_components (ctx, pix);
        const unsigned char* samples = fz_pixmap_samples (ctx, pix);
        out.rgb.resize (static_cast<size_t> (out.width) * static_cast<size_t> (out.height) * 3);
        for (int y = 0; y < out.height; ++y)
            for (int x = 0; x < out.width; ++x)
                for (int k = 0; k < 3; ++k)
                    out.rgb[(static_cast<size_t> (y) * static_cast<size_t> (out.width) + static_cast<size_t> (x)) * 3 + static_cast<size_t> (k)]
                        = samples[y * stride + x * n + k];
    }
    fz_always (ctx)
        fz_drop_pixmap (ctx, pix);
    fz_catch (ctx)
    {
        if (error != nullptr) *error = fz_caught_message (ctx);
        out = PageImage {};
    }
    fz_drop_document (ctx, doc);
    return out;
}

} // namespace ans
