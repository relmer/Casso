#include "Pch.h"

#include "Ui/Debugger/BreakpointBarCommands.h"
#include "Ui/Debugger/MemoryBarCommands.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerToolbarGlyphTests
    //
    //  Every icon on a debugger pane's toolbar is one icon-font code point. A
    //  glyph typed into the source as a literal character is read in the
    //  source's code page and arrives as several characters, which draw as
    //  boxes.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerToolbarGlyphTests)
    {
    public:

        static void AssertIconGlyphs (const std::vector<DxuiToolbar::Entry> & entries)
        {
            for (const DxuiToolbar::Entry & entry : entries)
            {
                const wchar_t *  glyph = entry.command->glyph;

                if (glyph == nullptr)
                {
                    continue;
                }

                Assert::AreEqual ((size_t) 1, wcslen (glyph),                   entry.command->label.c_str());
                Assert::IsTrue   (glyph[0] >= 0xE000 && glyph[0] <= 0xF8FF, entry.command->label.c_str());
            }
        }


        TEST_METHOD (EveryBreakpointBarGlyphIsOneIconCodePoint)
        {
            BreakpointBarCommands  commands ({});

            AssertIconGlyphs (commands.BuildEntries());
        }


        TEST_METHOD (EveryMemoryBarGlyphIsOneIconCodePoint)
        {
            MemoryBarCommands  commands ({});

            AssertIconGlyphs (commands.BuildEntries (nullptr));
        }
    };
}
