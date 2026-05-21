#pragma once

#include "Pch.h"







////////////////////////////////////////////////////////////////////////////////
//
//  RmlFontEngine_DWrite
//
//  Custom Rml::FontEngineInterface backed by DirectWrite. The
//  default RmlUi font engine uses FreeType, which we don't vendor;
//  we substitute DirectWrite + an in-memory grayscale atlas to keep
//  the dependency surface limited to Windows SDK components already
//  required by Casso's D3D11 path.
//
//  Strategy
//  --------
//      * LoadFontFace (file path)  -> IDWriteFactory::CreateFontFile
//                                     Reference + CreateFontFace; the
//                                     family/style/weight are derived
//                                     from the face itself via
//                                     IDWriteFontFace3::GetFamilyNames.
//      * LoadFontFace (memory)     -> custom IDWriteFontFileLoader
//                                     that wraps the byte buffer.
//      * GetFontFaceHandle (...)   -> looks up the registered face
//                                     by (family, style, weight) and
//                                     allocates a per-size "face slot"
//                                     that owns the glyph atlas.
//      * GenerateString            -> iterates the UTF-8 view as
//                                     code points, lazy-rasterises
//                                     each glyph via
//                                     IDWriteGlyphRunAnalysis::Create
//                                     AlphaTexture (A8 → RGBA8), and
//                                     emits one TexturedMesh per
//                                     face slot.
//      * GetVersion (handle)       -> bumped every time a new glyph
//                                     is added to the slot's atlas
//                                     so RmlUi re-queries the mesh
//                                     and picks up the refreshed
//                                     callback texture.
//
//  Scope notes
//  -----------
//  This is a deliberately minimal implementation suitable for the
//  P3 RmlUi shell-boot milestone:
//      * fixed 1024x1024 atlas per face slot (next-fit row packing)
//      * grayscale alpha rendering (no ClearType subpixel layout)
//      * no font effects (PrepareFontEffects returns 0)
//      * no kerning beyond what DWrite gives via design advance
//      * no fallback font chaining (`fallback_face` is accepted but
//        not actively walked when a glyph is missing)
//  Each of these can be lifted in a later phase without touching the
//  interface boundary.
//
////////////////////////////////////////////////////////////////////////////////

class RmlFontEngine_DWrite : public Rml::FontEngineInterface
{
public:
    RmlFontEngine_DWrite();
    ~RmlFontEngine_DWrite() override;

    void Initialize() override;
    void Shutdown   () override;

    bool LoadFontFace (
        const Rml::String  & file_name,
        int                  face_index,
        bool                 fallback_face,
        Rml::Style::FontWeight weight) override;

    bool LoadFontFace (
        Rml::Span<const Rml::byte>  data,
        int                          face_index,
        const Rml::String          & family,
        Rml::Style::FontStyle        style,
        Rml::Style::FontWeight       weight,
        bool                          fallback_face) override;

    Rml::FontFaceHandle GetFontFaceHandle (
        const Rml::String       & family,
        Rml::Style::FontStyle     style,
        Rml::Style::FontWeight    weight,
        int                       size) override;

    Rml::FontEffectsHandle PrepareFontEffects (
        Rml::FontFaceHandle         handle,
        const Rml::FontEffectList & font_effects) override;

    const Rml::FontMetrics & GetFontMetrics (Rml::FontFaceHandle handle) override;

    int GetStringWidth (
        Rml::FontFaceHandle              handle,
        Rml::StringView                  string,
        const Rml::TextShapingContext  & text_shaping_context,
        Rml::Character                   prior_character = Rml::Character::Null) override;

    int GenerateString (
        Rml::RenderManager             & render_manager,
        Rml::FontFaceHandle              face_handle,
        Rml::FontEffectsHandle           font_effects_handle,
        Rml::StringView                  string,
        Rml::Vector2f                    position,
        Rml::ColourbPremultiplied        colour,
        float                            opacity,
        const Rml::TextShapingContext  & text_shaping_context,
        Rml::TexturedMeshList          & mesh_list) override;

    int  GetVersion          (Rml::FontFaceHandle handle) override;

    void ReleaseFontResources() override;

private:
    // Per registered face (a single weight/style/family combination
    // loaded from disk or memory). Owns the IDWriteFontFace3.
    struct FaceData
    {
        ComPtr<IDWriteFontFace3>   face;
        std::wstring               family;
        Rml::Style::FontStyle      style       = Rml::Style::FontStyle::Normal;
        Rml::Style::FontWeight     weight      = Rml::Style::FontWeight::Normal;
        bool                       fallback    = false;

        // Cached design metrics (do not change with size).
        DWRITE_FONT_METRICS1       designMetrics = {};

        // Optional: keep ownership of in-memory font bytes alive.
        std::vector<Rml::byte>     memoryHold;
    };

    // Per (face × pixel-size) glyph atlas + metrics cache. Pointer
    // is the value handed back via FontFaceHandle.
    struct GlyphInfo
    {
        // Atlas-space sub-rectangle (pixels)
        int  atlasX     = 0;
        int  atlasY     = 0;
        int  width      = 0;
        int  height     = 0;

        // Pen offset (pixels) from the requested baseline-origin
        // point to the top-left of the rasterised bitmap.
        int  bearingX   = 0;
        int  bearingY   = 0;

        // Horizontal advance in pixels.
        float advanceX  = 0.0f;

        bool rasterized = false;  // true once atlas has the bitmap
    };

    struct FaceSlot
    {
        FaceData * faceData = nullptr;
        int        pixelSize = 16;

        Rml::FontMetrics  metrics  = {};
        float             pixelsPerDesignUnit = 0.0f;
        float             ascentPx  = 0.0f;

        // 1024x1024 RGBA8 software atlas. Premultiplied alpha.
        static constexpr int kAtlasW = 1024;
        static constexpr int kAtlasH = 1024;
        std::vector<Rml::byte>  atlas;

        // Next-fit row packer.
        int  cursorX   = 0;
        int  cursorY   = 0;
        int  rowHeight = 0;

        std::unordered_map<char32_t, GlyphInfo>  glyphs;

        int version = 1;  // bumped on each atlas mutation

        // Cached callback-texture wrapper. Invalidated (reassigned)
        // every time `version` bumps so RmlUi picks up the fresh
        // atlas bytes on its next GenerateString re-query.
        Rml::CallbackTextureSource  texSource;
        int                          texSourceVersion = 0;
    };

    HRESULT EnsureDWriteFactory();

    FaceData * FindRegisteredFace (
        const Rml::String       & family,
        Rml::Style::FontStyle     style,
        Rml::Style::FontWeight    weight) const;

    HRESULT PopulateFaceFromIDWriteFontFile (
        IDWriteFontFile         * file,
        int                       face_index,
        std::unique_ptr<FaceData> & outFace);

    HRESULT EnsureGlyph (FaceSlot & slot, char32_t codepoint);

    static char32_t DecodeUtf8 (const char * & p, const char * end);

    ComPtr<IDWriteFactory3>             m_factory;
    ComPtr<IDWriteGdiInterop>           m_gdiInterop;

    std::vector<std::unique_ptr<FaceData>>  m_faces;
    std::vector<std::unique_ptr<FaceSlot>>  m_slots;

    // Returned to RmlUi from GetFontMetrics; kept stable across calls.
    Rml::FontMetrics  m_lastMetrics = {};
};
