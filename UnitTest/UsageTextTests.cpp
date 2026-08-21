#include "Pch.h"

#include "TestHelpers.h"
#include "UsageText.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace UsageTextTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  UsageTextTests
    //
    //  Help is written one logical line per item and folded at print time, so
    //  the fold is the only thing standing between a flag table and a wall of
    //  text.
    //
    //  What is worth holding here is not that a long line breaks -- any fold
    //  does that -- but the two properties that make the result still read as a
    //  table: a continuation starts under the DESCRIPTION rather than at the
    //  left margin, and the words come out unchanged whatever the width.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS(UsageTextTests)
    {
    public:

        ////////////////////////////////////////////////////////////////////////
        //
        //  AFlagRowContinuesUnderItsDescription
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD(AFlagRowContinuesUnderItsDescription)
        {
            std::string               line = "    -o <file>            Rename the output file";
            std::vector<std::string>  rows = UsageText::Wrap (line, 40);

            Assert::AreEqual ((size_t) 2, rows.size());
            Assert::AreEqual (std::string ("    -o <file>            Rename the"), rows[0]);
            Assert::AreEqual (std::string ("                         output file"), rows[1]);
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  TheIndentComesFromTheLastGutterNotTheFirst
        //
        //  A description may hold a run of two spaces of its own, and it is the
        //  RIGHTMOST column boundary that the reader sees as the edge of the
        //  text.
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD(TheIndentComesFromTheLastGutterNotTheFirst)
        {
            Assert::AreEqual ((size_t) 25, UsageText::ContinuationIndent ("    -o <file>            Rename it"),
                              L"the gutter between a flag and its description sets the column");
            Assert::AreEqual ((size_t) 17, UsageText::ContinuationIndent ("  -o file  then  again"),
                              L"the last gutter wins, not the first");
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  ProseContinuesAtItsOwnIndent
        //
        //  A paragraph has no gutter to hang from. Continuing it at the left
        //  margin would make the second line read as a new item.
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD(ProseContinuesAtItsOwnIndent)
        {
            std::string               line = "    This is a sentence of ordinary prose with no gutter anywhere in it";
            std::vector<std::string>  rows = UsageText::Wrap (line, 30);

            Assert::AreEqual ((size_t) 4, UsageText::ContinuationIndent (line));
            Assert::IsTrue (rows.size() > 1, L"a line wider than the width must produce continuations");

            for (size_t i = 1; i < rows.size(); i++)
            {
                Assert::AreEqual (std::string ("    "), rows[i].substr (0, 4), L"a continuation keeps the paragraph's indent");
                Assert::AreNotEqual (' ', rows[i][4], L"and starts at text, not at more spaces");
            }
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  NoRowExceedsTheWidth
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD(NoRowExceedsTheWidth)
        {
            std::string  line = "  --max-cycles <n>       Stop after n cycles have been executed, whichever of the two comes first";

            for (size_t width = 40; width <= 120; width++)
            {
                for (const std::string & row : UsageText::Wrap (line, width))
                {
                    Assert::IsTrue (row.size() <= width, L"a row wider than the terminal is what the fold exists to prevent");
                }
            }
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  WrappingLosesNothingButTheBreaks
        //
        //  Every word survives, in order, at every width. A fold that dropped
        //  or doubled one would still look like a table.
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD(WrappingLosesNothingButTheBreaks)
        {
            std::string  line     = "    -w[<width>]          Wrap listing at <width> columns, 60 to 200 (default: 79, -w alone = 133)";
            std::string  expected = Words (line);

            for (size_t width = 40; width <= 120; width++)
            {
                std::string  rejoined;

                for (const std::string & row : UsageText::Wrap (line, width))
                {
                    rejoined += row + " ";
                }

                Assert::AreEqual (expected, Words (rejoined), L"the same words, however the line was folded");
            }
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  AShortLineIsHandedBackWhole
        //
        //  Callers print the result unconditionally, so a line that fits must
        //  come back as itself rather than as a rebuilt copy.
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD(AShortLineIsHandedBackWhole)
        {
            std::string               line = "  -v                     Verbose output";
            std::vector<std::string>  rows = UsageText::Wrap (line, 80);

            Assert::AreEqual ((size_t) 1, rows.size());
            Assert::AreEqual (line, rows[0]);
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  AnEmptyLineSurvives
        //
        //  The blank rows between sections go through the same call as the text
        //  does.
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD(AnEmptyLineSurvives)
        {
            std::vector<std::string>  rows = UsageText::Wrap ("", 80);

            Assert::AreEqual ((size_t) 1, rows.size());
            Assert::AreEqual (std::string (""), rows[0]);
            Assert::AreEqual ((size_t) 0, UsageText::ContinuationIndent (""));
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  AWidthTooNarrowToWrapIntoIsRefusedRatherThanLooped
        //
        //  Once the continuation column reaches the width there is no fold left
        //  that helps, and a loop that tried would never advance. Handing the
        //  line back over-long is the honest answer for a terminal that narrow.
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD(AWidthTooNarrowToWrapIntoIsRefusedRatherThanLooped)
        {
            std::string  line = "    -o <file>            Rename the output file";

            Assert::AreEqual ((size_t) 1, UsageText::Wrap (line, 0).size(), L"width 0 must not wrap");
            Assert::AreEqual (line,       UsageText::Wrap (line, 0)[0]);
            Assert::AreEqual ((size_t) 1, UsageText::Wrap (line, 20).size(), L"nor must a width inside the gutter");
            Assert::AreEqual (line,       UsageText::Wrap (line, 20)[0]);
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  AWordLongerThanTheColumnOverhangsRatherThanBeingCut
        //
        //  A path or a URL broken across two rows is worse than one that
        //  overhangs, because the reader cannot tell whether the break is part
        //  of it.
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD(AWordLongerThanTheColumnOverhangsRatherThanBeingCut)
        {
            std::string               line = "  -g <file>              https://example.invalid/a/very/long/path/that/will/not/fit";
            std::vector<std::string>  rows = UsageText::Wrap (line, 50);

            Assert::AreEqual ((size_t) 2, rows.size());
            Assert::IsTrue (rows[1].find ("https://example.invalid/a/very/long/path/that/will/not/fit") != std::string::npos,
                            L"the long word is left whole on its own row");
        }

    private:

        ////////////////////////////////////////////////////////////////////////
        //
        //  Words
        //
        //  The line's words, single-spaced, so two foldings of one line can be
        //  compared without the breaks getting in the way.
        //
        ////////////////////////////////////////////////////////////////////////

        static std::string Words (const std::string & text)
        {
            std::istringstream  in (text);
            std::string         word;
            std::string         joined;

            while (in >> word)
            {
                joined += (joined.empty() ? "" : " ") + word;
            }

            return joined;
        }
    };
}
