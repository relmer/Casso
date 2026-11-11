#include "Pch.h"

#include "DebuggerTests/HandlerTestRig.h"
#include "Debugger/DebugHandlerSet.h"
#include "Debugger/Project/DebugProject.h"
#include "Debugger/Project/DebugProjectStore.h"
#include "Debugger/Project/ProjectSwitchGuard.h"
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


        //  Two projects that each open the other switch back and forth until
        //  the guard pauses switching; a scope outside them ends the pause.
        TEST_METHOD (SwitchesBackAndForthAreHeld)
        {
            ProjectSwitchGuard  guard;
            ProjectScope        demo;
            ProjectScope        machine;
            ProjectScope        other;
            uint64_t            now     = 1000;
            int                 allowed = 0;



            demo.kind    = ProjectScopeKind::DebugFile;
            demo.key     = "demo";
            machine.kind = ProjectScopeKind::Machine;
            machine.key  = "Apple //e Enhanced";
            other.kind   = ProjectScopeKind::DebugFile;
            other.key    = "other";

            for (int i = 0; i < 20; i++, now += 16)
            {
                allowed += guard.TryAllow ((i % 2 == 0) ? machine : demo, now) ? 1 : 0;
            }

            Assert::AreEqual ((int) ProjectSwitchGuard::kMaxSwitches, allowed, L"a few switches, then none");
            Assert::IsTrue   (guard.IsHeld());
            Assert::IsFalse  (guard.TryAllow (demo, now + 10 * ProjectSwitchGuard::kWindowMs), L"paused however long it lasts");
            Assert::IsTrue   (guard.TryAllow (other, now), L"a new debug file ends the pause");
            Assert::IsFalse  (guard.IsHeld());
            Assert::IsTrue   (guard.TryAllow (demo, now + 1), L"and the old scopes switch again");
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

        //  Loading another debug file switches to its project, and the project
        //  switched from is saved after that load, so a debug file's project neither
        //  saves another debug file nor loads one again: two projects that did
        //  loaded each other's file over and over.
        TEST_METHOD (ADebugFilesProjectNeitherSavesNorLoadsAnotherDebugFile)
        {
            std::unique_ptr<SessionRig>    held     = std::make_unique<SessionRig>();   // heap: the rig overflows the frame (C6262)
            SessionRig                   & s        = *held;
            DebugProject                   one;
            DebugProject                   two;
            std::vector<std::string>       failures;



            s.rig.files.WriteAllText (L"C:\\Work\\one.dbg", "version\tmajor=2,minor=0\nsym\tid=0,name=\"ONE\",addrsize=absolute,val=0x6000,type=lab\n");
            s.rig.files.WriteAllText (L"C:\\Work\\two.dbg", "version\tmajor=2,minor=0\nsym\tid=0,name=\"TWO\",addrsize=absolute,val=0x7000,type=lab\n");

            s.rig.RunOk ("SYMUSER LOAD one.dbg");
            one.scope = { ProjectScopeKind::DebugFile, s.rig.session.GetDebugFileKey(), "one.dbg" };
            s.rig.RunOk ("SYMUSER LOAD two.dbg");
            two.scope = { ProjectScopeKind::DebugFile, s.rig.session.GetDebugFileKey(), "two.dbg" };

            one.CaptureEngine (s.rig.session);
            Assert::IsTrue (one.symbolFiles.empty(), L"one.dbg's project, saved with two.dbg loaded, doesn't list two.dbg");

            two.CaptureEngine (s.rig.session);
            Assert::AreEqual ((size_t) 1, two.symbolFiles.size(), L"two.dbg's project lists its own debug file");

            two.symbolFiles = { { SymbolTableId::User, L"C:\\Work\\one.dbg", 0 } };
            two.ApplyEngine (s.rig.session, failures);
            Assert::IsTrue (s.rig.session.GetDebugFilePath() == L"C:\\Work\\two.dbg", L"a project saved that way doesn't load the other file");
        }

        //  A load replaces its table, so the session has one load a table
        //  and a project replays only a table's last file. A project from
        //  before listed every file the table had been loaded from, BASIC
        //  programs among them; its BASIC programs are passed over.
        TEST_METHOD (OnlyATablesLastSymbolFileIsLoadedAgain)
        {
            std::unique_ptr<SessionRig>    firstRig  = std::make_unique<SessionRig>();   // heap: two rigs overflow the frame (C6262)
            std::unique_ptr<SessionRig>    secondRig = std::make_unique<SessionRig>();
            SessionRig                   & first     = *firstRig;
            SessionRig                   & second    = *secondRig;
            DebugProject                   project;
            std::vector<std::string>       failures;
            Word                           address   = 0;



            first.rig.files.WriteAllText (L"C:\\Work\\one.sym", "0900 FIRST\n");
            first.rig.files.WriteAllText (L"C:\\Work\\two.sym", "0A00 SECOND\n");
            first.rig.RunOk ("SYMUSER LOAD one.sym");
            first.rig.RunOk ("SYMUSER LOAD two.sym");
            Assert::AreEqual ((size_t) 1, first.rig.session.GetSymbolLoads().size(), L"the second load replaced the first");

            second.rig.files.WriteAllText (L"C:\\Work\\one.sym",  "0900 FIRST\n");
            second.rig.files.WriteAllText (L"C:\\Work\\game.bas", "10 HOME\n20 VTAB 5\n30 END\n");
            project.symbolFiles = { { SymbolTableId::User, L"C:\\Work\\one.sym", 0 }, { SymbolTableId::User, L"C:\\Work\\game.bas", 0 } };
            project.ApplyEngine (second.rig.session, failures);

            Assert::IsTrue  (failures.empty(), failures.empty() ? L"" : TextEncoding::NarrowToWide (failures.front()).c_str());
            Assert::IsTrue  (second.rig.session.GetSymbols().TryResolveIn (SymbolTableId::User, "FIRST", address), L"the table's last symbol file");
            Assert::IsFalse (second.rig.session.GetSymbols().TryResolveIn (SymbolTableId::User, "HOME", address), L"not the BASIC program");
        }


        //  A symbol the user added comes back in the next session, ahead of a
        //  breakpoint set by its name; one removed, or in a table cleared,
        //  does not.
        TEST_METHOD (ASymbolTheUserAddedComesBack)
        {
            std::unique_ptr<SessionRig>    firstRig  = std::make_unique<SessionRig>();
            std::unique_ptr<SessionRig>    secondRig = std::make_unique<SessionRig>();
            SessionRig                   & first     = *firstRig;
            SessionRig                   & second    = *secondRig;
            DebugProject                   project;
            DebugProject                   read;
            std::string                    error;
            std::vector<std::string>       failures;
            Word                           address   = 0;



            first.rig.RunOk ("SYMUSER DRAWLINE = 6000");
            first.rig.RunOk ("SYMUSER2 GONE = 6100");
            first.rig.RunOk ("SYMUSER2 CLEAR");
            first.rig.RunOk ("SYMUSER LATER = 6200");
            first.rig.RunOk ("SYMUSER ! LATER");
            first.rig.RunOk ("BP DRAWLINE");

            project.CaptureEngine (first.rig.session);

            Assert::IsTrue (DebugProject::TryParse (project.ToJson (L"C:\\Work"), L"C:\\Work", read, error), TextEncoding::NarrowToWide (error).c_str());
            read.ApplyEngine (second.rig.session, failures);

            Assert::IsTrue   (failures.empty(), failures.empty() ? L"" : TextEncoding::NarrowToWide (failures.front()).c_str());
            Assert::IsTrue   (second.rig.session.TryResolveSymbol ("DRAWLINE", address), L"the symbol is back");
            Assert::AreEqual ((Word) 0x6000, address);
            Assert::IsFalse  (second.rig.session.TryResolveSymbol ("GONE", address),  L"its table was cleared");
            Assert::IsFalse  (second.rig.session.TryResolveSymbol ("LATER", address), L"it was removed");
            Assert::AreEqual (List (first, "BPL"), List (second, "BPL"), L"the breakpoint set by its name with it");
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

        //  The script's numbers are its definition lines, so one that adds
        //  nothing, as a breakpoint on a symbol no longer defined does, moves
        //  no later disable or name onto another breakpoint.
        TEST_METHOD (ADefinitionThatAddsNothingMovesNoLaterNumber)
        {
            std::unique_ptr<SessionRig>    rigHeap  = std::make_unique<SessionRig>();
            SessionRig                   & session  = *rigHeap;
            DebugProject                   project;
            std::vector<std::string>       failures;
            BreakpointListData             list;



            project.engineScript       = { "BPC *", "BP NOSUCHLABEL", "BP 0400", "BP 0500", "BPD 2" };
            project.breakpointNames[2] = "five";
            project.ApplyEngine (session.rig.session, failures);

            BreakpointHandlers::ListAll (session.rig.session, list);

            Assert::AreEqual ((size_t) 1, failures.size(), L"the line that adds nothing");
            Assert::AreEqual ((size_t) 2, list.breakpoints.size());
            Assert::IsTrue   (list.breakpoints[0].enabled, L"BP 0400 is the script's 1");
            Assert::IsFalse  (list.breakpoints[1].enabled, L"BP 0500 is its 2");
            Assert::AreEqual (std::string ("five"), list.breakpoints[1].name);
        }

        //  A logging breakpoint's message and its running on come back with
        //  the project, through its JSON.
        TEST_METHOD (ABreakpointsLogComesBackWithTheProject)
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



            first.rig.RunOk ("BP 0300");
            first.rig.RunOk (std::format ("BPLOG {} GO \"A={{A}}\"", first.rig.session.GetBreakpoints().GetAll().at (0).id));

            project.CaptureEngine (first.rig.session, &first.handlers.GetExecution());

            Assert::IsTrue (DebugProject::TryParse (project.ToJson (L"C:\\Work"), L"C:\\Work", read, error), TextEncoding::NarrowToWide (error).c_str());
            read.ApplyEngine (second.rig.session, failures);

            BreakpointHandlers::ListAll (second.rig.session, list);

            Assert::AreEqual ((size_t) 1, list.breakpoints.size());
            Assert::AreEqual (std::string ("A={A}"), list.breakpoints[0].log);
            Assert::IsFalse  (list.breakpoints[0].stops, L"GO runs on");
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

        //  A save under the automatic file's path writes the project
        //  there and no redirect over it, and opening a redirect file opens
        //  the project it points at, not an empty one.
        TEST_METHOD (TheAutomaticPathIsNeverARedirectToItself)
        {
            InMemoryFileSystem         files;
            DebugProjectStore          store (files, L"C:\\Data\\DebugProjects");
            ProjectScope               scope   = ProjectScope::Choose ("", "", "", "", "Apple //e", L"C:\\Disks\\Game.dsk");
            DebugProject               project;
            DebugProject               again;
            std::vector<std::string>   notices;



            AssertSucceeded (store.Open (scope, project, notices), L"store.Open");

            project.engineScript = { "BPC *", "BP 0300" };
            AssertSucceeded (store.SaveAs (project, store.GetAutomaticPath (scope)), L"store.SaveAs");
            AssertSucceeded (store.Open (scope, again, notices), L"store.Open");
            Assert::IsTrue (again.engineScript == project.engineScript, L"the project, not a redirect to itself");

            AssertSucceeded (store.SaveAs (project, L"C:\\Work\\game.cassoproj"), L"store.SaveAs");
            AssertSucceeded (store.OpenFile (store.GetAutomaticPath (scope), again, notices), L"store.OpenFile");
            Assert::IsTrue   (again.engineScript == project.engineScript, L"the redirect's project");
            Assert::AreEqual (std::wstring (L"C:\\Work\\game.cassoproj"), again.filePath);
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



        //  A project file another program has open cannot be read for now,
        //  which is not the same as not being a project: no copy is made,
        //  and the empty project that opens is never saved over it.
        TEST_METHOD (AFileThatCannotBeReadIsNotSavedOver)
        {
            InMemoryFileSystem         files;
            DebugProjectStore          store (files, L"C:\\Data\\DebugProjects", [] { return std::string ("20261015-013000"); });
            ProjectScope               scope = ProjectScope::Choose ("", "", "", "", "Apple //e", L"");
            DebugProject               project;
            std::vector<std::string>   notices;
            std::wstring               path  = store.GetAutomaticPath (scope);
            HRESULT                    hr    = S_OK;



            files.WriteAllText (path, "the user's project");
            files.SetReadError (path, HRESULT_FROM_WIN32 (ERROR_SHARING_VIOLATION));

            AssertSucceeded (store.Open (scope, project, notices), L"store.Open");
            Assert::AreEqual ((size_t) 1, notices.size());
            Assert::IsFalse  (files.Exists (path + L".unreadable-20261015-013000"), L"nothing was read to copy");
            hr = store.Save (project);
            Assert::IsTrue   (FAILED (hr), L"the save is refused");
            Assert::AreEqual (std::string ("the user's project"), files.PeekContent (path));
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
