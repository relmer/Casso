#include "Pch.h"

#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/MemoryBarCommands.h"
#include "Ui/Debugger/Panes/MemoryEditModel.h"
#include "Ui/Debugger/RegisterHistory.h"
#include "Ui/Debugger/StackHistory.h"
#include "Ui/Debugger/WatchHistory.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PaneUndoTextTests
//
//  The registers, stack, memory and watch panes' Undo and Redo say what they
//  act on, in the context menu and in the toolbar's tips, quoting it: Undo
//  "changed 2 bytes at $0300". A memory window's bar carries Undo and Redo buttons.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (PaneUndoTextTests)
    {
    public:

        TEST_METHOD (AMemoryEditSaysHowManyBytesAndWhere)
        {
            MemoryEditModel                   model;
            std::vector<std::optional<Byte>>  bytes (16, std::optional<Byte> ((Byte) 0));
            std::vector<MemoryRegion>         regions (16, MemoryRegion::MainRam);
            std::vector<uint8_t>              data = { 0x11, 0x22 };



            model.SetOnPatch  ([] (Word, std::span<const Byte>) {});
            model.SetContents (0x0300, bytes, regions);

            Assert::AreEqual (std::wstring(), model.GetUndoText());
            Assert::IsTrue   (model.WriteBytes (0x0300, data));
            Assert::AreEqual (std::wstring (L"Undo \"changed 2 bytes at $0300\""), DebuggerWindow::GetUndoLabel (false, model.GetUndoText()));

            Assert::IsTrue   (model.Undo());
            Assert::AreEqual (std::wstring (L"Redo \"changed 2 bytes at $0300\""), DebuggerWindow::GetUndoLabel (true, model.GetRedoText()));
        }


        TEST_METHOD (AStackEditSaysTheByteItChanged)
        {
            StackHistory  history;



            history.Record (0x01FD, 0x12, 0x34, 0x0300);
            Assert::AreEqual (std::wstring (L"changed 1 byte at $01FD"), history.GetUndoText());

            (void) history.TryUndo (CommandMode::AppleWin);
            Assert::AreEqual (std::wstring (L"changed 1 byte at $01FD"), history.GetRedoText());
            Assert::AreEqual (std::wstring(),                            history.GetUndoText());
        }


        TEST_METHOD (ARegisterEditSaysTheRegister)
        {
            RegisterHistory  history;



            history.Record ("S", 0xF0, 0x80, 0x0300);
            Assert::AreEqual (std::wstring (L"Undo \"changed register S\""), DebuggerWindow::GetUndoLabel (false, history.GetUndoText()));
        }


        TEST_METHOD (AWatchEditSaysWhichWatchAndHow)
        {
            DebuggerViewSnapshot                 snapshot;
            DebuggerViewSnapshot::AutoWatchLine  line;



            line.label = "A";
            snapshot.autoWatches.push_back (line);

            Assert::AreEqual (std::wstring (L"moved watch 3"),   WatchHistory::GetEditText (snapshot, 3, std::nullopt, 0));
            Assert::AreEqual (std::wstring (L"changed watch 3"), WatchHistory::GetEditText (snapshot, 3, std::nullopt, 1));
            Assert::AreEqual (std::wstring (L"changed A"),       WatchHistory::GetEditText (snapshot, std::nullopt, 0, 1));
        }


        TEST_METHOD (NothingToSayLeavesTheBareVerb)
        {
            Assert::AreEqual (std::wstring (L"Undo"), DebuggerWindow::GetUndoLabel (false, L""));
            Assert::AreEqual (std::wstring (L"Redo"), DebuggerWindow::GetUndoLabel (true,  L""));
        }


        TEST_METHOD (TheMemoryBarHasUndoAndRedoWhoseTipsFollowTheWindow)
        {
            MemoryBarCommands::Handlers          handlers;
            bool                                 canUndo  = false;
            std::unique_ptr<MemoryBarCommands>   commands;



            handlers.isEnabled = [&canUndo] (int id) { return id != MemoryBarCommands::kUndo || canUndo; };
            handlers.getTip    = [&canUndo] (int id) { return (id == MemoryBarCommands::kUndo && canUndo) ? std::wstring (L"Undo changed 1 byte at $0300") : std::wstring(); };

            commands = std::make_unique<MemoryBarCommands> (std::move (handlers));

            Assert::IsNotNull (commands->Find (MemoryBarCommands::kUndo).get());
            Assert::IsNotNull (commands->Find (MemoryBarCommands::kRedo).get());
            Assert::IsFalse   (commands->Find (MemoryBarCommands::kUndo)->IsEnabled());

            canUndo = true;
            Assert::IsTrue    (commands->Find (MemoryBarCommands::kUndo)->IsEnabled());
            Assert::AreEqual  (std::wstring (L"Undo changed 1 byte at $0300"), commands->Find (MemoryBarCommands::kUndo)->GetTipText());
        }
    };
}
