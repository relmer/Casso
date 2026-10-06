#include "Pch.h"

#include "Ui/Dialogs/ReleaseNotesLayout.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesLayoutTests
//
//  The word flow behind the update dialog's notes, measured at a fixed ten
//  pixels per character so every position is exact.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ReleaseNotesLayoutTests)
{
public:

    static constexpr float  kCharPx = 10.0f;



    static float Measure (const std::wstring & text, const NotesRunStyle &)
    {
        return (float) text.size() * kCharPx;
    }



    static FormattedLine MakeParagraph (std::vector<FormattedRun> runs)
    {
        FormattedLine  line;

        line.kind = FormattedLineKind::Paragraph;
        line.runs = std::move (runs);
        return line;
    }



    TEST_METHOD (Flow_WrapsAtTheColumnEdge)
    {
        NotesLayoutMetrics           metrics;
        std::vector<PlacedNotesRun>  runs;
        float                        height = 0.0f;



        height = ReleaseNotesLayout::Flow ({ MakeParagraph ({ { "one two three" } }) }, 80.0f, metrics, Measure, runs);

        Assert::AreEqual ((size_t) 3, runs.size());
        Assert::AreEqual (0.0f,  runs[0].x);
        Assert::AreEqual (40.0f, runs[1].x, L"one space after 'one'");
        Assert::AreEqual (0.0f,  runs[2].x, L"'three' does not fit after 'two' and wraps");
        Assert::AreEqual (metrics.lineHeightPx, runs[2].y);
        Assert::AreEqual (metrics.lineHeightPx * 2.0f, height);
    }



    TEST_METHOD (Flow_StyledRunsShareALine)
    {
        NotesLayoutMetrics           metrics;
        std::vector<PlacedNotesRun>  runs;
        FormattedRun                 bold;
        FormattedRun                 code;
        FormattedRun                 tail;



        bold.text = "Bold";
        bold.bold = true;
        code.text = " code";
        code.code = true;
        tail.text = ", done";

        ReleaseNotesLayout::Flow ({ MakeParagraph ({ bold, code, tail }) }, 500.0f, metrics, Measure, runs);

        Assert::AreEqual ((size_t) 4, runs.size());
        Assert::IsTrue   (runs[0].style.bold);
        Assert::IsTrue   (runs[1].style.code);
        Assert::AreEqual (50.0f, runs[1].x, L"a run starting with a space is spaced");
        Assert::AreEqual (std::wstring (L","), runs[2].text);
        Assert::AreEqual (90.0f, runs[2].x, L"a run starting with punctuation joins the word before it");
        Assert::AreEqual (0.0f, runs[0].y);
        Assert::AreEqual (0.0f, runs[3].y);
    }



    TEST_METHOD (Flow_BulletsIndentByDepthWithAMark)
    {
        NotesLayoutMetrics           metrics;
        std::vector<PlacedNotesRun>  runs;
        FormattedLine                nested;



        nested.kind        = FormattedLineKind::Bullet;
        nested.indentLevel = 1;
        nested.runs        = { { "item" } };

        ReleaseNotesLayout::Flow ({ nested }, 500.0f, metrics, Measure, runs);

        Assert::AreEqual ((size_t) 2, runs.size());
        Assert::AreEqual (metrics.indentPx * 2.0f - metrics.bulletGapPx, runs[0].x, L"the mark sits in the gutter");
        Assert::AreEqual (metrics.indentPx * 2.0f, runs[1].x);
    }



    TEST_METHOD (Flow_HeadingIsBoldAndTallerWithAGapAbove)
    {
        NotesLayoutMetrics           metrics;
        std::vector<PlacedNotesRun>  runs;
        FormattedLine                heading;
        float                        height = 0.0f;



        heading.kind         = FormattedLineKind::Heading;
        heading.headingLevel = 1;
        heading.runs         = { { "Title" } };

        height = ReleaseNotesLayout::Flow ({ MakeParagraph ({ { "x" } }), heading }, 500.0f, metrics, Measure, runs);

        Assert::IsTrue   (runs[1].style.bold);
        Assert::AreEqual (metrics.headingSizePx[0], runs[1].style.sizePx);
        Assert::AreEqual (metrics.lineHeightPx + metrics.headingGapPx, runs[1].y);
        Assert::IsTrue   (height > metrics.lineHeightPx * 2.0f + metrics.headingGapPx);
    }



    //  A bold word is wider than the same word in the regular face, and the
    //  flow places what follows from the bold width the callback reports.
    TEST_METHOD (Flow_BoldWordsAreMeasuredBold)
    {
        NotesLayoutMetrics           metrics;
        std::vector<PlacedNotesRun>  runs;
        FormattedRun                 bold;
        FormattedRun                 tail;
        auto                         measure = [] (const std::wstring & text, const NotesRunStyle & style)
        {
            return (float) text.size() * (style.bold ? kCharPx * 2.0f : kCharPx);
        };



        bold.text = "flux";
        bold.bold = true;
        tail.text = " support";

        ReleaseNotesLayout::Flow ({ MakeParagraph ({ bold, tail }) }, 500.0f, metrics, measure, runs);

        Assert::AreEqual (80.0f,  runs[0].width, L"measured in the bold face");
        Assert::AreEqual (90.0f,  runs[1].x,     L"the next word starts after the bold width and a space");
    }



    TEST_METHOD (FindLinkAt_OnlyLinkWords)
    {
        NotesLayoutMetrics           metrics;
        std::vector<PlacedNotesRun>  runs;
        FormattedRun                 link;



        link.text    = "here";
        link.linkUrl = "https://example.com";

        ReleaseNotesLayout::Flow ({ MakeParagraph ({ { "see " }, link }) }, 500.0f, metrics, Measure, runs);

        Assert::IsNull    (ReleaseNotesLayout::FindLinkAt (runs, 5.0f, 5.0f));
        Assert::IsNotNull (ReleaseNotesLayout::FindLinkAt (runs, 45.0f, 5.0f));
        Assert::AreEqual  (std::string ("https://example.com"), ReleaseNotesLayout::FindLinkAt (runs, 45.0f, 5.0f)->linkUrl);
    }
};
