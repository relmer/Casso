#include "Pch.h"

#include "Ui/Debugger/DebuggerWindow.h"

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
    m_runButton         = CreateChild<DxuiButton>    (L"Run");
    m_runToCursorButton = CreateChild<DxuiButton>    (L"Run to Cursor");
    m_pauseButton       = CreateChild<DxuiButton>    (L"Pause");
    m_followPcButton    = CreateChild<DxuiButton>    (L"Follow PC");
    m_flagsLabel        = CreateChild<DxuiLabel>     (L"", DxuiTextRole::Body, DxuiTextHAlign::Left);
    m_codeList          = CreateChild<DxuiListView>  ();
    m_registerList      = CreateChild<DxuiListView>  ();
    m_breakpointList    = CreateChild<DxuiListView>  ();
    m_watchList         = CreateChild<DxuiListView>  ();
    m_stackList         = CreateChild<DxuiListView>  ();
    m_memoryList        = CreateChild<DxuiListView>  ();
    m_consoleList       = CreateChild<DxuiListView>  ();
    m_commandBox        = CreateChild<DxuiTextInput> ();
    m_memoryBox         = CreateChild<DxuiTextInput> ();
    m_pokeBox           = CreateChild<DxuiTextInput> ();
    m_pokeButton        = CreateChild<DxuiButton>    (L"Poke");

    ConfigureWidgets();
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

    m_pokeButton->SetOnClick ([this] { SubmitPokeBox(); });

    m_codeList->SetColumns ({ { L"",            34, false, DxuiTextHAlign::Center },
                              { L"Address",     90, false, DxuiTextHAlign::Left   },
                              { L"Bytes",       100, false, DxuiTextHAlign::Left  },
                              { L"Instruction", 0,  true,  DxuiTextHAlign::Left   },
                              { L"Symbol",      110, false, DxuiTextHAlign::Left  } });

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

    m_registerList->SetColumns   ({ { L"Reg",   70, false, DxuiTextHAlign::Left },
                                    { L"Value", 0,  true,  DxuiTextHAlign::Left } });
    m_breakpointList->SetColumns ({ { L"Breakpoints", 0, true, DxuiTextHAlign::Left } });
    m_watchList->SetColumns      ({ { L"Watch", 0, true,  DxuiTextHAlign::Left },
                                    { L"Value", 90, false, DxuiTextHAlign::Left } });
    m_stackList->SetColumns      ({ { L"Stack", 0, true,  DxuiTextHAlign::Left },
                                    { L"Value", 90, false, DxuiTextHAlign::Left } });
    m_memoryList->SetColumns     ({ { L"Address", 90,  false, DxuiTextHAlign::Left },
                                    { L"Bytes",   0,   true,  DxuiTextHAlign::Left },
                                    { L"Text",    150, false, DxuiTextHAlign::Left },
                                    { L"Region",  90,  false, DxuiTextHAlign::Left } });
    m_consoleList->SetColumns    ({ { L"Console", 0, true, DxuiTextHAlign::Left } });
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
        list->SetShowHeader (true);
    }

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
    return { m_codeList, m_registerList, m_breakpointList, m_watchList, m_stackList, m_memoryList, m_consoleList };
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
//  Controls across the top; code and the console on the left, the small panes
//  down the right; memory across the bottom with its two boxes.
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
    int   rightW   = px (300);
    int   rightX   = std::max (pad, width - pad - rightW);
    int   leftW    = std::max (px (100), rightX - 2 * pad);
    int   memoryH  = std::max (px (140), height / 4);
    int   memoryY  = height - pad - boxH - pad - memoryH;
    int   middleH  = std::max (px (120), memoryY - pad - top);
    int   codeH    = (middleH * 3) / 5;
    int   consoleY = top + codeH + pad;
    int   consoleH = std::max (px (40), middleH - codeH - pad - boxH - pad);
    int   regH     = std::min (px (270), middleH / 2);
    int   restH    = std::max (px (60), (middleH - regH - 3 * pad) / 3);
    int   paneY    = top;
    int   x        = pad;



    if (m_codeList == nullptr)
    {
        return;
    }

    for (DxuiButton * button : { m_stepButton, m_stepOverButton, m_runButton, m_runToCursorButton, m_pauseButton, m_followPcButton })
    {
        int  w = px ((button == m_runToCursorButton) ? 120 : 96);

        button->Layout (RECT { x, rowY, x + w, rowY + buttonH }, m_scaler);
        x += w + pad;
    }

    m_flagsLabel->Layout (RECT { x + pad, rowY, width - pad, rowY + buttonH }, m_scaler);

    m_codeList->Layout    (RECT { pad, top, pad + leftW, top + codeH }, m_scaler);
    m_consoleList->Layout (RECT { pad, consoleY, pad + leftW, consoleY + consoleH }, m_scaler);
    m_commandBox->Layout  (RECT { pad, consoleY + consoleH + pad, pad + leftW, consoleY + consoleH + pad + boxH }, m_scaler);

    m_registerList->Layout   (RECT { rightX, paneY, width - pad, paneY + regH  }, m_scaler);  paneY += regH  + pad;
    m_breakpointList->Layout (RECT { rightX, paneY, width - pad, paneY + restH }, m_scaler);  paneY += restH + pad;
    m_watchList->Layout      (RECT { rightX, paneY, width - pad, paneY + restH }, m_scaler);  paneY += restH + pad;
    m_stackList->Layout      (RECT { rightX, paneY, width - pad, top + middleH }, m_scaler);

    m_memoryList->Layout (RECT { pad, memoryY, width - pad, memoryY + memoryH }, m_scaler);

    x = pad;
    m_memoryBox->Layout  (RECT { x, memoryY + memoryH + pad, x + px (170), memoryY + memoryH + pad + boxH }, m_scaler);  x += px (170) + pad;
    m_pokeBox->Layout    (RECT { x, memoryY + memoryH + pad, x + px (230), memoryY + memoryH + pad + boxH }, m_scaler);  x += px (230) + pad;
    m_pokeButton->Layout (RECT { x, memoryY + memoryH + pad, x + px (90),  memoryY + memoryH + pad + boxH }, m_scaler);
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
                          { Widen (line.instruction) },
                          { Widen (line.symbol), true } });
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

    rows.clear();

    for (const DebuggerViewSnapshot::MemoryLine & line : m_snapshot->memory)
    {
        rows.push_back ({ { std::format (L"{:04X}", line.address) },
                          { Widen (line.bytes) },
                          { Widen (line.characters) },
                          { Widen (line.region), true } });
    }

    m_memoryList->SetRows (std::move (rows));
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



    if (m_host != nullptr && TryParseHexWord (m_memoryBox->GetText(), address))
    {
        m_host->SetDebuggerMemoryAddress (address);
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
    int   x       = ev.positionDip.x;
    int   y       = ev.positionDip.y;
    bool  lbDown  = (GetKeyState (VK_LBUTTON) & 0x8000) != 0;
    bool  handled = false;



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
        for (DxuiButton * button : { m_stepButton, m_stepOverButton, m_runButton, m_runToCursorButton, m_pauseButton, m_followPcButton, m_pokeButton })
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

        for (IDxuiControl * control : std::initializer_list<IDxuiControl *> { m_stepButton, m_stepOverButton, m_runButton, m_runToCursorButton,
                                                                               m_pauseButton, m_followPcButton, m_pokeButton,
                                                                               m_commandBox, m_memoryBox, m_pokeBox })
        {
            OfferPress (control, ev, handled);
        }

        for (DxuiListView * list : GetLists())
        {
            RECT  bounds = list->GetBounds();

            if (!handled && x >= bounds.left && x < bounds.right && y >= bounds.top && y < bounds.bottom)
            {
                handled = ForwardToList (list, ev);
                m_focusMgr.SetFocused (list);
                handled = true;
            }
        }

        return true;

    case DxuiMouseEventKind::Up:
        for (IDxuiControl * control : std::initializer_list<IDxuiControl *> { m_stepButton, m_stepOverButton, m_runButton, m_runToCursorButton,
                                                                               m_pauseButton, m_followPcButton, m_pokeButton,
                                                                               m_commandBox, m_memoryBox, m_pokeBox })
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

            if (x >= bounds.left && x < bounds.right && y >= bounds.top && y < bounds.bottom)
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



    if (ev.kind == DxuiKeyEventKind::Down && ev.vk == VK_RETURN)
    {
        if      (focused == m_commandBox) { SubmitCommandBox(); return true; }
        else if (focused == m_memoryBox)  { SubmitMemoryBox();  return true; }
        else if (focused == m_pokeBox)    { SubmitPokeBox();    return true; }
    }

    if (ev.kind == DxuiKeyEventKind::Char)
    {
        return m_commandBox->OnKey (ev) || m_memoryBox->OnKey (ev) || m_pokeBox->OnKey (ev);
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
    for (DxuiListView * list : GetLists())
    {
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
