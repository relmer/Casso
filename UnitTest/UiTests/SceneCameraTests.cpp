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

};
