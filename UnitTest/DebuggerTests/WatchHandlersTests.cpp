#include "Pch.h"

#include "Debugger/Handlers/WatchHandlers.h"
#include "HandlerTestRig.h"
#include "TestHelpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlersTests
//
//  Watches, zero-page pointers and bookmarks, and their saved scripts.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (WatchHandlersTests)
    {
    public:

        using Rig = HandlerRig<WatchHandlers>;



        TEST_METHOD (W_AddListDisableClear)
        {
            Rig  rig;



            rig.target.memory[0x0036] = 0xF0;
            rig.target.memory[0x0037] = 0xFD;

            Assert::AreEqual (std::string ("#0 $0036 = $FDF0"), rig.RunOk ("WA 36").text.at (0), L"a watch shows the word at its address");
            Assert::AreEqual (std::string ("#1 $003C = $0000"), rig.RunOk ("W 3C").text.at (0));
            Assert::AreEqual ((size_t) 2, rig.RunOk ("WL").text.size());
            Assert::AreEqual ((size_t) 2, rig.RunOk ("WA").text.size(), L"WA alone lists");

            Assert::AreEqual (std::string ("#0 $0036 = $FDF0 (disabled)"), rig.RunOk ("WD 0").text.at (0));
            Assert::AreEqual (std::string ("#0 $0036 = $FDF0"),            rig.RunOk ("WE 0").text.at (0));
            Assert::AreEqual (std::string ("Watch #0 cleared."),           rig.RunOk ("WC 0").text.at (0));
            Assert::AreEqual (std::string ("#1 $003C = $0000"),            rig.RunOk ("WL").text.at (0));
            Assert::AreEqual (std::string ("No watches."),                 rig.RunOk ("WC *").text.at (0));
            rig.RunFails ("WC 5", "no such watch");
            rig.RunFails ("WD 5", "no such watch");
        }



        TEST_METHOD (ZP_SlotsAndPointers)
        {
            Rig                       rig;
            std::vector<std::string>  list;



            rig.target.memory[0x0036] = 0xF0;
            rig.target.memory[0x0037] = 0xFD;

            Assert::AreEqual (std::string ("#0 $0036 -> $FDF0"), rig.RunOk ("ZP 36").text.at (0));
            Assert::AreEqual (std::string ("#3 $003C -> $0000"), rig.RunOk ("ZP3 3C").text.at (0), L"ZP3 fills slot 3");
            Assert::AreEqual (std::string ("#2 $0040 -> $0000"), rig.RunOk ("P2 40").text.at (0), L"P2 is ZP2");
            Assert::AreEqual (std::string ("#3 $0036 -> $FDF0"), rig.RunOk ("ZP3 36").text.at (0), L"a slot is replaced");

            list = rig.RunOk ("ZPL").text;
            Assert::AreEqual ((size_t) 3, list.size());
            Assert::IsTrue   (list[1].starts_with ("#2"), L"listed by slot");
            Assert::AreEqual (std::string ("#4 $0050 -> $0000"), rig.RunOk ("ZPA 50").text.at (0), L"the next id follows the highest slot");

            Assert::AreEqual (std::string ("#3 $0036 -> $FDF0 (disabled)"), rig.RunOk ("ZPD 3").text.at (0));
            rig.RunOk ("ZPE 3");
            Assert::AreEqual (std::string ("Zero-page pointer #3 cleared."), rig.RunOk ("ZPC 3").text.at (0));
            Assert::AreEqual (std::string ("No zero-page pointers."),        rig.RunOk ("ZPC *").text.at (0));
            rig.RunFails ("ZPC 9", "no such zero-page pointer");
        }



        TEST_METHOD (BM_AddListGo)
        {
            TestCpu  cpu;
            Rig      rig;
            Reply    reply;



            cpu.InitForTest();
            rig.target.instructionSet = cpu.GetInstructionSet();
            rig.target.memory[0x0400] = 0xA9;
            rig.target.memory[0x0401] = 0x41;

            Assert::AreEqual (std::string ("#0 $0400"), rig.RunOk ("BMA 400").text.at (0));
            Assert::AreEqual (std::string ("#1 $0500"), rig.RunOk ("bm 500").text.at (0));
            Assert::AreEqual ((size_t) 2, rig.RunOk ("BML").text.size());

            reply = rig.RunOk ("BMG 0");
            Assert::AreEqual ((size_t) 20, reply.text.size(), L"BMG lists code at the bookmark");
            Assert::AreEqual (std::string ("0400: A9 41    LDA  #$41"), reply.text[0]);

            rig.RunFails ("BMG 7", "no such bookmark");
            Assert::AreEqual (std::string ("Bookmark #1 cleared."), rig.RunOk ("BMC 1").text.at (0));
            Assert::AreEqual (std::string ("No bookmarks."),        rig.RunOk ("BMC *").text.at (0));
        }



        TEST_METHOD (Saves_WriteScripts_ReplayRestoresTheLists)
        {
            Rig                       rig;
            std::vector<std::string>  watches;
            std::vector<std::string>  pointers;
            std::string               script;



            rig.RunOk ("WA 36");
            Assert::AreEqual (std::string ("Saved 1 watch to w.txt."), rig.RunOk ("WSAVE w.txt").text.at (0));
            rig.RunOk ("WA 3C");
            rig.RunOk ("WD 1");
            rig.RunOk ("ZP 36");
            rig.RunOk ("ZP5 3C");
            rig.RunOk ("BMA 400");
            watches  = rig.RunOk ("WL").text;
            pointers = rig.RunOk ("ZPL").text;

            Assert::AreEqual (std::string ("Saved 2 watches to w.txt."), rig.RunOk ("WSAVE w.txt").text.at (0));
            Assert::AreEqual (std::string ("WC *\nWA 0036\nWA 003C\nWD 1\n"), rig.files.PeekContent (L"C:\\Work\\w.txt"));
            rig.RunOk ("ZPSAVE z.txt");
            Assert::AreEqual (std::string ("ZPC *\nZP0 0036\nZP5 003C\n"),    rig.files.PeekContent (L"C:\\Work\\z.txt"));
            rig.RunOk ("BMSAVE b.txt");
            Assert::AreEqual (std::string ("BMC *\nBMA 0400\n"),              rig.files.PeekContent (L"C:\\Work\\b.txt"));

            rig.RunOk ("WC *");
            rig.RunOk ("WA 1234");
            rig.RunOk ("ZPC *");

            for (const wchar_t * name : { L"C:\\Work\\w.txt", L"C:\\Work\\z.txt" })
            {
                script = rig.files.PeekContent (name);

                for (size_t start = 0, end = script.find ('\n'); end != std::string::npos; start = end + 1, end = script.find ('\n', start))
                {
                    rig.RunOk (script.substr (start, end - start));
                }
            }

            Assert::IsTrue (watches  == rig.RunOk ("WL").text,  L"the replayed watches list as the saved ones did");
            Assert::IsTrue (pointers == rig.RunOk ("ZPL").text, L"the replayed pointers keep their slots");
            rig.RunFails ("WSAVE", "invalid arguments");
        }
    };
}
