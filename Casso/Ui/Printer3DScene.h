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

    // Upload the composed fanfold canvas (premultiplied BGRA). Call when the
    // content changes; the paper mesh samples it every frame.
    HRESULT  SetContent (const uint32_t * bgra, int width, int height);

    // Paced head position across the printable width, 0..1 (FR-034).
    void     SetHeadColumn01 (float x01);

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

    // Axis-aligned front-face quad helper (z constant), plus fake cylinders:
    // an x-axis lathe (the platen knob), a y-axis lathe (the rounded platen
    // end towers), and a horizontal disc fan (the towers' top caps).
    static void  AppendFaceQuad (std::vector<Vertex> & out,
                                 float x0, float x1, float y0, float y1, float z,
                                 uint32_t argb, float shade);
    static void  AppendLatheX   (std::vector<Vertex> & out,
                                 float x0, float x1, float cy, float cz, float radius,
                                 float a0, float a1, int segments, uint32_t argb);
    static void  AppendLatheY   (std::vector<Vertex> & out,
                                 float cx, float cz, float radius, float y0, float y1,
                                 float a0, float a1, int segments, uint32_t argb);
    static void  AppendDiscY    (std::vector<Vertex> & out,
                                 float cx, float cz, float radius, float y,
                                 int segments, uint32_t argb, float shade);

    void  BuildBackdrop   (std::vector<Vertex> & out) const;
    void  BuildBodyBack   (std::vector<Vertex> & out) const;
    void  BuildPaper      (std::vector<Vertex> & out) const;
    void  BuildBodyFront  (std::vector<Vertex> & out) const;
    void  BuildControls   (std::vector<Vertex> & out) const;
    void  BuildCassoLogo  (std::vector<Vertex> & out) const;

    Dxui3DRenderer   m_renderer;
    float            m_head01        = 0.0f;
    int              m_contentWidth  = 0;
    int              m_contentHeight = 0;

    // Scratch vertex lists, reused across frames to avoid per-frame churn.
    // Draw order is the painter's algorithm: backdrop, body behind the paper,
    // the paper itself, then the body front over its lower edge.
    std::vector<Vertex>   m_backdrop;
    std::vector<Vertex>   m_solidBack;
    std::vector<Vertex>   m_paper;
    std::vector<Vertex>   m_solidFront;
};
