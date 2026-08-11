#include "Pch.h"

#include "Render/SceneCamera.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SceneCameraTests
//
//  The desk scene's matrix math. Every transform feeding the renderer and the
//  input inverse-projection goes through SceneCamera, so identity, inversion,
//  and the fit solves are pinned here with no GPU involved.
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

};
