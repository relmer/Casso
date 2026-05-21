#include "Pch.h"

#include "RmlFontEngine_DWrite.h"


#pragma comment (lib, "dwrite.lib")





////////////////////////////////////////////////////////////////////////////////
//
//  Custom IDWriteFontFileLoader for the in-memory LoadFontFace
//  overload. Each loaded blob is given a fresh stream that points
//  at a vector<byte> owned by the FaceData; the loader and stream
//  classes live for the lifetime of the engine.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    class MemoryFontFileStream : public IDWriteFontFileStream
    {
    public:
        MemoryFontFileStream (const Rml::byte * data, size_t size)
            : m_data (data), m_size (size)
        {
        }

        ULONG   STDMETHODCALLTYPE AddRef  () override { return InterlockedIncrement (&m_ref); }
        ULONG   STDMETHODCALLTYPE Release() override
        {
            ULONG v = InterlockedDecrement (&m_ref);
            if (v == 0) { delete this; }
            return v;
        }
        HRESULT STDMETHODCALLTYPE QueryInterface (REFIID iid, void ** ppv) override
        {
            if (iid == __uuidof (IUnknown) || iid == __uuidof (IDWriteFontFileStream))
            {
                *ppv = this; AddRef(); return S_OK;
            }
            *ppv = nullptr; return E_NOINTERFACE;
        }

        HRESULT STDMETHODCALLTYPE ReadFileFragment (
            void const ** fragmentStart, UINT64 fileOffset, UINT64 fragmentSize, void ** fragmentContext) override
        {
            if (fileOffset + fragmentSize > m_size) { return E_INVALIDARG; }
            *fragmentStart   = m_data + fileOffset;
            *fragmentContext = nullptr;
            return S_OK;
        }
        void STDMETHODCALLTYPE ReleaseFileFragment (void * /*fragmentContext*/) override {}

        HRESULT STDMETHODCALLTYPE GetFileSize       (UINT64 * fileSize)     override { *fileSize = m_size; return S_OK; }
        HRESULT STDMETHODCALLTYPE GetLastWriteTime  (UINT64 * lastWrite)    override { *lastWrite = 0; return S_OK; }

    private:
        const Rml::byte * m_data = nullptr;
        size_t            m_size = 0;
        LONG              m_ref  = 1;
    };


    class MemoryFontFileLoader : public IDWriteFontFileLoader
    {
    public:
        ULONG   STDMETHODCALLTYPE AddRef  () override { return InterlockedIncrement (&m_ref); }
        ULONG   STDMETHODCALLTYPE Release() override
        {
            ULONG v = InterlockedDecrement (&m_ref);
            if (v == 0) { delete this; }
            return v;
        }
        HRESULT STDMETHODCALLTYPE QueryInterface (REFIID iid, void ** ppv) override
        {
            if (iid == __uuidof (IUnknown) || iid == __uuidof (IDWriteFontFileLoader))
            {
                *ppv = this; AddRef(); return S_OK;
            }
            *ppv = nullptr; return E_NOINTERFACE;
        }

        HRESULT STDMETHODCALLTYPE CreateStreamFromKey (
            void const         * fontFileReferenceKey,
            UINT32               fontFileReferenceKeySize,
            IDWriteFontFileStream ** fontFileStream) override
        {
            if (fontFileReferenceKeySize != sizeof (Key)) { return E_INVALIDARG; }
            const Key * key = static_cast<const Key *> (fontFileReferenceKey);
            *fontFileStream = new MemoryFontFileStream (key->data, key->size);
            return S_OK;
        }

        struct Key { const Rml::byte * data; size_t size; };

    private:
        LONG m_ref = 1;
    };
} // namespace





////////////////////////////////////////////////////////////////////////////////
//
//  RmlFontEngine_DWrite
//
////////////////////////////////////////////////////////////////////////////////

RmlFontEngine_DWrite::RmlFontEngine_DWrite()
{
}



RmlFontEngine_DWrite::~RmlFontEngine_DWrite()
{
}



void RmlFontEngine_DWrite::Initialize()
{
    // RmlUi invokes Initialize after Rml::Initialise wires us in. We
    // don't create the DWrite factory eagerly here — it's deferred
    // until the first LoadFontFace so applications that never use
    // text don't pay for it.
}



void RmlFontEngine_DWrite::Shutdown()
{
    ReleaseFontResources();
    m_faces.clear();
    m_gdiInterop.Reset();
    m_factory.Reset();
}



void RmlFontEngine_DWrite::ReleaseFontResources()
{
    m_slots.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureDWriteFactory
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RmlFontEngine_DWrite::EnsureDWriteFactory()
{
    if (m_factory != nullptr)
    {
        return S_OK;
    }

    HRESULT hr = DWriteCreateFactory (DWRITE_FACTORY_TYPE_SHARED,
                                      __uuidof (IDWriteFactory3),
                                      reinterpret_cast<IUnknown **> (m_factory.GetAddressOf()));

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PopulateFaceFromIDWriteFontFile
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RmlFontEngine_DWrite::PopulateFaceFromIDWriteFontFile (
    IDWriteFontFile           * file,
    int                         face_index,
    std::unique_ptr<FaceData> & outFace)
{
    HRESULT hr = S_OK;

    BOOL                     isSupported = FALSE;
    DWRITE_FONT_FILE_TYPE    fileType    = DWRITE_FONT_FILE_TYPE_UNKNOWN;
    DWRITE_FONT_FACE_TYPE    faceType    = DWRITE_FONT_FACE_TYPE_UNKNOWN;
    UINT32                   numFaces    = 0;

    hr = file->Analyze (&isSupported, &fileType, &faceType, &numFaces);
    if (FAILED (hr) || !isSupported || numFaces == 0)
    {
        return FAILED (hr) ? hr : E_FAIL;
    }

    UINT32 useIndex = static_cast<UINT32> (face_index);
    if (useIndex >= numFaces) { useIndex = 0; }

    IDWriteFontFile * files[1] = { file };

    ComPtr<IDWriteFontFace>  baseFace;
    hr = m_factory->CreateFontFace (faceType, 1, files, useIndex, DWRITE_FONT_SIMULATIONS_NONE, &baseFace);
    if (FAILED (hr)) { return hr; }

    ComPtr<IDWriteFontFace3> face3;
    hr = baseFace.As (&face3);
    if (FAILED (hr)) { return hr; }

    auto fd = std::make_unique<FaceData> ();
    fd->face = face3;

    face3->GetMetrics (&fd->designMetrics);

    // Pull a sensible family name. GetFamilyNames may not be set on
    // every face; fall back to a synthetic "rmlui-face-N" name.
    ComPtr<IDWriteLocalizedStrings> familyNames;
    if (SUCCEEDED (face3->GetFamilyNames (&familyNames)) && familyNames != nullptr)
    {
        UINT32 count  = familyNames->GetCount();
        UINT32 idx    = 0;
        UINT32 length = 0;

        if (count > 0 && SUCCEEDED (familyNames->GetStringLength (idx, &length)) && length > 0)
        {
            std::wstring buf;
            buf.resize (length);
            if (SUCCEEDED (familyNames->GetString (idx, buf.data(), length + 1)))
            {
                fd->family = buf;
            }
        }
    }

    if (fd->family.empty())
    {
        fd->family = L"rmlui-face-" + std::to_wstring (m_faces.size());
    }

    DWRITE_FONT_STYLE   dwStyle  = face3->GetStyle  ();
    DWRITE_FONT_WEIGHT  dwWeight = face3->GetWeight();

    fd->style = (dwStyle == DWRITE_FONT_STYLE_NORMAL)
        ? Rml::Style::FontStyle::Normal
        : Rml::Style::FontStyle::Italic;
    fd->weight = static_cast<Rml::Style::FontWeight> (static_cast<int> (dwWeight));

    outFace = std::move (fd);
    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadFontFace (file path)
//
////////////////////////////////////////////////////////////////////////////////

bool RmlFontEngine_DWrite::LoadFontFace (
    const Rml::String      & file_name,
    int                      face_index,
    bool                     fallback_face,
    Rml::Style::FontWeight   weight)
{
    if (FAILED (EnsureDWriteFactory()))
    {
        return false;
    }

    // RmlUi gives us UTF-8. CreateFontFileReference wants wide.
    int wlen = MultiByteToWideChar (CP_UTF8, 0, file_name.c_str(), -1, nullptr, 0);
    if (wlen <= 0) { return false; }

    std::wstring wpath;
    wpath.resize (static_cast<size_t> (wlen - 1));
    MultiByteToWideChar (CP_UTF8, 0, file_name.c_str(), -1, wpath.data(), wlen);

    ComPtr<IDWriteFontFile> file;
    if (FAILED (m_factory->CreateFontFileReference (wpath.c_str(), nullptr, &file)))
    {
        return false;
    }

    std::unique_ptr<FaceData> fd;
    if (FAILED (PopulateFaceFromIDWriteFontFile (file.Get(), face_index, fd)))
    {
        return false;
    }

    fd->fallback = fallback_face;
    if (weight != Rml::Style::FontWeight::Auto)
    {
        fd->weight = weight;
    }

    m_faces.push_back (std::move (fd));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadFontFace (memory)
//
////////////////////////////////////////////////////////////////////////////////

bool RmlFontEngine_DWrite::LoadFontFace (
    Rml::Span<const Rml::byte>  data,
    int                          face_index,
    const Rml::String          & family,
    Rml::Style::FontStyle        style,
    Rml::Style::FontWeight       weight,
    bool                          fallback_face)
{
    if (data.empty())                              { return false; }
    if (FAILED (EnsureDWriteFactory()))            { return false; }

    // Copy the buffer so we own its lifetime (the spec says callers
    // must keep `data` alive until shutdown, but copying isolates us
    // from any caller-side bugs).
    auto fd = std::make_unique<FaceData> ();
    fd->memoryHold.assign (data.data(), data.data() + data.size());

    // One loader is enough for the lifetime of the engine; we just
    // re-register on every call. Registration is idempotent for the
    // same loader pointer.
    static MemoryFontFileLoader * s_loader = nullptr;
    if (s_loader == nullptr)
    {
        s_loader = new MemoryFontFileLoader();
        if (FAILED (m_factory->RegisterFontFileLoader (s_loader)))
        {
            return false;
        }
    }

    MemoryFontFileLoader::Key key = { fd->memoryHold.data(), fd->memoryHold.size() };

    ComPtr<IDWriteFontFile> file;
    if (FAILED (m_factory->CreateCustomFontFileReference (&key, sizeof (key), s_loader, &file)))
    {
        return false;
    }

    std::unique_ptr<FaceData> populated;
    if (FAILED (PopulateFaceFromIDWriteFontFile (file.Get(), face_index, populated)))
    {
        return false;
    }

    // Caller-supplied identification trumps anything we derived from
    // the face itself.
    populated->memoryHold = std::move (fd->memoryHold);
    populated->fallback   = fallback_face;
    populated->style      = style;

    if (weight != Rml::Style::FontWeight::Auto)
    {
        populated->weight = weight;
    }

    if (!family.empty())
    {
        int wlen = MultiByteToWideChar (CP_UTF8, 0, family.c_str(), -1, nullptr, 0);
        if (wlen > 0)
        {
            std::wstring wfam;
            wfam.resize (static_cast<size_t> (wlen - 1));
            MultiByteToWideChar (CP_UTF8, 0, family.c_str(), -1, wfam.data(), wlen);
            populated->family = wfam;
        }
    }

    m_faces.push_back (std::move (populated));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindRegisteredFace
//
////////////////////////////////////////////////////////////////////////////////

RmlFontEngine_DWrite::FaceData * RmlFontEngine_DWrite::FindRegisteredFace (
    const Rml::String       & family,
    Rml::Style::FontStyle     style,
    Rml::Style::FontWeight    weight) const
{
    // Convert family to wide once.
    std::wstring wfam;
    int wlen = MultiByteToWideChar (CP_UTF8, 0, family.c_str(), -1, nullptr, 0);
    if (wlen > 0)
    {
        wfam.resize (static_cast<size_t> (wlen - 1));
        MultiByteToWideChar (CP_UTF8, 0, family.c_str(), -1, wfam.data(), wlen);
    }

    // Exact match first.
    for (auto & fd : m_faces)
    {
        if (fd->family == wfam && fd->style == style && fd->weight == weight)
        {
            return fd.get();
        }
    }

    // Match family + style, ignore weight.
    for (auto & fd : m_faces)
    {
        if (fd->family == wfam && fd->style == style)
        {
            return fd.get();
        }
    }

    // Match family alone.
    for (auto & fd : m_faces)
    {
        if (fd->family == wfam)
        {
            return fd.get();
        }
    }

    // Last-ditch: any loaded face.
    if (!m_faces.empty())
    {
        return m_faces.front().get();
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetFontFaceHandle
//
////////////////////////////////////////////////////////////////////////////////

Rml::FontFaceHandle RmlFontEngine_DWrite::GetFontFaceHandle (
    const Rml::String       & family,
    Rml::Style::FontStyle     style,
    Rml::Style::FontWeight    weight,
    int                       size)
{
    FaceData * fd = FindRegisteredFace (family, style, weight);
    if (fd == nullptr) { return 0; }

    // De-duplicate slots: if we already have one for this face at
    // this size, hand it back.
    for (auto & slot : m_slots)
    {
        if (slot->faceData == fd && slot->pixelSize == size)
        {
            return reinterpret_cast<Rml::FontFaceHandle> (slot.get());
        }
    }

    auto slot = std::make_unique<FaceSlot> ();
    slot->faceData  = fd;
    slot->pixelSize = (size > 0) ? size : 16;
    slot->atlas.assign (static_cast<size_t> (FaceSlot::kAtlasW) * static_cast<size_t> (FaceSlot::kAtlasH) * 4,
                        0);

    const float upem = static_cast<float> (fd->designMetrics.designUnitsPerEm);
    slot->pixelsPerDesignUnit = (upem > 0.0f)
        ? static_cast<float> (slot->pixelSize) / upem
        : 1.0f;

    const float dpu = slot->pixelsPerDesignUnit;
    slot->ascentPx  = fd->designMetrics.ascent * dpu;

    slot->metrics.size                = slot->pixelSize;
    slot->metrics.ascent              = fd->designMetrics.ascent             * dpu;
    slot->metrics.descent             = fd->designMetrics.descent            * dpu;
    slot->metrics.line_spacing        = (fd->designMetrics.ascent +
                                         fd->designMetrics.descent +
                                         fd->designMetrics.lineGap)          * dpu;
    slot->metrics.x_height            = fd->designMetrics.xHeight            * dpu;
    slot->metrics.underline_position  = -fd->designMetrics.underlinePosition * dpu;
    slot->metrics.underline_thickness = fd->designMetrics.underlineThickness * dpu;
    slot->metrics.has_ellipsis        = false;

    Rml::FontFaceHandle handle = reinterpret_cast<Rml::FontFaceHandle> (slot.get());
    m_slots.push_back (std::move (slot));
    return handle;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrepareFontEffects
//
////////////////////////////////////////////////////////////////////////////////

Rml::FontEffectsHandle RmlFontEngine_DWrite::PrepareFontEffects (
    Rml::FontFaceHandle         /*handle*/,
    const Rml::FontEffectList & /*font_effects*/)
{
    // No effect support in the v1 DWrite engine.
    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetFontMetrics
//
////////////////////////////////////////////////////////////////////////////////

const Rml::FontMetrics & RmlFontEngine_DWrite::GetFontMetrics (Rml::FontFaceHandle handle)
{
    auto * slot = reinterpret_cast<FaceSlot *> (handle);
    if (slot != nullptr)
    {
        m_lastMetrics = slot->metrics;
    }
    return m_lastMetrics;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DecodeUtf8
//
//  Returns the next code point in [p, end). Advances p. Returns 0
//  and parks p at end on exhaustion. Malformed sequences are
//  reported as Replacement (U+FFFD) and consume one byte.
//
////////////////////////////////////////////////////////////////////////////////

char32_t RmlFontEngine_DWrite::DecodeUtf8 (const char * & p, const char * end)
{
    if (p >= end) { return 0; }

    unsigned char c0 = static_cast<unsigned char> (*p++);

    if (c0 < 0x80) { return c0; }

    auto tail = [&] (int n, char32_t cp) -> char32_t
    {
        for (int i = 0; i < n; ++i)
        {
            if (p >= end) { return 0xFFFD; }
            unsigned char b = static_cast<unsigned char> (*p++);
            if ((b & 0xC0) != 0x80) { return 0xFFFD; }
            cp = (cp << 6) | (b & 0x3F);
        }
        return cp;
    };

    if ((c0 & 0xE0) == 0xC0) { return tail (1, c0 & 0x1F); }
    if ((c0 & 0xF0) == 0xE0) { return tail (2, c0 & 0x0F); }
    if ((c0 & 0xF8) == 0xF0) { return tail (3, c0 & 0x07); }

    return 0xFFFD;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureGlyph
//
//  Lazily rasterises the requested code point into the slot's atlas
//  via IDWriteGlyphRunAnalysis::CreateAlphaTexture (DWRITE_TEXTURE_
//  ALIASED_1x1 — single-byte alpha). The byte is replicated into
//  RGBA8 (R=G=B=255, A=alpha) so the textured shader's pre-multiplied
//  source-over blend matches glyph appearance against any background.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RmlFontEngine_DWrite::EnsureGlyph (FaceSlot & slot, char32_t codepoint)
{
    if (slot.glyphs.count (codepoint) != 0)
    {
        return S_OK;
    }

    GlyphInfo gi;

    UINT32 cp     = static_cast<UINT32> (codepoint);
    UINT16 glyph  = 0;

    HRESULT hr = slot.faceData->face->GetGlyphIndices (&cp, 1, &glyph);
    if (FAILED (hr) || glyph == 0)
    {
        // Unknown glyph: still cache with zero advance so we don't
        // re-attempt every frame.
        slot.glyphs[codepoint] = gi;
        return S_OK;
    }

    DWRITE_GLYPH_METRICS gm = {};
    hr = slot.faceData->face->GetDesignGlyphMetrics (&glyph, 1, &gm, FALSE);
    if (FAILED (hr))
    {
        slot.glyphs[codepoint] = gi;
        return hr;
    }

    const float dpu = slot.pixelsPerDesignUnit;
    gi.advanceX = gm.advanceWidth * dpu;

    // Build a synthetic glyph run anchored at (0, 0) so the analysis
    // bounds come back in glyph-local pixel coordinates.
    FLOAT  advanceZero  = 0.0f;
    DWRITE_GLYPH_OFFSET glyphOffsetZero = { 0.0f, 0.0f };

    DWRITE_GLYPH_RUN run = {};
    run.fontFace      = slot.faceData->face.Get();
    run.fontEmSize    = static_cast<FLOAT> (slot.pixelSize);
    run.glyphCount    = 1;
    run.glyphIndices  = &glyph;
    run.glyphAdvances = &advanceZero;
    run.glyphOffsets  = &glyphOffsetZero;
    run.isSideways    = FALSE;
    run.bidiLevel     = 0;

    ComPtr<IDWriteGlyphRunAnalysis> analysis;
    hr = m_factory->CreateGlyphRunAnalysis (&run,
                                            1.0f,                              // pixelsPerDip
                                            nullptr,                           // transform
                                            DWRITE_RENDERING_MODE_NATURAL,
                                            DWRITE_MEASURING_MODE_NATURAL,
                                            0.0f, 0.0f,                        // baseline origin
                                            &analysis);

    if (FAILED (hr))
    {
        slot.glyphs[codepoint] = gi;
        return hr;
    }

    RECT bounds = {};
    hr = analysis->GetAlphaTextureBounds (DWRITE_TEXTURE_ALIASED_1x1, &bounds);
    if (FAILED (hr))
    {
        slot.glyphs[codepoint] = gi;
        return hr;
    }

    int gw = bounds.right  - bounds.left;
    int gh = bounds.bottom - bounds.top;

    if (gw <= 0 || gh <= 0)
    {
        // Whitespace etc. Still record advance so the cursor moves.
        gi.rasterized = true;
        slot.glyphs[codepoint] = gi;
        return S_OK;
    }

    // Next-fit row pack.
    if (slot.cursorX + gw > FaceSlot::kAtlasW)
    {
        slot.cursorX   = 0;
        slot.cursorY  += slot.rowHeight + 1;
        slot.rowHeight = 0;
    }

    if (slot.cursorY + gh > FaceSlot::kAtlasH)
    {
        // Atlas full. Treat as missing glyph (advance still recorded
        // above so layout doesn't desync). A more elaborate engine
        // would allocate a second page; we accept the truncation in v1.
        slot.glyphs[codepoint] = gi;
        return S_OK;
    }

    std::vector<Rml::byte> alpha (static_cast<size_t> (gw) * gh, 0);
    hr = analysis->CreateAlphaTexture (DWRITE_TEXTURE_ALIASED_1x1,
                                       &bounds,
                                       alpha.data(),
                                       static_cast<UINT32> (alpha.size()));

    if (FAILED (hr))
    {
        slot.glyphs[codepoint] = gi;
        return hr;
    }

    // Stamp into atlas as premultiplied white (R=G=B=A=alpha). RmlUi
    // multiplies by the per-vertex colour in the textured shader, so
    // this works for any text colour.
    for (int y = 0; y < gh; ++y)
    {
        for (int x = 0; x < gw; ++x)
        {
            Rml::byte a = alpha[static_cast<size_t> (y) * gw + x];

            size_t dstX = static_cast<size_t> (slot.cursorX + x);
            size_t dstY = static_cast<size_t> (slot.cursorY + y);
            size_t dst  = (dstY * FaceSlot::kAtlasW + dstX) * 4;

            slot.atlas[dst + 0] = a;
            slot.atlas[dst + 1] = a;
            slot.atlas[dst + 2] = a;
            slot.atlas[dst + 3] = a;
        }
    }

    gi.atlasX     = slot.cursorX;
    gi.atlasY     = slot.cursorY;
    gi.width      = gw;
    gi.height     = gh;
    gi.bearingX   = bounds.left;
    gi.bearingY   = bounds.top;   // relative to baseline; negative = above
    gi.rasterized = true;

    slot.cursorX  += gw + 1;
    if (gh > slot.rowHeight) { slot.rowHeight = gh; }

    slot.glyphs[codepoint] = gi;
    slot.version          += 1;

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetStringWidth
//
////////////////////////////////////////////////////////////////////////////////

int RmlFontEngine_DWrite::GetStringWidth (
    Rml::FontFaceHandle              handle,
    Rml::StringView                  string,
    const Rml::TextShapingContext  & text_shaping_context,
    Rml::Character                   /*prior_character*/)
{
    auto * slot = reinterpret_cast<FaceSlot *> (handle);
    if (slot == nullptr) { return 0; }

    float       total = 0.0f;
    const char * p    = string.begin();
    const char * end  = string.end   ();

    while (p < end)
    {
        char32_t cp = DecodeUtf8 (p, end);
        if (cp == 0) { break; }

        EnsureGlyph (*slot, cp);
        auto it = slot->glyphs.find (cp);
        if (it != slot->glyphs.end())
        {
            total += it->second.advanceX + text_shaping_context.letter_spacing;
        }
    }

    return static_cast<int> (total + 0.5f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GenerateString
//
////////////////////////////////////////////////////////////////////////////////

int RmlFontEngine_DWrite::GenerateString (
    Rml::RenderManager             & render_manager,
    Rml::FontFaceHandle              face_handle,
    Rml::FontEffectsHandle           /*font_effects_handle*/,
    Rml::StringView                  string,
    Rml::Vector2f                    position,
    Rml::ColourbPremultiplied        colour,
    float                            /*opacity*/,
    const Rml::TextShapingContext  & text_shaping_context,
    Rml::TexturedMeshList          & mesh_list)
{
    mesh_list.clear();

    auto * slot = reinterpret_cast<FaceSlot *> (face_handle);
    if (slot == nullptr) { return 0; }

    Rml::Mesh mesh;
    mesh.vertices.reserve (string.size() * 4);
    mesh.indices.reserve  (string.size() * 6);

    float        pen   = position.x;
    const float  baseY = position.y;
    const char * p     = string.begin();
    const char * end   = string.end   ();

    while (p < end)
    {
        char32_t cp = DecodeUtf8 (p, end);
        if (cp == 0) { break; }

        EnsureGlyph (*slot, cp);
        auto it = slot->glyphs.find (cp);
        if (it == slot->glyphs.end())
        {
            continue;
        }

        const GlyphInfo & gi = it->second;

        if (gi.width > 0 && gi.height > 0)
        {
            float x0 = pen + static_cast<float> (gi.bearingX);
            float y0 = baseY + static_cast<float> (gi.bearingY);
            float x1 = x0 + static_cast<float> (gi.width);
            float y1 = y0 + static_cast<float> (gi.height);

            float u0 = static_cast<float> (gi.atlasX)              / static_cast<float> (FaceSlot::kAtlasW);
            float v0 = static_cast<float> (gi.atlasY)              / static_cast<float> (FaceSlot::kAtlasH);
            float u1 = static_cast<float> (gi.atlasX + gi.width)   / static_cast<float> (FaceSlot::kAtlasW);
            float v1 = static_cast<float> (gi.atlasY + gi.height)  / static_cast<float> (FaceSlot::kAtlasH);

            int baseIndex = static_cast<int> (mesh.vertices.size());

            mesh.vertices.push_back ({ Rml::Vector2f (x0, y0), colour, Rml::Vector2f (u0, v0) });
            mesh.vertices.push_back ({ Rml::Vector2f (x1, y0), colour, Rml::Vector2f (u1, v0) });
            mesh.vertices.push_back ({ Rml::Vector2f (x1, y1), colour, Rml::Vector2f (u1, v1) });
            mesh.vertices.push_back ({ Rml::Vector2f (x0, y1), colour, Rml::Vector2f (u0, v1) });

            mesh.indices.push_back (baseIndex + 0);
            mesh.indices.push_back (baseIndex + 1);
            mesh.indices.push_back (baseIndex + 2);
            mesh.indices.push_back (baseIndex + 0);
            mesh.indices.push_back (baseIndex + 2);
            mesh.indices.push_back (baseIndex + 3);
        }

        pen += gi.advanceX + text_shaping_context.letter_spacing;
    }

    // Build/refresh the slot's callback-texture wrapper so RmlUi
    // sees the latest atlas bytes on the next render pass. We
    // re-seed the source whenever the atlas mutation version
    // outruns the version that the cached source was built against;
    // RmlUi's mesh cache is keyed on GetVersion(handle), so when we
    // refresh here it also re-queries GenerateString.
    if (slot->texSourceVersion != slot->version)
    {
        Rml::byte * atlasBytes = slot->atlas.data();
        size_t      atlasSize  = slot->atlas.size();

        slot->texSource = Rml::CallbackTextureSource ([atlasBytes, atlasSize] (const Rml::CallbackTextureInterface & iface) -> bool
            {
                Rml::Span<const Rml::byte> span (atlasBytes, atlasSize);
                return iface.GenerateTexture (span, Rml::Vector2i (FaceSlot::kAtlasW, FaceSlot::kAtlasH));
            });

        slot->texSourceVersion = slot->version;
    }

    Rml::TexturedMesh tm;
    tm.mesh    = std::move (mesh);
    tm.texture = slot->texSource.GetTexture (render_manager);
    mesh_list.push_back (std::move (tm));

    return static_cast<int> (pen - position.x + 0.5f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetVersion
//
////////////////////////////////////////////////////////////////////////////////

int RmlFontEngine_DWrite::GetVersion (Rml::FontFaceHandle handle)
{
    auto * slot = reinterpret_cast<FaceSlot *> (handle);
    return (slot != nullptr) ? slot->version : 0;
}
