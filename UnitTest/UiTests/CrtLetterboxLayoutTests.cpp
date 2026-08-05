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
};
