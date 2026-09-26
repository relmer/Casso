#include "Pch.h"

#include "Debugger/AppleWinCommandTable.h"
#include "Debugger/CassoCommandReference.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoCommandReferenceTests
//
//  The help reference swept against the AppleWin command table in both
//  directions: every command the table runs has one entry, and every entry
//  is a command the table runs. Aliases and names reported as not available
//  have none.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (CassoCommandReferenceTests)
    {
    public:

        static bool IsListed (const AppleWinCommand & command)
        {
            //  Accepted and reported, never run: not available, or of no
            //  effect in a window that shows every pane at once. CODE, DATA
            //  and CONSOLE bring a pane forward, so they are listed.
            static constexpr const char * kOnlyReported[] =
            {
                "BENCHMARK", "EXITBENCH", "SOURCE1", "SOURCE2", "WIN", "WINDOW", "\\",
            };

            bool  isRunnable = command.availability == CommandAvailability::Headless ||
                               command.availability == CommandAvailability::WindowOnly;
            bool  isReported = command.family == AppleWinCommandFamily::Views      ||
                               command.family == AppleWinCommandFamily::Appearance ||
                               command.family == AppleWinCommandFamily::Unsupported ||
                               std::any_of (std::begin (kOnlyReported), std::end (kOnlyReported),
                                            [&] (const char * name) { return strcmp (name, command.name) == 0; });



            return command.aliasOf == nullptr && isRunnable && !isReported;
        }



        static std::wstring Widen (const char * text)
        {
            std::string  narrow (text);



            return std::wstring (narrow.begin(), narrow.end());
        }



        TEST_METHOD (EveryListedCommand_HasExactlyOneEntry)
        {
            size_t  checked = 0;



            for (const AppleWinCommand & command : AppleWinCommandTable::GetAll())
            {
                size_t  matches = 0;



                if (!IsListed (command))
                {
                    continue;
                }

                for (const CassoCommandReference::Entry & entry : CassoCommandReference::GetAll())
                {
                    if (std::string (entry.name) == command.name)
                    {
                        ++matches;
                    }
                }

                Assert::AreEqual ((size_t) 1, matches, Widen (command.name).c_str());
                ++checked;
            }

            Logger::WriteMessage (std::format ("Commands with an entry: {}\n", checked).c_str());

            Assert::IsTrue   (checked > 0);
            Assert::AreEqual (checked, CassoCommandReference::GetAll().size());
        }



        TEST_METHOD (EveryEntry_IsAListedCommand)
        {
            size_t  checked = 0;



            for (const CassoCommandReference::Entry & entry : CassoCommandReference::GetAll())
            {
                const AppleWinCommand  * command = AppleWinCommandTable::Find (entry.name);
                std::wstring             where   = Widen (entry.name);



                Assert::IsNotNull (command, where.c_str());
                Assert::AreEqual  (std::string (entry.name), std::string (command->name), where.c_str());
                Assert::IsTrue    (IsListed (*command), where.c_str());
                ++checked;
            }

            Assert::IsTrue (checked > 0);
        }



        TEST_METHOD (EveryEntry_HasSyntaxAndDescription)
        {
            size_t  checked = 0;



            for (const CassoCommandReference::Entry & entry : CassoCommandReference::GetAll())
            {
                std::string   syntax      (entry.syntax);
                std::string   description (entry.description);
                std::wstring  where       = Widen (entry.name);



                Assert::IsTrue  (syntax.starts_with (entry.name),                    where.c_str());
                Assert::IsFalse (description.empty(),                                where.c_str());
                Assert::IsTrue  (isupper ((unsigned char) description.front()) != 0, where.c_str());
                Assert::IsFalse (description.ends_with ('.'),                        where.c_str());
                ++checked;
            }

            Assert::IsTrue (checked > 0);
        }



        TEST_METHOD (Find_ResolvesAliasesAndIgnoresCase)
        {
            const CassoCommandReference::Entry  * carry = CassoCommandReference::Find ("RC");
            const CassoCommandReference::Entry  * step  = CassoCommandReference::Find ("tl");
            const CassoCommandReference::Entry  * bp    = CassoCommandReference::Find ("bp");



            Assert::IsNotNull (carry);
            Assert::AreEqual  (std::string ("CLC"), std::string (carry->name));
            Assert::IsNotNull (step);
            Assert::AreEqual  (std::string ("T"), std::string (step->name));
            Assert::IsNotNull (bp);
            Assert::AreEqual  (std::string ("BP"), std::string (bp->name));
        }



        TEST_METHOD (Find_ReportedAndUnknownNames_AreNull)
        {
            Assert::IsNull (CassoCommandReference::Find ("SHR"));
            Assert::IsNull (CassoCommandReference::Find ("TEXT"));
            Assert::IsNull (CassoCommandReference::Find ("FROB"));
            Assert::IsNull (CassoCommandReference::Find (""));
        }



        TEST_METHOD (EveryCategory_HasATitle)
        {
            for (const CassoCommandReference::Entry & entry : CassoCommandReference::GetAll())
            {
                Assert::IsTrue (strlen (CassoCommandReference::GetCategoryTitle (entry.category)) > 0, Widen (entry.name).c_str());
            }
        }
    };
}
