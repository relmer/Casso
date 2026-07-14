#pragma once

#include "Pch.h"

#include "Render/Dxui3DRenderer.h"




////////////////////////////////////////////////////////////////////////////////
//
//  Printer3DScene
//
//  The printer panel's 3D presentation (FR-032, research R-017): a procedural
//  ImageWriter-class printer body anchored at the bottom of the view, with
//  the fanfold content texture mapped onto a paper strip that rises from the
//  platen slot, leans gently back, and curls away out of view -- earlier
//  pages rolling off behind the printer. The paced head carriage (FR-034)
//  slides along the body's smoked front window as ink is laid down.
//
//  Everything is geometry built here at render time (no image assets): quads
//  with baked per-vertex lighting, drawn back-to-front through the scoped
//  Dxui3DRenderer from the panel window's before-present hook, so the scene
//  composites under the Dxui panel chrome. The printed CONTENT stays the flat
//  2D canvas the testable pipeline produced; this class only presents it.
//
////////////////////////////////////////////////////////////////////////////////

class Printer3DScene
{
public:
    Printer3DScene  () = default;
    ~Printer3DScene () = default;

    // Non-owning device/context -- pass the panel window's own (its swap chain
    // lives on that device, not the emulator renderer's).
    HRESULT  Initialize (ID3D11Device * device, ID3D11DeviceContext * context);

    bool     IsInitialized () const { return m_renderer.IsInitialized (); }

    // Install the user-authored CAD model of the printer (Wavefront OBJ+MTL
    // text, e.g. the embedded Tinkercad export). Replaces the procedural body
    // -- the paper, paced head, and Casso badge stay procedural and anchor
    // themselves to the loaded platen. On failure the scene keeps the
    // procedural body, so a bad model never blanks the panel.
    HRESULT  SetModel (const std::string & objText, const std::string & mtlText);

    bool     HasModel () const { return !m_mesh.empty (); }

    // Upload the composed fanfold canvas (premultiplied BGRA). Call when the
    // content changes; the paper mesh samples it every frame.
    HRESULT  SetContent (const uint32_t * bgra, int width, int height);

    // Paced head position across the printable width, 0..1 (FR-034).
    void     SetHeadColumn01 (float x01);

    // How much paper has physically fed PAST the head, as a fraction of the
    // content canvas (0 = fresh sheet, its leading edge aligned at the
    // strike line; 1 = a full page risen above the platen). The paper mesh
    // above the head is built only to this length, so a fresh sheet shows
    // no phantom blank page and the sheet visibly grows as printing feeds.
    void     SetPaperFeed01 (float feed01);

    // Draw the scene into the currently bound render target, restricted to
    // `targetPx` (window pixel coordinates). Call from the window's
    // before-present hook: the panel chrome paints over it afterwards.
    void     Render (const RECT & targetPx);

private:
    using Vertex = Dxui3DRenderer::Vertex;

    static void  AppendQuad (std::vector<Vertex> & out,
                             const float p00[3], const float p10[3],
                             const float p01[3], const float p11[3],
                             float u0, float v0, float u1, float v1,
                             uint32_t argb, float shade);

    // Axis-aligned front-face quad helper (z constant), plus rounded pieces:
    // an x-axis lathe (the platen end housings' rounded tops and the knob
    // barrel) and a y/z-plane fan at constant x (the housings' side caps).
    static void  AppendFaceQuad (std::vector<Vertex> & out,
                                 float x0, float x1, float y0, float y1, float z,
                                 uint32_t argb, float shade);
    static void  AppendLatheX   (std::vector<Vertex> & out,
                                 float x0, float x1, float cy, float cz, float radius,
                                 float a0, float a1, int segments, uint32_t argb);
    static void  AppendDiscX    (std::vector<Vertex> & out,
                                 float x, float cy, float cz, float radius,
                                 float a0, float a1, int segments,
                                 uint32_t argb, float shade);

    void  BuildBackdrop   (std::vector<Vertex> & out) const;
    void  BuildBodyBack   (std::vector<Vertex> & out) const;
    void  BuildPaper      (std::vector<Vertex> & out) const;
    void  BuildBodyFront  (std::vector<Vertex> & out) const;
    void  BuildControls   (std::vector<Vertex> & out) const;
    void  BuildCassoLogo  (std::vector<Vertex> & out, float faceZ) const;
    void  BuildHeadOverlay (std::vector<Vertex> & out) const;

    Dxui3DRenderer   m_renderer;
    float            m_head01        = 0.0f;
    float            m_paperFeed01   = 1.0f;
    int              m_contentWidth  = 0;
    int              m_contentHeight = 0;

    // The loaded CAD body (already remapped/scaled/re-grounded, lighting
    // baked); empty = procedural fallback. The platen and front-face anchors
    // are measured off the loaded geometry so the paper, paced head, and
    // badge land on the printer exactly where the user built its features.
    // Black-colored model parts become the translucent smoked window
    // (m_meshGlass), drawn last so the platen shows through it.
    std::vector<Vertex>   m_mesh;
    std::vector<Vertex>   m_meshGlass;
    float                 m_meshFrontZ  = 0.0f;   // front face plane (badge sits on it)
    float                 m_platenY     = 0.0f;   // roller axis
    float                 m_platenZ     = 0.0f;
    float                 m_platenR     = 0.0f;   // roller radius
    float                 m_paperStartY = 0.0f;   // paper emergence (from the platen)
    float                 m_paperZ      = 0.0f;
    float                 m_logoLeft    = 0.0f;   // badge placement on the front face
    float                 m_logoTopY    = 0.0f;

    // Scratch vertex lists, reused across frames to avoid per-frame churn.
    // Draw order is the painter's algorithm: backdrop, body behind the paper,
    // the paper itself, then the body front over its lower edge. (The loaded
    // mesh path instead uses the renderer's real depth buffer.)
    std::vector<Vertex>   m_backdrop;
    std::vector<Vertex>   m_solidBack;
    std::vector<Vertex>   m_paper;
    std::vector<Vertex>   m_solidFront;
};
