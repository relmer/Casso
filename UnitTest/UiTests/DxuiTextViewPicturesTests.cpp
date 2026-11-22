#include "Pch.h"
#include "FakePictureHost.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewPicturesTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiTextViewPicturesTests)
{
public:

    static void  MakeMap (FakePictureHost & host)
    {
        host.picture.bgra.assign (40 * 40, 0xFF336699);
        host.picture.width  = 40;
        host.picture.height = 40;
        host.picture.pingX  = 20;
        host.picture.pingY  = 20;
        host.picture.zoom   = true;
    }


    //  The picture starts on its row's first line, a pixel a pixel.
    TEST_METHOD (HitTest_FindsThePixelUnderThePoint)
    {
        FakePictureHost       host;
        DxuiTextViewPictures  pictures (host);
        int                   row = 0;
        int                   x   = 0;
        int                   y   = 0;

        MakeMap (host);

        Assert::IsTrue   (pictures.HitTest (POINT { 6 + 3, 6 + 16 + 5 }, row, x, y), L"The picture starts on the second line");
        Assert::AreEqual (1, row);
        Assert::AreEqual (3, x);
        Assert::AreEqual (5, y);
        Assert::IsFalse  (pictures.HitTest (POINT { 6 + 3, 6 + 4 }, row, x, y), L"The first line is text");
        Assert::IsFalse  (pictures.HitTest (POINT { 6 + 50, 6 + 16 + 5 }, row, x, y), L"Past the picture's right edge");
    }


    TEST_METHOD (GetLineCount_IsThePicturesHeightInLines)
    {
        FakePictureHost       host;
        DxuiTextViewPictures  pictures (host);

        MakeMap (host);

        Assert::AreEqual (3, pictures.GetLineCount (1, 400.0f, 16), L"40 pixels of 16-pixel lines");
        Assert::AreEqual (0, pictures.GetLineCount (0, 400.0f, 16), L"a text row has no picture");
    }


    TEST_METHOD (SetOutline_ReportsAMove)
    {
        FakePictureHost       host;
        DxuiTextViewPictures  pictures (host);

        Assert::IsTrue  (pictures.SetOutline (1, RECT { 0, 0, 5, 5 }));
        Assert::IsFalse (pictures.SetOutline (1, RECT { 0, 0, 5, 5 }), L"the same cell again");
        Assert::IsTrue  (pictures.SetOutline (-1, RECT {}));
    }


    //  The wheel zooms a zoomable picture, and the hit test follows the zoom.
    TEST_METHOD (Zoom_ZoomsAZoomablePictureOnly)
    {
        FakePictureHost       host;
        DxuiTextViewPictures  pictures (host);
        int                   row = 0;
        int                   x   = 0;
        int                   y   = 0;

        MakeMap (host);

        Assert::IsFalse (pictures.Zoom (POINT { 6 + 3, 6 + 4 }, 1.0f), L"off the picture, the view scrolls");
        Assert::IsTrue  (pictures.Zoom (POINT { 6 + 20, 6 + 16 + 20 }, 4.0f));
        Assert::IsTrue  (pictures.GetMapZoom() > 2.0f);
        Assert::IsTrue  (pictures.HitTest (POINT { 6 + 20, 6 + 16 + 20 }, row, x, y));
        Assert::AreEqual (20, x, L"the pixel under the pointer stays put");

        host.picture.zoom = false;

        Assert::IsFalse (pictures.Zoom (POINT { 6 + 20, 6 + 16 + 20 }, 1.0f), L"a picture that isn't a map isn't zoomed");
    }


    TEST_METHOD (Pan_StartsOnAZoomablePicture)
    {
        FakePictureHost       host;
        DxuiTextViewPictures  pictures (host);

        MakeMap (host);
        pictures.Zoom (POINT { 6 + 20, 6 + 16 + 20 }, 4.0f);

        Assert::IsFalse (pictures.TryStartPan (POINT { 6 + 3, 6 + 4 }), L"off the picture, the view selects text");
        Assert::IsTrue  (pictures.TryStartPan (POINT { 6 + 20, 6 + 16 + 20 }));

        pictures.Pan (POINT { 6 + 400, 6 + 16 + 400 });
        pictures.EndPan();

        Assert::IsFalse  (pictures.IsPanning());
        Assert::AreEqual (0L, pictures.GetMapOrigin().x);
    }


    TEST_METHOD (IsPinging_OnlyWithAPingPoint)
    {
        FakePictureHost       host;
        DxuiTextViewPictures  pictures (host);

        MakeMap (host);

        Assert::IsTrue (pictures.IsPinging());

        host.picture.pingX = -1;

        Assert::IsFalse (pictures.IsPinging());
    }
};
