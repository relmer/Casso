#include "Pch.h"

#include "ControllerRig.h"

#include "CppUnitTest.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BloadCompanionWindowTests
    //
    //  A symbol file BLOAD finds beside the binary is reported in the window's
    //  console, with the path it was read from, whether the binary's path was
    //  typed absolute or relative.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (BloadCompanionWindowTests)
    {
    public:
        static std::string Joined (const Reply & reply)
        {
            std::string  all = reply.error.detail;



            for (const std::string & line : reply.text)
            {
                all += "\n" + line;
            }

            return all;
        }

        static std::wstring Widen (const std::string & text)
        {
            return std::wstring (text.begin(), text.end());
        }



        TEST_METHOD (AbsolutePath_ConsoleShowsSymbolLoadWithPath)
        {
            ControllerRig  rig;
            Reply          reply;
            std::string    all;



            rig.files.WriteAllText (L"C:\\Work\\prog.bin", std::string ("\xA9\x41\x60", 3));
            rig.files.WriteAllText (L"C:\\Work\\prog.sym", "0300 START\n");

            reply = rig.Run ("BLOAD C:\\Work\\prog.bin 300");
            all   = Joined (reply);

            Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, Widen (all).c_str());
            Assert::IsTrue   (all.find ("Loaded 1 symbols into user from C:\\Work\\prog.sym.") != std::string::npos, Widen (all).c_str());
        }

        TEST_METHOD (EveryMode_ConsoleShowsSymbolLoad)
        {
            for (CommandMode mode : { CommandMode::AppleWin, CommandMode::WinDbg })
            {
                ControllerRig  rig;
                Reply          reply;
                std::string    all;



                rig.files.WriteAllText (L"C:\\Work\\prog.bin", std::string ("\xA9\x41\x60", 3));
                rig.files.WriteAllText (L"C:\\Work\\prog.sym", "0300 START\n");

                reply = rig.Run (mode == CommandMode::WinDbg ? "!bload C:\\Work\\prog.bin 300" : "BLOAD C:\\Work\\prog.bin 300", mode);
                all   = Joined (reply);

                Assert::IsTrue (all.find ("from C:\\Work\\prog.sym") != std::string::npos, Widen (all).c_str());
            }
        }
    };
}
