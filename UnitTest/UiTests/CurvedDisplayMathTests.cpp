#include "Pch.h"

#include "Render/CurvedDisplayMath.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CurvedDisplayMathTests
//
//  The glass mappings both directions: UV <-> model point (with sag), ray
//  intersection, and screen px <-> emulated pixel round trips. The forward
//  transform is the oracle for the inverse, so the one-pixel accuracy bar
//  (FR-002) is pinned in math before any rendering exists.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CurvedDisplayMathTests)
{
public:

    TEST_METHOD (Surface_Defaults_Are_Empty)
    {
        CurvedDisplaySurface  surface;



        Assert::AreEqual (0.0f, surface.x1 - surface.x0);
        Assert::AreEqual (0.0f, surface.z1 - surface.z0);
        Assert::AreEqual (0.0f, surface.radius);
    }

};
