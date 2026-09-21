#include "Pch.h"

#include "Debugger/RomSymbols.h"
#include "Debugger/SymbolDescriptions.h"

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

                    Assert::IsNotNull (SymbolDescriptions::Find (name), std::wstring (name.begin(), name.end()).c_str());
                }
            }
        }


        TEST_METHOD (AnUnknownNameHasNoDescription)
        {
            Assert::IsNull    (SymbolDescriptions::Find ("NOTASYMBOL"));
            Assert::IsNotNull (SymbolDescriptions::Find ("RD80STORE"));
        }
    };
}
