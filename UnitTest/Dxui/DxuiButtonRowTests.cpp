#include "Pch.h"

#include "Window/DxuiButtonRow.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiButtonRowTests
//
//  The width a button needs for its label.
//
//  Worth pinning because the failure it prevents is silent. Buttons were laid
//  out at a fixed width, so a label longer than it wrapped and drew OUTSIDE the
//  button -- which no test could notice, because nothing threw, nothing
//  returned an error, and the pixels were merely wrong. It shipped that way
//  until somebody looked at a dialog with a long label on it.
//
//  The measurement is an estimate: layout runs without a text renderer, so the
//  width comes from an average glyph width. These assert the properties that
//  matter (never below the standard width, grows with the label, generous
//  enough not to clip) rather than exact pixels, which would pin the estimate's
//  constants and break on any tuning.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiButtonRowTests
{
    TEST_CLASS (DxuiButtonRowTests)
    {
    public:

        TEST_METHOD (WidthForLabel_ShortLabel_KeepsTheStandardWidth)
        {
            // Every existing dialog has short labels, so none of them may move.
            Assert::AreEqual (DxuiButtonRow::kButtonWidthDip,
                              DxuiButtonRow::GetWidthForLabel (L"OK"),
                              L"a short label keeps the standard button width");

            Assert::AreEqual (DxuiButtonRow::kButtonWidthDip,
                              DxuiButtonRow::GetWidthForLabel (L"Cancel"),
                              L"and so does a slightly longer one");
        }



        TEST_METHOD (WidthForLabel_EmptyLabel_StillHasAButton)
        {
            Assert::AreEqual (DxuiButtonRow::kButtonWidthDip,
                              DxuiButtonRow::GetWidthForLabel (std::wstring()),
                              L"an empty label must not collapse the button to nothing");
        }



        TEST_METHOD (WidthForLabel_LongLabel_GrowsPastTheStandardWidth)
        {
            // The case that was broken: this label wrapped to three lines and
            // spilled outside its own button.
            int  wide = DxuiButtonRow::GetWidthForLabel (L"Salvage readable sectors...");

            Assert::IsTrue (wide > DxuiButtonRow::kButtonWidthDip,
                L"a label the standard width cannot hold must widen the button");
        }



        TEST_METHOD (WidthForLabel_GrowsMonotonicallyWithTheLabel)
        {
            int  shortW  = DxuiButtonRow::GetWidthForLabel (L"Salvage readable sectors...");
            int  longerW = DxuiButtonRow::GetWidthForLabel (L"Salvage readable sectors and then some more");

            Assert::IsTrue (longerW > shortW,
                L"a longer label needs a wider button");
        }



        TEST_METHOD (WidthForLabel_LeavesRoomBeyondTheText)
        {
            // The estimate is deliberately generous: a button whose text just
            // touches its edges reads as broken even when nothing is clipped.
            std::wstring  label = L"Salvage readable sectors...";
            int           width = DxuiButtonRow::GetWidthForLabel (label);

            Assert::IsTrue (width > (int) label.size() * 8,
                L"the width must exceed a bare glyph-count estimate, leaving padding");
        }
    };
}
