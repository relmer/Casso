#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  StandingStripSource
//
//  Labels for both ends of the strip and a playhead line, and a count of
//  clicks on the trailing label.
//
////////////////////////////////////////////////////////////////////////////////

class StandingStripSource : public IDxuiImageStripSource
{
public:
    void   SetCellLayout   (int count, SIZE cellPx) override { (void) count; (void) cellPx; }
    Image  GetCellImage    (int index) override              { (void) index; return nullptr; }
    Image  GetPreviewImage (int index) override              { (void) index; return nullptr; }
    void   OnCellClicked   (int index) override              { clicked.push_back (index); }

    std::wstring  GetLeadingLabel() override         { return leading; }
    std::wstring  GetTrailingLabel() override        { return trailing; }
    bool          IsTrailingLabelAccented() override { return true; }
    void          OnTrailingLabelClicked() override  { trailingClicks++; }

    bool  TryGetPlayhead (float & outOffset, std::wstring & outTop, std::wstring & outBottom) override
    {
        outOffset = playhead;
        outTop    = playheadTop;
        outBottom = playheadBottom;
        return hasPlayhead;
    }

    std::vector<int>  clicked;
    std::wstring      leading        = L"10:00:00 PM";
    std::wstring      trailing       = L"Live";
    int               trailingClicks = 0;
    bool              hasPlayhead    = false;
    float             playhead       = 0.0f;
    std::wstring      playheadTop    = L"10:42:17 PM";
    std::wstring      playheadBottom = L"(Power + 1:02:03)";
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStripStandingTests
//
//  A strip standing on end, as the timeline is docked against a side: where
//  history begins above the first picture, Live below the last and still
//  clickable, and the playhead line's labels beside the line, all within the
//  strip's width.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiImageStripStandingTests)
{
public:

    static constexpr float  kAspect = 560.0f / 384.0f;
    static constexpr LONG   kWidth  = 74;
    static constexpr LONG   kHeight = 700;


    static void SetUpStrip (DxuiImageStrip & strip, StandingStripSource & source, MockDxuiTextRenderer & text)
    {
        DxuiDpiScaler  scaler;

        strip.SetSource       (&source);
        strip.SetAspect       (kAspect);
        strip.SetTextRenderer (&text);
        strip.SetLabelRoomDp  (20.0f);
        strip.Layout          (RECT { 0, 0, kWidth, kHeight }, true, scaler);
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


    TEST_METHOD (EndLabelsSitAboveTheFirstPictureAndBelowTheLast)
    {
        DxuiImageStrip            strip;
        StandingStripSource       source;
        MockDxuiTextRenderer      text;
        MockDxuiPainter           painter;
        MockDxuiTheme             theme;
        RECT                      pictures = {};
        const RecordedTextCall  * lead     = nullptr;
        const RecordedTextCall  * trail    = nullptr;

        SetUpStrip  (strip, source, text);
        strip.Paint (painter, text, theme, false, false, true);

        pictures = strip.GetPicturesRect();
        lead     = FindText (text, source.leading);
        trail    = FindText (text, source.trailing);

        Assert::IsNotNull (lead,  L"the leading label is drawn standing up");
        Assert::IsNotNull (trail, L"and so is Live");

        Assert::IsTrue (pictures.top > 0,                              L"the pictures start below the leading label");
        Assert::IsTrue (lead->y >= 0.0f,                               L"the leading label is in the strip");
        Assert::IsTrue (lead->y + lead->height <= (float) pictures.top, L"and above the first picture");
        Assert::IsTrue (pictures.bottom < kHeight,                     L"the pictures end above Live");
        Assert::IsTrue (trail->y >= (float) pictures.bottom,           L"Live is below the last picture");
        Assert::IsTrue (trail->y + trail->height <= (float) kHeight,   L"and in the strip");

        for (const RecordedTextCall * label : { lead, trail })
        {
            Assert::IsTrue (label->x >= 0.0f,                          L"not past the strip's left side");
            Assert::IsTrue (label->x + label->width <= (float) kWidth, L"nor its right side");
            Assert::IsTrue (label->fontSizeDip > 0.0f,                 L"drawn at a size");
        }
    }


    TEST_METHOD (LiveStandingUpIsClickable)
    {
        DxuiImageStrip         strip;
        StandingStripSource    source;
        MockDxuiTextRenderer   text;
        int                    y = 0;

        SetUpStrip (strip, source, text);

        y = (strip.GetPicturesRect().bottom + kHeight) / 2;

        Assert::IsTrue   (strip.OnLButtonDown (kWidth / 2, y), L"a press on Live is taken");
        Assert::IsTrue   (strip.OnClick       (kWidth / 2, y), L"and its click");
        Assert::AreEqual (1, source.trailingClicks,            L"goes to the source");
        Assert::IsTrue   (source.clicked.empty(),              L"no cell was clicked");
    }


    TEST_METHOD (AStandingStripLaysOutAgainWhenAnEndLabelAppears)
    {
        DxuiImageStrip         strip;
        StandingStripSource    source;
        MockDxuiTextRenderer   text;

        source.leading.clear();
        SetUpStrip (strip, source, text);

        Assert::IsFalse (strip.HasOutgrownLabelRoom(), L"no label, no room wanted");

        source.leading = L"10:00:00 PM";
        Assert::IsTrue  (strip.HasOutgrownLabelRoom(), L"history's start time needs room above the pictures");
    }


    TEST_METHOD (PlayheadLabelsStandingUpStayInsideTheStripBesideTheLine)
    {
        DxuiImageStrip            strip;
        StandingStripSource       source;
        MockDxuiTextRenderer      text;
        MockDxuiPainter           painter;
        MockDxuiTheme             theme;
        const RecordedTextCall  * top     = nullptr;
        const RecordedTextCall  * bottom  = nullptr;
        const RecordedTextCall  * line    = nullptr;

        source.hasPlayhead = true;
        SetUpStrip (strip, source, text);

        for (float offset : { 0.0f, (float) strip.GetCellCount() / 2.0f, (float) strip.GetCellCount() })
        {
            source.playhead = offset;
            text.Reset();
            painter.Reset();
            strip.Paint (painter, text, theme, false, false, true);

            top    = FindText (text, source.playheadTop);
            bottom = FindText (text, source.playheadBottom);

            Assert::IsNotNull (top,    L"the top label is drawn standing up");
            Assert::IsNotNull (bottom, L"the bottom label is drawn standing up");
            Assert::AreEqual<size_t> (1, text.IconCalls().size(), L"the line is drawn");

            line = &text.IconCalls()[0];

            for (const RecordedTextCall * label : { top, bottom })
            {
                Assert::IsTrue (label->x >= 0.0f,                            L"not past the strip's left side");
                Assert::IsTrue (label->x + label->width <= (float) kWidth,   L"nor its right side");
                Assert::IsTrue (label->y >= 0.0f,                            L"not above the strip");
                Assert::IsTrue (label->y + label->height <= (float) kHeight, L"nor below it");

                Assert::IsTrue (label->y + label->height <= line->y || label->y >= line->y + line->height, L"clear of the line");
            }

            Assert::IsTrue (top->y < bottom->y, L"the top label above the bottom one");

            for (const RecordedPaintCall & call : painter.Calls())
            {
                if (call.kind == RecordedPaintKind::FillRect && call.argb == theme.Background())
                {
                    Assert::IsTrue (call.x >= 0.0f && call.x + call.width <= (float) kWidth, L"each label's plate stays within the strip's width");
                }
            }
        }
    }
};
