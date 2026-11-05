#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Debugger/CassoCommandReference.h"
#include "Debugger/DebugHandlerSet.h"
#include "Debugger/Handlers/MachineHandlers.h"
#include "Debugger/IMachineControl.h"
#include "Debugger/ReplyJson.h"
#include "HandlerTestRig.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineCommandTests
//
//  RESET, PASTE, DISK INSERT and EJECT, SCREEN, SCREENSHOT and EXIT, in every
//  dialect, through a machine control that stores each call.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  RecordingMachineControl
    //
    ////////////////////////////////////////////////////////////////////////////////

    class RecordingMachineControl : public IMachineControl
    {
    public:
        void     Reset          (bool isPowerCycle) override                         { resets.push_back (isPowerCycle); }
        void     InsertDisk     (int drive, const std::wstring & path) override      { inserts.emplace_back (drive, path); }
        void     EjectDisk      (int drive) override                                 { ejects.push_back (drive); }
        void     PasteText      (const std::wstring & text) override                 { pasted += text; }
        void     Exit           () override                                          { ++exits; }

        bool     GetScreenText  (int & columns, std::vector<std::string> & rows) override
        {
            columns = screenColumns;
            rows    = screenRows;
            return hasScreen;
        }

        HRESULT  SaveScreenshot (const std::wstring & path, int & width, int & height) override
        {
            shotPath = path;
            width    = 560;
            height   = 384;
            return shotResult;
        }

        std::vector<bool>                          resets;
        std::vector<std::pair<int, std::wstring>>  inserts;
        std::vector<int>                           ejects;
        std::wstring                               pasted;
        int                                        exits         = 0;
        bool                                       hasScreen     = true;
        int                                        screenColumns = 40;
        std::vector<std::string>                   screenRows    = { "]10 GR", "]RUN" };
        std::wstring                               shotPath;
        HRESULT                                    shotResult    = S_OK;
    };





    TEST_CLASS (MachineCommandTests)
    {
    public:

        struct Rig
        {
            MockDebugTarget            target;
            RecordingNotificationSink  sink;
            DebugSession               session { target, sink, RunState::Paused };
            DebugHandlerSet            handlers;
            RecordingMachineControl    control;

            Rig()
            {
                handlers.Attach (session);
                session.SetMachineControl (&control);
            }
        };



        TEST_METHOD (EveryDialectReachesTheMachineCommands)
        {
            static constexpr const char * kCases[][2] =
            {
                { "APPLEWIN",  "RESET"        },
                { "CASSO",     "RESET POWER"  },
                { "MONITOR",   "/RESET"       },
                { "WINDBG",    "!reset power" },
                { "GSSQUARED", "reset"        },
            };



            for (const auto & each : kCases)
            {
                Rig     rig;
                Reply   reply;



                rig.session.ExecuteLine (std::string ("MODE ") + each[0], CommandMode::AppleWin);
                reply = rig.session.ExecuteLine (each[1]);

                Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, TextEncoding::NarrowToWide (std::string (each[1]) + ": " + reply.error.detail).c_str());
                Assert::AreEqual ((size_t) 1, rig.control.resets.size());
                Assert::AreEqual (std::string (each[1]).find_first_of ("Pp") != std::string::npos, (bool) rig.control.resets[0]);
            }
        }



        TEST_METHOD (ResetTakesSoftOrPowerOnly)
        {
            Rig     rig;
            Reply   reply = rig.session.ExecuteLine ("RESET HARD", CommandMode::AppleWin);



            Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status);
            Assert::IsTrue   (rig.control.resets.empty());
        }



        //  The quotes come off, \n is a return, and the case is as typed.
        TEST_METHOD (PasteTypesItsTextWithItsEscapes)
        {
            Rig     rig;
            Reply   reply = rig.session.ExecuteLine ("PASTE \"10 GR\\n20 PRINT \\\"Hi\\\"\\n\"", CommandMode::AppleWin);



            Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, TextEncoding::NarrowToWide (reply.error.detail).c_str());
            Assert::AreEqual (std::wstring (L"10 GR\r20 PRINT \"Hi\"\r"), rig.control.pasted);

            reply = rig.session.ExecuteLine ("PASTE abc", CommandMode::AppleWin);
            Assert::AreEqual (std::wstring (L"10 GR\r20 PRINT \"Hi\"\rabc"), rig.control.pasted, L"unquoted text is typed in its case");

            reply = rig.session.ExecuteLine ("PASTE \"bad \\q\"", CommandMode::AppleWin);
            Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status, L"an escape it doesn't take");

            reply = rig.session.ExecuteLine ("PASTE", CommandMode::AppleWin);
            Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status, L"no text");
        }



        TEST_METHOD (DiskInsertAndEjectGoToTheirDrives)
        {
            Rig            rig;
            Reply          reply;
            wchar_t        temp[MAX_PATH] = {};
            std::wstring   file;



            GetTempPathW (MAX_PATH, temp);
            file = std::wstring (temp) + L"machine-command-test.dsk";
            CloseHandle (CreateFileW (file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));

            reply = rig.session.ExecuteLine ("DISK INSERT D2 \"" + TextEncoding::WideToNarrow (file) + "\"", CommandMode::AppleWin);
            Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, TextEncoding::NarrowToWide (reply.error.detail).c_str());
            Assert::AreEqual ((size_t) 1, rig.control.inserts.size());
            Assert::AreEqual (1, rig.control.inserts[0].first);
            Assert::AreEqual (file, rig.control.inserts[0].second);

            reply = rig.session.ExecuteLine ("DISK INSERT " + TextEncoding::WideToNarrow (file), CommandMode::AppleWin);
            Assert::AreEqual (0, rig.control.inserts[1].first, L"drive 1 by default");

            reply = rig.session.ExecuteLine ("DISK EJECT D2", CommandMode::AppleWin);
            Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status);
            Assert::AreEqual (1, rig.control.ejects.at (0));

            reply = rig.session.ExecuteLine ("DISK INSERT D1 nowhere\\missing.dsk", CommandMode::AppleWin);
            Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status, L"a missing file");
            Assert::AreEqual ((size_t) 2, rig.control.inserts.size());

            DeleteFileW (file.c_str());
        }



        TEST_METHOD (ScreenRepliesWithTheRows)
        {
            Rig        rig;
            Reply      reply = rig.session.ExecuteLine ("SCREEN", CommandMode::AppleWin);
            auto     * data  = std::get_if<TextScreenData> (&reply.data);



            Assert::IsNotNull (data);
            Assert::AreEqual  (40, data->columns);
            Assert::AreEqual  (std::string ("]RUN"), data->rows.at (1));
            Assert::IsTrue    (ReplyJson::WriteReply (reply, 1).find ("\"kind\":\"textScreen\"") != std::string::npos);
        }



        TEST_METHOD (ScreenshotSavesToTheFileOrADefaultName)
        {
            Rig        rig;
            Reply      reply = rig.session.ExecuteLine ("SCREENSHOT \"My Shot.png\"", CommandMode::AppleWin);
            auto     * data  = std::get_if<ScreenshotData> (&reply.data);
            SYSTEMTIME when  = { 2026, 11, 3, 4, 13, 5, 9, 42 };



            Assert::IsNotNull (data);
            Assert::IsTrue    (rig.control.shotPath.ends_with (L"My Shot.png"), rig.control.shotPath.c_str());
            Assert::AreEqual  (560, data->width);
            Assert::AreEqual  (384, data->height);

            Assert::AreEqual (std::string ("casso-screenshot-20261104-130509-042.png"), MachineHandlers::GetDefaultScreenshotName (when));

            rig.control.shotResult = E_ACCESSDENIED;
            reply = rig.session.ExecuteLine ("SCREENSHOT", CommandMode::AppleWin);
            Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status);
            Assert::IsTrue   (rig.control.shotPath.find (L"casso-screenshot-") != std::wstring::npos);
        }



        TEST_METHOD (ExitClosesCasso)
        {
            Rig     rig;
            Reply   reply = rig.session.ExecuteLine ("EXIT", CommandMode::AppleWin);



            Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status);
            Assert::AreEqual (1, rig.control.exits);
        }



        TEST_METHOD (WithoutTheEmulatorTheCommandsAreNotAvailable)
        {
            MockDebugTarget            target;
            RecordingNotificationSink  sink;
            DebugSession               session (target, sink, RunState::Paused);
            DebugHandlerSet            handlers;



            handlers.Attach (session);

            for (const char * line : { "RESET", "PASTE hi", "SCREEN", "SCREENSHOT", "EXIT", "DISK EJECT" })
            {
                Reply  reply = session.ExecuteLine (line, CommandMode::AppleWin);



                Assert::AreEqual ((int) CommandStatus::NotAvailable, (int) reply.status, TextEncoding::NarrowToWide (line).c_str());
            }
        }



        TEST_METHOD (EachCommandHasAHelpRow)
        {
            for (const char * name : { "RESET", "PASTE", "SCREEN", "SCREENSHOT", "EXIT" })
            {
                Assert::IsNotNull (CassoCommandReference::Find (name), TextEncoding::NarrowToWide (name).c_str());
            }
        }
    };
}
