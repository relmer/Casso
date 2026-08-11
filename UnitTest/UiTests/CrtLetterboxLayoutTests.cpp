#include "Pch.h"

#include "CrtPostProcess.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CrtLetterboxLayoutTests
//
//  Aspect-fit geometry: where the emulator image lands inside a surface of any
//  shape.
//
//  Both constraint directions are covered, and they are different code paths --
//  a WIDE surface pillarboxes and centers horizontally, a TALL one letterboxes
//  and centers vertically. Testing one shape leaves half the arithmetic
//  unexercised.
//
//  The aspect RATIO is asserted on the result, not just the position, since a
//  fit that centers correctly while stretching produces a picture that looks
//  almost right and is subtly wrong -- the exact failure a user reports as
//  "the display looks off" without being able to say why.
//
//  Degenerate surfaces are covered because they occur during window creation
//  and teardown: a zero or negative extent must yield an empty rect rather than
//  an inverted one.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CrtLetterboxLayoutTests)
{
public:
    TEST_METHOD (ComputeLetterboxRectInRect_Uses_Whole_Content_Height_When_Wide)
    {
        RECT  content = { 0, 60, 1000, 700 };
        RECT  out     = ComputeLetterboxRectInRect (content);

        Assert::AreEqual (73L,   out.left);
        Assert::AreEqual (60L,   out.top);
        Assert::AreEqual (926L,  out.right);
        Assert::AreEqual (700L,  out.bottom);
    }


    TEST_METHOD (ComputeLetterboxRectInRect_Centers_Letterbox_When_Narrow)
    {
        RECT  content = { 0, 60, 700, 700 };
        RECT  out     = ComputeLetterboxRectInRect (content);

        Assert::AreEqual (0L,    out.left);
        Assert::AreEqual (117L,  out.top);
        Assert::AreEqual (700L,  out.right);
        Assert::AreEqual (642L,  out.bottom);
    }


    TEST_METHOD (ComputeLetterboxRectInRect_Returns_Empty_For_Empty_Content)
    {
        RECT  content = { 0, 60, 0, 60 };
        RECT  out     = ComputeLetterboxRectInRect (content);

        Assert::AreEqual (0L,  out.left);
        Assert::AreEqual (0L,  out.top);
        Assert::AreEqual (0L,  out.right);
        Assert::AreEqual (0L,  out.bottom);
    }


    TEST_METHOD (ComputeLetterboxRect_Matches_InRect_FullBuffer_Behavior)
    {
        RECT  oldPath = ComputeLetterboxRect (1000, 700);
        RECT  full    = { 0, 0, 1000, 700 };
        RECT  inRect  = ComputeLetterboxRectInRect (full);

        Assert::AreEqual (oldPath.left,   inRect.left);
        Assert::AreEqual (oldPath.top,    inRect.top);
        Assert::AreEqual (oldPath.right,  inRect.right);
        Assert::AreEqual (oldPath.bottom, inRect.bottom);
    }


    TEST_METHOD (ComputeUvRectForFit_Maps_The_Fitted_Subrect)
    {
        RECT       fitted = { 100, 50, 900, 650 };
        CrtUvRect  uv     = ComputeUvRectForFit (fitted, 1000, 700);

        Assert::AreEqual (0.1f, uv.u0, 1e-6f);
        Assert::AreEqual (50.0f / 700.0f, uv.v0, 1e-6f);
        Assert::AreEqual (0.9f, uv.u1, 1e-6f);
        Assert::AreEqual (650.0f / 700.0f, uv.v1, 1e-6f);
    }


    TEST_METHOD (ComputeUvRectForFit_Composes_With_AspectFit)
    {
        // The subrect of the offscreen texture the scene samples is exactly
        // where the direct path would have put the picture on screen.
        RECT       content = { 0, 0, 1120, 768 };
        RECT       fitted  = ComputeAspectFitRectInRect (content, 560, 384);
        CrtUvRect  uv      = ComputeUvRectForFit (fitted, 1120, 768);

        Assert::AreEqual ((float) fitted.left   / 1120.0f, uv.u0, 1e-6f);
        Assert::AreEqual ((float) fitted.top    /  768.0f, uv.v0, 1e-6f);
        Assert::AreEqual ((float) fitted.right  / 1120.0f, uv.u1, 1e-6f);
        Assert::AreEqual ((float) fitted.bottom /  768.0f, uv.v1, 1e-6f);
    }


    TEST_METHOD (ComputeUvRectForFit_Degenerate_Texture_Yields_Full_Rect)
    {
        RECT       fitted = { 10, 10, 20, 20 };
        CrtUvRect  uv     = ComputeUvRectForFit (fitted, 0, 0);

        Assert::AreEqual (0.0f, uv.u0);
        Assert::AreEqual (0.0f, uv.v0);
        Assert::AreEqual (1.0f, uv.u1);
        Assert::AreEqual (1.0f, uv.v1);
    }
};
