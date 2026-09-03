#include "Pch.h"

#include "Render/SceneCamera.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SceneCameraTests
//
//  The desk scene's matrix math. Every transform feeding the renderer and the
//  input inverse-projection goes through SceneCamera, so identity, inversion,
//  projection round trips, and the contain-fit are pinned here with no GPU.
//
//  The projection round trip (world -> screen -> ray -> world) is the
//  foundation the curved-display accuracy bar rests on: if these drift, the
//  one-pixel input requirement cannot hold no matter what the glass math does.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SceneCameraTests)
{
public:

    TEST_METHOD (Identity44_Is_Identity)
    {
        float  m[16] = {};



        SceneCamera::Identity44 (m);

        for (int r = 0; r < 4; r++)
        {
            for (int c = 0; c < 4; c++)
            {
                Assert::AreEqual (r == c ? 1.0f : 0.0f, m[r * 4 + c]);
            }
        }
    }

    TEST_METHOD (Mul44_By_Identity_Is_Noop)
    {
        float  identity[16] = {};
        float  view[16]     = {};
        float  out[16]      = {};
        float  eye[3]       = { 10.0f, 20.0f, 30.0f };
        float  at[3]        = { 0.0f, 5.0f, 0.0f };



        SceneCamera::Identity44 (identity);
        SceneCamera::LookAtRH   (eye, at, view);
        SceneCamera::Mul44      (view, identity, out);

        for (int i = 0; i < 16; i++)
        {
            Assert::AreEqual (view[i], out[i], 1e-6f);
        }
    }

    TEST_METHOD (Inverse44_Recovers_Transformed_Points)
    {
        float  view[16]     = {};
        float  proj[16]     = {};
        float  viewProj[16] = {};
        float  inv[16]      = {};
        float  eye[3]       = { 120.0f, 180.0f, 600.0f };
        float  at[3]        = { 124.0f, 137.0f, 0.0f };
        float  samples[][3] = { { 0.0f, 0.0f, 0.0f },
                                { 124.0f, 137.0f, -6.0f },
                                { -180.0f, 40.0f, 170.0f },
                                { 260.0f, 226.0f, -280.0f } };



        SceneCamera::LookAtRH         (eye, at, view);
        SceneCamera::PerspectiveFovRH (0.35f, 1120.0f / 768.0f, 1.0f, 2000.0f, proj);
        SceneCamera::Mul44            (view, proj, viewProj);

        Assert::IsTrue (SceneCamera::Inverse44 (viewProj, inv));

        // The behavioral contract the input path relies on: a point taken
        // through the matrix and back is recovered (mm scale). A raw
        // M * M^-1 identity check drowns in float residue from the large
        // translation; recovery is what the inverse is FOR.
        for (const float (& sample)[3] : samples)
        {
            float   ndc[3]  = {};
            float   back[3] = {};

            Assert::IsTrue (SceneCamera::TransformPoint (viewProj, sample, ndc));
            Assert::IsTrue (SceneCamera::TransformPoint (inv, ndc, back));

            // Depth recovery is the ill-conditioned axis of a perspective
            // NDC (classic z-precision falloff), so it gets a looser bar;
            // lateral recovery is what the input path consumes.
            Assert::AreEqual (sample[0], back[0], 0.05f);
            Assert::AreEqual (sample[1], back[1], 0.05f);
            Assert::AreEqual (sample[2], back[2], 0.5f);
        }
    }

    // A camera looking ALONG its up vector. cross(up, forward) is zero-length
    // there, and dividing by that length fills the matrix with NaN -- which is
    // silent, because every later comparison against a NaN is simply false. It
    // cost a shadow pass that rendered, sampled, and occluded nothing while
    // reporting success at every step.
    TEST_METHOD (LookAtUpRH_Looking_Along_Up_Still_Gives_A_Usable_Basis)
    {
        const float  eye[3]   = { 10.0f, 20.0f, 30.0f };
        const float  at[3]    = { 10.0f, -80.0f, 30.0f };   // straight down -Y
        const float  up[3]    = { 0.0f, 1.0f, 0.0f };   // ...which is up
        float        view[16] = {};
        float        out[3]   = {};

        SceneCamera::LookAtUpRH (eye, at, up, view);

        for (int i = 0; i < 16; i++)
        {
            Assert::IsFalse (std::isnan (view[i]), L"view matrix has a NaN");
        }

        // The eye maps to the origin and the target lands straight ahead:
        // right-handed, so "ahead" is negative z.
        Assert::IsTrue (SceneCamera::TransformPoint (view, eye, out));
        Assert::AreEqual (0.0f, out[0], 1e-3f);
        Assert::AreEqual (0.0f, out[1], 1e-3f);
        Assert::AreEqual (0.0f, out[2], 1e-3f);

        Assert::IsTrue (SceneCamera::TransformPoint (view, at, out));
        Assert::AreEqual (0.0f, out[0], 1e-3f);
        Assert::AreEqual (0.0f, out[1], 1e-3f);
        Assert::AreEqual (-100.0f, out[2], 1e-2f);
    }

    // The ordinary case still routes through the same code and must not move.
    TEST_METHOD (LookAtUpRH_With_Y_Up_Matches_LookAtRH)
    {
        const float  eye[3] = { 3.0f, 4.0f, 5.0f };
        const float  at[3]  = { -2.0f, 1.0f, -6.0f };
        const float  up[3]  = { 0.0f, 1.0f, 0.0f };
        float        a[16]  = {};
        float        b[16]  = {};

        SceneCamera::LookAtRH   (eye, at, a);
        SceneCamera::LookAtUpRH (eye, at, up, b);

        for (int i = 0; i < 16; i++)
        {
            Assert::AreEqual (a[i], b[i], 1e-5f);
        }
    }

    TEST_METHOD (Inverse44_Singular_Reports_False)
    {
        float  zero[16] = {};
        float  out[16]  = {};



        Assert::IsFalse (SceneCamera::Inverse44 (zero, out));

        // Falls back to identity so a caller that ignores the result gets a
        // no-op transform instead of NaNs.
        Assert::AreEqual (1.0f, out[0]);
        Assert::AreEqual (1.0f, out[15]);
    }

    TEST_METHOD (FitContainFovY_Wide_Viewport_Keeps_Fov)
    {
        Assert::AreEqual (0.5f, SceneCamera::FitContainFovY (0.5f, 1.4f, 1.8f), 1e-6f);
        Assert::AreEqual (0.5f, SceneCamera::FitContainFovY (0.5f, 1.4f, 1.4f), 1e-6f);
    }

    TEST_METHOD (FitContainFovY_Narrow_Viewport_Widens_By_Aspect_Deficit)
    {
        float  fovY  = 0.5f;
        float  wider = SceneCamera::FitContainFovY (fovY, 1.4f, 0.7f);



        Assert::IsTrue (wider > fovY);

        // tan scales by exactly the aspect deficit (2x here).
        Assert::AreEqual (std::tan (fovY * 0.5f) * 2.0f, std::tan (wider * 0.5f), 1e-5f);
    }

    TEST_METHOD (Project_Then_Ray_Passes_Through_The_Point)
    {
        float  view[16]     = {};
        float  proj[16]     = {};
        float  viewProj[16] = {};
        float  inv[16]      = {};
        float  eye[3]       = { 124.0f, 160.0f, 700.0f };
        float  at[3]        = { 124.0f, 137.0f, 0.0f };
        RECT   viewport     = { 0, 0, 1120, 768 };
        float  world[3]     = { 60.0f, 200.0f, -40.0f };
        float  screen[2]    = {};
        float  origin[3]    = {};
        float  dir[3]       = {};



        SceneCamera::LookAtRH         (eye, at, view);
        SceneCamera::PerspectiveFovRH (0.35f, 1120.0f / 768.0f, 1.0f, 2000.0f, proj);
        SceneCamera::Mul44            (view, proj, viewProj);

        Assert::IsTrue (SceneCamera::ProjectToScreen (viewProj, world, viewport, screen));
        Assert::IsTrue (SceneCamera::Inverse44 (viewProj, inv));
        Assert::IsTrue (SceneCamera::ScreenRayFromPx (inv, viewport, screen[0], screen[1], origin, dir));

        // Distance from the world point to the ray: cross(point - origin, dir).
        {
            float   po[3]    = { world[0] - origin[0], world[1] - origin[1], world[2] - origin[2] };
            float   cross[3] = { po[1] * dir[2] - po[2] * dir[1],
                                 po[2] * dir[0] - po[0] * dir[2],
                                 po[0] * dir[1] - po[1] * dir[0] };
            float   dist     = std::sqrt (cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2]);

            Assert::IsTrue (dist < 0.05f);
        }
    }

    TEST_METHOD (Project_Behind_Eye_Reports_False)
    {
        float  view[16]     = {};
        float  proj[16]     = {};
        float  viewProj[16] = {};
        float  eye[3]       = { 0.0f, 0.0f, 100.0f };
        float  at[3]        = { 0.0f, 0.0f, 0.0f };
        RECT   viewport     = { 0, 0, 800, 600 };
        float  behind[3]    = { 0.0f, 0.0f, 200.0f };
        float  screen[2]    = {};



        SceneCamera::LookAtRH         (eye, at, view);
        SceneCamera::PerspectiveFovRH (0.6f, 800.0f / 600.0f, 1.0f, 1000.0f, proj);
        SceneCamera::Mul44            (view, proj, viewProj);

        Assert::IsFalse (SceneCamera::ProjectToScreen (viewProj, behind, viewport, screen));
    }

    TEST_METHOD (Glass_Fill_Camera_Covers_The_Viewport_Exactly_On_The_Binding_Axis)
    {
        // A glass rect wider than the viewport's aspect: height binds (the
        // vertical span fits exactly) and the width crops offscreen.
        float  view[16]     = {};
        float  proj[16]     = {};
        float  viewProj[16] = {};
        RECT   viewport     = { 0, 0, 1200, 900 };   // 4:3 viewport
        float  glassW       = 216.0f;
        float  glassH       = 144.0f;                // 1.5 aspect > 4:3
        float  standoff     = SceneCamera::SolveCoverStandoff (glassW, glassH, 0.6f,
                                                               1200.0f / 900.0f);



        SceneCamera::SolveStraightOnCamera (10.0f, 120.0f, -6.0f, standoff,
                                            0.6f, 1200.0f / 900.0f, 1.0f, 5000.0f,
                                            view, proj, viewProj);

        // The vertical edge midpoints land exactly on the viewport's top and
        // bottom edges; the horizontal midpoints land OUTSIDE (cropped).
        {
            float  top[3]    = { 10.0f, 120.0f + glassH * 0.5f, -6.0f };
            float  bottom[3] = { 10.0f, 120.0f - glassH * 0.5f, -6.0f };
            float  left[3]   = { 10.0f - glassW * 0.5f, 120.0f, -6.0f };
            float  px[2]     = {};

            Assert::IsTrue (SceneCamera::ProjectToScreen (viewProj, top, viewport, px));
            Assert::AreEqual (0.0f, px[1], 0.5f);
            Assert::AreEqual (600.0f, px[0], 0.5f);   // centered horizontally

            Assert::IsTrue (SceneCamera::ProjectToScreen (viewProj, bottom, viewport, px));
            Assert::AreEqual (900.0f, px[1], 0.5f);

            Assert::IsTrue (SceneCamera::ProjectToScreen (viewProj, left, viewport, px));
            Assert::IsTrue (px[0] < -0.5f);           // cropped past the left edge
        }
    }

    TEST_METHOD (Glass_Fill_Camera_Binds_On_Width_For_A_Wide_Viewport)
    {
        // A 21:9-ish viewport against the same glass: width binds, the top
        // and bottom crop.
        float  view[16]     = {};
        float  proj[16]     = {};
        float  viewProj[16] = {};
        RECT   viewport     = { 0, 0, 2100, 900 };
        float  glassW       = 216.0f;
        float  glassH       = 144.0f;
        float  standoff     = SceneCamera::SolveCoverStandoff (glassW, glassH, 0.6f,
                                                               2100.0f / 900.0f);



        SceneCamera::SolveStraightOnCamera (0.0f, 0.0f, 0.0f, standoff,
                                            0.6f, 2100.0f / 900.0f, 1.0f, 5000.0f,
                                            view, proj, viewProj);

        {
            float  right[3] = { glassW * 0.5f, 0.0f, 0.0f };
            float  top[3]   = { 0.0f, glassH * 0.5f, 0.0f };
            float  px[2]    = {};

            Assert::IsTrue (SceneCamera::ProjectToScreen (viewProj, right, viewport, px));
            Assert::AreEqual (2100.0f, px[0], 0.5f);

            Assert::IsTrue (SceneCamera::ProjectToScreen (viewProj, top, viewport, px));
            Assert::IsTrue (px[1] < -0.5f);           // cropped past the top
        }
    }

    TEST_METHOD (Contain_Standoff_Lands_A_Point_Exactly_On_The_Edge)
    {
        // The standoff solved for one point places that point ON the
        // frustum's edge -- the binding axis's edge, and inside on the other.
        float  view[16]     = {};
        float  proj[16]     = {};
        float  viewProj[16] = {};
        RECT   viewport     = { 0, 0, 1600, 900 };
        float  aspect       = 1600.0f / 900.0f;
        float  high[3]      = { 12.0f, 70.0f, 20.0f };   // 20 in front of the plane
        float  standoff     = SceneCamera::SolveContainStandoff (high[0], high[1], high[2], 0.6f, aspect);
        float  px[2]        = {};



        SceneCamera::SolveStraightOnCamera (0.0f, 0.0f, 0.0f, standoff, 0.6f, aspect,
                                            1.0f, 5000.0f, view, proj, viewProj);

        Assert::IsTrue (SceneCamera::ProjectToScreen (viewProj, high, viewport, px));
        Assert::AreEqual (0.0f, px[1], 0.5f, L"the height is what this point demands");
        Assert::IsTrue (px[0] > 0.0f && px[0] < 1600.0f);
    }

    TEST_METHOD (Contain_Standoff_Backs_Off_For_A_Nearer_Point)
    {
        // A point standing proud of the aim plane sits closer to the camera,
        // so it projects further off-axis: the standoff grows by exactly its
        // lift, which is what makes a curved sheet's bulging edges solvable
        // point by point.
        float  flat  = SceneCamera::SolveContainStandoff (0.0f, 40.0f, 0.0f, 0.6f, 1.5f);
        float  proud = SceneCamera::SolveContainStandoff (0.0f, 40.0f, 7.5f, 0.6f, 1.5f);



        Assert::AreEqual (flat + 7.5f, proud, 1e-4f);
    }

};
