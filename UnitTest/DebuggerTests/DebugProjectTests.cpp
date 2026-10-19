#include "Pch.h"

#include "DebuggerTests/HandlerTestRig.h"
#include "Debugger/DebugHandlerSet.h"
#include "Debugger/Project/DebugProject.h"
#include "Debugger/Project/DebugProjectStore.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProjectTests
//
//  The debug project's engine part, its JSON and its store, over in-memory
//  files (FR-166 to FR-168, FR-170, SC-046): what one session sets comes
//  back in another; a symbol file beside the project is written relative to
//  it; the store opens, saves, saves under a name and copies an unreadable
//  file aside; and a save waits two seconds past the last change.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (DebugProjectTests)
    {
    public:

        struct SessionRig
        {
            HandlerRig<BreakpointHandlers>  rig;
            DebugHandlerSet                 handlers;

            SessionRig()
            {
                handlers.Attach (rig.session);
            }
        };

        static std::string List (SessionRig & session, const std::string & line)
        {
            std::string  text;



            for (const std::string & each : session.rig.RunOk (line).text)
            {
                text += each + "\n";
            }

            return text;
        }


        //  Breakpoints with a name and a disabled one, a watch, a zero-page
        //  pointer, a bookmark, a data block, a step filter and a symbol file
        //  set in one session are all there in the next, through the JSON.
        TEST_METHOD (WhatOneSessionSetsComesBackInAnother)
        {
            std::unique_ptr<SessionRig>    firstRig  = std::make_unique<SessionRig>();
            std::unique_ptr<SessionRig>    secondRig = std::make_unique<SessionRig>();
            SessionRig                   & first     = *firstRig;
            SessionRig                   & second    = *secondRig;
            DebugProject                   project;
            DebugProject                   read;
            std::string                    error;
            std::vector<std::string>       failures;



            first.rig.files.WriteAllText (L"C:\\Work\\prog.sym", "0900 PRINTIT\n");
            second.rig.files.WriteAllText (L"C:\\Work\\prog.sym", "0900 PRINTIT\n");

            first.rig.RunOk ("SYMUSER LOAD prog.sym");
            first.rig.RunOk ("BP PRINTIT");
            first.rig.RunOk ("BPM 400:40F");
            first.rig.RunOk ("BPBASIC 100");
            first.rig.RunOk ("BPNAME 0 \"Print it\"");
            first.rig.RunOk ("BPD 1");
            first.rig.RunOk ("WA 300");
            first.rig.RunOk ("ZP0 6");
            first.rig.RunOk ("BMA 800");
            first.rig.RunOk ("DB TABLE = 1000:100F");
            first.rig.RunOk ("SKIP 0A00.0A20");

            project.CaptureEngine (first.rig.session);

            Assert::IsTrue (DebugProject::TryParse (project.ToJson (L"C:\\Work"), L"C:\\Work", read, error), TextEncoding::NarrowToWide (error).c_str());
            read.ApplyEngine (second.rig.session, failures);

            Assert::IsTrue   (failures.empty(), failures.empty() ? L"" : TextEncoding::NarrowToWide (failures.front()).c_str());
            Assert::AreEqual (List (first, "BPL"),  List (second, "BPL"));
            Assert::AreEqual (List (first, "WL"),   List (second, "WL"));
            Assert::AreEqual (List (first, "ZPL"),  List (second, "ZPL"));
            Assert::AreEqual (List (first, "BML"),  List (second, "BML"));
            Assert::AreEqual (List (first, "DB"),   List (second, "DB"));
            Assert::AreEqual (List (first, "SKIP"), List (second, "SKIP"));
            Assert::IsTrue   (List (second, "BPL").find ("\"Print it\"") != std::string::npos);
            Assert::IsTrue   (List (second, "BPL").find ("BASIC line 100") != std::string::npos);
            Assert::IsTrue   (List (second, "WL").find ("0300") != std::string::npos);
            Assert::IsTrue   (List (second, "DB").find ("TABLE") != std::string::npos);
            Assert::IsTrue   (List (second, "SKIP").find ("0A00") != std::string::npos);
            Assert::AreEqual ((size_t) 1, second.rig.session.GetSymbolLoads().size());
        }

        //  A project saved with the same breakpoint twice restores it once,
        //  and the second is no failure.
        TEST_METHOD (ASavedDuplicateBreakpointRestoresOnce)
        {
            std::unique_ptr<SessionRig>    rigHeap  = std::make_unique<SessionRig>();
            SessionRig                   & session  = *rigHeap;
            DebugProject                   project;
            std::vector<std::string>       failures;



            project.engineScript = { "BPC *", "BP C283", "BP C283" };
            project.ApplyEngine (session.rig.session, failures);

            Assert::IsTrue   (failures.empty(), failures.empty() ? L"" : TextEncoding::NarrowToWide (failures.front()).c_str());
            Assert::AreEqual ((size_t) 1, session.rig.session.GetBreakpoints().GetAll().size());
        }

        //  The session's other state comes back too: a symbol table turned
        //  off, the CD directory, the stopwatch, the profiler, the command
        //  mode and the trip count's mark; and the window's states by name.
        TEST_METHOD (TheSessionsSettingsAndTheWindowsStatesComeBack)
        {
            std::unique_ptr<SessionRig>    firstRig  = std::make_unique<SessionRig>();
            std::unique_ptr<SessionRig>    secondRig = std::make_unique<SessionRig>();
            SessionRig                   & first     = *firstRig;
            SessionRig                   & second    = *secondRig;
            DebugProject                   project;
            DebugProject                   read;
            std::string                    error;
            std::vector<std::string>       failures;



            first.rig.RunOk ("SYMDOS33 OFF");
            first.rig.RunOk ("CD \"C:\\Work\"");
            first.rig.RunOk ("STOPWATCH 300 310");
            first.rig.RunOk ("PROFILE ON");
            first.rig.session.SetCycleMarker (1234);
            first.rig.RunOk ("MODE MONITOR");

            project.CaptureEngine (first.rig.session, &first.handlers.GetExecution());
            project.windowStrings = { { "console", "BP 300\nG\n" }, { "focusedPane", "watches" } };

            Assert::IsTrue (DebugProject::TryParse (project.ToJson (L"C:\\Work"), L"C:\\Work", read, error), TextEncoding::NarrowToWide (error).c_str());
            read.ApplyEngine (second.rig.session, failures);

            Assert::IsTrue   (failures.empty(), failures.empty() ? L"" : TextEncoding::NarrowToWide (failures.front()).c_str());
            Assert::IsFalse  (second.rig.session.GetSymbols().IsEnabled (SymbolTableId::Dos33), L"the table stays off");
            Assert::IsTrue   (second.rig.session.GetSymbols().IsEnabled (SymbolTableId::Main));
            Assert::IsTrue   (second.rig.session.GetCurrentDirectory() == L"C:\\Work");
            Assert::IsTrue   (second.rig.session.GetMode() == CommandMode::Monitor);
            Assert::AreEqual ((uint64_t) 1234, second.rig.session.GetCycleMarker());
            Assert::IsTrue   (std::ranges::find (read.engineScript, std::string ("STOPWATCH 0300 0310")) != read.engineScript.end());
            Assert::IsTrue   (std::ranges::find (read.engineScript, std::string ("PROFILE ON")) != read.engineScript.end());
            Assert::IsTrue   (read.windowStrings == project.windowStrings, L"the window's states through the JSON");
        }

        //  Every breakpoint, watchpoint, watch and bookmark comes back under
        //  its own id, gaps and all, and the next one added takes the id it
        //  would have taken in the first session.
        TEST_METHOD (EachItemKeepsItsIdAndTheNextIdIsKept)
        {
            std::unique_ptr<SessionRig>    firstRig  = std::make_unique<SessionRig>();
            std::unique_ptr<SessionRig>    secondRig = std::make_unique<SessionRig>();
            SessionRig                   & first     = *firstRig;
            SessionRig                   & second    = *secondRig;
            DebugProject                   project;
            DebugProject                   read;
            std::string                    error;
            std::vector<std::string>       failures;
            BreakpointListData             list;
            std::vector<int>               ids;



            first.rig.RunOk ("BP 300");
            first.rig.RunOk ("BPM 400");
            first.rig.RunOk ("BP 302");
            first.rig.RunOk ("BPC 0");
            first.rig.RunOk ("WA 300");
            first.rig.RunOk ("WA 301");
            first.rig.RunOk ("WC 0");
            first.rig.RunOk ("BMA 800");
            first.rig.RunOk ("BMA 900");
            first.rig.RunOk ("BMC 0");

            project.CaptureEngine (first.rig.session);

            Assert::IsTrue (DebugProject::TryParse (project.ToJson (L"C:\\Work"), L"C:\\Work", read, error), TextEncoding::NarrowToWide (error).c_str());
            read.ApplyEngine (second.rig.session, failures);
            Assert::IsTrue (failures.empty(), failures.empty() ? L"" : TextEncoding::NarrowToWide (failures.front()).c_str());

            BreakpointHandlers::ListAll (second.rig.session, list);

            for (const BreakpointInfo & each : list.breakpoints)
            {
                ids.push_back (each.id);
            }

            Assert::IsTrue   ((std::vector<int> { 1, 2 }) == ids, L"the watchpoint and the breakpoint are ids 1 and 2");
            Assert::AreEqual (1, second.rig.session.GetWatches().GetAll()[0].id, L"the watch is id 1");
            Assert::AreEqual (1, second.rig.session.GetBookmarks().GetAll()[0].id, L"the bookmark is id 1");

            Assert::IsTrue   (second.rig.RunOk ("BP 500").text.front().find ("3") != std::string::npos, L"the next breakpoint is 3");
            Assert::AreEqual (2, second.rig.session.GetWatches().Add (0x0302), L"the next watch is 2");
        }



        //  TF ON and TF OFF can run again with nothing changed, as a debug
        //  project runs them when it opens.
        TEST_METHOD (TfOnAndOffCanRunAgain)
        {
            SessionRig  rig;
            Reply       reply;



            rig.rig.RunOk ("TF ON \"trace.txt\"");
            reply = rig.rig.RunOk ("TF ON \"trace.txt\"");
            Assert::IsTrue   (reply.text.front().find ("Trace on") != std::string::npos, L"on, not turned off");
            Assert::AreEqual (std::string ("TF ON \"trace.txt\""), rig.handlers.GetExecution().MakeScript().back());

            rig.rig.RunOk ("TF OFF");
            reply = rig.rig.RunOk ("TF OFF");
            Assert::IsTrue   (reply.text.front().find ("Trace off") != std::string::npos);
            Assert::AreEqual (std::string ("TF OFF"), rig.handlers.GetExecution().MakeScript().back());
        }

        //  A symbol file beside or below the project's file is written
        //  relative to it, so the project moves with its program; one
        //  elsewhere is written whole.
        TEST_METHOD (APathInsideTheProjectsFolderIsRelative)
        {
            DebugProject  project;
            DebugProject  read;
            std::string   json;
            std::string   error;



            project.symbolFiles = { { SymbolTableId::User, L"C:\\Work\\Prog\\build\\prog.dbg", 0x0800 },
                                    { SymbolTableId::User2, L"D:\\Shared\\rom.sym", 0 } };
            project.breakpointUndo = "2|1:0||";

            json = project.ToJson (L"C:\\Work\\Prog");

            Assert::IsTrue (json.find ("\"build\\\\prog.dbg\"") != std::string::npos, TextEncoding::NarrowToWide (json).c_str());
            Assert::IsTrue (json.find ("\"D:\\\\Shared\\\\rom.sym\"") != std::string::npos, TextEncoding::NarrowToWide (json).c_str());

            Assert::IsTrue (DebugProject::TryParse (json, L"E:\\Moved", read, error));
            Assert::AreEqual (std::wstring (L"E:\\Moved\\build\\prog.dbg"), read.symbolFiles[0].path, L"moved with the project");
            Assert::AreEqual (std::wstring (L"D:\\Shared\\rom.sym"),        read.symbolFiles[1].path);
            Assert::AreEqual (0x0800, read.symbolFiles[0].offset);
            Assert::AreEqual (std::string ("2|1:0||"), read.breakpointUndo);
        }

        //  The scope's automatic file; a save under a name writes that file
        //  and points the automatic one at it, so the scope opens it.
        TEST_METHOD (TheStoreOpensSavesAndFollowsANamedProject)
        {
            InMemoryFileSystem         files;
            DebugProjectStore          store (files, L"C:\\Data\\DebugProjects");
            ProjectScope               scope   = ProjectScope::Choose ("", "", "", "", "Apple //e", L"C:\\Disks\\Game.dsk");
            DebugProject               project;
            DebugProject               again;
            std::vector<std::string>   notices;



            AssertSucceeded (store.Open (scope, project, notices), L"store.Open");
            Assert::IsTrue (project.isAutomatic && project.engineScript.empty() && notices.empty(), L"a new scope opens empty");
            Assert::AreEqual (store.GetAutomaticPath (scope), project.filePath);

            project.engineScript    = { "BPC *", "BP 0300" };
            project.startupCommands = { { "ECHO ready", CommandMode::AppleWin }, { "r", CommandMode::WinDbg } };
            AssertSucceeded (store.Save (project), L"store.Save");
            AssertSucceeded (store.Open (scope, again, notices), L"store.Open");
            Assert::IsTrue (again.engineScript == project.engineScript && again.startupCommands == project.startupCommands);

            AssertSucceeded (store.SaveAs (project, L"C:\\Work\\game.cassoproj"), L"store.SaveAs");
            Assert::IsFalse (project.isAutomatic);
            Assert::IsTrue  (files.Exists (L"C:\\Work\\game.cassoproj"));

            AssertSucceeded (store.Open (scope, again, notices), L"store.Open");
            Assert::IsFalse (again.isAutomatic, L"the automatic file points at the chosen one");
            Assert::AreEqual (std::wstring (L"C:\\Work\\game.cassoproj"), again.filePath);

            files.Delete (L"C:\\Work\\game.cassoproj");
            AssertSucceeded (store.Open (scope, again, notices), L"store.Open");
            Assert::IsTrue (again.isAutomatic && again.engineScript.empty());
            Assert::AreEqual ((size_t) 1, notices.size(), L"the chosen file has gone");
        }

        //  A file that is not a project is copied aside under a dated name,
        //  never deleted, and an empty project opens with a notice.
        TEST_METHOD (AnUnreadableFileIsCopiedAside)
        {
            InMemoryFileSystem         files;
            DebugProjectStore          store (files, L"C:\\Data\\DebugProjects", [] { return std::string ("20261015-013000"); });
            ProjectScope               scope = ProjectScope::Choose ("", "", "", "", "Apple //e", L"");
            DebugProject               project;
            std::vector<std::string>   notices;



            files.WriteAllText (store.GetAutomaticPath (scope), "{ this is not");

            AssertSucceeded (store.Open (scope, project, notices), L"store.Open");
            Assert::IsTrue   (project.engineScript.empty());
            Assert::AreEqual ((size_t) 1, notices.size());
            Assert::IsTrue   (files.Exists (store.GetAutomaticPath (scope) + L".unreadable-20261015-013000"));
            Assert::AreEqual (std::string ("{ this is not"), files.PeekContent (store.GetAutomaticPath (scope) + L".unreadable-20261015-013000"));
        }

        TEST_METHOD (ASaveWaitsTwoSecondsPastTheLastChange)
        {
            InMemoryFileSystem  files;
            DebugProjectStore   store (files, L"C:\\Data");



            Assert::IsFalse (store.IsSaveDue (10'000));

            store.MarkChanged (1000);
            store.MarkChanged (1500);
            Assert::IsFalse (store.IsSaveDue (3499), L"each change starts the wait again");
            Assert::IsTrue  (store.IsSaveDue (3500));

            store.ClearChanged();
            Assert::IsFalse (store.IsSaveDue (9000));
        }

        //  The most specific signal chooses the scope, and each scope has a file of its own.
        TEST_METHOD (TheMostSpecificSignalChoosesTheScope)
        {
            ProjectScope  debug   = ProjectScope::Choose ("abc", "prog.dbg", "def@0800", "prog.bin", "Apple //e", L"C:\\Game.dsk");
            ProjectScope  binary  = ProjectScope::Choose ("",    "",         "def@0800", "prog.bin", "Apple //e", L"C:\\Game.dsk");
            ProjectScope  boot    = ProjectScope::Choose ("",    "",         "",         "",         "Apple //e", L"C:\\Game.dsk");
            ProjectScope  machine = ProjectScope::Choose ("",    "",         "",         "",         "Apple //e", L"");
            ProjectScope  other   = ProjectScope::Choose ("",    "",         "",         "",         "Apple //c", L"C:\\Game.dsk");



            Assert::IsTrue (debug.kind   == ProjectScopeKind::DebugFile);
            Assert::IsTrue (binary.kind  == ProjectScopeKind::Binary);
            Assert::IsTrue (boot.kind    == ProjectScopeKind::MachineBoot);
            Assert::IsTrue (machine.kind == ProjectScopeKind::Machine);

            Assert::IsTrue  (debug.IsMoreSpecificThan (boot));
            Assert::IsFalse (machine.IsMoreSpecificThan (boot));

            Assert::AreNotEqual (boot.GetFileName(), other.GetFileName(), L"one disk on two machine types");
            Assert::AreNotEqual (boot.GetFileName(), machine.GetFileName());
            Assert::AreEqual    (boot.GetFileName(), ProjectScope::Choose ("", "", "", "", "Apple //e", L"C:\\Game.dsk").GetFileName());
        }
    };
}
