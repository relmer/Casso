#include "Pch.h"

#include "Debugger/RomSymbols.h"
#include "Debugger/SymbolDescriptions.h"
#include "Ui/Debugger/SymbolComments.h"
#include "Core/TextEncoding.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolDescriptionsTests
//
//  Every name the shipped tables define has a line saying what it is, so a
//  symbol added to RomSymbols without one fails here rather than showing a
//  bare address in the code pane's tooltip.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (SymbolDescriptionsTests)
    {
    public:

        TEST_METHOD (EveryShippedSymbolIsDescribed)
        {
            for (const char * table : { RomSymbols::GetMain ("Apple2e"), RomSymbols::GetMain ("Apple2c"), RomSymbols::GetMain ("Apple2Plus"), RomSymbols::GetBasic(), RomSymbols::GetDos33(), RomSymbols::GetProDos() })
            {
                std::istringstream  lines (table);
                std::string         line;

                while (std::getline (lines, line))
                {
                    size_t       equals = line.find ('=');
                    std::string  name   = line.substr (0, equals);

                    if (line.empty() || line[0] == ';' || equals == std::string::npos)
                    {
                        continue;
                    }

                    Assert::IsNotNull (SymbolDescriptions::Find (name), TextEncoding::NarrowToWide (name).c_str());
                }
            }
        }


        //  Each Applesoft routine a BASIC program's profile lists is in the table.
        TEST_METHOD (TheApplesoftRoutinesAProfileListsAreInTheTable)
        {
            std::string  table = RomSymbols::GetBasic();



            for (const char * entry : { "EXECUTE_STATEMENT=$D828", "OUTDO=$DB5C", "FRM_ELEMENT=$DE60", "MUL10=$EA39" })
            {
                Assert::IsTrue (table.find (entry) != std::string::npos, TextEncoding::NarrowToWide (entry).c_str());
            }
        }


        //  A macro's call and the lines of its body all start at the call's
        //  address: the label is defined on the line with a label in its
        //  first column, the call, not on the body's first line.
        TEST_METHOD (ALabelOnAMacroCallIsDefinedOnTheCall)
        {
            std::map<std::pair<int, int>, Word>  lines  = { { { 0, 5 }, 0x0302 }, { { 0, 9 }, 0x0302 }, { { 0, 14 }, 0x0302 } };
            std::vector<std::string>             source = { "", "", "", "clear   macro", "        lda #0", "        endm", "", "store   macro addr",
                                                            "        clear", "        sta addr", "        endm", "", "start   ldx #3", "loop    store $0400" };
            auto                                 isLabel = [&source] (int, int line) { return SymbolComments::IsLabelLine (source[(size_t) line - 1]); };



            Assert::IsTrue (SymbolComments::FindDefiningLine (lines, 0x0302, isLabel) == std::optional<std::pair<int, int>> ({ 0, 14 }), L"the call, where loop is defined");
            Assert::IsTrue (SymbolComments::FindDefiningLine (lines, 0x0302)          == std::optional<std::pair<int, int>> ({ 0, 5 }),  L"the earliest line with no source to read");
            Assert::IsFalse (SymbolComments::IsLabelLine ("        lda #0"));
            Assert::IsFalse (SymbolComments::IsLabelLine ("; a comment"));
            Assert::IsTrue  (SymbolComments::IsLabelLine ("loop    store $0400"));
        }


        //  A label's source comment is on the first line whose code starts at
        //  its address, or on the line above when that line is the label
        //  alone; a semicolon in quotes is not a comment.
        TEST_METHOD (ALabelsCommentIsOnItsDefiningLine)
        {
            std::map<std::pair<int, int>, Word>  lines  = { { { 0, 3 }, 0x0300 }, { { 0, 5 }, 0x0302 }, { { 1, 2 }, 0x0300 } };
            std::vector<std::string>             source = { "        ORG $300",
                                                            "WAIT    ; until a key",
                                                            "        LDA KBD      ; the key",
                                                            "        LDA #';'",
                                                            "DONE    RTS          ; back" };

            Assert::IsTrue   (SymbolComments::FindDefiningLine (lines, 0x0300) == std::optional<std::pair<int, int>> ({ 0, 3 }), L"the earliest line");
            Assert::IsFalse  (SymbolComments::FindDefiningLine (lines, 0x0400).has_value());

            Assert::AreEqual (std::string ("until a key"), SymbolComments::GetDefiningComment (source, 3, "WAIT"), L"the label alone above");
            Assert::AreEqual (std::string ("the key"),     SymbolComments::GetDefiningComment (source, 3, "OTHER"), L"the code's line");
            Assert::AreEqual (std::string ("back"),        SymbolComments::GetDefiningComment (source, 5, "DONE"));
            Assert::AreEqual (std::string(),               SymbolComments::GetLineComment ("        LDA #';'"), L"in quotes");
            Assert::AreEqual (std::string(),               SymbolComments::GetDefiningComment (source, 9, "WAIT"), L"past the end");
        }


        TEST_METHOD (AnUnknownNameHasNoDescription)
        {
            Assert::IsNull    (SymbolDescriptions::Find ("NOTASYMBOL"));
            Assert::IsNotNull (SymbolDescriptions::Find ("RD80STORE"));
        }
    };
}
