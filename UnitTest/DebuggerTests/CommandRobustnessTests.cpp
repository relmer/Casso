#include "Pch.h"

#include "ControllerRig.h"
#include "Debugger/AppleWinCommandTable.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CommandRobustnessTests
//
//  Every command name, in every command mode, with arguments a person or a
//  client might type by mistake: none, a star, a negative number, numbers
//  too large for 64 bits, a lone quote or parenthesis, and words of the
//  wrong kind. Each line gets a reply, an error at worst; none throws.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (CommandRobustnessTests)
    {
    public:

        TEST_METHOD (NoCommandThrowsOnHostileArguments)
        {
            static constexpr const char *  kArguments[] =
            {
                "", "*", "-1", "0", "FFFFFFFF", "99999999999999999999999", "-99999999999999999999s",
                "$", "#", "\"", "\"unterminated", "(((", ")", "{", "}", "0:FFFF", "FFFF:0", "300 .w", "x.y.z",
                "1 2 3 4 5 6 7 8 9", "IF", "IF (", "ON OFF", "CLEAR CLEAR", "C:\\NoSuch\\File.bin", "\\\\server\\share\\x",
                "0n", "0x", "@", "!", "%", "~", "300.s", "SYMBOL+", "*0", "-0", "1e9999",
                "READ 99999999999999999999", "LIST 99999999999999999999", "T 99999999999999999999", "CYCLE 99999999999999999999",
                "POS 99999999999999999999", "0300 99999999999999999999", "300 FFFFFFFFFFFFFFFFFFFF", "1 99999999999999999999",
                "ON 99999999999999999999", "LOAD \"\"", "SAVE \"\"", "BLOCK 99999999999999999999", "QT 99999999999999999999",
            };

            static constexpr CommandMode  kModes[] = { CommandMode::AppleWin, CommandMode::Monitor, CommandMode::GSSquared, CommandMode::WinDbg };

            std::unique_ptr<ControllerRig>  rig      = std::make_unique<ControllerRig>();
            std::vector<std::string>        failures;
            size_t                          lines    = 0;



            rig->controller.GetSession().SetBudget (2000);

            for (CommandMode mode : kModes)
            {
                for (const AppleWinCommand & command : AppleWinCommandTable::GetAll())
                {
                    for (const char * argument : kArguments)
                    {
                        std::string  name = (mode == CommandMode::AppleWin) ? std::string (command.name) : ToLower (command.name);
                        std::string  line = std::string (argument).empty() ? name : name + " " + argument;

                        try
                        {
                            (void) rig->Run (line, mode);
                        }
                        catch (const std::exception & e)
                        {
                            failures.push_back (std::format ("{} [mode {}]: {}", line, (int) mode, e.what()));
                        }

                        lines++;
                    }
                }
            }

            Logger::WriteMessage (std::format ("{} lines run\n", lines).c_str());

            for (const std::string & failure : failures)
            {
                Logger::WriteMessage ((failure + "\n").c_str());
            }

            Assert::AreEqual ((size_t) 0, failures.size(), L"lines that threw: see the output");
        }

    private:

        static std::string  ToLower (const char * text)
        {
            std::string  lower = text;

            std::ranges::transform (lower, lower.begin(), [] (char ch) { return (char) tolower ((unsigned char) ch); });
            return lower;
        }
    };
}
