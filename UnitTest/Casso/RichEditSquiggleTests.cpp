#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "RichEditSquiggle.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RichEditSquiggleTests
//
//  Spec-006 T075. Headless coverage of the FR-014e label-building
//  helper. The actual RichEdit-touching squiggle apply is exercised
//  manually under T076's GATE; here we only test the pure substring
//  composition.
//
////////////////////////////////////////////////////////////////////////////////

namespace RichEditSquiggleTests
{
    using RejectedSpan = TrackSectorPredicate::RejectedSpan;

    TEST_CLASS (RichEditSquiggleTests)
    {
    public:

        TEST_METHOD (BuildIgnoredTokensLabel_emptySpans_returnsEmptyString)
        {
            std::vector<RejectedSpan>  spans;
            std::wstring               out;

            out = BuildIgnoredTokensLabel (L"anything", spans);

            Assert::IsTrue (out.empty());
        }



        TEST_METHOD (BuildIgnoredTokensLabel_singleSpan_returnsIgnoredPrefixPlusToken)
        {
            // "0-2, abc, 17" -- the "abc" token spans [5..8).
            std::wstring               expr = L"0-2, abc, 17";
            std::vector<RejectedSpan>  spans;

            spans.push_back ({ 5, 8 });

            Assert::AreEqual (std::wstring (L"Ignored: abc"),
                              BuildIgnoredTokensLabel (expr, spans));
        }



        TEST_METHOD (BuildIgnoredTokensLabel_twoSpans_joinedWithCommaSpace)
        {
            // "xx, 5, yy" -- "xx" spans [0..2), "yy" spans [7..9).
            std::wstring               expr = L"xx, 5, yy";
            std::vector<RejectedSpan>  spans;

            spans.push_back ({ 0, 2 });
            spans.push_back ({ 7, 9 });

            Assert::AreEqual (std::wstring (L"Ignored: xx, yy"),
                              BuildIgnoredTokensLabel (expr, spans));
        }



        TEST_METHOD (BuildIgnoredTokensLabel_clipsSpansThatRunPastEnd)
        {
            std::wstring               expr = L"abc";
            std::vector<RejectedSpan>  spans;

            spans.push_back ({ 0, 100 });

            Assert::AreEqual (std::wstring (L"Ignored: abc"),
                              BuildIgnoredTokensLabel (expr, spans));
        }
    };
}
