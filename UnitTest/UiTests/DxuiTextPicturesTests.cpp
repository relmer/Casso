#include "Pch.h"
#include "FakeTextGeometry.h"
#include "FakeTextViewport.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextPicturesTests
//
//  A text row, then a 40-pixel picture's row on lines 1 to 3: the text 6
//  pixels in from the left and top, 16-pixel lines, and room for 400 pixels
//  of picture at 96 DPI.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiTextPicturesTests)
{
public:

    struct Rig
    {
        DxuiTextLayout                    layout;
        FakeTextGeometry                  geometry { 12 + 400, 400 };
        FakeTextViewport                  viewport;
        DxuiTextPictures                  pictures { layout, geometry, viewport };
        std::shared_ptr<DxuiTextPicture>  picture  = std::make_shared<DxuiTextPicture>();

        Rig()
        {
            std::vector<DxuiTextLayout::Row>  rows (2);

            picture->bgra.assign (40 * 40, 0xFF336699);
            picture->width  = 40;
            picture->height = 40;
            picture->pingX  = 20;
            picture->pingY  = 20;
            picture->zoom   = true;

            rows[0].cells   = { L"above" };
            rows[1].cells   = { L"" };
            rows[1].picture = picture;

            layout.SetCellSize (8, 16);
            layout.SetRows (std::move (rows));
            layout.BuildColumnCells();
            layout.BuildLines (50, 1.0f);
        }
    };


    //  The picture starts on its row's first line, a pixel a pixel.
    TEST_METHOD (HitTest_FindsThePixelUnderThePoint)
    {
        Rig  rig;
        int  row = 0;
        int  x   = 0;
        int  y   = 0;

        Assert::AreEqual (4, rig.layout.GetLineCount(), L"a line of text and three of picture");
        Assert::IsTrue   (rig.pictures.HitTest (POINT { 6 + 3, 6 + 16 + 5 }, row, x, y), L"The picture starts on the second line");
        Assert::AreEqual (1, row);
        Assert::AreEqual (3, x);
        Assert::AreEqual (5, y);
        Assert::IsFalse  (rig.pictures.HitTest (POINT { 6 + 3, 6 + 4 }, row, x, y), L"The first line is text");
        Assert::IsFalse  (rig.pictures.HitTest (POINT { 6 + 50, 6 + 16 + 5 }, row, x, y), L"Past the picture's right edge");
    }


    TEST_METHOD (GetPictureLines_IsThePicturesHeightInLines)
    {
        DxuiTextPicture  picture;

        picture.width  = 40;
        picture.height = 40;

        Assert::AreEqual (3, DxuiTextPictures::GetPictureLines (picture, 400.0f, 1.0f, 16), L"40 pixels of 16-pixel lines");
        Assert::AreEqual (2, DxuiTextPictures::GetPictureLines (picture, 20.0f,  1.0f, 16), L"half as tall, fit to half the width");

        picture.height = 1;

        Assert::AreEqual (1, DxuiTextPictures::GetPictureLines (picture, 400.0f, 1.0f, 16), L"at least one line");
    }


    TEST_METHOD (SetOutline_ReportsAMove)
    {
        Rig  rig;

        Assert::IsTrue  (rig.pictures.SetOutline (1, RECT { 0, 0, 5, 5 }));
        Assert::IsFalse (rig.pictures.SetOutline (1, RECT { 0, 0, 5, 5 }), L"the same cell again");
        Assert::IsTrue  (rig.pictures.SetOutline (-1, RECT {}));
    }


    //  The wheel zooms a zoomable picture, and the hit test follows the zoom.
    TEST_METHOD (Zoom_ZoomsAZoomablePictureOnly)
    {
        Rig  rig;
        int  row = 0;
        int  x   = 0;
        int  y   = 0;

        Assert::IsFalse  (rig.pictures.Zoom (POINT { 6 + 3, 6 + 4 }, 1.0f), L"off the picture, the view scrolls");
        Assert::IsTrue   (rig.pictures.Zoom (POINT { 6 + 20, 6 + 16 + 20 }, 4.0f));
        Assert::IsTrue   (rig.pictures.GetMapZoom() > 2.0f);
        Assert::IsTrue   (rig.pictures.HitTest (POINT { 6 + 20, 6 + 16 + 20 }, row, x, y));
        Assert::AreEqual (20, x, L"the pixel under the pointer stays put");

        rig.picture->zoom = false;

        Assert::IsFalse (rig.pictures.Zoom (POINT { 6 + 20, 6 + 16 + 20 }, 1.0f), L"a picture that isn't a map isn't zoomed");
    }


    TEST_METHOD (Pan_StartsOnAZoomablePicture)
    {
        Rig  rig;

        rig.pictures.Zoom (POINT { 6 + 20, 6 + 16 + 20 }, 4.0f);

        Assert::IsFalse (rig.pictures.TryStartPan (POINT { 6 + 3, 6 + 4 }), L"off the picture, the view selects text");
        Assert::IsTrue  (rig.pictures.TryStartPan (POINT { 6 + 20, 6 + 16 + 20 }));

        rig.pictures.Pan (POINT { 6 + 400, 6 + 16 + 400 });
        rig.pictures.EndPan();

        Assert::IsFalse  (rig.pictures.IsPanning());
        Assert::AreEqual (0L, rig.pictures.GetMapOrigin().x);
    }


    TEST_METHOD (IsPinging_OnlyWithAPingPoint)
    {
        Rig  rig;

        Assert::IsTrue (rig.pictures.IsPinging());

        rig.picture->pingX = -1;

        Assert::IsFalse (rig.pictures.IsPinging());
    }
};
