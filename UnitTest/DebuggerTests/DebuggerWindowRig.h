#pragma once

#include "CaptureTests/FakeHostDialogs.h"
#include "ControllerRig.h"
#include "Core/TextEncoding.h"
#include "Debugger/DebugSession.h"
#include "Dxui/MockDxuiPainter.h"
#include "Dxui/MockDxuiTextRenderer.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerCommands.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Widgets/DxuiHexView.h"
#include "Widgets/DxuiListView.h"
#include "Widgets/DxuiTextView.h"

#include "CppUnitTest.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WindowRigHost
//
//  The emulator's side of the debugger window over a controller rig: what
//  EmulatorShell does with each request the window makes, done at once on the
//  test's thread where the shell posts it to the CPU thread.
//
//  A LINE RUNS AS THE SHELL RUNS A WINDOW LINE: through ExecuteWindowLine in
//  the session's own mode, then echoed under the prompt of the mode the session
//  is in once it has run, and the reply after it. The echo is a copy of the
//  shell's, since the shell's handler is a lambda a test cannot reach, so a
//  test asserts the reply, not the echo's prompt.
//
//  A snapshot is built for every frame the window asks for, as the CPU thread
//  builds one after anything the window did, unless `publishes` is false: a
//  window that has not yet had its first snapshot.
//
////////////////////////////////////////////////////////////////////////////////

class WindowRigHost : public IDebuggerWindowHost
{
public:
    explicit WindowRigHost (ControllerRig & rig) : m_rig (rig) {}

    //  What the window asked for, in order, and each line's reply.
    std::vector<std::string>                              sent;
    std::vector<Reply>                                    replies;
    int                                                   pauses       = 0;
    std::vector<std::pair<int, std::string>>              goTos;
    std::vector<std::pair<std::optional<Word>, int>>      codeAddresses;
    std::vector<std::string>                              schemesSaved;

    //  The key scheme preference the host keeps.
    std::string                                           savedScheme;

    //  False until the CPU thread has published a snapshot.
    bool                                                  publishes    = true;

    //  A machine switch, which the rig's machine cannot make under a live
    //  controller: the name the next snapshots carry instead of its own.
    std::optional<std::string>                            machineOverride;

    FakeHostDialogs                                       dialogs;


    void  RunDebuggerCommand (const std::string & line) override
    {
        DebugSession  & session = m_rig.controller.GetSession();
        Reply           reply;



        sent.push_back (line);

        reply = m_rig.view.ExecuteWindowLine (session, line, session.GetMode());
        replies.push_back (reply);

        m_console.push_back (DebugSession::GetPrompt (session.GetMode()) + line);
        m_console.insert (m_console.end(), reply.text.begin(), reply.text.end());
    }


    //  As the shell's PauseDebugRun asks the controller.
    void  PauseDebugger() override
    {
        pauses++;
        m_rig.controller.RequestPause();
    }


    void  SetDebuggerCodeLines (int lines, int view) override
    {
        m_rig.view.SetCodeLines (lines, view);
    }


    //  As EmulatorShell::SetDebugView takes a code view's address.
    void  SetDebuggerCodeAddress (std::optional<Word> address, int view) override
    {
        codeAddresses.push_back ({ address, view });

        if (address.has_value() && !m_rig.view.IsCodeViewOpen (view))
        {
            m_rig.view.OpenCodeView (view, *address);
        }
        else if (address.has_value())
        {
            m_rig.view.CenterCodeOn (*address, view);
        }
        else
        {
            m_rig.view.ShowPcIn (view);
        }
    }


    void  SetDebuggerCodeTop    (Word top, int view) override { m_rig.view.OpenCodeViewAt (view, top); }
    void  SetDebuggerFollowView (int view)           override { m_rig.view.SetFollowView (view); }
    void  CloseDebuggerCodeView (int view)           override { m_rig.view.CloseCodeView (view); }


    void  SetDebuggerMemoryWindow (int id, std::optional<Word> address) override
    {
        if (id == 1 && address.has_value())
        {
            m_rig.view.SetMemoryAddress (*address);
        }
        else if (address.has_value())
        {
            m_rig.view.OpenMemoryWindow (id, *address);
        }
        else
        {
            m_rig.view.CloseMemoryWindow (id);
        }
    }


    void  SetDebuggerTraceTop (std::optional<uint64_t> first) override { m_rig.view.SetTraceTop (first); }


    void  GoToDebuggerMemory (int window, const std::string & text) override
    {
        goTos.push_back ({ window, text });
        m_rig.view.RequestGoTo (m_rig.controller.GetSession(), window, text);
    }


    void  ScrollDebuggerCode (int lines, int view) override { m_rig.view.ScrollCode (lines, view); }


    bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> & snapshot,
                              std::vector<std::string>                     & consoleLines) override
    {
        DebuggerViewSnapshot  built;



        if (publishes)
        {
            built = m_rig.view.Build (m_rig.controller.GetSession(), m_rig.cpuManager.IsPaused());

            if (machineOverride.has_value())
            {
                built.machine = *machineOverride;
            }

            snapshot = std::make_shared<const DebuggerViewSnapshot> (std::move (built));
        }

        consoleLines.swap (m_console);
        m_console.clear();

        return true;
    }


    void  OnDebuggerWindowClosed() override {}

    IHostDialogs &  GetHostDialogs() noexcept override { return dialogs; }

    std::string  GetDebuggerKeyScheme    () override                      { return savedScheme; }
    void         SetDebuggerKeyScheme    (const std::string & name) override { savedScheme = name; schemesSaved.push_back (name); }
    std::string  GetDebuggerLayout       () override                      { return {}; }
    void         SetDebuggerLayout       (const std::string &) override   {}
    std::string  GetDebuggerOpenViews    () override                      { return {}; }
    void         SetDebuggerOpenViews    (const std::string &) override   {}
    bool         TryGetDebuggerPlacement (RECT &) override                { return false; }
    void         SetDebuggerPlacement    (const RECT &) override          {}

    SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
    SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

private:
    ControllerRig             & m_rig;
    std::vector<std::string>    m_console;
};





////////////////////////////////////////////////////////////////////////////////
//
//  TestDebuggerWindow
//
//  The debugger window with its controls built and no HWND, driven as a user
//  drives it: keys go in as DxuiWindow::DispatchKey hands them on, to OnKey
//  and then to the key map when OnKey leaves them, and the mouse as
//  DispatchMouse hands it on. A frame takes the host's update, lays the window
//  out and paints it through recording surfaces, which is where the lists fit
//  their columns. A menu the window would open and a copy it would make are
//  kept rather than shown, since both need the system.
//
////////////////////////////////////////////////////////////////////////////////

class TestDebuggerWindow : public DebuggerWindow
{
public:
    static constexpr int  kWidthDip   = 1100;
    static constexpr int  kHeightDip  = 840;
    static constexpr int  kRowTextDip = 80;

    struct Menu
    {
        std::vector<DxuiPopupMenuItem>  items;
        POINT                           at = {};
    };

    std::vector<Menu>  menus;
    int                copies = 0;
    std::wstring       copied;


    TestDebuggerWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
    {
        m_theme = &theme;
        m_host  = &host;
    }


    using DebuggerWindow::GetCommandBox;
    using DebuggerWindow::GetMemoryBox;
    using DebuggerWindow::GetPokeBox;
    using DebuggerWindow::GetConsoleView;
    using DebuggerWindow::GetCodeList;
    using DebuggerWindow::GetBreakpointList;
    using DebuggerWindow::GetWatchList;
    using DebuggerWindow::GetRegisterList;
    using DebuggerWindow::GetWatchEditor;
    using DebuggerWindow::GetMemoryPane;
    using DebuggerWindow::GetCommands;
    using DebuggerWindow::GetDockSite;
    using DebuggerWindow::GetFocusedControl;
    using DebuggerWindow::GetSnapshot;
    using DebuggerWindow::GetTextZoom;
    using DebuggerWindow::GetKeyScheme;
    using DebuggerWindow::GetConsoleLines;
    using DebuggerWindow::GetMenuCommands;
    using DebuggerWindow::OnMouse;
    using DebuggerWindow::TakeHostUpdate;


    //  As Create builds the window, less the HWND, and the first frame.
    void  Open()
    {
        OnCreate();
        ApplySavedKeyScheme();
        Relayout();
        Frame();
    }


    //  One UI frame: its intake, a layout so what arrived is placed, and a
    //  paint, where the lists fit their columns to their text.
    void  Frame()
    {
        TakeHostUpdate();
        Relayout();
        Paint (m_painter, m_text, *m_theme);
        Relayout();

        m_painter.Reset();
        m_text.Reset();
    }


    void  Relayout()
    {
        DxuiDpiScaler  scaler;



        scaler.SetDpi (USER_DEFAULT_SCREEN_DPI);
        Layout (RECT { 0, 0, kWidthDip, kHeightDip }, scaler);
    }


    //  A key-down with the modifiers held, as DispatchKey hands it on. True
    //  when something took it.
    bool  Press (WPARAM vk, bool ctrl = false, bool alt = false, bool shift = false)
    {
        return Dispatch (DxuiKeyEventKind::Down, vk, ctrl, alt, shift);
    }


    //  The character TranslateMessage makes of a key-down, delivered after it.
    bool  Char (wchar_t ch, bool ctrl = false, bool alt = false, bool shift = false)
    {
        return Dispatch (DxuiKeyEventKind::Char, (WPARAM) ch, ctrl, alt, shift);
    }


    //  A key as the keyboard delivers it: the key-down, then its character
    //  when it has one, whatever the key-down did.
    void  Stroke (WPARAM vk, wchar_t ch, bool ctrl = false, bool alt = false, bool shift = false)
    {
        (void) Press (vk, ctrl, alt, shift);

        if (ch != 0)
        {
            (void) Char (ch, ctrl, alt, shift);
        }
    }


    //  Text typed on a US keyboard, a key-down and a character each.
    void  TypeText (const std::wstring & text)
    {
        for (wchar_t ch : text)
        {
            WPARAM  vk    = 0;
            bool    shift = false;



            GetUsKey (ch, vk, shift);
            Stroke (vk, ch, false, false, shift);
        }
    }


    //  The key a character is typed with on a US keyboard.
    static void  GetUsKey (wchar_t ch, WPARAM & vk, bool & shift)
    {
        static constexpr std::pair<wchar_t, std::pair<WPARAM, bool>>  kKeys[] =
        {
            { L' ', { VK_SPACE,      false } }, { L'.', { VK_OEM_PERIOD, false } }, { L',', { VK_OEM_COMMA, false } },
            { L'=', { VK_OEM_PLUS,   false } }, { L'-', { VK_OEM_MINUS,  false } }, { L'/', { VK_OEM_2,     false } },
            { L':', { VK_OEM_1,      true  } }, { L'$', { '4',           true  } }, { L'(', { '9',          true  } },
            { L')', { '0',           true  } }, { L'#', { '3',           true  } }, { L'!', { '1',          true  } },
            { L'"', { VK_OEM_7,      true  } }, { L'*', { '8',           true  } }, { L'?', { VK_OEM_2,     true  } },
            { L'\\', { VK_OEM_5,     false } },
        };



        vk    = (WPARAM) towupper (ch);
        shift = iswupper (ch) != 0;

        for (const auto & [key, press] : kKeys)
        {
            if (key == ch)
            {
                vk    = press.first;
                shift = press.second;
            }
        }
    }


    //  A left click at a point in the window.
    void  ClickAt (POINT at, bool ctrl = false)
    {
        (void) Mouse (DxuiMouseEventKind::Down, at, ctrl);
        (void) Mouse (DxuiMouseEventKind::Up,   at, ctrl);
    }


    //  A left click in the middle of a control.
    void  Click (IDxuiControl * control)
    {
        RECT  bounds = control->GetBounds();



        ClickAt (POINT { (bounds.left + bounds.right) / 2, (bounds.top + bounds.bottom) / 2 });
    }


    //  Where a click lands on a row, `xDip` in from the list's left edge, in
    //  window coordinates; false when the row is not in view.
    static bool  TryGetRowPoint (const DxuiListView * list, int row, int xDip, POINT & at)
    {
        RECT  bounds = list->GetBounds();
        int   y      = list->GetHeaderHeightPx() + (row - list->GetTopRow()) * list->GetRowHeightDip() + list->GetRowHeightDip() / 2;



        if (row < 0 || list->HitTestRow (xDip, y) != row)
        {
            return false;
        }

        at = POINT { bounds.left + xDip, bounds.top + y };
        return true;
    }


    //  A left click on a row, which selects it; false when the row is not in
    //  view. The default lands past a code view's gutter and marker.
    bool  ClickRow (DxuiListView * list, int row, int xDip = kRowTextDip)
    {
        POINT  at = {};



        if (!TryGetRowPoint (list, row, xDip, at))
        {
            return false;
        }

        ClickAt (at);
        return true;
    }


    bool  Wheel (POINT at, float notches, bool ctrl)
    {
        DxuiMouseEvent  ev;



        ev.kind        = DxuiMouseEventKind::Wheel;
        ev.positionDip = at;
        ev.wheelDelta  = notches;
        ev.ctrl        = ctrl;

        return OnMouse (ev);
    }


protected:
    void  PresentMenu (std::vector<DxuiPopupMenuItem> menu, POINT clientPx) override
    {
        menus.push_back (Menu { std::move (menu), clientPx });
    }


    void  CopyConsoleSelection() override
    {
        copies++;
        copied = GetConsoleView()->GetSelectionText();
    }


private:
    MockDxuiPainter       m_painter;
    MockDxuiTextRenderer  m_text;


    bool  Dispatch (DxuiKeyEventKind kind, WPARAM code, bool ctrl, bool alt, bool shift)
    {
        DxuiKeyEvent  ev;



        ev.kind  = kind;
        ev.vk    = code;
        ev.ctrl  = ctrl;
        ev.alt   = alt;
        ev.shift = shift;

        return OnKey (ev) || RouteMappedKey (ev);
    }


    bool  Mouse (DxuiMouseEventKind kind, POINT at, bool ctrl)
    {
        DxuiMouseEvent  ev;



        ev.kind        = kind;
        ev.button      = DxuiMouseButton::Left;
        ev.positionDip = at;
        ev.ctrl        = ctrl;

        return OnMouse (ev);
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  WindowRig
//
//  The window over a real machine, as a user sees it: a controller rig with
//  its channel open, the host the shell would be, and the window, opened in
//  the key scheme the preferences give.
//
//  The machine holds a small program with a call in it:
//
//      $0300  JSR $0310
//      $0303  INX
//      $0304  JMP $0303
//      $0310  LDA #$41
//      $0312  STA $0400
//      $0315  RTS
//
//  with the PC on the JSR, A and X zero, $0400 zero, and the stack pointer
//  at $FF.
//
////////////////////////////////////////////////////////////////////////////////

class WindowRig
{
public:
    using Assert = Microsoft::VisualStudio::CppUnitTestFramework::Assert;

    static constexpr Word    kCall     = 0x0300;
    static constexpr Word    kLoop     = 0x0303;
    static constexpr Word    kJump     = 0x0304;
    static constexpr Word    kRoutine  = 0x0310;
    static constexpr Word    kStore    = 0x0312;
    static constexpr Word    kReturn   = 0x0315;
    static constexpr Word    kScreen   = 0x0400;
    static constexpr size_t  kDialects = 5;
    static constexpr size_t  kSchemes  = 3;

    ControllerRig       rig;
    CassoTheme          theme  = CassoTheme::MakeSkeuomorphic();
    WindowRigHost       host   { rig };
    TestDebuggerWindow  window { theme, host };


    explicit WindowRig (const std::string & scheme = std::string(), bool publishes = true)
    {
        static constexpr Byte  kProgram[] =
        {
            0x20, 0x10, 0x03,       // $0300  JSR $0310
            0xE8,                   // $0303  INX
            0x4C, 0x03, 0x03,       // $0304  JMP $0303
        };
        static constexpr Byte  kSubroutine[] =
        {
            0xA9, 0x41,             // $0310  LDA #$41
            0x8D, 0x00, 0x04,       // $0312  STA $0400
            0x60,                   // $0315  RTS
        };
        Cpu6502Registers  r      = GetRegisters();
        HRESULT           opened = S_OK;



        for (size_t i = 0; i < std::size (kProgram); i++)
        {
            rig.machine.GetMemoryBus().WriteByte ((Word) (kCall + i), kProgram[i]);
        }

        for (size_t i = 0; i < std::size (kSubroutine); i++)
        {
            rig.machine.GetMemoryBus().WriteByte ((Word) (kRoutine + i), kSubroutine[i]);
        }

        rig.machine.GetMemoryBus().WriteByte (kScreen, 0x00);

        r.pc = kCall;
        r.a  = 0x00;
        r.x  = 0x00;
        r.sp = 0xFF;
        SetRegisters (r);

        //  The shell opens the channel before the window takes a command, and
        //  opening it starts the call-stack record.
        opened = rig.controller.Open();
        Assert::IsTrue (SUCCEEDED (opened), L"the channel opened");

        host.savedScheme = scheme;
        host.publishes   = publishes;
        window.Open();
    }


    DebugSession &    GetSession   () { return rig.controller.GetSession(); }
    Cpu6502Registers  GetRegisters () { return GetSession().GetTarget().GetRegisters(); }
    Word              GetPc        () { return GetRegisters().pc; }
    uint64_t          GetCycles    () { return GetSession().GetTarget().GetCycleCount(); }
    Byte              Peek         (Word address) { return rig.machine.GetMemoryBus().ReadByte (address); }
    void              Poke         (Word address, Byte value) { rig.machine.GetMemoryBus().WriteByte (address, value); }
    void              SetRegisters (const Cpu6502Registers & r) { GetSession().GetTarget().SetRegisters (r); }
    bool              IsPaused     () { return rig.cpuManager.IsPaused(); }


    //  The CPU thread's frames, as EmulatorShell::ExecuteCpuSlices runs them:
    //  a slice of the machine, then the run driver told how much ran. Slices
    //  run only while the CPU manager is not paused; the loop ends at the
    //  slice that ends a run, at a slice that ran nothing, or at the limit.
    //  Returns the number of slices run.
    int  RunSlices (int limit = 64)
    {
        static constexpr uint32_t  kSliceCycles = 1023;
        int                        slices       = 0;
        uint32_t                   ran          = 0;
        bool                       ended        = false;



        while (slices < limit && !rig.cpuManager.IsPaused())
        {
            ran   = (uint32_t) rig.machine.RunCycles (kSliceCycles);
            ended = rig.controller.GetRunDriver().OnSliceExecuted (ran);

            slices++;

            if (ended || ran == 0)
            {
                break;
            }
        }

        return slices;
    }


    //  Slices, then the window's next frame.
    void  Settle (int limit = 64)
    {
        (void) RunSlices (limit);
        window.Frame();
    }


    //  A key-down, the slices it starts, and the frame after.
    void  PressAndSettle (WPARAM vk, bool ctrl = false, bool alt = false, bool shift = false)
    {
        (void) window.Press (vk, ctrl, alt, shift);
        Settle();
    }


    //  The emulator's own Resume, as the shell reports it: the CPU manager let
    //  go and the session told the machine runs freely, with no debugger run.
    void  ResumeEmulator()
    {
        rig.cpuManager.SetPaused (false);
        GetSession().OnUserResumed();
    }


    //  A line typed into the command box and run with Enter, then the frame
    //  that shows what it did.
    void  Submit (const std::wstring & line)
    {
        window.Click (window.GetCommandBox());
        window.TypeText (line);
        window.Stroke (VK_RETURN, L'\r');
        window.Frame();
    }


    //  A click on the row of a code view that shows an address.
    void  SelectCode (Word address, int view = 0)
    {
        int  row = GetCodeRow (address, view);



        Assert::IsTrue (window.ClickRow (window.GetCodeList (view), row), L"the address is on a row in view");
        Assert::AreEqual (row, window.GetCodeList (view)->GetSelectedRow(), L"the click selected the row");
    }


    //  The row of a code view that shows an address in the snapshot the
    //  window last took, or -1.
    int  GetCodeRow (Word address, int view = 0)
    {
        const DebuggerViewSnapshot  * snapshot = window.GetSnapshot();



        if (snapshot == nullptr)
        {
            return -1;
        }

        for (size_t i = 0; i < snapshot->codeViews[(size_t) view].size(); i++)
        {
            if (snapshot->codeViews[(size_t) view][i].address == address)
            {
                return (int) i;
            }
        }

        return -1;
    }


    //  The breakpoints the snapshot the window last took lists: address and
    //  whether each is enabled.
    std::vector<std::pair<Word, bool>>  GetBreakpoints()
    {
        std::vector<std::pair<Word, bool>>  listed;



        for (const DebuggerViewSnapshot::BreakpointLine & bp : window.GetSnapshot()->breakpoints)
        {
            listed.push_back ({ bp.address, bp.enabled });
        }

        return listed;
    }


    //  A command bar entry clicked: the strip runs an entry only while it is
    //  enabled. False when it was not.
    bool  ClickEntry (int id)
    {
        std::shared_ptr<DxuiCommand>  command = window.GetCommands()->Find (id);



        Assert::IsNotNull (command.get(), L"the command bar has the entry");

        if (!command->IsEnabled())
        {
            return false;
        }

        command->dispatch();
        return true;
    }


    bool  IsEntryEnabled (int id)
    {
        std::shared_ptr<DxuiCommand>  command = window.GetCommands()->Find (id);



        Assert::IsNotNull (command.get(), L"the command bar has the entry");
        return command->IsEnabled();
    }


    //  The drop-down rows: the machine's panels, then the five dialects, then
    //  the three key schemes.
    std::shared_ptr<DxuiCommand>  GetDialectRow (const std::wstring & label)
    {
        return GetMenuRow (label, window.GetSnapshot()->panels.size(), kDialects);
    }


    std::shared_ptr<DxuiCommand>  GetSchemeRow (const std::wstring & label)
    {
        return GetMenuRow (label, window.GetSnapshot()->panels.size() + kDialects, kSchemes);
    }


    std::shared_ptr<DxuiCommand>  GetPanelRow (const std::wstring & label)
    {
        return GetMenuRow (label, 0, window.GetSnapshot()->panels.size());
    }


    //  A drop-down row chosen, as the menu runs it on a click, and the frame
    //  that brings what it did.
    void  Choose (const std::shared_ptr<DxuiCommand> & row)
    {
        std::shared_ptr<DxuiCommand>  held = row;



        held->dispatch();
        window.Frame();
    }


    void  ChooseDialect (const std::wstring & label) { Choose (GetDialectRow (label)); }
    void  ChooseScheme  (const std::wstring & label) { Choose (GetSchemeRow  (label)); }


    //  Every console line since the last call.
    std::vector<std::string>  TakeConsole()
    {
        std::vector<std::string>  lines (window.GetConsoleLines().begin() + (ptrdiff_t) m_consoleRead, window.GetConsoleLines().end());



        m_consoleRead = window.GetConsoleLines().size();
        return lines;
    }


    //  Every line the window sent since the last call.
    std::vector<std::string>  TakeSent()
    {
        std::vector<std::string>  lines (host.sent.begin() + (ptrdiff_t) m_sentRead, host.sent.end());



        m_sentRead = host.sent.size();
        return lines;
    }


    //  Lines joined one to a line, so a comparison's failure shows them all.
    static std::wstring  Join (const std::vector<std::string> & lines)
    {
        std::string  text;



        for (const std::string & line : lines)
        {
            text += line + "\n";
        }

        return TextEncoding::NarrowToWide (text);
    }


    static void  AssertLines (const std::vector<std::string> & expected, const std::vector<std::string> & actual, const wchar_t * message)
    {
        Assert::AreEqual (Join (expected), Join (actual), message);
    }


private:
    std::shared_ptr<DxuiCommand>  GetMenuRow (const std::wstring & label, size_t first, size_t count)
    {
        const std::vector<std::shared_ptr<DxuiCommand>> & rows = window.GetMenuCommands();



        Assert::IsTrue (rows.size() >= first + count, L"the drop-downs are built");

        for (size_t i = first; i < first + count; i++)
        {
            if (rows[i]->label == label)
            {
                return rows[i];
            }
        }

        Assert::Fail ((L"no drop-down row " + label).c_str());
        return nullptr;
    }


    size_t  m_consoleRead = 0;
    size_t  m_sentRead    = 0;
};
