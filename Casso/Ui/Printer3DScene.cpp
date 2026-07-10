#include "Pch.h"

#include "Printer3DScene.h"




////////////////////////////////////////////////////////////////////////////////
//
//  Scene constants -- world units; the printer body is ~1.84 wide. Everything
//  here is a tuning knob for the look; nothing depends on exact values.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    // Body (ImageWriter-class case).
    constexpr float   s_kBodyHalfW  = 0.92f;    // x extent
    constexpr float   s_kBodyH      = 0.40f;    // deck height (y)
    constexpr float   s_kBodyZFront = 0.42f;
    constexpr float   s_kBodyZBack  = -0.42f;

    // Paper slot in the deck; the paper strip passes through its middle.
    constexpr float   s_kSlotZFront = -0.08f;
    constexpr float   s_kSlotZBack  = -0.24f;
    constexpr float   s_kSlotHalfW  = 0.84f;

    // Paper strip: leans back from vertical, then curls away over a roll.
    constexpr float   s_kPaperHalfW = 0.78f;
    constexpr float   s_kPaperZ     = -0.16f;
    constexpr float   s_kPaperTilt  = 12.0f * 3.14159265f / 180.0f;
    constexpr float   s_kStraightLen = 1.15f;   // arclength before the curl (flat page dominates)
    constexpr float   s_kCurlRadius  = 0.28f;
    constexpr int     s_kPaperSlices = 48;

    // Camera: in front and above, looking slightly down so the printer sits at
    // the bottom of the frame and the paper rises through the middle.
    constexpr float   s_kEye[3]   = { 0.0f, 1.25f, 3.30f };
    constexpr float   s_kAt[3]    = { 0.0f, 0.85f, 0.0f };
    constexpr float   s_kFovY     = 34.0f * 3.14159265f / 180.0f;

    // Palette (ARGB): ImageWriter platinum + accents.
    constexpr uint32_t   s_kArgbMat      = 0xFF33363B;   // panel mat behind everything
    constexpr uint32_t   s_kArgbCase     = 0xFFD6D1C5;   // platinum
    constexpr uint32_t   s_kArgbCaseLip  = 0xFFB9B4A8;   // darker lower front lip
    constexpr uint32_t   s_kArgbSlot     = 0xFF17181B;   // paper slot recess
    constexpr uint32_t   s_kArgbWindow   = 0xE0121418;   // smoked carriage window
    constexpr uint32_t   s_kArgbCarriage = 0xFF2A2D33;   // print-head carriage
    constexpr uint32_t   s_kArgbLed      = 0xFF35C060;   // power LED
    constexpr uint32_t   s_kArgbRibbon[4] = { 0xFF202020, 0xFFF0C810, 0xFFC83030, 0xFF2848A8 };


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Row-vector matrix helpers (clip = v * view * proj), matching the
    //  renderer's row_major cbuffer. Hand-rolled to keep the 3D path free of
    //  header dependencies -- three small functions is all a fixed scene needs.
    //
    ////////////////////////////////////////////////////////////////////////////

    void Mul44 (const float a[16], const float b[16], float out[16])
    {
        for (int r = 0; r < 4; r++)
        {
            for (int c = 0; c < 4; c++)
            {
                out[r * 4 + c] = a[r * 4 + 0] * b[0 * 4 + c]
                               + a[r * 4 + 1] * b[1 * 4 + c]
                               + a[r * 4 + 2] * b[2 * 4 + c]
                               + a[r * 4 + 3] * b[3 * 4 + c];
            }
        }
    }


    void LookAtRH (const float eye[3], const float at[3], float out[16])
    {
        float   z[3] = { eye[0] - at[0], eye[1] - at[1], eye[2] - at[2] };
        float   zl   = std::sqrt (z[0] * z[0] + z[1] * z[1] + z[2] * z[2]);
        float   x[3] = {};
        float   xl   = 0.0f;
        float   y[3] = {};

        z[0] /= zl; z[1] /= zl; z[2] /= zl;

        // x = normalize(cross(up, z)) with up = (0,1,0)
        x[0] = z[2]; x[1] = 0.0f; x[2] = -z[0];
        xl   = std::sqrt (x[0] * x[0] + x[2] * x[2]);
        x[0] /= xl; x[2] /= xl;

        // y = cross(z, x)
        y[0] = z[1] * x[2] - z[2] * x[1];
        y[1] = z[2] * x[0] - z[0] * x[2];
        y[2] = z[0] * x[1] - z[1] * x[0];

        out[0]  = x[0]; out[1]  = y[0]; out[2]  = z[0]; out[3]  = 0.0f;
        out[4]  = x[1]; out[5]  = y[1]; out[6]  = z[1]; out[7]  = 0.0f;
        out[8]  = x[2]; out[9]  = y[2]; out[10] = z[2]; out[11] = 0.0f;
        out[12] = -(x[0] * eye[0] + x[1] * eye[1] + x[2] * eye[2]);
        out[13] = -(y[0] * eye[0] + y[1] * eye[1] + y[2] * eye[2]);
        out[14] = -(z[0] * eye[0] + z[1] * eye[1] + z[2] * eye[2]);
        out[15] = 1.0f;
    }


    void PerspectiveFovRH (float fovY, float aspect, float zn, float zf, float out[16])
    {
        float   ys = 1.0f / std::tan (fovY * 0.5f);
        float   xs = ys / aspect;

        memset (out, 0, 16 * sizeof (float));
        out[0]  = xs;
        out[5]  = ys;
        out[10] = zf / (zn - zf);
        out[11] = -1.0f;
        out[14] = zn * zf / (zn - zf);
    }


    void IdentityMvp (float out[16])
    {
        memset (out, 0, 16 * sizeof (float));
        out[0] = out[5] = out[10] = out[15] = 1.0f;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Printer3DScene::Initialize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Printer3DScene::Initialize (ID3D11Device * device, ID3D11DeviceContext * context)
{
    return m_renderer.Initialize (device, context);
}




////////////////////////////////////////////////////////////////////////////////
//
//  Printer3DScene::SetContent
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Printer3DScene::SetContent (const uint32_t * bgra, int width, int height)
{
    HRESULT   hr = m_renderer.UpdateContentTexture (bgra, width, height);

    if (SUCCEEDED (hr))
    {
        m_contentWidth  = width;
        m_contentHeight = height;
    }

    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Printer3DScene::SetHeadColumn01
//
////////////////////////////////////////////////////////////////////////////////

void Printer3DScene::SetHeadColumn01 (float x01)
{
    m_head01 = std::clamp (x01, 0.0f, 1.0f);
}




////////////////////////////////////////////////////////////////////////////////
//
//  Printer3DScene::AppendQuad
//
//  p00..p11 are the quad corners at (u0,v0) (u1,v0) (u0,v1) (u1,v1); `shade`
//  bakes the lighting into the tint, and colors are emitted premultiplied so
//  translucent pieces (the smoked window) composite correctly.
//
////////////////////////////////////////////////////////////////////////////////

void Printer3DScene::AppendQuad (std::vector<Vertex> & out,
                                 const float p00[3], const float p10[3],
                                 const float p01[3], const float p11[3],
                                 float u0, float v0, float u1, float v1,
                                 uint32_t argb, float shade)
{
    float   a = (float) ((argb >> 24) & 0xFF) / 255.0f;
    float   r = (float) ((argb >> 16) & 0xFF) / 255.0f * shade * a;
    float   g = (float) ((argb >>  8) & 0xFF) / 255.0f * shade * a;
    float   b = (float) ((argb      ) & 0xFF) / 255.0f * shade * a;

    Vertex   v00 = { p00[0], p00[1], p00[2], u0, v0, r, g, b, a };
    Vertex   v10 = { p10[0], p10[1], p10[2], u1, v0, r, g, b, a };
    Vertex   v01 = { p01[0], p01[1], p01[2], u0, v1, r, g, b, a };
    Vertex   v11 = { p11[0], p11[1], p11[2], u1, v1, r, g, b, a };

    out.push_back (v00);
    out.push_back (v10);
    out.push_back (v01);
    out.push_back (v01);
    out.push_back (v10);
    out.push_back (v11);
}




////////////////////////////////////////////////////////////////////////////////
//
//  Printer3DScene::BuildBackdrop
//
//  Full-viewport mat quad in NDC (drawn with an identity matrix): the scene
//  paints its own background because the panel deliberately does not fill
//  this rect (a painter fill would flush AFTER the hook and cover the scene).
//
////////////////////////////////////////////////////////////////////////////////

void Printer3DScene::BuildBackdrop (std::vector<Vertex> & out) const
{
    float   tl[3] = { -1.0f,  1.0f, 0.0f };
    float   tr[3] = {  1.0f,  1.0f, 0.0f };
    float   bl[3] = { -1.0f, -1.0f, 0.0f };
    float   br[3] = {  1.0f, -1.0f, 0.0f };

    AppendQuad (out, tl, tr, bl, br, 0, 0, 1, 1, s_kArgbMat, 1.0f);
}




////////////////////////////////////////////////////////////////////////////////
//
//  Printer3DScene::BuildBodyBack
//
//  The parts of the printer BEHIND the paper: the deck strip behind the slot
//  and the slot recess itself. Drawn before the paper so the strip visibly
//  emerges out of the slot.
//
////////////////////////////////////////////////////////////////////////////////

void Printer3DScene::BuildBodyBack (std::vector<Vertex> & out) const
{
    // Deck behind the slot (light hits the deck from the front: back is dimmer).
    {
        float   p00[3] = { -s_kBodyHalfW, s_kBodyH, s_kBodyZBack };
        float   p10[3] = {  s_kBodyHalfW, s_kBodyH, s_kBodyZBack };
        float   p01[3] = { -s_kBodyHalfW, s_kBodyH, s_kSlotZBack };
        float   p11[3] = {  s_kBodyHalfW, s_kBodyH, s_kSlotZBack };

        AppendQuad (out, p00, p10, p01, p11, 0, 0, 1, 1, s_kArgbCase, 0.86f);
    }

    // Slot recess.
    {
        float   p00[3] = { -s_kSlotHalfW, s_kBodyH, s_kSlotZBack };
        float   p10[3] = {  s_kSlotHalfW, s_kBodyH, s_kSlotZBack };
        float   p01[3] = { -s_kSlotHalfW, s_kBodyH, s_kSlotZFront };
        float   p11[3] = {  s_kSlotHalfW, s_kBodyH, s_kSlotZFront };

        AppendQuad (out, p00, p10, p01, p11, 0, 0, 1, 1, s_kArgbSlot, 1.0f);
    }

    // Deck side strips flanking the slot recess.
    {
        float   l00[3] = { -s_kBodyHalfW, s_kBodyH, s_kSlotZBack };
        float   l10[3] = { -s_kSlotHalfW, s_kBodyH, s_kSlotZBack };
        float   l01[3] = { -s_kBodyHalfW, s_kBodyH, s_kSlotZFront };
        float   l11[3] = { -s_kSlotHalfW, s_kBodyH, s_kSlotZFront };

        AppendQuad (out, l00, l10, l01, l11, 0, 0, 1, 1, s_kArgbCase, 0.90f);

        float   r00[3] = {  s_kSlotHalfW, s_kBodyH, s_kSlotZBack };
        float   r10[3] = {  s_kBodyHalfW, s_kBodyH, s_kSlotZBack };
        float   r01[3] = {  s_kSlotHalfW, s_kBodyH, s_kSlotZFront };
        float   r11[3] = {  s_kBodyHalfW, s_kBodyH, s_kSlotZFront };

        AppendQuad (out, r00, r10, r01, r11, 0, 0, 1, 1, s_kArgbCase, 0.90f);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Printer3DScene::BuildPaper
//
//  The fanfold strip: rises from the slot with a slight backward lean, then
//  curls away from the viewer over a roll and heads down behind the printer.
//  The content canvas maps 1:1 by arclength (square texels), canvas bottom --
//  the live row -- at the platen. Slices darken as the surface turns away
//  from the frontal light.
//
////////////////////////////////////////////////////////////////////////////////

void Printer3DScene::BuildPaper (std::vector<Vertex> & out) const
{
    float   aspect   = (m_contentWidth > 0) ? (float) m_contentHeight / (float) m_contentWidth
                                            : 1584.0f / 1368.0f;
    float   total    = 2.0f * s_kPaperHalfW * aspect;   // arclength covering the whole canvas
    float   dy       = std::cos (s_kPaperTilt);
    float   dz       = -std::sin (s_kPaperTilt);
    float   ny       = dz;                              // curl normal: down-and-back
    float   nz       = -dy;
    float   startY   = s_kBodyH - 0.04f;                // begins just inside the slot
    float   p1y      = startY + dy * s_kStraightLen;    // where the curl begins
    float   p1z      = s_kPaperZ + dz * s_kStraightLen;
    float   cy       = p1y + s_kCurlRadius * ny;        // curl center
    float   cz       = p1z + s_kCurlRadius * nz;

    float   prevY    = startY;
    float   prevZ    = s_kPaperZ;
    float   prevV    = 1.0f;
    float   prevSh   = 1.0f;

    for (int i = 1; i <= s_kPaperSlices; i++)
    {
        float   s     = total * (float) i / (float) s_kPaperSlices;
        float   yPos  = 0.0f;
        float   zPos  = 0.0f;
        float   shade = 1.0f;

        if (s <= s_kStraightLen)
        {
            yPos = startY + dy * s;
            zPos = s_kPaperZ + dz * s;
        }
        else
        {
            float   phi = (s - s_kStraightLen) / s_kCurlRadius;

            yPos  = cy + s_kCurlRadius * (-ny * std::cos (phi) + dy * std::sin (phi));
            zPos  = cz + s_kCurlRadius * (-nz * std::cos (phi) + dz * std::sin (phi));
            shade = (std::max) (0.38f, std::cos (phi * 0.75f));
        }

        {
            float   v   = 1.0f - s / total;
            float   p00[3] = { -s_kPaperHalfW, prevY, prevZ };
            float   p10[3] = {  s_kPaperHalfW, prevY, prevZ };
            float   p01[3] = { -s_kPaperHalfW, yPos, zPos };
            float   p11[3] = {  s_kPaperHalfW, yPos, zPos };

            // Per-slice shade via two AppendQuads would double vertices; write
            // the quad directly so top/bottom edges carry their own shades.
            size_t   base = out.size ();

            AppendQuad (out, p00, p10, p01, p11, 0.0f, prevV, 1.0f, v, 0xFFFFFFFF, 1.0f);

            for (size_t k = 0; k < 6; k++)
            {
                bool    isTopEdge = (out[base + k].v < prevV);   // smaller v == farther along
                float   sh        = isTopEdge ? shade : prevSh;

                out[base + k].r *= sh;
                out[base + k].g *= sh;
                out[base + k].b *= sh;
            }
        }

        prevY  = yPos;
        prevZ  = zPos;
        prevV  = 1.0f - s / total;
        prevSh = shade;

        // The curl's tail fell behind the printer: the rest is out of sight.
        // (Only the curl -- the straight section legitimately starts below
        // deck level, inside the slot.)
        if (s > s_kStraightLen && yPos < s_kBodyH + 0.03f)
        {
            break;
        }
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Printer3DScene::BuildBodyFront
//
//  Everything in front of the paper: the deck ahead of the slot, the front
//  face with its darker lip, the smoked carriage window with the paced head
//  carriage and its four-color ribbon cartridge, and the power LED.
//
////////////////////////////////////////////////////////////////////////////////

void Printer3DScene::BuildBodyFront (std::vector<Vertex> & out) const
{
    // Deck ahead of the slot.
    {
        float   p00[3] = { -s_kBodyHalfW, s_kBodyH, s_kSlotZFront };
        float   p10[3] = {  s_kBodyHalfW, s_kBodyH, s_kSlotZFront };
        float   p01[3] = { -s_kBodyHalfW, s_kBodyH, s_kBodyZFront };
        float   p11[3] = {  s_kBodyHalfW, s_kBodyH, s_kBodyZFront };

        AppendQuad (out, p00, p10, p01, p11, 0, 0, 1, 1, s_kArgbCase, 1.0f);
    }

    // Front face (top at the deck edge, bottom at the desk).
    {
        float   p00[3] = { -s_kBodyHalfW, s_kBodyH, s_kBodyZFront };
        float   p10[3] = {  s_kBodyHalfW, s_kBodyH, s_kBodyZFront };
        float   p01[3] = { -s_kBodyHalfW, 0.06f,    s_kBodyZFront };
        float   p11[3] = {  s_kBodyHalfW, 0.06f,    s_kBodyZFront };

        AppendQuad (out, p00, p10, p01, p11, 0, 0, 1, 1, s_kArgbCase, 0.90f);
    }

    // Darker lower lip.
    {
        float   p00[3] = { -s_kBodyHalfW, 0.06f, s_kBodyZFront };
        float   p10[3] = {  s_kBodyHalfW, 0.06f, s_kBodyZFront };
        float   p01[3] = { -s_kBodyHalfW, 0.0f,  s_kBodyZFront };
        float   p11[3] = {  s_kBodyHalfW, 0.0f,  s_kBodyZFront };

        AppendQuad (out, p00, p10, p01, p11, 0, 0, 1, 1, s_kArgbCaseLip, 0.85f);
    }

    // Smoked carriage window (slightly proud of the front face).
    {
        float   zf     = s_kBodyZFront + 0.002f;
        float   p00[3] = { -0.80f, 0.32f, zf };
        float   p10[3] = {  0.80f, 0.32f, zf };
        float   p01[3] = { -0.80f, 0.14f, zf };
        float   p11[3] = {  0.80f, 0.14f, zf };

        AppendQuad (out, p00, p10, p01, p11, 0, 0, 1, 1, s_kArgbWindow, 1.0f);
    }

    // Head carriage riding the window at the paced column (FR-034), with the
    // four-color ribbon cartridge on its back.
    {
        float   zf   = s_kBodyZFront + 0.004f;
        float   cx   = -0.60f + m_head01 * 1.20f;
        float   p00[3] = { cx - 0.09f, 0.30f, zf };
        float   p10[3] = { cx + 0.09f, 0.30f, zf };
        float   p01[3] = { cx - 0.09f, 0.16f, zf };
        float   p11[3] = { cx + 0.09f, 0.16f, zf };

        AppendQuad (out, p00, p10, p01, p11, 0, 0, 1, 1, s_kArgbCarriage, 1.0f);

        for (int i = 0; i < 4; i++)
        {
            float   rx     = cx - 0.072f + (float) i * 0.038f;
            float   zr     = zf + 0.002f;
            float   r00[3] = { rx,          0.285f, zr };
            float   r10[3] = { rx + 0.030f, 0.285f, zr };
            float   r01[3] = { rx,          0.255f, zr };
            float   r11[3] = { rx + 0.030f, 0.255f, zr };

            AppendQuad (out, r00, r10, r01, r11, 0, 0, 1, 1, s_kArgbRibbon[i], 1.0f);
        }
    }

    // Power LED, bottom-left.
    {
        float   zf     = s_kBodyZFront + 0.002f;
        float   p00[3] = { -0.86f, 0.115f, zf };
        float   p10[3] = { -0.80f, 0.115f, zf };
        float   p01[3] = { -0.86f, 0.085f, zf };
        float   p11[3] = { -0.80f, 0.085f, zf };

        AppendQuad (out, p00, p10, p01, p11, 0, 0, 1, 1, s_kArgbLed, 1.0f);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Printer3DScene::Render
//
////////////////////////////////////////////////////////////////////////////////

void Printer3DScene::Render (const RECT & targetPx)
{
    HRESULT          hr = S_OK;
    int              w  = targetPx.right - targetPx.left;
    int              h  = targetPx.bottom - targetPx.top;
    float            view[16];
    float            proj[16];
    float            mvp[16];
    float            identity[16];
    D3D11_VIEWPORT   vp = {};

    if (!m_renderer.IsInitialized () || w <= 0 || h <= 0)
    {
        return;
    }

    vp.TopLeftX = (float) targetPx.left;
    vp.TopLeftY = (float) targetPx.top;
    vp.Width    = (float) w;
    vp.Height   = (float) h;
    vp.MaxDepth = 1.0f;

    LookAtRH         (s_kEye, s_kAt, view);
    PerspectiveFovRH (s_kFovY, (float) w / (float) h, 0.1f, 20.0f, proj);
    Mul44            (view, proj, mvp);
    IdentityMvp      (identity);

    m_backdrop.clear ();
    m_solidBack.clear ();
    m_paper.clear ();
    m_solidFront.clear ();

    BuildBackdrop  (m_backdrop);
    BuildBodyBack  (m_solidBack);
    BuildPaper     (m_paper);
    BuildBodyFront (m_solidFront);

    IGNORE_RETURN_VALUE (hr, m_renderer.DrawTriangles (m_backdrop.data (),   m_backdrop.size (),   identity, false, vp));
    IGNORE_RETURN_VALUE (hr, m_renderer.DrawTriangles (m_solidBack.data (),  m_solidBack.size (),  mvp,      false, vp));
    IGNORE_RETURN_VALUE (hr, m_renderer.DrawTriangles (m_paper.data (),      m_paper.size (),      mvp,      true,  vp));
    IGNORE_RETURN_VALUE (hr, m_renderer.DrawTriangles (m_solidFront.data (), m_solidFront.size (), mvp,      false, vp));
}
