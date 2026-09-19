#include "Pch.h"

#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Debugger/Source/SourcePathList.h"

#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Ui/Chrome/CassoTheme.h"





static constexpr LPCWSTR  s_kpszWindowTitle = L"Debugger";
static constexpr LPCWSTR  s_kpszClassName   = L"CassoDebuggerWindow";





////////////////////////////////////////////////////////////////////////////////
//
//  Widen
//
////////////////////////////////////////////////////////////////////////////////

static std::wstring Widen (const std::string & text)
{
    return TextEncoding::NarrowToWide (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryParseHexWord
//
//  Hex with or without a `$` or `0x` in front. The window's small boxes take an
//  address the way a person types one, and a stray prefix is not an error.
//
////////////////////////////////////////////////////////////////////////////////

static bool TryParseHexWord (std::wstring text, Word & value)
{
    uint32_t  parsed = 0;



    while (!text.empty() && iswspace (text.front())) { text.erase (0, 1); }
    while (!text.empty() && iswspace (text.back()))  { text.pop_back(); }

    if (!text.empty() && text[0] == L'$')
    {
        text.erase (0, 1);
    }
    else if (text.size() > 2 && text[0] == L'0' && (text[1] == L'x' || text[1] == L'X'))
    {
        text.erase (0, 2);
    }

    if (text.empty() || text.size() > 4)
    {
        return false;
    }

    for (wchar_t c : text)
    {
        if (!iswxdigit (c))
        {
            return false;
        }

        parsed = parsed * 16 + (uint32_t) (iswdigit (c) ? c - L'0' : (towupper (c) - L'A' + 10));
    }

    value = (Word) parsed;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::~DebuggerWindow
//
////////////////////////////////////////////////////////////////////////////////

DebuggerWindow::~DebuggerWindow()
{
    DestroyBackend();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::Create
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebuggerWindow::Create (HINSTANCE hInstance, HWND hwndOwner, const CassoTheme * theme, IDebuggerWindowHost * host)
{
    HRESULT                   hr     = S_OK;
    DxuiWindow::CreateParams  params;



    BAIL_OUT_IF (IsCreated(), S_OK);

    m_theme = theme;
    m_host  = host;

    params.title                    = s_kpszWindowTitle;
    params.hInstance                = hInstance;
    params.ownerHwnd                = hwndOwner;
    params.initialSizeDip           = { kPreferredWidthDip, kPreferredHeightDip };
    params.minSizeDip               = { kMinWidthDip, kMinHeightDip };
    params.resizable                = true;
    params.insetContentBelowCaption = false;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.classNameOverride        = s_kpszClassName;

    hr = DxuiWindow::Create (params);
    CHR (hr);

    ApplyKeyScheme (GetSavedKeyScheme());

    SetTheme (m_theme);
    Show();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OnCreate()
{
    m_stepButton        = CreateChild<DxuiButton>    (L"Step");
    m_stepOverButton    = CreateChild<DxuiButton>    (L"Step Over");
    m_stepOutButton     = CreateChild<DxuiButton>    (L"Step Out");
    m_runButton         = CreateChild<DxuiButton>    (L"Run");
    m_runToCursorButton = CreateChild<DxuiButton>    (L"Run to Cursor");
    m_pauseButton       = CreateChild<DxuiButton>    (L"Pause");
    m_followPcButton    = CreateChild<DxuiButton>    (L"Follow PC");
    m_keysButton        = CreateChild<DxuiButton>    (L"Keys");
    m_flagsLabel        = CreateChild<DxuiLabel>     (L"", DxuiTextRole::Body, DxuiTextHAlign::Left);
    m_codeList          = CreateChild<DxuiListView>  ();
    m_registerList      = CreateChild<DxuiListView>  ();
    m_breakpointList    = CreateChild<DxuiListView>  ();
    m_watchList         = CreateChild<DxuiListView>  ();
    m_stackList         = CreateChild<DxuiListView>  ();
    m_consoleList       = CreateChild<DxuiListView>  ();
    m_commandBox        = CreateChild<DxuiTextInput> ();
    m_memoryBox         = CreateChild<DxuiTextInput> ();
    m_pokeBox           = CreateChild<DxuiTextInput> ();
    m_pokeButton        = CreateChild<DxuiButton>    (L"Poke");
    m_groupButton       = CreateChild<DxuiButton>    (L"Bytes");
    m_addMemoryButton   = CreateChild<DxuiButton>    (L"+ Memory");
    m_removeMemoryButton = CreateChild<DxuiButton>   (L"- Memory");

    //  All four windows exist from the start; the ones not open are hidden.
    for (int id = 1; id <= DebuggerViewState::kMaxMemoryWindows; id++)
    {
        DxuiHexView  * view = CreateChild<DxuiHexView>();

        m_memoryPanes[(size_t) (id - 1)] = std::make_unique<MemoryPane> (
            id, view,
            [this] (int window, Word first)          { if (m_host != nullptr) { m_host->SetDebuggerMemoryWindow (window, first); } },
            [this] (const std::string & line)        { RunCommand (line); },
            [this] (const std::string & line)        { AppendConsole ({ line }); });

        view->SetVisible (id == 1);
        m_memoryOpen[(size_t) (id - 1)] = (id == 1);
    }

    //  Shown only while a debug file is loaded.
    m_sourceView   = CreateChild<DxuiTextView>();
    m_sourceBanner = CreateChild<DxuiActionBanner>();
    m_sourcePane   = std::make_unique<SourcePane> (
        m_sourceView, m_sourceBanner,
        [this] (const DebugSourceFile & record, const std::wstring & path, const std::string & key)
        {
            return (m_host != nullptr) ? m_host->FindDebuggerSource (record, path, key) : SourceLookup();
        },
        [this] (const std::string & line) { RunCommand (line); },
        [this] (Word address)             { if (m_host != nullptr) { m_host->SetDebuggerCodeAddress (address); } });

    m_sourceView->SetVisible   (false);
    m_sourceBanner->SetVisible (false);

    //  Last, so its strips and the drop overlay paint over the panes.
    m_dockSite = CreateChild<DxuiDockSite>();

    ConfigureWidgets();
    ConfigureDockSite();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ConfigureWidgets
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ConfigureWidgets()
{
    auto  run = [this] (const std::string & line)
    {
        if (m_host != nullptr)
        {
            m_host->RunDebuggerCommand (line);
        }
    };



    m_stepButton->SetOnClick     ([run] { run (DebuggerViewState::GetStepLine());     });
    m_stepOverButton->SetOnClick ([run] { run (DebuggerViewState::GetStepOverLine()); });
    m_stepOutButton->SetOnClick  ([run] { run (DebuggerViewState::GetStepOutLine());  });
    m_runButton->SetOnClick      ([run] { run (DebuggerViewState::GetRunLine());      });

    //  Pause is not a command line: it is the channel's pause, which stops a
    //  run in progress, and the command box has no equivalent.
    m_pauseButton->SetOnClick ([this] { if (m_host != nullptr) { m_host->PauseDebugger(); } });

    m_runToCursorButton->SetOnClick ([this, run]
    {
        int  row = m_codeList->GetSelectedRow();

        if (m_snapshot != nullptr && row >= 0 && row < (int) m_snapshot->code.size())
        {
            run (DebuggerViewState::GetRunToCursorLine (m_snapshot->code[(size_t) row].address));
        }
    });

    m_followPcButton->SetOnClick ([this] { if (m_host != nullptr) { m_host->SetDebuggerCodeAddress (std::nullopt); } });
    m_keysButton->SetOnClick     ([this] { CycleKeyScheme(); });

    m_pokeButton->SetOnClick ([this] { SubmitPokeBox(); });

    m_groupButton->SetOnClick ([this]
    {
        static const wchar_t * const  kNames[] = { L"", L"Bytes", L"Words", L"", L"Longs" };

        m_groupButton->SetLabel (kNames[GetActiveMemoryPane()->CycleGrouping()]);
    });

    m_addMemoryButton->SetOnClick    ([this] { AddMemoryWindow();    });
    m_removeMemoryButton->SetOnClick ([this] { RemoveMemoryWindow(); });

    for (const std::unique_ptr<MemoryPane> & pane : m_memoryPanes)
    {
        pane->Configure (GetHwnd());
    }

    m_sourcePane->Configure (GetHwnd());
    SetAcceptsDroppedFiles  (true);

    //  A code row selected shows its line in the source pane (FR-054).
    m_codeList->SetOnSelectionChanged ([this] (int row)
    {
        if (m_snapshot != nullptr && row >= 0 && row < (int) m_snapshot->code.size())
        {
            m_sourcePane->ShowLine (m_snapshot->code[(size_t) row].sourceFileId, m_snapshot->code[(size_t) row].sourceLine);
        }
    });

    //  Every column fits its contents and none stretches, so a pane is as wide
    //  as what it shows and no wider (FR-026a). The marker column alone has a
    //  set width, since its glyphs are not text a fit could measure.
    m_codeList->SetColumns ({ { L"",            kMarkerColumnDip, false, DxuiTextHAlign::Center },
                              { L"Address",     0, false, DxuiTextHAlign::Left },
                              { L"Bytes",       0, false, DxuiTextHAlign::Left },
                              { L"Label",       0, false, DxuiTextHAlign::Left },
                              { L"Instruction", 0, false, DxuiTextHAlign::Left } });

    //  A single click only selects, so a line can be chosen for Run to Cursor
    //  without also toggling its breakpoint.
    m_codeList->SetActivateOnDoubleClick       (true);
    m_breakpointList->SetActivateOnDoubleClick (true);

    //  Activating a line -- a double-click or Enter -- toggles its breakpoint,
    //  the way a margin click does in an editor.
    m_codeList->SetOnActivateRow ([this, run] (int row)
    {
        if (m_snapshot != nullptr && row >= 0 && row < (int) m_snapshot->code.size())
        {
            run (DebuggerViewState::GetToggleBreakpointLine (*m_snapshot, m_snapshot->code[(size_t) row].address));
        }
    });

    m_registerList->SetColumns   ({ { L"Reg",         0, false, DxuiTextHAlign::Left },
                                    { L"Value",       0, false, DxuiTextHAlign::Left } });
    m_breakpointList->SetColumns ({ { L"Breakpoints", 0, false, DxuiTextHAlign::Left } });
    m_watchList->SetColumns      ({ { L"Watch",       0, false, DxuiTextHAlign::Left },
                                    { L"Value",       0, false, DxuiTextHAlign::Left } });
    m_stackList->SetColumns      ({ { L"Stack",       0, false, DxuiTextHAlign::Left },
                                    { L"Value",       0, false, DxuiTextHAlign::Left } });
    m_consoleList->SetColumns    ({ { L"Console",     0, false, DxuiTextHAlign::Left } });
    m_consoleList->EnableStickyTail (true);

    //  Activating a breakpoint in the list clears it.
    m_breakpointList->SetOnActivateRow ([this, run] (int row)
    {
        if (m_snapshot != nullptr && row >= 0 && row < (int) m_snapshot->breakpoints.size())
        {
            run (std::format ("BPC {}", m_snapshot->breakpoints[(size_t) row].id));
        }
    });

    for (DxuiListView * list : GetLists())
    {
        MakeDense (list);
    }

    //  The console can hold thousands of lines, so it fits its column from a
    //  character count rather than measuring every line it has ever held.
    m_consoleList->SetPreciseAutoFit (false);

    for (DxuiTextInput * box : { m_commandBox, m_memoryBox, m_pokeBox })
    {
        box->SetHwnd      (GetHwnd());
        box->SetMaxLength (256);
    }

    m_commandBox->SetPlaceholder (L"Command (Enter to run)");
    m_memoryBox->SetPlaceholder  (L"Memory at (hex)");
    m_pokeBox->SetPlaceholder    (L"Address byte, e.g. 0300 A9");

    m_focusMgr.Attach   (this);
    m_focusMgr.SetTheme (m_theme);
    m_focusMgr.Rebuild();
    m_focusMgr.SetFocused (m_commandBox);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetLists
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView *> DebuggerWindow::GetLists() const
{
    return { m_codeList, m_registerList, m_breakpointList, m_watchList, m_stackList, m_consoleList };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::MakeDense
//
//  One setting for every pane, so the panes read as one listing rather than
//  seven lists that happen to share a window.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::MakeDense (DxuiListView * list)
{
    list->SetShowHeader      (true);
    list->SetMonospace       (true);
    list->SetFontSizeDip     (kPaneFontDip);
    list->SetRowHeightDip    (kPaneRowDip);
    list->SetHeaderHeightDip (kPaneHeaderDip);
    list->SetCellPaddingDip  (kPanePadDip, kPanePadDip);
    list->SetPreciseAutoFit  (true);
    list->SetRefitOnSetRows  (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetToolbarButtons
//
//  Left to right, in the order they are laid out.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiButton *> DebuggerWindow::GetToolbarButtons() const
{
    return { m_stepButton, m_stepOverButton, m_stepOutButton, m_runButton, m_runToCursorButton,
             m_pauseButton, m_followPcButton, m_keysButton };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetPressTargets
//
//  Every control a mouse press is offered to before the lists.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IDxuiControl *> DebuggerWindow::GetPressTargets() const
{
    std::vector<IDxuiControl *>  targets;



    for (DxuiButton * button : GetToolbarButtons())
    {
        targets.push_back (button);
    }

    for (DxuiButton * button : GetMemoryButtons())
    {
        targets.push_back (button);
    }

    targets.insert (targets.end(), { m_commandBox, m_memoryBox, m_pokeBox });

    return targets;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetMemoryButtons
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiButton *> DebuggerWindow::GetMemoryButtons() const
{
    return { m_pokeButton, m_groupButton, m_addMemoryButton, m_removeMemoryButton };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetOpenMemoryPanes
//
//  In window order, which is the order they are laid out.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<MemoryPane *> DebuggerWindow::GetOpenMemoryPanes() const
{
    std::vector<MemoryPane *>  open;



    for (const std::unique_ptr<MemoryPane> & pane : m_memoryPanes)
    {
        if (pane != nullptr && m_memoryOpen[(size_t) (pane->GetId() - 1)])
        {
            open.push_back (pane.get());
        }
    }

    return open;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetActiveMemoryPane
//
//  The window last clicked in, which the memory box and the grouping button
//  act on; the first window until another is clicked.
//
////////////////////////////////////////////////////////////////////////////////

MemoryPane * DebuggerWindow::GetActiveMemoryPane() const
{
    return (m_activePane != nullptr) ? m_activePane : m_memoryPanes[0].get();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetFocusedMemoryPane
//
////////////////////////////////////////////////////////////////////////////////

MemoryPane * DebuggerWindow::GetFocusedMemoryPane() const
{
    IDxuiControl  * focused = m_focusMgr.GetFocusedControl();



    for (MemoryPane * pane : GetOpenMemoryPanes())
    {
        if (focused != nullptr && focused == pane->GetView())
        {
            return pane;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ApplyMemoryWindows
//
//  The snapshot says which windows are open: each one it holds is shown and
//  given its bytes, and the rest are hidden. A different machine clears every
//  window's undo, since the addresses it would restore belonged to the old
//  one.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplyMemoryWindows()
{
    std::array<bool, DebuggerViewState::kMaxMemoryWindows>  open    = {};
    bool                                                    changed = false;



    if (m_snapshot->machine != m_machine)
    {
        m_machine = m_snapshot->machine;

        for (const std::unique_ptr<MemoryPane> & pane : m_memoryPanes)
        {
            pane->ClearHistory();
        }
    }

    for (const DebuggerViewSnapshot::MemoryWindow & window : m_snapshot->memoryWindows)
    {
        if (window.id >= 1 && window.id <= DebuggerViewState::kMaxMemoryWindows)
        {
            open[(size_t) (window.id - 1)] = true;
            m_memoryPanes[(size_t) (window.id - 1)]->Apply (window);
        }
    }

    for (size_t i = 0; i < m_memoryPanes.size(); i++)
    {
        //  A window opened comes to the front of its tab group.
        if (m_memoryOpen[i] != open[i])
        {
            m_memoryOpen[i] = open[i];
            changed         = true;

            if (open[i])
            {
                (void) m_dockSite->EditPaneLayout().Activate (DebuggerLayout::GetMemoryPaneId ((int) i + 1));
            }
        }

        if (!open[i] && m_activePane == m_memoryPanes[i].get())
        {
            m_activePane = nullptr;
        }
    }

    if (changed)
    {
        m_dockSite->Relayout();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::AddMemoryWindow
//
//  The lowest-numbered closed window opens where the active one is.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::AddMemoryWindow()
{
    Word  at = (Word) (GetActiveMemoryPane()->GetView()->GetTopRow() * DebuggerViewState::kMemoryRowBytes);



    for (const std::unique_ptr<MemoryPane> & pane : m_memoryPanes)
    {
        if (!m_memoryOpen[(size_t) (pane->GetId() - 1)] && m_host != nullptr)
        {
            m_host->SetDebuggerMemoryWindow (pane->GetId(), at);
            return;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RemoveMemoryWindow
//
//  The highest-numbered open window closes; the first never does.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::RemoveMemoryWindow()
{
    for (size_t i = m_memoryPanes.size(); i > 1; i--)
    {
        MemoryPane  * pane = m_memoryPanes[i - 1].get();

        if (m_memoryOpen[(size_t) (pane->GetId() - 1)] && m_host != nullptr)
        {
            m_host->SetDebuggerMemoryWindow (pane->GetId(), std::nullopt);
            return;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ApplySource
//
//  The source pane takes the snapshot, and the layout changes when the pane
//  or its banner comes or goes, or the banner's text or actions change: a new
//  action has no place until it is laid out.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplySource()
{
    bool          shown       = false;
    bool          bannerShown = false;
    std::wstring  bannerKey;



    m_sourcePane->Apply (*m_snapshot);
    bannerKey = m_sourceBanner->GetText() + (m_sourceBanner->GetAction (0) != nullptr ? m_sourceBanner->GetAction (0)->GetAccessibleName() : L"");

    shown       = m_sourcePane->IsActive();
    bannerShown = shown && m_sourcePane->HasBanner();

    if (shown != m_sourceShown)
    {
        m_sourceShown       = shown;
        m_sourceBannerShown = bannerShown;
        m_sourceBannerKey   = bannerKey;
        m_dockSite->Relayout();
    }
    else if (bannerShown != m_sourceBannerShown || bannerKey != m_sourceBannerKey)
    {
        m_sourceBannerShown = bannerShown;
        m_sourceBannerKey   = bannerKey;
        m_sourceFrame->Relayout();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteSourceMouse
//
//  The banner's actions, then the text: a click moves the code pane to the
//  line, a second click on the same place within the double-click time
//  toggles the line's breakpoint, and a drag selects text as it does in any
//  text view.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteSourceMouse (const DxuiMouseEvent & ev)
{
    POINT     at       = ev.positionDip;
    RECT      bounds   = m_sourceView->GetBounds();
    RECT      banner   = m_sourceBanner->GetBounds();
    bool      inside   = at.x >= bounds.left && at.x < bounds.right && at.y >= bounds.top && at.y < bounds.bottom;
    bool      onBanner = at.x >= banner.left && at.x < banner.right && at.y >= banner.top && at.y < banner.bottom;
    uint64_t  now      = GetTickCount64();
    bool      isDouble = false;



    //  A source pane behind another tab takes no input.
    if (!m_sourceShown || !m_sourceView->IsVisible())
    {
        return false;
    }

    if (m_sourceBannerShown && (onBanner || ev.kind == DxuiMouseEventKind::Up || ev.kind == DxuiMouseEventKind::Move))
    {
        (void) m_sourceBanner->OnMouse (ev);

        //  An action changes what the pane shows while the machine is paused,
        //  when no snapshot is coming to show it.
        if (onBanner && ev.kind == DxuiMouseEventKind::Up && m_snapshot != nullptr)
        {
            ApplySource();
        }

        if (onBanner && ev.kind == DxuiMouseEventKind::Down)
        {
            return true;
        }
    }

    if (m_sourceView->IsInteracting() && ev.kind != DxuiMouseEventKind::Down)
    {
        (void) m_sourceView->OnMouse (ev);
        return true;
    }

    if (!inside || (ev.kind != DxuiMouseEventKind::Down && ev.kind != DxuiMouseEventKind::Wheel))
    {
        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left)
    {
        isDouble = (now - m_sourceClickMs) <= (uint64_t) GetDoubleClickTime() &&
                   std::abs (at.x - m_sourceClickAt.x) <= GetSystemMetrics (SM_CXDOUBLECLK) &&
                   std::abs (at.y - m_sourceClickAt.y) <= GetSystemMetrics (SM_CYDOUBLECLK);

        m_sourceClickMs = isDouble ? 0 : now;
        m_sourceClickAt = at;

        if (isDouble)
        {
            m_sourcePane->OnDoubleClick (at);
        }
        else
        {
            m_sourcePane->OnClick (at);
        }

        m_focusMgr.SetFocused (m_sourceView);
        NoteViewFocus (true);
    }

    (void) m_sourceView->OnMouse (ev);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::NoteViewFocus
//
//  Steps follow the view: the source pane steps by source line, the
//  disassembly by instruction (FR-056). Sent only when it changes, so a click
//  does not fill the console.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::NoteViewFocus (bool isSource)
{
    if (m_host == nullptr || m_snapshot == nullptr || !m_snapshot->source.has_value())
    {
        return;
    }

    if (m_snapshot->source->stepBySource != isSource)
    {
        m_host->RunDebuggerCommand (isSource ? "SRC ON" : "SRC OFF");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OnFilesDropped
//
//  The first file goes to the source pane, matched against the loaded debug
//  file's records by the host.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::OnFilesDropped (const std::vector<std::wstring> & paths)
{
    SourceLookup  lookup;
    int           index = -1;



    if (paths.empty() || m_host == nullptr || m_snapshot == nullptr || !m_snapshot->source.has_value())
    {
        return false;
    }

    lookup = m_host->MatchDroppedDebuggerSource (m_snapshot->source->files, paths.front(), m_snapshot->source->programKey, index);
    m_sourcePane->ShowDropped (lookup, index);
    ApplySource();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteMemoryMouse
//
//  A drag in a window keeps its events until it ends; a press or the wheel goes
//  to the window under it, and a press makes it the active one. Releases reach
//  every window, so none is left thinking a drag is still on.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteMemoryMouse (const DxuiMouseEvent & ev)
{
    POINT  at = ev.positionDip;



    for (MemoryPane * pane : GetOpenMemoryPanes())
    {
        DxuiHexView  * view   = pane->GetView();
        RECT           bounds = view->GetBounds();
        bool           inside = view->IsVisible() && at.x >= bounds.left && at.x < bounds.right && at.y >= bounds.top && at.y < bounds.bottom;

        if (view->IsDragging() && ev.kind != DxuiMouseEventKind::Down)
        {
            (void) view->OnMouse (ev);
            return true;
        }

        if (inside && (ev.kind == DxuiMouseEventKind::Down || ev.kind == DxuiMouseEventKind::Wheel))
        {
            (void) view->OnMouse (ev);

            if (ev.kind == DxuiMouseEventKind::Down)
            {
                m_focusMgr.SetFocused (view);
                m_activePane = pane;
            }

            return true;
        }

        if (ev.kind == DxuiMouseEventKind::Up)
        {
            (void) view->OnMouse (ev);
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetFocusedBox
//
////////////////////////////////////////////////////////////////////////////////

DxuiTextInput * DebuggerWindow::GetFocusedBox() const
{
    IDxuiControl  * focused = m_focusMgr.GetFocusedControl();



    for (DxuiTextInput * box : { m_commandBox, m_memoryBox, m_pokeBox })
    {
        if (focused != nullptr && focused == box)
        {
            return box;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RunCommand
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::RunCommand (const std::string & line)
{
    if (m_host != nullptr)
    {
        m_host->RunDebuggerCommand (line);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetSavedKeyScheme
//
//  A name this build does not know reads as the default.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerKeyScheme DebuggerWindow::GetSavedKeyScheme() const
{
    DebuggerKeyScheme  scheme = DebuggerKeySchemes::kDefault;



    if (m_host != nullptr && !DebuggerKeySchemes::TryParse (m_host->GetDebuggerKeyScheme(), scheme))
    {
        scheme = DebuggerKeySchemes::kDefault;
    }

    return scheme;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ApplyKeyScheme
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplyKeyScheme (DebuggerKeyScheme scheme)
{
    const DxuiKeyMap &  map = DebuggerKeySchemes::GetMap (scheme);



    m_keyScheme = scheme;
    SetKeyMap (&map);

    if (m_keysButton != nullptr)
    {
        m_keysButton->SetLabel (L"Keys: " + map.GetName());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::CycleKeyScheme
//
//  Visual Studio, AppleWin, GSSquared, and round again; the choice is saved at
//  once, so it holds whether or not the window is closed cleanly.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::CycleKeyScheme()
{
    static constexpr DebuggerKeyScheme  kOrder[] = { DebuggerKeyScheme::VisualStudio,
                                                     DebuggerKeyScheme::AppleWin,
                                                     DebuggerKeyScheme::GSSquared };
    size_t                              next     = 0;



    for (size_t i = 0; i < std::size (kOrder); ++i)
    {
        if (kOrder[i] == m_keyScheme)
        {
            next = (i + 1) % std::size (kOrder);
        }
    }

    ApplyKeyScheme (kOrder[next]);

    if (m_host != nullptr)
    {
        m_host->SetDebuggerKeyScheme (DebuggerKeySchemes::GetName (kOrder[next]));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OnMappedCommand
//
//  Every action is the command its button sends, except Pause, which is the
//  channel's pause. A cursor action with no line to act on still consumes the
//  key, so it does not fall through to anything else.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::OnMappedCommand (int commandId)
{
    DebuggerKeySchemes::Action  action = (DebuggerKeySchemes::Action) commandId;
    std::optional<std::string>  line;
    int                         row    = (m_codeList != nullptr) ? m_codeList->GetSelectedRow() : -1;



    if (action == DebuggerKeySchemes::Action::Pause)
    {
        if (m_host != nullptr)
        {
            m_host->PauseDebugger();
        }

        return true;
    }

    line = DebuggerViewState::GetActionLine (action, m_snapshot.get(), row);

    if (line.has_value())
    {
        RunCommand (*line);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteBoxKey
//
//  A key-down in a focused text box that the box does not keep goes to the
//  scheme first. A memory window counts as a box that is never empty, since
//  every character typed into it is an edit. When the scheme took a Space, the character WM_CHAR delivers
//  for it is swallowed, or an empty box that stepped would be left holding a
//  space. Returns true when the key was decided here.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteBoxKey (const DxuiKeyEvent & ev, bool & handled)
{
    DxuiTextInput  * box      = GetFocusedBox();
    bool             inMemory = GetFocusedMemoryPane() != nullptr;
    bool             decided  = false;



    if (ev.kind == DxuiKeyEventKind::Char)
    {
        decided        = m_swallowSpace && ev.vk == L' ';
        handled        = decided;
        m_swallowSpace = false;
    }
    else if (ev.kind == DxuiKeyEventKind::Down && (box != nullptr || inMemory) &&
             !DebuggerKeySchemes::DoesBoxKeepKey (ev.vk, ev.ctrl, ev.alt, true, box != nullptr && box->GetText().empty()) &&
             RouteMappedKey (ev))
    {
        m_swallowSpace = ev.vk == VK_SPACE;
        handled        = true;
        decided        = true;
    }

    return decided;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OnWindowClose
//
//  Closing the window closes the debugger: the host closes the channel, so
//  attached clients receive `closing`. The window is hidden rather than
//  destroyed and is shown again when the debugger reopens.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OnWindowClose()
{
    Hide();

    if (m_host != nullptr)
    {
        m_host->OnDebuggerWindowClosed();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::Layout
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    m_widthDip  = std::max (1, (int) (boundsDip.right  - boundsDip.left));
    m_heightDip = std::max (1, (int) (boundsDip.bottom - boundsDip.top));
    m_scaler    = scaler;

    LayoutWidgets();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::LayoutWidgets
//
//  Controls across the top and the memory bar across the bottom stay where
//  they are; the dock site between them arranges every pane.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::LayoutWidgets()
{
    auto  px       = [this] (int dip) { return m_scaler.ToPx (dip); };
    int   pad      = px (8);
    int   buttonH  = px (30);
    int   boxH     = px (30);
    int   width    = m_widthDip;
    int   height   = m_heightDip;
    int   captionH = GetCaptionHeightPx();
    int   rowY     = captionH + pad;
    int   top      = rowY + buttonH + pad;
    int   barY     = std::max (top + px (120), height - pad - boxH);
    int   x        = pad;



    if (m_codeList == nullptr || m_dockSite == nullptr)
    {
        return;
    }

    for (DxuiButton * button : GetToolbarButtons())
    {
        int  w = px ((button == m_keysButton) ? 170 : (button == m_runToCursorButton) ? 120 : 96);

        button->Layout (RECT { x, rowY, x + w, rowY + buttonH }, m_scaler);
        x += w + pad;
    }

    m_flagsLabel->Layout (RECT { x + pad, rowY, width - pad, rowY + buttonH }, m_scaler);

    m_dockSite->Layout (RECT { pad, top, width - pad, barY - pad }, m_scaler);

    x = pad;
    m_memoryBox->Layout  (RECT { x, barY, x + px (170), barY + boxH }, m_scaler);  x += px (170) + pad;
    m_pokeBox->Layout    (RECT { x, barY, x + px (230), barY + boxH }, m_scaler);  x += px (230) + pad;

    for (DxuiButton * button : GetMemoryButtons())
    {
        button->Layout (RECT { x, barY, x + px (90), barY + boxH }, m_scaler);
        x += px (90) + pad;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ConfigureDockSite
//
//  Every pane goes to the site under its layout id. The source pane and the
//  console are frames over several controls; the rest are one control each.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ConfigureDockSite()
{
    auto          boxHeight = [] (int, const DxuiDpiScaler & scaler) { return scaler.ToPx (30); };
    std::wstring  savedText;



    m_sourceFrame = std::make_unique<DebuggerPaneFrame> (L"Source");
    m_sourceFrame->AddPart (m_sourceBanner,
                            [this] (int width, const DxuiDpiScaler & scaler) { return (int) m_sourceBanner->GetPreferredHeightPx ((float) width, scaler); },
                            [this] { return m_sourceBannerShown; });
    m_sourceFrame->AddPart (m_sourceView);

    m_consoleFrame = std::make_unique<DebuggerPaneFrame> (L"Console");
    m_consoleFrame->AddPart (m_consoleList);
    m_consoleFrame->AddPart (m_commandBox, boxHeight);

    m_dockSite->AddPane (DebuggerLayout::kCode,        L"Disassembly", m_codeList);
    m_dockSite->AddPane (DebuggerLayout::kSource,      L"Source",      m_sourceFrame.get());
    m_dockSite->AddPane (DebuggerLayout::kConsole,     L"Console",     m_consoleFrame.get());
    m_dockSite->AddPane (DebuggerLayout::kRegisters,   L"Registers",   m_registerList);
    m_dockSite->AddPane (DebuggerLayout::kBreakpoints, L"Breakpoints", m_breakpointList);
    m_dockSite->AddPane (DebuggerLayout::kWatches,     L"Watches",     m_watchList);
    m_dockSite->AddPane (DebuggerLayout::kStack,       L"Stack",       m_stackList);

    for (const std::unique_ptr<MemoryPane> & pane : m_memoryPanes)
    {
        m_dockSite->AddPane (DebuggerLayout::GetMemoryPaneId (pane->GetId()),
                             std::format (L"Memory {}", pane->GetId()), pane->GetView());
    }

    m_dockSite->SetShownFn    ([this] (const std::wstring & pane) { return IsPaneShown (pane); });
    savedText = (m_host != nullptr) ? SourcePathList::Utf8ToWide (m_host->GetDebuggerLayout()) : std::wstring();
    m_dockSite->SetPaneLayout (DebuggerLayout::Restore (savedText));

    //  Every change the user makes is saved as it happens, so a crash or a
    //  closed emulator loses nothing.
    m_dockSite->SetOnChanged ([this]
    {
        if (m_host != nullptr)
        {
            m_host->SetDebuggerLayout (SourcePathList::WideToUtf8 (m_dockSite->GetPaneLayout().ToText()));
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsPaneShown
//
//  The source pane shows while a debug file is loaded and a memory window
//  while it is open; the rest always show.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsPaneShown (const std::wstring & pane) const
{
    if (pane == DebuggerLayout::kSource)
    {
        return m_sourceShown;
    }

    for (const std::unique_ptr<MemoryPane> & memory : m_memoryPanes)
    {
        if (pane == DebuggerLayout::GetMemoryPaneId (memory->GetId()))
        {
            return m_memoryOpen[(size_t) (memory->GetId() - 1)];
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetPaneOfFocus
//
//  The pane holding the focused control, including the parts of a frame.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::GetPaneOfFocus() const
{
    IDxuiControl  * focused = m_focusMgr.GetFocusedControl();



    if (focused == nullptr)
    {
        return L"";
    }

    if (focused == m_consoleList || focused == m_commandBox)
    {
        return DebuggerLayout::kConsole;
    }

    if (focused == m_sourceView)
    {
        return DebuggerLayout::kSource;
    }

    return m_dockSite->GetPaneOf (focused);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ShowDockToMenu
//
//  The Dock To choices for a pane as a context menu (FR-042); the one chosen
//  runs through the site like a drop would.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ShowDockToMenu (const std::wstring & pane, POINT clientPx)
{
    std::vector<DxuiDockSite::MenuItem>  items  = m_dockSite->GetDockToMenu (pane);
    HMENU                                menu   = nullptr;
    POINT                                screen = clientPx;
    int                                  chosen = 0;



    if (items.empty())
    {
        return;
    }

    menu = CreatePopupMenu();

    if (menu == nullptr)
    {
        return;
    }

    for (size_t i = 0; i < items.size(); i++)
    {
        AppendMenuW (menu, MF_STRING, (UINT_PTR) (i + 1), items[i].label.c_str());
    }

    ClientToScreen (GetHwnd(), &screen);
    chosen = (int) TrackPopupMenu (menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, screen.x, screen.y, 0, GetHwnd(), nullptr);
    DestroyMenu (menu);

    if (chosen >= 1 && chosen <= (int) items.size())
    {
        (void) items[(size_t) (chosen - 1)].action();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RenderFrame
//
//  Once per UI frame: take whatever the CPU thread published, then repaint.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::RenderFrame()
{
    std::shared_ptr<const DebuggerViewSnapshot>  snapshot;
    std::vector<std::string>                     console;
    int64_t                                      now = (int64_t) GetTickCount64();



    if (!IsCreated() || m_host == nullptr)
    {
        return;
    }

    if (m_host->TakeDebuggerUpdate (snapshot, console))
    {
        if (snapshot != nullptr)
        {
            m_snapshot = std::move (snapshot);
            ApplySnapshot();
        }

        AppendConsole (console);
    }

    for (DxuiListView * list : GetLists())
    {
        list->Tick (now);
    }

    for (MemoryPane * pane : GetOpenMemoryPanes())
    {
        pane->FollowScroll();
        (void) pane->GetView()->TickScrollbars (now);
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ApplySnapshot
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplySnapshot()
{
    std::vector<std::vector<DxuiListView::Cell>>  rows;
    int                                           current = -1;



    for (size_t i = 0; i < m_snapshot->code.size(); i++)
    {
        const DebuggerViewSnapshot::CodeLine & line   = m_snapshot->code[i];
        std::wstring                           marker;

        if (line.isCurrent)
        {
            marker  = s_kpszTriangleRight;
            current = (int) i;
        }
        else if (line.hasBreakpoint)
        {
            marker = std::wstring (1, s_kchBullet);
        }

        rows.push_back ({ { marker },
                          { std::format (L"{:04X}", line.address) },
                          { Widen (line.bytes) },
                          { Widen (line.label) },
                          { Widen (line.instruction) } });
    }

    m_codeList->SetRows (std::move (rows));

    if (current >= 0 && m_codeList->GetSelectedRow() < 0)
    {
        m_codeList->EnsureVisible (current);
    }

    rows.clear();

    for (const DebuggerViewSnapshot::RegisterRow & reg : m_snapshot->registers)
    {
        rows.push_back ({ { Widen (reg.name) }, { Widen (reg.value) } });
    }

    m_registerList->SetRows (std::move (rows));
    m_flagsLabel->SetText   (L"Flags  " + Widen (m_snapshot->flags));

    rows.clear();

    for (const DebuggerViewSnapshot::BreakpointLine & bp : m_snapshot->breakpoints)
    {
        rows.push_back ({ { Widen (bp.text), !bp.enabled } });
    }

    m_breakpointList->SetRows (std::move (rows));

    rows.clear();

    for (const DebuggerViewSnapshot::WatchLine & watch : m_snapshot->watches)
    {
        rows.push_back ({ { std::format (L"#{} ${:04X}", watch.id, watch.address) }, { Widen (watch.value) } });
    }

    m_watchList->SetRows (std::move (rows));

    rows.clear();

    for (const DebuggerViewSnapshot::StackLine & entry : m_snapshot->stack)
    {
        rows.push_back ({ { std::format (L"${:04X}", entry.address) }, { std::format (L"{:02X}", entry.value) } });
    }

    m_stackList->SetRows (std::move (rows));

    ApplyMemoryWindows();
    ApplySource();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::AppendConsole
//
//  Bounded, so a long session does not grow the list without limit; the
//  oldest lines go first.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::AppendConsole (const std::vector<std::string> & lines)
{
    std::vector<std::vector<DxuiListView::Cell>>  rows;



    if (lines.empty())
    {
        return;
    }

    m_console.insert (m_console.end(), lines.begin(), lines.end());

    if ((int) m_console.size() > kConsoleLineLimit)
    {
        m_console.erase (m_console.begin(), m_console.end() - kConsoleLineLimit);
    }

    for (const std::string & line : m_console)
    {
        rows.push_back ({ { Widen (line) } });
    }

    m_consoleList->SetRows (std::move (rows));
    m_consoleList->UpdateAutoFitFromRows();
    m_consoleList->EnsureVisible ((int) m_console.size() - 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SubmitCommandBox
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SubmitCommandBox()
{
    HRESULT                   hr      = S_OK;
    std::string               line    = TextEncoding::WideToNarrow (m_commandBox->GetText());
    std::optional<DebugVerb>  fileVerb;
    FileDialogSpec            spec;
    std::filesystem::path     chosen;
    bool                      picked  = false;



    BAIL_OUT_IF (m_host == nullptr, S_OK);

    //  The mode comes from the last snapshot; one built before a mode change
    //  can only miss a prompt, and the handler then reports the missing name.
    fileVerb = DebuggerViewState::GetMissingFileVerb (line, m_snapshot != nullptr ? m_snapshot->mode : CommandMode::AppleWin);

    if (fileVerb.has_value())
    {
        spec.filters = { { L"All files", L"*.*" } };

        hr = (*fileVerb == DebugVerb::ReadFile) ? m_host->GetHostDialogs().PickFileToOpen (GetHwnd(), spec, chosen, picked)
                                                : m_host->GetHostDialogs().PickFileToSave (GetHwnd(), spec, chosen, picked);
        CHR (hr);

        //  Backing out of the picker keeps the line in the box, unrun.
        BAIL_OUT_IF (!picked, S_OK);

        line = DebuggerViewState::GetLineWithFileName (line, TextEncoding::WideToNarrow (chosen.wstring()));
    }

    m_host->RunDebuggerCommand (line);
    m_commandBox->SetText (L"");

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SubmitMemoryBox
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SubmitMemoryBox()
{
    Word  address = 0;



    if (TryParseHexWord (m_memoryBox->GetText(), address))
    {
        GetActiveMemoryPane()->GoTo (address);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SubmitPokeBox
//
//  "address byte". A box that does not hold both is left as typed so the
//  mistake can be corrected rather than retyped.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SubmitPokeBox()
{
    std::wstring  text  = m_pokeBox->GetText();
    size_t        space = text.find_first_of (L" \t,=");
    Word          address = 0;
    Word          value   = 0;



    if (space == std::wstring::npos)
    {
        return;
    }

    if (!TryParseHexWord (text.substr (0, space), address) || !TryParseHexWord (text.substr (space + 1), value) || value > 0xFF)
    {
        return;
    }

    if (m_host != nullptr)
    {
        m_host->RunDebuggerCommand (DebuggerViewState::GetPokeLine (address, (Byte) value));
    }

    m_pokeBox->SetText (L"");
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ForwardToList
//
//  A list takes coordinates relative to itself.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::ForwardToList (DxuiListView * list, const DxuiMouseEvent & ev)
{
    DxuiMouseEvent  local  = ev;
    RECT            bounds = list->GetBounds();



    local.positionDip = { ev.positionDip.x - bounds.left, ev.positionDip.y - bounds.top };
    return list->OnMouse (local);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OfferPress
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OfferPress (IDxuiControl * control, const DxuiMouseEvent & ev, bool & handled)
{
    if (!handled && control != nullptr && control->OnMouse (ev))
    {
        m_focusMgr.SetFocused (control);
        handled = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::OnMouse (const DxuiMouseEvent & ev)
{
    int           x       = ev.positionDip.x;
    int           y       = ev.positionDip.y;
    bool          lbDown  = (GetKeyState (VK_LBUTTON) & 0x8000) != 0;
    bool          handled = false;
    std::wstring  pane;



    //  The site first: its strips, its sashes and a drag in progress lie over
    //  the panes.
    if (m_dockSite->OnMouse (ev))
    {
        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Right)
    {
        pane = m_dockSite->GetPaneAt (ev.positionDip);

        if (!pane.empty())
        {
            ShowDockToMenu (pane, ev.positionDip);
        }

        return true;
    }

    if (RouteMemoryMouse (ev) || RouteSourceMouse (ev))
    {
        return true;
    }

    for (DxuiListView * list : GetLists())
    {
        if (list->IsInteracting() && ev.kind != DxuiMouseEventKind::Down)
        {
            ForwardToList (list, ev);
            return true;
        }
    }

    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        for (DxuiButton * button : GetToolbarButtons())
        {
            button->SetMouse (x, y, button->HitTest (x, y) && lbDown);
        }

        for (DxuiButton * button : GetMemoryButtons())
        {
            button->SetMouse (x, y, button->HitTest (x, y) && lbDown);
        }

        for (DxuiTextInput * box : { m_commandBox, m_memoryBox, m_pokeBox })
        {
            box->SetMouseHover (x, y);
        }

        return true;

    case DxuiMouseEventKind::Down:
        if (ev.button != DxuiMouseButton::Left)
        {
            return true;
        }

        for (IDxuiControl * control : GetPressTargets())
        {
            OfferPress (control, ev, handled);
        }

        for (DxuiListView * list : GetLists())
        {
            RECT  bounds = list->GetBounds();

            if (!handled && list->IsVisible() && x >= bounds.left && x < bounds.right && y >= bounds.top && y < bounds.bottom)
            {
                handled = ForwardToList (list, ev);
                m_focusMgr.SetFocused (list);
                handled = true;

                if (list == m_codeList)
                {
                    NoteViewFocus (false);
                }
            }
        }

        return true;

    case DxuiMouseEventKind::Up:
        for (IDxuiControl * control : GetPressTargets())
        {
            control->OnMouse (ev);
        }

        for (DxuiListView * list : GetLists())
        {
            ForwardToList (list, ev);
        }

        return true;

    case DxuiMouseEventKind::Wheel:
        for (DxuiListView * list : GetLists())
        {
            RECT  bounds = list->GetBounds();

            if (list->IsVisible() && x >= bounds.left && x < bounds.right && y >= bounds.top && y < bounds.bottom)
            {
                ForwardToList (list, ev);
            }
        }

        return true;

    default:
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteDockKey
//
//  Docking from the keyboard (FR-042): Shift+F10 or the menu key opens Dock To
//  for the focused pane, and Alt+Shift with an arrow moves it into the group
//  that way.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteDockKey (const DxuiKeyEvent & ev)
{
    std::wstring  pane   = GetPaneOfFocus();
    RECT          bounds = {};
    DxuiDockSide  side   = DxuiDockSide::Left;



    if (pane.empty())
    {
        return false;
    }

    if (ev.vk == VK_APPS || (ev.vk == VK_F10 && ev.shift && !ev.ctrl && !ev.alt))
    {
        bounds = m_focusMgr.GetFocusedControl()->GetBounds();
        ShowDockToMenu (pane, POINT { bounds.left, bounds.top });
        return true;
    }

    if (!ev.alt || !ev.shift || ev.ctrl)
    {
        return false;
    }

    switch (ev.vk)
    {
    case VK_LEFT:  side = DxuiDockSide::Left;   break;
    case VK_RIGHT: side = DxuiDockSide::Right;  break;
    case VK_UP:    side = DxuiDockSide::Top;    break;
    case VK_DOWN:  side = DxuiDockSide::Bottom; break;
    default:       return false;
    }

    (void) m_dockSite->MovePaneByArrow (pane, side);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OnKey
//
//  Enter in a box submits it. The text input has no submit event of its own,
//  so the window catches Enter before the focused box would ignore it.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::OnKey (const DxuiKeyEvent & ev)
{
    IDxuiControl  * focused = m_focusMgr.GetFocusedControl();
    bool            handled = false;



    if (RouteBoxKey (ev, handled))
    {
        return handled;
    }

    if (ev.kind == DxuiKeyEventKind::Down && RouteDockKey (ev))
    {
        return true;
    }

    if (ev.kind == DxuiKeyEventKind::Down && ev.vk == VK_RETURN)
    {
        if      (focused == m_commandBox) { SubmitCommandBox(); return true; }
        else if (focused == m_memoryBox)  { SubmitMemoryBox();  return true; }
        else if (focused == m_pokeBox)    { SubmitPokeBox();    return true; }
    }

    if (ev.kind == DxuiKeyEventKind::Char && GetFocusedMemoryPane() != nullptr)
    {
        return focused->OnKey (ev);
    }

    if (ev.kind == DxuiKeyEventKind::Char)
    {
        return m_commandBox->OnKey (ev) || m_memoryBox->OnKey (ev) || m_pokeBox->OnKey (ev);
    }

    //  Ctrl+Z in a memory window undoes that window's last edit.
    if (ev.kind == DxuiKeyEventKind::Down && ev.ctrl && !ev.alt && ev.vk == 'Z' && GetFocusedMemoryPane() != nullptr)
    {
        (void) GetFocusedMemoryPane()->Undo();
        return true;
    }

    if (ev.kind == DxuiKeyEventKind::Down)
    {
        handled = (focused != nullptr) && focused->OnKey (ev);

        if (!handled && ev.vk == VK_TAB)
        {
            m_focusMgr.HandleKey (ev.shift ? DxuiFocusKey::ShiftTab : DxuiFocusKey::Tab);
            handled = true;
        }
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DebuggerWindow::GetCursorForPoint (POINT clientPx) const
{
    LPCWSTR  sash = (m_dockSite != nullptr) ? m_dockSite->GetCursorForPoint (clientPx) : nullptr;



    if (sash != nullptr)
    {
        return sash;
    }

    for (DxuiListView * list : GetLists())
    {
        if (!list->IsVisible())
        {
            continue;
        }

        RECT     bounds = list->GetBounds();
        POINT    local  = { clientPx.x - bounds.left, clientPx.y - bounds.top };
        LPCWSTR  cursor = list->GetCursorForPoint (local);

        if (cursor != nullptr)
        {
            return cursor;
        }
    }

    return DxuiWindow::GetCursorForPoint (clientPx);
}
