#include "Pch.h"

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

    static constexpr int  kPadPx   = 6;
    static constexpr int  kLinePx  = 16;
    static constexpr int  kLines   = 4;

    class FakeHost : public IDxuiTextPictureHost
    {
    public:
        FakeHost() { m_scaler.SetDpi (96); }

        int                      GetPictureLineCount() const override          { return kLines; }
        int                      GetPictureLineRow   (int line) const override { return (line == 0) ? 0 : 1; }
        int                      GetPictureLineTopPx (int line) const override { return kPadPx + line * kLinePx; }
        int                      GetPictureLineAtPx  (int y) const override    { return (y >= kPadPx && y < kPadPx + kLines * kLinePx) ? (y - kPadPx) / kLinePx : -1; }
        int                      GetPictureRowCount  () const override         { return 2; }
        const DxuiTextPicture  * GetRowPicture       (int row) const override  { return (row == 1) ? picture.get() : nullptr; }
        float                    GetPictureLeftPx    () const override         { return (float) kPadPx; }
        float                    GetPictureWidthPx   () const override         { return 400.0f; }
        const DxuiDpiScaler    & GetPictureScaler    () const override         { return m_scaler; }

        std::shared_ptr<DxuiTextPicture>  picture = std::make_shared<DxuiTextPicture>();

    private:
        DxuiDpiScaler  m_scaler;
    };


    struct Rig
    {
        FakeHost                            host;
        DxuiTextPictures                    pictures { host };
        std::shared_ptr<DxuiTextPicture>  & picture  = host.picture;

        Rig()
        {
            picture->bgra.assign (40 * 40, 0xFF336699);
            picture->width  = 40;
            picture->height = 40;
            picture->pingX  = 20;
            picture->pingY  = 20;
            picture->zoom   = true;
        }
    };

    //  The picture starts on its row's first line, a pixel a pixel.
    TEST_METHOD (HitTest_FindsThePixelUnderThePoint)
    {
        Rig  rig;
        int  row = 0;
        int  x   = 0;
        int  y   = 0;

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
