#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr float  s_kStripAspect = 560.0f / 384.0f;





////////////////////////////////////////////////////////////////////////////////
//
//  RecordingStripSource
//
//  Keeps what the strip told it and which cells were clicked.
//
////////////////////////////////////////////////////////////////////////////////

class RecordingStripSource : public IDxuiImageStripSource
{
public:
    void   SetCellLayout   (int count, SIZE cellPx) override { cells = count; size = cellPx; layouts++; }
    Image  GetCellImage    (int index) override              { (void) index; return nullptr; }
    Image  GetPreviewImage (int index) override              { (void) index; return nullptr; }
    void   OnCellClicked   (int index) override              { clicked.push_back (index); }

    std::wstring  GetLeadingLabel() override         { return leading; }
    std::wstring  GetTrailingLabel() override        { return trailing; }
    bool          IsTrailingLabelAccented() override { return isLive; }
    void          OnTrailingLabelClicked() override  { trailingClicks++; }

    bool  TryGetPlayhead (float & outOffset, std::wstring & outTop, std::wstring & outBottom) override
    {
        outOffset = playhead;
        outTop    = playheadTop;
        outBottom = playheadBottom;
        return hasPlayhead;
    }

    int               cells          = -1;
    SIZE              size           = {};
    int               layouts        = 0;
    std::vector<int>  clicked;
    std::wstring      leading;
    std::wstring      trailing;
    bool              isLive         = false;
    int               trailingClicks = 0;
    bool              hasPlayhead    = false;
    float             playhead       = 0.0f;
    std::wstring      playheadTop;
    std::wstring      playheadBottom;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStripTests
//
//  A strip of pictures in a toolbar: cells as thick as the strip at the
//  pictures' aspect ratio, end to end from the leading edge with no gaps and
//  no overlaps, as many as fit whole, standing on end down a side; the
//  pointer finds the cell under it and a click reports it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiImageStripTests)
{
public:

    //  The playhead line is easy to take hold of: a press within the grip
    //  either side of it grabs it, not only one on its pixel.
    TEST_METHOD (APressWithinTheGripTakesHoldOfThePlayhead)
    {
        constexpr float  kGrip = 9.0f;

        Assert::IsTrue  (DxuiImageStrip::IsOnPlayhead (100, 100.0f, kGrip), L"on the line");
        Assert::IsTrue  (DxuiImageStrip::IsOnPlayhead (109, 100.0f, kGrip), L"at the edge of the grip to the right");
        Assert::IsTrue  (DxuiImageStrip::IsOnPlayhead (91,  100.0f, kGrip), L"and to the left");
        Assert::IsFalse (DxuiImageStrip::IsOnPlayhead (110, 100.0f, kGrip), L"past it");
    }


    TEST_METHOD (CellsKeepThePictureAspect)
    {
        Assert::AreEqual (47, DxuiImageStrip::GetCellLength (32, s_kStripAspect, false), L"lying down: 32 * 560 / 384, rounded");
        Assert::AreEqual (22, DxuiImageStrip::GetCellLength (32, s_kStripAspect, true),  L"standing up: 32 * 384 / 560, rounded");
        Assert::AreEqual (0,  DxuiImageStrip::GetCellLength (0,  s_kStripAspect, false), L"no thickness, no cell");
    }


    TEST_METHOD (TheCountIsTheNearestWholeNumberOfCells)
    {
        Assert::AreEqual (10, DxuiImageStrip::GetCellCount (479, 47), L"479 / 47 is nearest ten");
        Assert::AreEqual (11, DxuiImageStrip::GetCellCount (500, 47), L"500 / 47 is nearest eleven");
        Assert::AreEqual (1,  DxuiImageStrip::GetCellCount (20,  47), L"any length holds one");
        Assert::AreEqual (0,  DxuiImageStrip::GetCellCount (0,   47), L"no length");
        Assert::AreEqual (0,  DxuiImageStrip::GetCellCount (100, 0),  L"no cell length");
    }


    TEST_METHOD (CellsFillTheStripWithNoGapsOrOverlaps)
    {
        constexpr RECT  kStrip = { 100, 10, 600, 42 };
        constexpr int   kCount = 11;
        constexpr int   kCell  = 500 / kCount;

        RECT  previous = {};
        RECT  cell     = {};
        int   i        = 0;



        for (i = 0; i < kCount; i++)
        {
            cell = DxuiImageStrip::GetCellRect (kStrip, i, kCount, kCell, false);

            Assert::AreEqual ((int) kStrip.top,    (int) cell.top,    L"as thick as the strip");
            Assert::AreEqual ((int) kStrip.bottom, (int) cell.bottom, L"as thick as the strip");

            if (i < kCount - 1)
            {
                Assert::AreEqual (kCell, (int) (cell.right - cell.left), L"every cell but the last the same length");
            }

            if (i == 0)
            {
                Assert::AreEqual ((int) kStrip.left, (int) cell.left, L"the first starts at the leading edge");
            }
            else
            {
                Assert::AreEqual ((int) previous.right, (int) cell.left, L"each starts where the one before ends");
            }

            previous = cell;
        }

        Assert::AreEqual ((int) kStrip.right, (int) previous.right, L"the last ends at the trailing edge, with no gap");

        cell = DxuiImageStrip::GetCellRect (kStrip, 2, 4, 22, true);
        Assert::AreEqual ((int) kStrip.top + 44, (int) cell.top, L"standing up, cells run down");
        Assert::AreEqual ((int) kStrip.left, (int) cell.left, L"across the whole thickness");
    }


    TEST_METHOD (ThePointerFindsTheCellUnderIt)
    {
        constexpr RECT  kStrip = { 100, 10, 600, 42 };



        Assert::AreEqual (0,  DxuiImageStrip::HitTestCell (kStrip, 11, 45, false, 100, 20), L"the leading edge is the first cell");
        Assert::AreEqual (1,  DxuiImageStrip::HitTestCell (kStrip, 11, 45, false, 145, 20), L"the next cell starts at 145");
        Assert::AreEqual (10, DxuiImageStrip::HitTestCell (kStrip, 11, 45, false, 599, 20), L"the last cell reaches the trailing edge");
        Assert::AreEqual (-1, DxuiImageStrip::HitTestCell (kStrip, 11, 45, false, 600, 20), L"past the strip");
        Assert::AreEqual (-1, DxuiImageStrip::HitTestCell (kStrip, 11, 45, false, 200, 50), L"off the strip");
    }

    TEST_METHOD (LayoutTellsTheSourceAndAClickReportsTheCell)
    {
        DxuiImageStrip        strip;
        RecordingStripSource  source;
        DxuiDpiScaler         scaler;
        bool                  taken = false;



        strip.SetSource (&source);
        strip.SetAspect (s_kStripAspect);
        strip.Layout    (RECT { 0, 0, 479, 32 }, false, scaler);

        Assert::AreEqual (10, source.cells,   L"479 / 47 is nearest ten");
        Assert::AreEqual (47, (int) source.size.cx, L"each the strip's share, close to the aspect");
        Assert::AreEqual (32, (int) source.size.cy, L"and as tall as the strip");

        Assert::IsTrue  (strip.OnLButtonDown (100, 5), L"a press on a cell arms the entry");
        Assert::IsTrue  (strip.OnLButtonDown (475, 5), L"the last cell reaches the end of the strip");

        taken = strip.OnClick (100, 5);

        Assert::IsTrue (taken, L"a click on a cell is taken");
        Assert::AreEqual<size_t> (1, source.clicked.size(), L"one click reported");
        Assert::AreEqual (2, source.clicked[0], L"x 100 is the third cell");

        taken = strip.OnClick (475, 5);
        Assert::IsTrue (taken, L"the end of the strip is the last cell");
        Assert::AreEqual (9, source.clicked[1], L"x 475 is the last cell");

        strip.Layout (RECT { 0, 0, 32, 100 }, false, scaler);

        Assert::AreEqual (5,  source.cells,         L"standing up: 100 / 22 is nearest five");
        Assert::AreEqual (32, (int) source.size.cx, L"as wide as the strip");
        Assert::AreEqual (20, (int) source.size.cy, L"the strip's share of its length");
        Assert::AreEqual (22, strip.GetMinWidthPx (scaler), L"it can shrink to one cell");
    }


    //  A real press makes Windows send a mouse move before the release: the
    //  preview hidden on the press and the capture taken both make one, and a
    //  hand on a real mouse makes more. A move that stays on the cell is not
    //  a drag off it, so the release is still a click.
    TEST_METHOD (AMoveBetweenPressAndReleaseOnTheCellStillClicks)
    {
        DxuiToolbar                      bar;
        DxuiImageStrip                   strip;
        RecordingStripSource             source;
        MockDxuiTextRenderer             text;
        DxuiDpiScaler                    scaler;
        std::vector<DxuiToolbar::Entry>  entries (1);
        auto                             command = std::make_shared<DxuiCommand>();
        constexpr int                    x       = 300;
        constexpr int                    y       = 21;



        command->id    = 1;
        command->label = L"History timeline";

        entries[0].command = command;
        entries[0].custom  = &strip;

        strip.SetSource (&source);
        strip.SetAspect (s_kStripAspect);

        bar.SetTextRenderer (&text);
        bar.SetEntries      (std::move (entries));
        bar.Layout          (RECT { 0, 0, 600, 42 }, scaler);

        Assert::IsTrue (source.cells > 2, L"the strip was laid out with cells");

        bar.OnToolbarMouseMove   (x, y);
        bar.OnToolbarLButtonDown (x, y);
        bar.OnToolbarMouseMove   (x, y);
        bar.OnToolbarMouseMove   (x + 1, y);
        bar.OnToolbarLButtonUp   (x + 1, y);

        Assert::AreEqual<size_t> (1, source.clicked.size(), L"the release after a move on the same cell is a click");
    }


    //  Lying down with label room, the label where the strip begins sits left
    //  of the first picture and the trailing label right of the last, both
    //  plain text centered on the pictures top to bottom; the trailing one is
    //  in the accent color only while the source accents it, and a click on
    //  it goes to the source.
    TEST_METHOD (EndLabelsSitBesideThePicturesCenteredOnThem)
    {
        DxuiImageStrip                 strip;
        RecordingStripSource           source;
        MockDxuiTextRenderer           text;
        MockDxuiPainter                painter;
        MockDxuiTheme                  theme;
        DxuiDpiScaler                  scaler;
        RECT                           pictures = {};
        const RecordedTextCall       * lead     = nullptr;
        const RecordedTextCall       * trail    = nullptr;
        int                            y        = 0;



        source.leading  = L"10:00 PM";
        source.trailing = L"Live";
        source.isLive   = true;

        strip.SetSource       (&source);
        strip.SetAspect       (s_kStripAspect);
        strip.SetTextRenderer (&text);
        strip.SetLabelRoomDp  (16.0f);
        strip.Layout          (RECT { 0, 0, 600, 74 }, true, scaler);
        strip.Paint           (painter, text, theme, false, false, true);

        pictures = strip.GetPicturesRect();
        y        = (pictures.top + pictures.bottom) / 2;

        for (const RecordedTextCall & call : text.Calls())
        {
            lead  = (call.text == source.leading)  ? &call : lead;
            trail = (call.text == source.trailing) ? &call : trail;
        }

        Assert::IsNotNull (lead,  L"the leading label is drawn");
        Assert::IsNotNull (trail, L"the trailing label is drawn");

        Assert::IsTrue   (pictures.left > 0,                       L"the pictures start after the leading label");
        Assert::IsTrue   (lead->x + lead->width <= pictures.left,  L"the leading label is left of the first picture");
        Assert::AreEqual ((float) pictures.top, lead->y,           L"level with the pictures");
        Assert::AreEqual ((float) (pictures.bottom - pictures.top), lead->height, L"as tall as the pictures");
        Assert::IsTrue   (lead->vAlign == DxuiTextVAlign::Center,  L"centered on them");

        Assert::IsTrue   (pictures.right < 600,                    L"the pictures end before the trailing label");
        Assert::IsTrue   (trail->x >= pictures.right,              L"the trailing label is right of the last picture");
        Assert::AreEqual ((float) pictures.top, trail->y,          L"level with the pictures");
        Assert::IsTrue   (trail->vAlign == DxuiTextVAlign::Center, L"centered on them");
        Assert::AreEqual (theme.Accent(), trail->argb,             L"accented while live");

        source.isLive = false;
        text.Reset();
        strip.Paint (painter, text, theme, false, false, true);

        for (const RecordedTextCall & call : text.Calls())
        {
            trail = (call.text == source.trailing) ? &call : trail;
        }

        Assert::AreEqual (theme.Foreground(), trail->argb, L"plain while replaying");

        Assert::IsTrue  (strip.OnLButtonDown (599, y), L"a press on the trailing label is taken");
        Assert::IsTrue  (strip.OnClick       (599, y), L"and its click");
        Assert::AreEqual (1, source.trailingClicks,    L"goes to the source");
        Assert::IsFalse (strip.OnLButtonDown (1, y),   L"the leading label takes no press");
        Assert::IsTrue  (source.clicked.empty(),       L"no cell was clicked");
    }


    //  The cell under the pointer is underlined in the accent color, not
    //  boxed: a box beside the playhead line looks like the line itself.
    TEST_METHOD (TheHoveredCellIsUnderlinedNotBoxed)
    {
        constexpr float       kBarPx   = 4.0f;
        DxuiImageStrip        strip;
        RecordingStripSource  source;
        MockDxuiTextRenderer  text;
        MockDxuiPainter       painter;
        MockDxuiTheme         theme;
        DxuiDpiScaler         scaler;
        RECT                  cell     = {};
        int                   cellPx   = 0;
        bool                  isUnder  = false;
        bool                  isBoxed  = false;



        strip.SetSource (&source);
        strip.SetAspect (s_kStripAspect);
        strip.Layout    (RECT { 0, 0, 479, 32 }, false, scaler);

        cellPx = source.size.cx;
        cell   = DxuiImageStrip::GetCellRect (strip.GetPicturesRect(), 2, strip.GetCellCount(), cellPx, false);

        strip.OnMouseMove (cell.left + 1, 5);
        strip.Paint       (painter, text, theme, false, false, false);

        Assert::AreEqual (2, strip.GetHoveredCell(), L"the pointer is over the third cell");

        for (const RecordedPaintCall & call : painter.Calls())
        {
            if (call.kind != RecordedPaintKind::FillRect || call.argb != theme.Accent())
            {
                continue;
            }

            isUnder = isUnder || (call.x == (float) cell.left && call.width == (float) (cell.right - cell.left) &&
                                  call.y == (float) cell.bottom - kBarPx && call.height == kBarPx);
            isBoxed = isBoxed || call.height > kBarPx;
        }

        Assert::IsTrue  (isUnder, L"a bar along the bottom of the hovered cell");
        Assert::IsFalse (isBoxed, L"no accent fill behind the whole cell");
    }


    //  The playhead line reaches past the pictures above and below, so it
    //  reads as a line across the strip rather than the edge of a cell.
    TEST_METHOD (ThePlayheadReachesPastThePictures)
    {
        DxuiImageStrip                 strip;
        RecordingStripSource           source;
        MockDxuiTextRenderer           text;
        MockDxuiPainter                painter;
        MockDxuiTheme                  theme;
        DxuiDpiScaler                  scaler;
        RECT                           pictures = {};
        const RecordedTextCall       * line     = nullptr;



        source.hasPlayhead = true;
        source.playhead    = 3.0f;

        strip.SetSource       (&source);
        strip.SetAspect       (s_kStripAspect);
        strip.SetTextRenderer (&text);
        strip.SetLabelRoomDp  (20.0f);
        strip.Layout          (RECT { 0, 0, 600, 82 }, true, scaler);
        strip.Paint           (painter, text, theme, false, false, true);

        pictures = strip.GetPicturesRect();

        Assert::AreEqual<size_t> (1, text.IconCalls().size(), L"only the line draws as a picture; the cells are empty");

        line = &text.IconCalls()[0];

        Assert::IsTrue (line->y < (float) pictures.top,                     L"the line starts above the pictures");
        Assert::IsTrue (line->y + line->height > (float) pictures.bottom,   L"and ends below them");
        Assert::IsTrue (line->y >= 0.0f && line->y + line->height <= 82.0f, L"within the strip");
    }


    //  The line's labels stay inside the strip with the line at either end
    //  of it, clear of the part of the line past the pictures, and with room
    //  for a line of text.
    TEST_METHOD (PlayheadLabelsStayInsideTheStripAtBothEnds)
    {
        constexpr LONG                 kWidth   = 600;
        constexpr LONG                 kHeight  = 82;
        DxuiImageStrip                 strip;
        RecordingStripSource           source;
        MockDxuiTextRenderer           text;
        MockDxuiPainter                painter;
        MockDxuiTheme                  theme;
        DxuiDpiScaler                  scaler;
        const RecordedTextCall       * top      = nullptr;
        const RecordedTextCall       * bottom   = nullptr;
        const RecordedTextCall       * line     = nullptr;
        float                          textW    = 0.0f;
        float                          textH    = 0.0f;
        HRESULT                        hr       = S_OK;



        source.hasPlayhead    = true;
        source.playheadTop    = L"10:42:17 PM";
        source.playheadBottom = L"(Power + 1:02:03)";

        strip.SetSource       (&source);
        strip.SetAspect       (s_kStripAspect);
        strip.SetTextRenderer (&text);
        strip.SetLabelRoomDp  (20.0f);
        strip.Layout          (RECT { 0, 0, kWidth, kHeight }, true, scaler);

        hr = text.MeasureString (source.playheadBottom.c_str(), 11.0f, L"", textW, textH);
        Assert::AreEqual (S_OK, hr);

        for (float offset : { 0.0f, (float) strip.GetCellCount() })
        {
            source.playhead = offset;
            text.Reset();
            strip.Paint (painter, text, theme, false, false, true);

            top    = nullptr;
            bottom = nullptr;

            for (const RecordedTextCall & call : text.Calls())
            {
                top    = (call.text == source.playheadTop)    ? &call : top;
                bottom = (call.text == source.playheadBottom) ? &call : bottom;
            }

            Assert::IsNotNull (top,    L"the top label is drawn");
            Assert::IsNotNull (bottom, L"the bottom label is drawn");
            Assert::AreEqual<size_t> (1, text.IconCalls().size(), L"the line is drawn");

            line = &text.IconCalls()[0];

            for (const RecordedTextCall * label : { top, bottom })
            {
                Assert::IsTrue (label->x >= 0.0f,                            L"not past the leading end");
                Assert::IsTrue (label->x + label->width <= (float) kWidth,   L"not past the trailing end");
                Assert::IsTrue (label->y >= 0.0f,                            L"not above the strip");
                Assert::IsTrue (label->y + label->height <= (float) kHeight, L"not below it");
                Assert::IsTrue (label->height >= textH,                      L"tall enough for its text");
            }

            Assert::IsTrue (top->y + top->height <= line->y,                L"the top label clears the line above the pictures");
            Assert::IsTrue (bottom->y >= line->y + line->height,            L"the bottom label clears it below");
        }
    }
};





#ifdef _DEBUG





////////////////////////////////////////////////////////////////////////////////
//
//  PictureStripSource
//
//  Hands out whatever pictures the test has put in place, per cell.
//
////////////////////////////////////////////////////////////////////////////////

class PictureStripSource : public IDxuiImageStripSource
{
public:
    void   SetCellLayout   (int count, SIZE cellPx) override { (void) cellPx; thumbs.resize ((size_t) count); previews.resize ((size_t) count); }
    Image  GetCellImage    (int index) override              { return thumbs[(size_t) index]; }
    Image  GetPreviewImage (int index) override              { return previews[(size_t) index]; }
    void   OnCellClicked   (int index) override              { (void) index; }

    static Image  MakePicture (int width, int height)
    {
        auto  image = std::make_shared<DxuiIconImage>();



        image->width  = width;
        image->height = height;
        image->bgraPremul.assign ((size_t) width * (size_t) height, 0xFF000000u);
        return image;
    }

    std::vector<Image>  thumbs;
    std::vector<Image>  previews;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStripPreviewTests
//
//  The hover preview stays up while the pointer moves over the strip: one
//  popup, opened once, that keeps its picture until the next one is ready.
//  A popup closed and opened again for each cell crossed is a visible
//  flicker, so the number of popups taken from the pool is the measure.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiImageStripPreviewTests)
{
public:

    TEST_METHOD_INITIALIZE (ResetUiThread)
    {
        DxuiResetUiThreadIdForTest();
    }


    TEST_METHOD (CrossingCellsKeepsOnePopupOpen)
    {
        DxuiHwndSource      host (RECT { 0, 0, 1024, 768 }, 6.0f, std::make_unique<DxuiPanel>());
        DxuiImageStrip      strip;
        PictureStripSource  source;
        DxuiDpiScaler       scaler;
        size_t              acquired = 0;
        int                 x        = 0;



        strip.SetSource    (&source);
        strip.SetAspect    (s_kStripAspect);
        strip.SetPopupHost (&host);
        strip.Layout       (RECT { 0, 0, 479, 32 }, false, scaler);

        for (size_t i = 0; i < source.previews.size(); i++)
        {
            source.thumbs[i]   = PictureStripSource::MakePicture (47, 32);
            source.previews[i] = PictureStripSource::MakePicture (280, 192);
        }

        strip.OnMouseMove (5, 5);
        Assert::IsTrue (strip.IsPreviewShown(), L"the first cell's preview is up");

        acquired = host.GetPopupHits() + host.GetPopupMisses();

        for (x = 5; x < 470; x += 3)
        {
            strip.OnMouseMove (x, 5);
            strip.Sync();
            Assert::IsTrue (strip.IsPreviewShown(), L"never closed while the pointer is over the strip");
        }

        Assert::AreEqual (acquired, host.GetPopupHits() + host.GetPopupMisses(), L"one popup for the whole sweep, never reopened");
    }


    TEST_METHOD (ACellNotDrawnYetKeepsThePreviewUp)
    {
        DxuiHwndSource      host (RECT { 0, 0, 1024, 768 }, 6.0f, std::make_unique<DxuiPanel>());
        DxuiImageStrip      strip;
        PictureStripSource  source;
        DxuiDpiScaler       scaler;
        size_t              acquired = 0;



        strip.SetSource    (&source);
        strip.SetAspect    (s_kStripAspect);
        strip.SetPopupHost (&host);
        strip.Layout       (RECT { 0, 0, 479, 32 }, false, scaler);

        source.thumbs[0]   = PictureStripSource::MakePicture (47, 32);
        source.thumbs[1]   = PictureStripSource::MakePicture (47, 32);
        source.previews[0] = PictureStripSource::MakePicture (280, 192);

        strip.OnMouseMove (5, 5);
        acquired = host.GetPopupHits() + host.GetPopupMisses();

        strip.OnMouseMove (52, 5);
        strip.Sync();
        Assert::IsTrue (strip.IsPreviewShown(), L"the next cell's thumbnail stands in until its picture is ready");

        source.previews[1] = PictureStripSource::MakePicture (280, 192);
        strip.Sync();
        Assert::IsTrue (strip.IsPreviewShown(), L"its picture replaces the thumbnail in place");
        Assert::AreEqual (acquired, host.GetPopupHits() + host.GetPopupMisses(), L"the same popup throughout");

        strip.OnMouseLeave();
        Assert::IsFalse (strip.IsPreviewShown(), L"leaving the strip closes it");
    }
};

#endif
