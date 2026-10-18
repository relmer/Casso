#include "Pch.h"

#include "Update/ReleaseNotesFormatter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesFormatterTests
//
//  The markdown subset the update dialog renders, and the rule that syntax
//  outside it is shown as text rather than lost.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ReleaseNotesFormatterTests)
{
public:

    TEST_METHOD (Blocks_HeadingsBulletsParagraphsBlanks)
    {
        std::vector<FormattedLine>  lines;



        ReleaseNotesFormatter::Format ("### Added\r\n"
                                       "\r\n"
                                       "- First\r\n"
                                       "  - Nested\r\n"
                                       "* Star bullet\r\n"
                                       "\r\n"
                                       "\r\n"
                                       "A paragraph\r\n"
                                       "that wraps.\r\n"
                                       "\r\n",
                                       lines);

        Assert::AreEqual ((size_t) 7, lines.size());
        Assert::IsTrue   (lines[0].kind == FormattedLineKind::Heading);
        Assert::AreEqual (3, lines[0].headingLevel);
        Assert::AreEqual (std::string ("Added"), lines[0].runs[0].text);
        Assert::IsTrue   (lines[1].kind == FormattedLineKind::Blank);
        Assert::IsTrue   (lines[2].kind == FormattedLineKind::Bullet);
        Assert::AreEqual (0, lines[2].indentLevel);
        Assert::IsTrue   (lines[3].kind == FormattedLineKind::Bullet);
        Assert::AreEqual (1, lines[3].indentLevel);
        Assert::AreEqual (std::string ("Nested"), lines[3].runs[0].text);
        Assert::IsTrue   (lines[4].kind == FormattedLineKind::Bullet);
        Assert::IsTrue   (lines[5].kind == FormattedLineKind::Blank, L"blank runs collapse to one");
        Assert::IsTrue   (lines[6].kind == FormattedLineKind::Paragraph);
        Assert::AreEqual ((size_t) 1, lines[6].runs.size());
        Assert::AreEqual (std::string ("A paragraph that wraps."), lines[6].runs[0].text);
    }



    TEST_METHOD (Bullet_WrappedLineContinuesIt)
    {
        std::vector<FormattedLine>  lines;



        ReleaseNotesFormatter::Format ("- GH #115: a long entry\n  that wraps onto **two** lines\n", lines);

        Assert::AreEqual ((size_t) 1, lines.size());
        Assert::AreEqual ((size_t) 3, lines[0].runs.size());
        Assert::AreEqual (std::string ("GH #115: a long entry that wraps onto "), lines[0].runs[0].text);
        Assert::IsTrue   (lines[0].runs[1].bold);
        Assert::AreEqual (std::string ("two"), lines[0].runs[1].text);
        Assert::AreEqual (std::string (" lines"), lines[0].runs[2].text);
    }



    TEST_METHOD (Inline_BoldCodeLink)
    {
        std::vector<FormattedRun>  runs;



        ReleaseNotesFormatter::FormatInline ("Use **bold `code`** and [the page](https://x.test/a) now", runs);

        Assert::AreEqual ((size_t) 6, runs.size());
        Assert::IsTrue   (runs[0] == FormattedRun { "Use ",     false, false, "" });
        Assert::IsTrue   (runs[1] == FormattedRun { "bold ",    true,  false, "" });
        Assert::IsTrue   (runs[2] == FormattedRun { "code",     true,  true,  "" });
        Assert::IsTrue   (runs[3] == FormattedRun { " and ",    false, false, "" });
        Assert::IsTrue   (runs[4] == FormattedRun { "the page", false, false, "https://x.test/a" });
        Assert::IsTrue   (runs[5] == FormattedRun { " now",     false, false, "" });
    }



    TEST_METHOD (Inline_UnmatchedMarkersStayText)
    {
        std::vector<FormattedRun>  runs;



        ReleaseNotesFormatter::FormatInline ("2 ** 3, a ` tick, [not a link] (x), [empty]()", runs);

        Assert::AreEqual ((size_t) 1, runs.size());
        Assert::AreEqual (std::string ("2 ** 3, a ` tick, [not a link] (x), [empty]()"), runs[0].text);
        Assert::IsFalse  (runs[0].bold);
    }



    TEST_METHOD (Unknown_SyntaxKeptAsText)
    {
        std::vector<FormattedLine>  lines;



        ReleaseNotesFormatter::Format ("##### Deep heading\n> quoted\n| a | b |\n", lines);

        Assert::AreEqual ((size_t) 1, lines.size(), L"consecutive text lines join into one paragraph");
        Assert::IsTrue   (lines[0].kind == FormattedLineKind::Paragraph);
        Assert::AreEqual (std::string ("##### Deep heading > quoted | a | b |"), lines[0].runs[0].text);
    }



    TEST_METHOD (Inline_HtmlTagsAndCommentsDropped_TextBetweenKept)
    {
        std::vector<FormattedRun>  runs;



        ReleaseNotesFormatter::FormatInline ("read it. <a id=\"v1-29\"></a><!-- note -->Then <b>this</b>", runs);

        Assert::AreEqual ((size_t) 1, runs.size());
        Assert::AreEqual (std::string ("read it. Then this"), runs[0].text);
    }



    TEST_METHOD (Inline_LessThanThatIsNotATagStays)
    {
        std::vector<FormattedRun>  runs;



        ReleaseNotesFormatter::FormatInline ("a < b, <3, `<a>` and <!-- open", runs);

        Assert::AreEqual (std::string ("a < b, <3, "), runs[0].text);
        Assert::IsTrue   (runs[1].code);
        Assert::AreEqual (std::string ("<a>"), runs[1].text, L"a tag inside code is code");
        Assert::AreEqual (std::string (" and <!-- open"), runs[2].text, L"an unclosed comment is text");
    }
};
