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

    int           GetMarkedCell() override            { return marked; }
    std::wstring  GetLeadingLabel() override          { return leading; }
    std::wstring  GetLeadingTip() override            { return leadingTip; }
    void          OnLeadingLabelClicked() override    { leadingClicks++; }
    std::wstring  GetTrailingLabel() override         { return trailing; }
    bool          IsTrailingLabelAccented() override  { return isLive; }
    bool          IsTrailingLabelClickable() override { return !isLive; }
    std::wstring  GetTrailingTip() override           { return trailingTip; }
    void          OnTrailingLabelClicked() override   { trailingClicks++; }

    bool  TryGetPlayhead (float & outOffset, std::wstring & outTop, std::wstring & outBottom) override
    {
        outOffset = playhead;
        outTop    = playheadTop;
        outBottom = playheadBottom;
        return hasPlayhead;
    }

    bool  TryGetCellLabels (int index, std::wstring & outTop, std::wstring & outBottom) override
    {
        (void) index;
        outTop    = cellTop;
        outBottom = cellBottom;
        return !cellTop.empty();
    }

    //  With isLabeledByOffset, each point's labels give its offset.
    bool  TryGetLabelsAt (int index, float offset, std::wstring & outTop, std::wstring & outBottom) override
    {
        if (!isLabeledByOffset)
        {
            return IDxuiImageStripSource::TryGetLabelsAt (index, offset, outTop, outBottom);
        }

        outTop    = GetTopLabel (offset);
        outBottom = std::format (L"below {:.4f}", offset);
        return true;
    }

    void  OnStripClicked (int index, float offset) override
    {
        offsets.push_back (offset);
        IDxuiImageStripSource::OnStripClicked (index, offset);
    }

    void  OnPlayheadDragged (float offset, bool isFinal) override
    {
        drags.push_back (offset);
        finals.push_back (isFinal);
    }

    static std::wstring  GetTopLabel (float offset) { return std::format (L"above {:.4f}", offset); }

    int                 cells             = -1;
    SIZE                size              = {};
    int                 layouts           = 0;
    std::vector<int>    clicked;
    std::wstring        leading;
    std::wstring        leadingTip;
    int                 leadingClicks     = 0;
    std::wstring        trailing;
    std::wstring        trailingTip;
    bool                isLive            = false;
    int                 trailingClicks    = 0;
    bool                hasPlayhead       = false;
    float               playhead          = 0.0f;
    std::wstring        playheadTop;
    std::wstring        playheadBottom;
    int                 marked            = -1;
    std::wstring        cellTop;
    std::wstring        cellBottom;
    bool                isLabeledByOffset = false;
    std::vector<float>  offsets;                     // where each click on the pictures landed, in cells
    std::vector<float>  drags;                       // where the playhead line was dragged to, in cells
    std::vector<bool>   finals;
};





////////////////////////////////////////////////////////////////////////////////
//
//  LightMockTheme
//
//  The mock theme on a light background.
//
////////////////////////////////////////////////////////////////////////////////

class LightMockTheme : public MockDxuiTheme
{
public:
    uint32_t  Background() const override { return 0xFFFBFBFBu; }
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
        Assert::IsTrue  (strip.OnLButtonDown (1, y),   L"a press on the leading label is taken");
        Assert::IsTrue  (strip.OnClick       (1, y),   L"and its click");
        Assert::AreEqual (1, source.leadingClicks,     L"goes to the source");
        Assert::IsTrue  (source.clicked.empty(),       L"no cell was clicked");
    }


    //  Both end labels are buttons: the pointer over one gives it the
    //  toolbar's hover chrome from the theme, a press its pressed chrome,
    //  and it has the source's tip. Nothing else on the strip takes either.
    TEST_METHOD (EndLabelsAreButtonsWithHoverPressedAndTips)
    {
        DxuiImageStrip                 strip;
        RecordingStripSource           source;
        MockDxuiTextRenderer           text;
        MockDxuiPainter                painter;
        MockDxuiTheme                  theme;
        DxuiDpiScaler                  scaler;
        RECT                           pictures = {};
        RECT                           anchor   = {};
        RecordedTextCall               lead;
        RecordedTextCall               trail;
        const wchar_t                * tip      = nullptr;
        POINT                          onLead   = {};
        POINT                          onTrail  = {};
        int                            y        = 0;



        source.leading     = L"10:00 PM";
        source.leadingTip  = L"Go to the start of history";
        source.trailing    = L"Live";
        source.trailingTip = L"Return to live";

        strip.SetSource       (&source);
        strip.SetAspect       (s_kStripAspect);
        strip.SetTextRenderer (&text);
        strip.SetLabelRoomDp  (16.0f);
        strip.Layout          (RECT { 0, 0, 600, 74 }, true, scaler);
        strip.Paint           (painter, text, theme, false, false, true);

        pictures = strip.GetPicturesRect();
        y        = (pictures.top + pictures.bottom) / 2;
        Assert::IsNotNull (FindText (text, source.leading),  L"the leading label is drawn");
        Assert::IsNotNull (FindText (text, source.trailing), L"the trailing label is drawn");

        lead    = *FindText (text, source.leading);
        trail   = *FindText (text, source.trailing);
        onLead  = POINT { (LONG) lead.x + 1,  y };
        onTrail = POINT { (LONG) trail.x + 1, y };

        Assert::IsFalse (HasButtonChrome (painter, theme.ButtonHover()),   L"idle, neither label has chrome");
        Assert::IsFalse (HasButtonChrome (painter, theme.ButtonPressed()), L"idle, neither label has chrome");

        for (POINT pt : { onLead, onTrail })
        {
            const RecordedTextCall  * label = (pt.x == onLead.x) ? &lead : &trail;

            strip.OnMouseMove (pt.x, pt.y);
            painter.Reset();
            strip.Paint (painter, text, theme, false, false, true);

            Assert::IsTrue (HasButtonChrome (painter, theme.ButtonHover(), label), L"the pointer over a label gives it the hover chrome around its text");

            strip.OnLButtonDown (pt.x, pt.y);
            painter.Reset();
            strip.Paint (painter, text, theme, false, false, true);

            Assert::IsTrue  (HasButtonChrome (painter, theme.ButtonPressed(), label), L"a press gives it the pressed chrome");
            Assert::IsFalse (HasButtonChrome (painter, theme.ButtonHover()),          L"in place of the hover chrome");

            strip.OnLButtonUp (pt.x, pt.y);
            strip.OnClick     (pt.x, pt.y);
            painter.Reset();
            strip.Paint (painter, text, theme, false, false, true);

            Assert::IsFalse (HasButtonChrome (painter, theme.ButtonPressed()), L"the release lets it up");
        }

        tip = strip.GetTooltipAt (onLead.x, onLead.y, anchor);
        Assert::IsNotNull (tip, L"the leading label has a tip");
        Assert::AreEqual (source.leadingTip, std::wstring (tip), L"the source's");
        Assert::IsTrue (anchor.right <= pictures.left, L"anchored on the label");

        tip = strip.GetTooltipAt (onTrail.x, onTrail.y, anchor);
        Assert::IsNotNull (tip, L"the trailing label has a tip");
        Assert::AreEqual (source.trailingTip, std::wstring (tip), L"the source's");

        Assert::IsNull (strip.GetTooltipAt (pictures.left + 5, y, anchor), L"the pictures have no tip");

        strip.OnMouseMove (pictures.left + 5, y);
        painter.Reset();
        strip.Paint (painter, text, theme, false, false, true);

        Assert::IsFalse (HasButtonChrome (painter, theme.ButtonHover()), L"over the pictures, neither label has chrome");

        strip.OnMouseMove (onLead.x, onLead.y);
        strip.OnMouseLeave();
        painter.Reset();
        strip.Paint (painter, text, theme, false, false, true);

        Assert::IsFalse (HasButtonChrome (painter, theme.ButtonHover()), L"leaving the strip takes the hover away");
        Assert::AreEqual (1, source.leadingClicks,  L"the leading label was clicked once");
        Assert::AreEqual (1, source.trailingClicks, L"and the trailing one once");
    }


    //  While the source is live, Live is a label, not a button: no hover or
    //  pressed chrome and no click, but it keeps its tip. The leading label
    //  is a button all the same.
    TEST_METHOD (LiveIsNoButtonWhileLive)
    {
        DxuiImageStrip         strip;
        RecordingStripSource   source;
        MockDxuiTextRenderer   text;
        MockDxuiPainter        painter;
        MockDxuiTheme          theme;
        DxuiDpiScaler          scaler;
        RECT                   pictures = {};
        RECT                   anchor   = {};
        RecordedTextCall       trail;
        RecordedTextCall       lead;
        const wchar_t        * tip      = nullptr;
        int                    onTrail  = 0;
        int                    onLead   = 0;
        int                    y        = 0;



        source.leading     = L"10:00 PM";
        source.trailing    = L"Live";
        source.trailingTip = L"Running live";
        source.isLive      = true;

        strip.SetSource       (&source);
        strip.SetAspect       (s_kStripAspect);
        strip.SetTextRenderer (&text);
        strip.SetLabelRoomDp  (16.0f);
        strip.Layout          (RECT { 0, 0, 600, 74 }, true, scaler);
        strip.Paint           (painter, text, theme, false, false, true);

        pictures = strip.GetPicturesRect();
        y        = (pictures.top + pictures.bottom) / 2;
        Assert::IsNotNull (FindText (text, source.trailing), L"the trailing label is drawn");
        Assert::IsNotNull (FindText (text, source.leading),  L"the leading label is drawn");

        trail   = *FindText (text, source.trailing);
        lead    = *FindText (text, source.leading);
        onTrail = (int) trail.x + 1;
        onLead  = (int) lead.x + 1;

        strip.OnMouseMove (onTrail, y);
        painter.Reset();
        strip.Paint (painter, text, theme, false, false, true);
        Assert::IsFalse (HasButtonChrome (painter, theme.ButtonHover()), L"no hover chrome over Live while live");

        strip.OnLButtonDown (onTrail, y);
        painter.Reset();
        strip.Paint (painter, text, theme, false, false, true);
        Assert::IsFalse (HasButtonChrome (painter, theme.ButtonPressed()), L"no pressed chrome on Live while live");

        strip.OnLButtonUp (onTrail, y);
        strip.OnClick     (onTrail, y);
        Assert::AreEqual (0, source.trailingClicks, L"a click on Live while live goes nowhere");
        Assert::IsTrue   (source.clicked.empty(),   L"and is no click on a cell");

        tip = strip.GetTooltipAt (onTrail, y, anchor);
        Assert::IsNotNull (tip, L"Live keeps its tip while live");
        Assert::AreEqual  (source.trailingTip, std::wstring (tip), L"the source's");

        strip.OnMouseMove (onLead, y);
        painter.Reset();
        strip.Paint (painter, text, theme, false, false, true);
        Assert::IsTrue (HasButtonChrome (painter, theme.ButtonHover(), &lead), L"the start time is a button all the same");

        source.isLive = false;
        strip.OnMouseMove (onTrail, y);
        painter.Reset();
        strip.Paint (painter, text, theme, false, false, true);
        Assert::IsTrue (HasButtonChrome (painter, theme.ButtonHover(), &trail), L"replaying, Live is a button again");
    }


    //  The cell under the pointer is framed by a thin line in the theme's
    //  hover frame color, over the picture's edge; the marked cell keeps its
    //  accent underline, and the hovered one has none.
    TEST_METHOD (TheHoveredCellIsFramedAndTheMarkedCellUnderlined)
    {
        constexpr float       kBarPx   = 4.0f;
        constexpr float       kThinPx  = 2.0f;
        DxuiImageStrip        strip;
        RecordingStripSource  source;
        MockDxuiTextRenderer  text;
        MockDxuiPainter       painter;
        MockDxuiTheme         theme;
        DxuiDpiScaler         scaler;
        RECT                  hovered  = {};
        RECT                  marked   = {};
        RECT                  frame    = {};
        int                   cellPx   = 0;
        int                   pieces   = 0;
        bool                  isUnder  = false;
        bool                  isHovBar = false;



        source.marked = 4;

        strip.SetSource (&source);
        strip.SetAspect (s_kStripAspect);
        strip.Layout    (RECT { 0, 0, 479, 32 }, false, scaler);

        cellPx  = source.size.cx;
        hovered = DxuiImageStrip::GetCellRect (strip.GetPicturesRect(), 2, strip.GetCellCount(), cellPx, false);
        marked  = DxuiImageStrip::GetCellRect (strip.GetPicturesRect(), 4, strip.GetCellCount(), cellPx, false);

        strip.OnMouseMove (hovered.left + 1, 5);
        strip.Paint       (painter, text, theme, false, false, false);

        Assert::AreEqual (2, strip.GetHoveredCell(), L"the pointer is over the third cell");

        for (const RecordedPaintCall & call : painter.Calls())
        {
            if (call.kind != RecordedPaintKind::FillRect || call.argb != theme.Accent())
            {
                continue;
            }

            isUnder  = isUnder  || (call.x == (float) marked.left && call.y == (float) marked.bottom - kBarPx && call.height == kBarPx);
            isHovBar = isHovBar || (call.x < (float) hovered.right && call.x + call.width > (float) hovered.left);
        }

        Assert::IsTrue  (isUnder,  L"the marked cell keeps its accent underline");
        Assert::IsFalse (isHovBar, L"the hovered cell has no accent bar");

        pieces = GetIconBounds (text, theme.PictureHoverFrame(), kThinPx, frame);

        Assert::IsTrue   (pieces >= 4,                  L"a frame of four thin sides in the hover frame color");
        Assert::AreEqual (hovered.left,   frame.left,   L"along the cell's left edge");
        Assert::AreEqual (hovered.right,  frame.right,  L"its right edge");
        Assert::AreEqual (hovered.top,    frame.top,    L"its top");
        Assert::AreEqual (hovered.bottom, frame.bottom, L"and its bottom");
        Assert::AreEqual (0u, theme.PictureHoverFrameEdge(), L"no outer line on a dark background");
    }


    //  On a light background the near-white frame gets a dark line outside
    //  it, so its edge does not run into the panel.
    TEST_METHOD (OnALightThemeTheHoverFrameHasADarkOuterLine)
    {
        constexpr float       kThinPx  = 2.0f;
        DxuiImageStrip        strip;
        RecordingStripSource  source;
        MockDxuiTextRenderer  text;
        MockDxuiPainter       painter;
        LightMockTheme        theme;
        DxuiDpiScaler         scaler;
        RECT                  hovered  = {};
        RECT                  edge     = {};
        RECT                  frame    = {};
        int                   pieces   = 0;



        strip.SetSource (&source);
        strip.SetAspect (s_kStripAspect);
        strip.Layout    (RECT { 0, 0, 479, 32 }, false, scaler);

        hovered = DxuiImageStrip::GetCellRect (strip.GetPicturesRect(), 2, strip.GetCellCount(), source.size.cx, false);

        strip.OnMouseMove (hovered.left + 1, 5);
        strip.Paint       (painter, text, theme, false, false, false);

        Assert::AreNotEqual (0u, theme.PictureHoverFrameEdge(), L"a light background has an outer line");

        pieces = GetIconBounds (text, theme.PictureHoverFrameEdge(), kThinPx, edge);

        Assert::IsTrue   (pieces >= 4,                 L"the outer line goes all the way round");
        Assert::AreEqual (hovered.left,   edge.left,   L"at the cell's edges");
        Assert::AreEqual (hovered.bottom, edge.bottom, L"at the cell's edges");

        pieces = GetIconBounds (text, theme.PictureHoverFrame(), kThinPx, frame);

        Assert::IsTrue (pieces >= 4,                L"the frame is still drawn");
        Assert::IsTrue (frame.left > edge.left,     L"inside the outer line");
        Assert::IsTrue (frame.bottom < edge.bottom, L"inside the outer line");
    }


    //  Over a cell, the source's labels there show above it and below it, in
    //  the room kept for the playhead's labels, centered on the pointer and
    //  kept within the strip at either end. Off the strip they are gone.
    TEST_METHOD (TheHoveredCellsTimeShowsAboveAndBelowIt)
    {
        constexpr LONG                 kWidth   = 600;
        constexpr LONG                 kHeight  = 82;
        DxuiImageStrip                 strip;
        RecordingStripSource           source;
        MockDxuiTextRenderer           text;
        MockDxuiPainter                painter;
        MockDxuiTheme                  theme;
        DxuiDpiScaler                  scaler;
        RECT                           pictures = {};
        RECT                           cell     = {};
        const RecordedTextCall       * top      = nullptr;
        const RecordedTextCall       * bottom   = nullptr;
        int                            count    = 0;



        source.cellTop    = L"10:42:17 PM";
        source.cellBottom = L"(Power + 1:02:03)";

        strip.SetSource       (&source);
        strip.SetAspect       (s_kStripAspect);
        strip.SetTextRenderer (&text);
        strip.SetLabelRoomDp  (20.0f);
        strip.Layout          (RECT { 0, 0, kWidth, kHeight }, true, scaler);

        pictures = strip.GetPicturesRect();
        count    = strip.GetCellCount();

        Assert::IsTrue (count > 2, L"cells to hover");

        for (int index : { 0, count / 2, count - 1 })
        {
            cell = DxuiImageStrip::GetCellRect (pictures, index, count, source.size.cx, false);

            strip.OnMouseMove ((cell.left + cell.right) / 2, (pictures.top + pictures.bottom) / 2);
            text.Reset();
            strip.Paint (painter, text, theme, false, false, true);

            top    = FindText (text, source.cellTop);
            bottom = FindText (text, source.cellBottom);

            Assert::IsNotNull (top,    L"the cell's time is drawn");
            Assert::IsNotNull (bottom, L"its time since power-on is drawn");

            Assert::IsTrue (top->y >= 0.0f && top->y + top->height <= (float) pictures.top,           L"the time sits above the pictures, inside the strip");
            Assert::IsTrue (bottom->y >= (float) pictures.bottom && bottom->y + bottom->height <= (float) kHeight, L"the power time below them, inside the strip");

            for (const RecordedTextCall * label : { top, bottom })
            {
                Assert::IsTrue (label->x >= 0.0f,                          L"not past the leading end");
                Assert::IsTrue (label->x + label->width <= (float) kWidth, L"not past the trailing end");
            }

            if (index == count / 2)
            {
                Assert::AreEqual ((float) (cell.left + cell.right) / 2.0f, top->x + top->width / 2.0f, 0.5f, L"centered on the pointer, here mid-cell");
            }
        }

        strip.OnMouseLeave();
        text.Reset();
        strip.Paint (painter, text, theme, false, false, true);

        Assert::IsNull (FindText (text, source.cellTop), L"gone once the pointer leaves");
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


    //  Every pixel along the pictures is a point of its own: the first is
    //  the leading end, the last the trailing end, each one further on than
    //  the one before, and the line drawn at a pixel's offset stands on that
    //  pixel. Standing up, the same runs top to bottom.
    TEST_METHOD (EveryPixelAlongThePicturesIsAnOffsetOfItsOwn)
    {
        DxuiImageStrip        strip;
        RecordingStripSource  source;
        MockDxuiTextRenderer  text;
        DxuiDpiScaler         scaler;
        RECT                  pictures = {};
        float                 offset   = 0.0f;
        float                 previous = -1.0f;
        int                   y        = 0;
        int                   x        = 0;



        strip.SetSource       (&source);
        strip.SetAspect       (s_kStripAspect);
        strip.SetTextRenderer (&text);
        strip.SetLabelRoomDp  (20.0f);
        strip.Layout          (RECT { 0, 0, 600, 82 }, true, scaler);

        pictures = strip.GetPicturesRect();
        y        = (pictures.top + pictures.bottom) / 2;

        Assert::IsTrue (strip.GetCellCount() > 2, L"cells to cross");

        for (x = pictures.left; x < pictures.right; x++)
        {
            offset = strip.GetOffsetAt (x, y);

            Assert::IsTrue (offset > previous, L"each pixel further on than the one before");
            Assert::AreEqual ((float) x, strip.GetLineAlong (offset), 0.01f, L"the line at a pixel's offset stands on the pixel");

            previous = offset;
        }

        Assert::AreEqual (0.0f,                         strip.GetOffsetAt (pictures.left, y),      L"the first pixel is the leading end");
        Assert::AreEqual ((float) strip.GetCellCount(), strip.GetOffsetAt (pictures.right - 1, y), L"the last pixel is the trailing end");
        Assert::AreEqual (0.0f,                         strip.GetOffsetAt (pictures.left - 9, y),  L"before the pictures: the leading end");
        Assert::AreEqual ((float) strip.GetCellCount(), strip.GetOffsetAt (pictures.right + 9, y), L"past them: the trailing end");

        strip.Layout (RECT { 0, 0, 40, 400 }, true, scaler);

        pictures = strip.GetPicturesRect();

        Assert::AreEqual (0.0f,                         strip.GetOffsetAt (20, pictures.top),        L"standing up, the top pixel is the leading end");
        Assert::AreEqual ((float) strip.GetCellCount(), strip.GetOffsetAt (20, pictures.bottom - 1), L"and the bottom pixel the trailing end");
    }


    //  A click anywhere on the pictures gives the source the exact point
    //  under the pointer, not only the cell it is in.
    TEST_METHOD (AClickGivesTheSourceThePointUnderThePointer)
    {
        DxuiImageStrip        strip;
        RecordingStripSource  source;
        DxuiDpiScaler         scaler;
        RECT                  pictures = {};
        RECT                  cell     = {};
        int                   y        = 0;
        int                   x        = 0;



        strip.SetSource (&source);
        strip.SetAspect (s_kStripAspect);
        strip.Layout    (RECT { 0, 0, 479, 32 }, false, scaler);

        pictures = strip.GetPicturesRect();
        cell     = DxuiImageStrip::GetCellRect (pictures, 2, strip.GetCellCount(), source.size.cx, false);
        x        = cell.left + 7;
        y        = (pictures.top + pictures.bottom) / 2;

        strip.OnLButtonDown (x, y);
        strip.OnClick       (x, y);

        strip.OnLButtonDown (pictures.right - 1, y);
        strip.OnClick       (pictures.right - 1, y);

        Assert::AreEqual<size_t> (2, source.offsets.size(), L"both clicks give the point");
        Assert::AreEqual (strip.GetOffsetAt (x, y), source.offsets[0], L"the point under the pointer");
        Assert::IsTrue   (source.offsets[0] > 2.0f && source.offsets[0] < 3.0f, L"partway along the third cell");
        Assert::AreEqual ((float) strip.GetCellCount(), source.offsets[1], L"the last pixel is the trailing end");
        Assert::AreEqual (2, source.clicked[0], L"and the cell it is in");
    }


    //  The line dragged follows the pointer pixel by pixel, the source hears
    //  each pixel's point, the line's labels give the point under the
    //  pointer, and letting go gives the point where it was let go.
    TEST_METHOD (ADraggedPlayheadFollowsThePointerPixelByPixel)
    {
        DxuiImageStrip                 strip;
        RecordingStripSource           source;
        MockDxuiTextRenderer           text;
        MockDxuiPainter                painter;
        MockDxuiTheme                  theme;
        DxuiDpiScaler                  scaler;
        RECT                           pictures = {};
        const RecordedTextCall       * line     = nullptr;
        int                            x        = 0;
        int                            y        = 0;
        int                            step     = 0;



        source.hasPlayhead       = true;
        source.playhead          = 2.0f;
        source.playheadTop       = L"where the machine stands";
        source.isLabeledByOffset = true;

        strip.SetSource       (&source);
        strip.SetAspect       (s_kStripAspect);
        strip.SetTextRenderer (&text);
        strip.SetLabelRoomDp  (20.0f);
        strip.Layout          (RECT { 0, 0, 600, 82 }, true, scaler);

        pictures = strip.GetPicturesRect();
        x        = (int) std::lround (strip.GetLineAlong (source.playhead));
        y        = (pictures.top + pictures.bottom) / 2;

        Assert::IsTrue (strip.OnLButtonDown (x, y), L"the press takes the line");
        Assert::IsTrue (strip.IsDraggingPlayhead(), L"and drags it");

        for (step = 1; step <= 5; step++)
        {
            strip.OnMouseMove (x + step, y);

            Assert::AreEqual<size_t> ((size_t) step, source.drags.size(), L"every pixel moved is heard");
            Assert::AreEqual (strip.GetOffsetAt (x + step, y), source.drags.back(), L"at the point under the pointer");
            Assert::IsFalse  (source.finals.back(), L"while it is held");
        }

        text.Reset();
        strip.Paint (painter, text, theme, false, false, true);

        Assert::AreEqual<size_t> (1, text.IconCalls().size(), L"the line is drawn");
        line = &text.IconCalls()[0];

        Assert::AreEqual ((float) (x + 5), line->x + line->width / 2.0f, 0.01f, L"under the pointer");
        Assert::IsNotNull (FindText (text, RecordingStripSource::GetTopLabel (strip.GetOffsetAt (x + 5, y))), L"labeled with the point under the pointer");
        Assert::IsNull    (FindText (text, source.playheadTop), L"not where the machine stood");

        strip.OnLButtonUp (x + 7, y);

        Assert::IsTrue   (source.finals.back(), L"letting go is final");
        Assert::AreEqual (strip.GetOffsetAt (x + 7, y), source.drags.back(), L"at the point where it was let go");
    }


    //  Over the pictures, the labels give the point under the pointer,
    //  centered on it, and change as it moves within a cell.
    TEST_METHOD (HoverLabelsGiveThePointUnderThePointer)
    {
        DxuiImageStrip                 strip;
        RecordingStripSource           source;
        MockDxuiTextRenderer           text;
        MockDxuiPainter                painter;
        MockDxuiTheme                  theme;
        DxuiDpiScaler                  scaler;
        RECT                           pictures = {};
        RECT                           cell     = {};
        const RecordedTextCall       * top      = nullptr;
        int                            y        = 0;



        source.isLabeledByOffset = true;

        strip.SetSource       (&source);
        strip.SetAspect       (s_kStripAspect);
        strip.SetTextRenderer (&text);
        strip.SetLabelRoomDp  (20.0f);
        strip.Layout          (RECT { 0, 0, 600, 82 }, true, scaler);

        pictures = strip.GetPicturesRect();
        cell     = DxuiImageStrip::GetCellRect (pictures, 2, strip.GetCellCount(), source.size.cx, false);
        y        = (pictures.top + pictures.bottom) / 2;

        for (int x : { (int) cell.left + 4, (int) cell.right - 4 })
        {
            strip.OnMouseMove (x, y);
            text.Reset();
            strip.Paint (painter, text, theme, false, false, true);

            top = FindText (text, RecordingStripSource::GetTopLabel (strip.GetOffsetAt (x, y)));

            Assert::AreEqual (2, strip.GetHoveredCell(), L"over the same cell");
            Assert::IsNotNull (top, L"the label gives the point under the pointer");
            Assert::AreEqual ((float) x, top->x + top->width / 2.0f, 0.5f, L"centered on the pointer");
        }
    }


    //  The pointer over the playhead line's grip shows the left and right
    //  resize cursor, up and down standing up, and keeps it while the line
    //  is dragged wherever the pointer goes; elsewhere the strip leaves the
    //  cursor alone. The toolbar holding the strip shows it too.
    TEST_METHOD (ThePlayheadLineShowsTheResizeCursor)
    {
        DxuiToolbar                      bar;
        DxuiImageStrip                   strip;
        RecordingStripSource             source;
        MockDxuiTextRenderer             text;
        DxuiDpiScaler                    scaler;
        std::vector<DxuiToolbar::Entry>  entries (1);
        auto                             command  = std::make_shared<DxuiCommand>();
        RECT                             pictures = {};
        int                              x        = 0;
        int                              y        = 0;



        source.hasPlayhead = true;
        source.playhead    = 2.0f;

        strip.SetSource       (&source);
        strip.SetAspect       (s_kStripAspect);
        strip.SetTextRenderer (&text);
        strip.Layout          (RECT { 0, 0, 600, 42 }, true, scaler);

        pictures = strip.GetPicturesRect();
        x        = (int) std::lround (strip.GetLineAlong (source.playhead));
        y        = (pictures.top + pictures.bottom) / 2;

        Assert::IsTrue (strip.GetCursorAt (x,      y) == IDC_SIZEWE, L"on the line");
        Assert::IsTrue (strip.GetCursorAt (x + 8,  y) == IDC_SIZEWE, L"within its grip");
        Assert::IsTrue (strip.GetCursorAt (x + 40, y) == nullptr,    L"away from it");

        strip.OnLButtonDown (x, y);

        Assert::IsTrue (strip.GetCursorAt (x + 200, y + 200) == IDC_SIZEWE, L"kept while dragged, wherever the pointer is");

        strip.OnLButtonUp (x + 200, y);

        Assert::IsTrue (strip.GetCursorAt (x + 200, y + 200) == nullptr, L"until it is let go");

        source.hasPlayhead = false;
        Assert::IsTrue (strip.GetCursorAt (x, y) == nullptr, L"no line, no grip");
        source.hasPlayhead = true;

        strip.Layout (RECT { 0, 0, 40, 400 }, true, scaler);
        Assert::IsTrue (strip.GetCursorAt (20, (int) std::lround (strip.GetLineAlong (source.playhead))) == IDC_SIZENS, L"standing up, up and down");

        command->id    = 1;
        command->label = L"History timeline";

        entries[0].command = command;
        entries[0].custom  = &strip;

        bar.SetTextRenderer (&text);
        bar.SetEntries      (std::move (entries));
        bar.Layout          (RECT { 0, 0, 600, 42 }, scaler);

        pictures = strip.GetPicturesRect();
        x        = (int) std::lround (strip.GetLineAlong (source.playhead));
        y        = (pictures.top + pictures.bottom) / 2;

        Assert::IsTrue (bar.GetCursorForPoint (POINT { x, y }) == IDC_SIZEWE, L"the toolbar shows the strip's cursor");
    }

private:

    //  How many lines in `argb` were drawn, each no thicker than `thinPx`
    //  across, and the rect they span together.
    static int GetIconBounds (const MockDxuiTextRenderer & text, uint32_t argb, float thinPx, RECT & outBounds)
    {
        int  pieces = 0;



        outBounds = RECT { LONG_MAX, LONG_MAX, LONG_MIN, LONG_MIN };

        for (const RecordedTextCall & call : text.IconCalls())
        {
            if (call.argb != argb || (std::min) (call.width, call.height) > thinPx)
            {
                continue;
            }

            outBounds.left   = (std::min) (outBounds.left,   (LONG) std::lround (call.x));
            outBounds.top    = (std::min) (outBounds.top,    (LONG) std::lround (call.y));
            outBounds.right  = (std::max) (outBounds.right,  (LONG) std::lround (call.x + call.width));
            outBounds.bottom = (std::max) (outBounds.bottom, (LONG) std::lround (call.y + call.height));
            pieces++;
        }

        return pieces;
    }


    static const RecordedTextCall * FindText (const MockDxuiTextRenderer & text, const std::wstring & label)
    {
        const RecordedTextCall  * found = nullptr;



        for (const RecordedTextCall & call : text.Calls())
        {
            found = (call.text == label) ? &call : found;
        }

        return found;
    }


    //  Whether a rounded fill in `fill` was drawn, around `label`'s text
    //  when one is given, with the theme's button border around it.
    static bool HasButtonChrome (const MockDxuiPainter & painter, uint32_t fill, const RecordedTextCall * label = nullptr)
    {
        bool  isFilled   = false;
        bool  isBordered = false;



        for (const RecordedPaintCall & call : painter.Calls())
        {
            bool  isAround = label == nullptr ||
                             (call.x <= label->x && call.x + call.width  >= label->x + 1.0f &&
                              call.y <= label->y && call.y + call.height >= label->y + label->height);

            isFilled   = isFilled   || (call.kind == RecordedPaintKind::FillRoundedRect    && call.argb == fill && isAround);
            isBordered = isBordered || (call.kind == RecordedPaintKind::OutlineRoundedRect && isAround);
        }

        return isFilled && isBordered;
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
    void   SetHoveredCell  (int index) override              { hovered = index; }

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
    int                 hovered = -2;
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


    //  The preview always shows the snapshot the hovered cell shows, whichever
    //  way the pointer came. Moving to a cell whose full-size picture is not
    //  drawn yet shows its own thumbnail scaled up, never the sharp picture
    //  of the cell the pointer came from, until its own arrives; a cell
    //  showing the same snapshot as the one before keeps the sharp picture.
    TEST_METHOD (ThePreviewShowsTheHoveredCellsSnapshotFromEitherSide)
    {
        DxuiHwndSource      host (RECT { 0, 0, 1024, 768 }, 6.0f, std::make_unique<DxuiPanel>());
        DxuiImageStrip      strip;
        PictureStripSource  source;
        DxuiDpiScaler       scaler;
        size_t              i = 0;



        strip.SetSource    (&source);
        strip.SetAspect    (s_kStripAspect);
        strip.SetPopupHost (&host);
        strip.Layout       (RECT { 400, 300, 879, 332 }, false, scaler);

        for (i = 0; i < source.thumbs.size(); i++)
        {
            source.thumbs[i] = PictureStripSource::MakePicture (47, 32);
        }

        source.thumbs[3]   = source.thumbs[2];
        source.previews[0] = PictureStripSource::MakePicture (280, 192);
        source.previews[2] = PictureStripSource::MakePicture (280, 192);

        //  Forward from the first cell onto the second.
        strip.OnMouseMove (405, 310);
        Assert::IsTrue (strip.GetShownPreview() == source.previews[0], L"the first cell's sharp picture");

        strip.OnMouseMove (452, 310);
        strip.Sync();
        Assert::IsTrue (strip.GetShownPreview() == source.thumbs[1], L"coming forward, the second cell's own snapshot, not the first's");

        //  Back from the third cell onto the second.
        strip.OnMouseMove (499, 310);
        strip.Sync();
        Assert::IsTrue (strip.GetShownPreview() == source.previews[2], L"the third cell's sharp picture");

        strip.OnMouseMove (452, 310);
        strip.Sync();
        Assert::IsTrue (strip.GetShownPreview() == source.thumbs[1], L"coming back, the same snapshot as coming forward");

        source.previews[1] = PictureStripSource::MakePicture (280, 192);
        strip.Sync();
        Assert::IsTrue (strip.GetShownPreview() == source.previews[1], L"its sharp picture replaces it once it is ready");

        //  The fourth cell shows the third's snapshot: its sharp picture stays.
        strip.OnMouseMove (499, 310);
        strip.Sync();
        strip.OnMouseMove (546, 310);
        strip.Sync();
        Assert::IsTrue (strip.GetShownPreview() == source.previews[2], L"a cell showing the same snapshot keeps the sharp picture");

        //  A cell with no picture at all has nothing of its own to preview.
        source.thumbs[4] = nullptr;
        strip.OnMouseMove (593, 310);
        strip.Sync();
        Assert::IsFalse (strip.IsPreviewShown(), L"no picture of the cell's snapshot, no preview of another's");
    }


    //  The preview opens below the labels kept under the pictures, so it
    //  never covers the hovered cell's time.
    TEST_METHOD (ThePreviewOpensBelowTheLabels)
    {
        constexpr RECT        kStrip = { 400, 300, 1000, 382 };
        DxuiHwndSource        host (RECT { 0, 0, 1024, 768 }, 6.0f, std::make_unique<DxuiPanel>());
        DxuiImageStrip        strip;
        PictureStripSource    source;
        MockDxuiTextRenderer  text;
        DxuiDpiScaler         scaler;
        RECT                  pictures = {};



        strip.SetSource       (&source);
        strip.SetAspect       (s_kStripAspect);
        strip.SetPopupHost    (&host);
        strip.SetTextRenderer (&text);
        strip.SetLabelRoomDp  (20.0f);
        strip.Layout          (kStrip, true, scaler);

        std::fill (source.previews.begin(), source.previews.end(), PictureStripSource::MakePicture (280, 192));

        pictures = strip.GetPicturesRect();
        strip.OnMouseMove (pictures.left + 60, (pictures.top + pictures.bottom) / 2);

        Assert::IsTrue (strip.IsPreviewShown(), L"the preview is up");
        Assert::IsTrue (strip.GetPreviewRectPx().top >= kStrip.bottom, L"below the room for the labels, not over it");
    }


    //  The preview follows the pointer along the strip a pixel at a time,
    //  within a cell as well as across cells, rather than jumping from one
    //  cell's edge to the next.
    TEST_METHOD (ThePreviewFollowsThePointer)
    {
        DxuiHwndSource      host (RECT { 0, 0, 1024, 768 }, 6.0f, std::make_unique<DxuiPanel>());
        DxuiImageStrip      strip;
        PictureStripSource  source;
        DxuiDpiScaler       scaler;
        LONG                first    = 0;
        size_t              acquired = 0;



        strip.SetSource    (&source);
        strip.SetAspect    (s_kStripAspect);
        strip.SetPopupHost (&host);
        strip.Layout       (RECT { 400, 300, 879, 332 }, false, scaler);

        std::fill (source.previews.begin(), source.previews.end(), PictureStripSource::MakePicture (280, 192));

        strip.OnMouseMove (460, 310);
        first    = strip.GetPreviewRectPx().left;
        acquired = host.GetPopupHits() + host.GetPopupMisses();

        strip.OnMouseMove (463, 310);
        Assert::AreEqual (first + 3, strip.GetPreviewRectPx().left, L"three pixels along within the cell, three along on screen");

        strip.OnMouseMove (550, 310);
        strip.Sync();
        Assert::AreEqual (first + 90, strip.GetPreviewRectPx().left, L"and across cells the same way");

        Assert::AreEqual (acquired, host.GetPopupHits() + host.GetPopupMisses(), L"moved, never reopened");
    }


    //  The source hears which cell the pointer is over, so it can draw the
    //  pictures around it, and that it has left.
    TEST_METHOD (HoveringTellsTheSourceWhichCell)
    {
        DxuiImageStrip      strip;
        PictureStripSource  source;
        DxuiDpiScaler       scaler;



        strip.SetSource (&source);
        strip.SetAspect (s_kStripAspect);
        strip.Layout    (RECT { 0, 0, 479, 32 }, false, scaler);

        strip.OnMouseMove (100, 5);
        Assert::AreEqual (2, source.hovered, L"the third cell");

        strip.OnMouseLeave();
        Assert::AreEqual (-1, source.hovered, L"and then none");
    }
};

#endif
