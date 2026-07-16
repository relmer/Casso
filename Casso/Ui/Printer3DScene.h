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

    // Front-panel status lamps. Each lamp carries a FIXED meaning (Power,
    // Select, Print Quality, Error -- see LampRole); this only sets the live
    // inputs those meanings read: `online` lights the SELECT lamp (printer
    // ready) and `error` lights the red fault lamp. Power stays lit (powered)
    // and Print Quality follows the selected quality. Unlit lamps show a dim
    // base-color lens; lit lamps brighten and gain a soft halo.
    void     SetLeds (bool online, bool error);

    // Preview magnification, 1 = fit (default). The camera narrows its field
    // of view by this factor, so 2 shows the paper at twice the size without
    // moving the eye or rebuilding any geometry. Clamped to a sane range.
    void     SetZoom (float zoom);

    // Horizontal pan, normalized: 0 = centered, +/-1 = a paper edge centered in
    // the view. Slides the camera sideways (only meaningful once zoomed in, when
    // the paper is wider than the field of view) without moving the eye in.
    void     SetPanX (float panXNorm);

    // Whole-world vertical pan, normalized: 0 = resting frame, +/-1 = the hard
    // stop up / down. Translates the ENTIRE printer + paper (not the backdrop)
    // in world Y, so panning past the paper's scroll limit nudges the machine
    // itself rather than hitting a wall. Clamped, so it can never leave view.
    void     SetWorldPanY (float panYNorm);

    // Camera vertical framing, normalized: 0 = default eye, +1 = look up toward
    // the paper top, -1 = look down onto the deck (bringing the lower-front
    // controls / LEDs into view when zoomed). Slides the eye + look-at together
    // in world Y (the vertical partner of SetPanX), so a magnified view can be
    // panned to any part of the machine.
    void     SetCameraPanY (float panYNorm);

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

    // Fixed real-panel meaning of each front-panel lamp, so they light for
    // their own reason instead of all together (see BuildLedBatches /
    // RoleIntensity). SetModel assigns a role to each clustered lamp.
    enum class LampRole { Power, Select, Quality, Error };

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

    // A soft radial halo in the x/y plane at constant z: a triangle fan whose
    // center carries the (premultiplied) color and whose rim is transparent, so
    // it composites as a glow. Used behind a lit LED lens.
    static void  AppendGlowZ    (std::vector<Vertex> & out,
                                 float cx, float cy, float z, float rx, float ry,
                                 uint32_t argb, float coreAlpha, int segments);

    // A solid flat round disc in the x/y plane at constant z: a surface-level
    // LED lens face, uniform (premultiplied) color. Round so a lamp reads as a
    // lens rather than the CAD model's blocky lifted cap.
    static void  AppendDiscZ    (std::vector<Vertex> & out,
                                 float cx, float cy, float z, float radius,
                                 uint32_t argb, float shade, int segments);

    // One front-panel LED at (cx,cy): a dim base lens when intensity ~0, a
    // bright lens plus halo as intensity -> 1. halfW sets the lens half-width
    // (the on/off pair uses narrower half-lamps).
    static void  AppendLed      (std::vector<Vertex> & out,
                                 float cx, float cy, float z,
                                 uint32_t argb, float intensity, float halfW);

    // Rebuild the per-frame LED batches (round surface-level lenses + glow
    // halos) from the loaded lamps, each at its role's live brightness.
    void  BuildLedBatches ();

    // The live brightness (0..1) of a lamp given its fixed role and the current
    // online / quality / error inputs. Drives each lamp INDIVIDUALLY so the
    // front panel no longer pulses every lamp together.
    float RoleIntensity (LampRole role) const;

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
    float            m_zoom          = 1.0f;
    float            m_panX          = 0.0f;   // world-space horizontal camera offset
    float            m_worldPanY     = 0.0f;   // world-space vertical shift of the whole scene
    float            m_camPanY        = 0.0f;   // world-space vertical camera framing offset
    bool             m_ledOnline     = true;   // SELECT lamp: printer online / ready
    float            m_ledQuality01  = 0.0f;   // PRINT QUALITY lamp: selected quality (draft = off)
    bool             m_ledError      = false;  // ERROR lamp: paper out / fault
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

    // Front-panel status lamps, each with a fixed real-panel meaning so they no
    // longer light in unison: Power (steady, powered), Select (online/ready),
    // Print Quality (selected quality), Error (red paper/fault). SetModel
    // assigns roles by left-to-right position; RoleIntensity maps each role to
    // its live brightness. The LED faces are lifted out of the static mesh only
    // so the lamp centers/extents can be clustered from them -- the render draws
    // round surface-level lenses at those centers, NOT the blocky lifted faces.
    struct LedFace   { float  p[3][3]; float shade; bool red; };
    struct LedLamp   { float  cx, cy, cz, halfW, halfH; bool red; LampRole role; };
    std::vector<LedFace>   m_ledFaces;
    std::vector<LedLamp>   m_ledLamps;
    std::vector<Vertex>    m_ledBatch;    // scratch: recolored lens faces
    std::vector<Vertex>    m_glowBatch;   // scratch: halo fans
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
