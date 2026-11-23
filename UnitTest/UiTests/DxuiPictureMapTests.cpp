#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPictureMapTests
//
//  A 100-pixel-square picture drawn at (6, 22), a pixel a pixel: zooming
//  around the pointer, panning inside the picture, and the ping's period.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiPictureMapTests)
{
public:

    static constexpr DxuiPictureMap::Key    kKey   { 1, 100, 100, 50, 50 };
    static constexpr DxuiPictureMap::Frame  kFrame { 6.0f, 22.0f, 1.0f };


    //  The pixel under the pointer stays put, and the others close in on it.
    TEST_METHOD (Zoom_KeepsThePixelUnderThePointer)
    {
        DxuiPictureMap  map;

        map.Zoom (kKey, kFrame, POINT { 6 + 50, 22 + 50 }, 1.0f);

        Assert::AreEqual (1.25f, map.GetZoom());
        Assert::AreEqual (10L,   map.GetOrigin().x, L"pixel 50 stays under the pointer: 50 - 50 / 1.25");
        Assert::AreEqual (10L,   map.GetOrigin().y);
    }


    TEST_METHOD (Zoom_StaysBetweenTheWholePictureAndTheLimit)
    {
        DxuiPictureMap  map;

        map.Zoom (kKey, kFrame, POINT { 6 + 50, 22 + 50 }, -3.0f);
        Assert::AreEqual (1.0f, map.GetZoom(), L"no smaller than the whole picture");

        map.Zoom (kKey, kFrame, POINT { 6 + 50, 22 + 50 }, 40.0f);
        Assert::AreEqual ((float) DxuiPictureMap::kMaxZoom, map.GetZoom());
    }


    //  A different picture, or the same row with a different picture, starts
    //  unzoomed.
    TEST_METHOD (Zoom_IsForOnePicture)
    {
        DxuiPictureMap       map;
        DxuiPictureMap::Key  other { 1, 100, 100, 20, 20 };

        map.Zoom (kKey, kFrame, POINT { 6 + 50, 22 + 50 }, 2.0f);

        Assert::IsTrue   (map.IsFor (kKey));
        Assert::AreEqual (1.0f, map.GetZoom (other));
        Assert::AreEqual (0.0f, map.GetOriginX (other));
    }


    //  Dragged left, the view moves right across the picture; dragged far,
    //  it stops at the picture's edge.
    TEST_METHOD (Pan_FollowsTheDragInsideThePicture)
    {
        DxuiPictureMap  map;

        map.Zoom     (kKey, kFrame, POINT { 6 + 50, 22 + 50 }, 4.0f);
        map.StartPan (kKey, POINT { 6 + 50, 22 + 50 });
        map.Pan      (kFrame, POINT { 6 + 40, 22 + 50 });

        Assert::IsTrue (map.IsPanning());
        Assert::IsTrue (map.GetOrigin().x > 30, L"ten pixels left at about 2.4 times is about four picture pixels right");

        map.Pan    (kFrame, POINT { 6 + 400, 22 + 400 });
        map.EndPan();

        Assert::IsFalse  (map.IsPanning());
        Assert::AreEqual (0L, map.GetOrigin().x, L"stopped at the picture's left");
        Assert::AreEqual (0L, map.GetOrigin().y, L"and its top");
    }


    //  The ping runs for the first part of every period, with one more frame
    //  as it ends so its last ring is erased, then rests.
    TEST_METHOD (TickPing_RunsThenRestsEachPeriod)
    {
        DxuiPictureMap  map;

        Assert::IsTrue  (map.TickPing (10000,        true), L"the ping starts on the first tick");
        Assert::IsTrue  (map.TickPing (10000 + 1000, true), L"its rings are spreading");
        Assert::IsTrue  (map.TickPing (10000 + 1600, true), L"one frame to erase the last ring");
        Assert::IsFalse (map.TickPing (10000 + 2000, true), L"at rest until the next period");
        Assert::IsTrue  (map.TickPing (10000 + 3100, true), L"the next ping");
        Assert::IsTrue  (map.TickPing (10000 + 3200, false), L"a frame to erase it when the picture goes");
        Assert::IsFalse (map.TickPing (10000 + 3300, false));
    }
};
