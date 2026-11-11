#include "Pch.h"

#include "Ui/Debugger/DebuggerLayout.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  LayoutTextRobustnessTests
//
//  The pane layout and a toolbar's dock are read from text saved in the
//  preferences file, which can be edited or damaged. Random text, every
//  prefix of the debugger's default layout and copies of it with characters
//  changed are read; a layout that parses is arranged, written again and
//  restored as the debugger restores one. None throws.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (LayoutTextRobustnessTests)
    {
    public:

        TEST_METHOD (NoLayoutTextThrows)
        {
            static constexpr std::wstring_view  kAlphabet = L"0123456789.,;:=|[]{}()<>-+ abcxyzABCXYZ\"'\\/\t\n";

            std::wstring               valid  = DebuggerLayout::MakeDefault().ToText();
            std::vector<std::wstring>  inputs = { std::wstring(), valid };
            std::vector<std::string>   failures;
            std::mt19937               random (0x1A70035u);
            size_t                     parsed = 0;



            for (int i = 0; i < 300; i++)
            {
                std::wstring  text ((size_t) (random() % 300), L' ');

                for (wchar_t & ch : text)
                {
                    ch = (random() % 8 == 0) ? (wchar_t) (random() % 0xD000) : kAlphabet[random() % kAlphabet.size()];
                }

                inputs.push_back (text);
            }

            for (size_t length = 0; length <= valid.size(); length += 3)
            {
                inputs.push_back (valid.substr (0, length));
            }

            for (int copy = 0; copy < 600; copy++)
            {
                std::wstring  changed = valid;

                for (int change = 0; change < 3; change++)
                {
                    changed[random() % changed.size()] = kAlphabet[random() % kAlphabet.size()];
                }

                inputs.push_back (changed);
            }

            for (const std::wstring & input : inputs)
            {
                try
                {
                    DxuiPaneLayout  layout;

                    if (DxuiPaneLayout::TryParse (input, layout))
                    {
                        parsed++;
                        (void) layout.Arrange       (RECT { 0, 0, 1200, 900 }, nullptr, nullptr);
                        (void) layout.ArrangeSplits (RECT { 0, 0, 1200, 900 }, nullptr, nullptr);
                        (void) DebuggerLayout::Restore (layout.ToText()).Arrange (RECT { 0, 0, 1200, 900 }, nullptr, nullptr);
                    }

                    (void) DxuiToolbarDock::FromText (input).ToText();
                }
                catch (const std::exception & e)
                {
                    failures.push_back (std::format ("input of {} characters: {}", input.size(), e.what()));
                }
            }

            Logger::WriteMessage (std::format ("{} inputs, {} parsed as layouts\n", inputs.size(), parsed).c_str());

            for (const std::string & failure : failures)
            {
                Logger::WriteMessage ((failure + "\n").c_str());
            }

            Assert::AreEqual ((size_t) 0, failures.size(), L"inputs that threw: see the output");
        }
    };
}
