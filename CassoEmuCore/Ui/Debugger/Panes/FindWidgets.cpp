#include "Pch.h"

#include "Ui/Debugger/Panes/FindWidgets.h"

#include "Core/UnicodeSymbols.h"
#include "Debugger/Source/SourcePathList.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"
#include "Ui/Debugger/Panes/FindWidgetPlate.h"
#include "Ui/Debugger/WholeWordButton.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidget::GetControls
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IDxuiControl *> FindWidget::GetControls() const
{
    return { plate, chevron, box, caseButton, wordButton, regexButton, status, prevButton, nextButton, selButton, closeButton };
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidget::GetButtons
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiButton *> FindWidget::GetButtons() const
{
    return { caseButton, wordButton, regexButton, prevButton, nextButton, selButton, closeButton };
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::FindWidgetSet
//
////////////////////////////////////////////////////////////////////////////////

FindWidgetSet::FindWidgetSet (IDebuggerPaneHost & host, IFindWidgetHost & window) :
    m_host   (host),
    m_window (window)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::Activate
//
////////////////////////////////////////////////////////////////////////////////

FindWidget & FindWidgetSet::Activate (const std::wstring & pane)
{
    FindWidget  & widget = m_widgets[pane];



    if (widget.box == nullptr)
    {
        Create (pane, widget);
    }

    m_activePane = pane;
    m_active     = &widget;
    return widget;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::Create
//
//  Each control acts on its own pane's widget.
//
////////////////////////////////////////////////////////////////////////////////

void FindWidgetSet::Create (const std::wstring & pane, FindWidget & widget)
{
    DxuiPanel  & parent = m_host.GetControlParent();



    ApplySaved (pane, widget);

    //  The widget floats over its pane's text, so it comes after every
    //  pane's controls: its plate, then what is on the plate.
    widget.plate       = parent.CreateChild<FindWidgetPlate> ();
    widget.chevron     = parent.CreateChild<DxuiButton>      (s_kpszMdl2ChevronRight);
    widget.box         = parent.CreateChild<DxuiTextInput>   ();
    widget.caseButton  = parent.CreateChild<DxuiButton>      (L"Aa");
    widget.wordButton  = parent.CreateChild<WholeWordButton> ();
    widget.regexButton = parent.CreateChild<DxuiButton>      (L".*");
    widget.status      = parent.CreateChild<DxuiLabel>       ();
    widget.prevButton  = parent.CreateChild<DxuiButton>      (s_kpszMdl2Up);
    widget.nextButton  = parent.CreateChild<DxuiButton>      (s_kpszMdl2Down);
    widget.selButton   = parent.CreateChild<DxuiButton>      (s_kpszMdl2AlignLeft);
    widget.closeButton = parent.CreateChild<DxuiButton>      (s_kpszMdl2Cancel);

    ShowOptions (widget);

    //  Icon glyphs at Visual Studio Code's 16 pixels, which read at a glance
    //  where a text face's arrows are thin and small.
    for (DxuiButton * button : { widget.chevron, widget.prevButton, widget.nextButton, widget.selButton, widget.closeButton })
    {
        button->SetGlyphFont (L"Segoe MDL2 Assets", (button == widget.chevron) ? 12.0f : 16.0f);
    }

    widget.box->SetHwnd        (m_host.GetOwnerHwnd());
    widget.box->SetMaxLength   (256);
    widget.box->SetPlaceholder ((pane == DebuggerLayout::kConsole) ? L"Find in the console" : L"Find in the source");

    //  Each option is a toggle, emphasized while on, as Visual Studio Code's
    //  find bar draws them.
    widget.caseButton->SetOnClick  ([this, pane] { FindWidget & w = Activate (pane); SetOptions (!w.matchCase, w.wholeWord, w.isRegex); RefocusBox(); });
    widget.wordButton->SetOnClick  ([this, pane] { FindWidget & w = Activate (pane); SetOptions (w.matchCase, !w.wholeWord, w.isRegex); RefocusBox(); });
    widget.regexButton->SetOnClick ([this, pane] { FindWidget & w = Activate (pane); SetOptions (w.matchCase, w.wholeWord, !w.isRegex); RefocusBox(); });

    //  A button press takes the keys, so each puts them back in the box
    //  and the next Enter searches again.
    widget.prevButton->SetOnClick  ([this, pane] { (void) Activate (pane); Find (false); RefocusBox(); });
    widget.nextButton->SetOnClick  ([this, pane] { (void) Activate (pane); Find (true);  RefocusBox(); });
    widget.closeButton->SetOnClick ([this, pane] { (void) Activate (pane); Close(); });

    widget.selButton->SetOnClick ([this, pane] { FindWidget & w = Activate (pane); SetInSelection (!w.inSelection); RefocusBox(); });

    //  The chevron opens replace in Visual Studio Code. The console and the
    //  source are read-only, so it is there, as the widget has it, but off.
    widget.chevron->SetEnabled (false);

    widget.plate->SetVisible  (false);
    widget.box->SetVisible    (false);
    widget.status->SetVisible (false);

    //  Every match is found and highlighted as the text is typed.
    widget.box->SetOnChange ([this, pane] (const std::wstring &) { (void) Activate (pane); SearchAsTyped(); });

    //  Flat and borderless, as a pane toolbar's buttons are.
    for (DxuiButton * button : { widget.chevron, widget.caseButton, widget.wordButton, widget.regexButton, widget.prevButton, widget.nextButton, widget.selButton, widget.closeButton })
    {
        button->SetVariant (DxuiButton::Variant::Toolbar);
        button->SetVisible (false);
    }

    m_window.KeepOverlaysLast();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::ApplySaved
//
////////////////////////////////////////////////////////////////////////////////

void FindWidgetSet::ApplySaved (const std::wstring & pane, FindWidget & widget) const
{
    auto  saved = m_saved.find (pane);



    if (saved == m_saved.end())
    {
        return;
    }

    widget.matchCase = saved->second.matchCase;
    widget.wholeWord = saved->second.wholeWord;
    widget.isRegex   = saved->second.isRegex;
    widget.history   = saved->second.history;

    ShowOptions (widget);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::ShowOptions
//
//  The toggles drawn on for the options on, once the widget has them.
//
////////////////////////////////////////////////////////////////////////////////

void FindWidgetSet::ShowOptions (FindWidget & widget)
{
    if (widget.caseButton == nullptr)
    {
        return;
    }

    widget.caseButton->SetEmphasis  (widget.matchCase);
    widget.wordButton->SetEmphasis  (widget.wholeWord);
    widget.regexButton->SetEmphasis (widget.isRegex);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::IsOpenIn
//
////////////////////////////////////////////////////////////////////////////////

bool FindWidgetSet::IsOpenIn (const std::wstring & pane) const
{
    auto  it = m_widgets.find (pane);



    return it != m_widgets.end() && it->second.isOpen;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::GetBoxOf
//
//  Null for a pane find has never opened in.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTextInput * FindWidgetSet::GetBoxOf (const std::wstring & pane) const
{
    auto  it = m_widgets.find (pane);



    return (it != m_widgets.end()) ? it->second.box : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::GetBounds
//
////////////////////////////////////////////////////////////////////////////////

RECT FindWidgetSet::GetBounds() const
{
    return (m_active != nullptr) ? m_active->plate->GetBounds() : RECT {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::GetActiveControls
//
//  The active widget's controls that take a press or the keys; its count
//  takes neither.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IDxuiControl *> FindWidgetSet::GetActiveControls() const
{
    if (m_active == nullptr)
    {
        return {};
    }

    return { m_active->box, m_active->caseButton, m_active->wordButton, m_active->regexButton, m_active->prevButton, m_active->nextButton, m_active->selButton, m_active->closeButton };
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::GetHistory
//
//  Oldest first.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> FindWidgetSet::GetHistory (const std::wstring & pane) const
{
    auto  it = m_widgets.find (pane);



    return (it != m_widgets.end()) ? it->second.history : std::vector<std::wstring>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::GetTarget
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FindWidgetSet::GetTarget() const
{
    std::wstring  pane = m_window.GetFocusPane();



    if (m_window.GetFindView (pane) != nullptr)
    {
        return pane;
    }

    return DebuggerLayout::kConsole;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::SetVisible
//
//  Through each control's own type: a button has a visibility flag of its
//  own beside the base's.
//
////////////////////////////////////////////////////////////////////////////////

void FindWidgetSet::SetVisible (FindWidget & widget, bool shown)
{
    widget.plate->SetVisible  (shown);
    widget.box->SetVisible    (shown);
    widget.status->SetVisible (shown);

    for (DxuiButton * button : { widget.chevron, widget.caseButton, widget.wordButton, widget.regexButton, widget.prevButton, widget.nextButton, widget.selButton, widget.closeButton })
    {
        button->SetVisible (shown);
    }

    m_window.UpdateFindLayer();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::Move
//
//  The widget's controls into another window, as the memory bar moves.
//  Focus on one of them does not move with it, so it goes to the command
//  line in the window the control moves out of.
//
////////////////////////////////////////////////////////////////////////////////

void FindWidgetSet::Move (FindWidget & widget, DxuiWindow * to)
{
    DxuiWindow  * from = (widget.host != nullptr) ? widget.host : &m_window.GetMainWindow();



    if (to == from)
    {
        return;
    }

    //  The plate goes first, so what is on it paints over it.
    for (IDxuiControl * control : std::initializer_list<IDxuiControl *> { widget.plate, widget.chevron, widget.box, widget.caseButton, widget.wordButton, widget.regexButton,
                                                                          widget.prevButton, widget.nextButton, widget.selButton, widget.closeButton, widget.status })
    {
        m_window.ReleaseFocusOf (control);

        std::unique_ptr<IDxuiControl>  owned = from->DetachChild (control);

        if (owned != nullptr)
        {
            (void) to->AttachChild (std::move (owned));
        }
    }

    widget.host = to;

    //  The site's strips and drop zones paint over the panes, so it is the
    //  main window's last child.
    if (to == &m_window.GetMainWindow())
    {
        m_window.KeepDockLast();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::Place
//
//  Each pane's widget is placed over its own pane, open or not.
//
////////////////////////////////////////////////////////////////////////////////

void FindWidgetSet::Place()
{
    for (auto & [pane, widget] : m_widgets)
    {
        if (widget.box != nullptr)
        {
            PlaceOne (pane, widget);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::PlaceOne
//
//  The widget floats at the top right of its pane's text, clear of the
//  scrollbar, as Visual Studio Code's does: the chevron, the box with the
//  three option toggles inside its right end, the count, the arrows, find
//  in selection and Close. It narrows with a narrow pane, the box narrowing
//  first. With find closed, or the pane out of sight, it hides.
//
////////////////////////////////////////////////////////////////////////////////

void FindWidgetSet::PlaceOne (const std::wstring & pane, FindWidget & widget)
{
    constexpr int           kButtonDip = 24;
    const DxuiDpiScaler   & scaler     = m_host.GetPaneScaler();
    auto                    px         = [&scaler] (int dip) { return scaler.ToPx (dip); };
    DxuiTextView          * view       = m_window.GetFindView (pane);
    bool                    shown      = widget.isOpen && view != nullptr && view->IsVisible();
    int                     button     = px (kButtonDip);
    RECT                    area       = {};
    RECT                    plate      = {};
    int                     top        = 0;
    int                     bottom     = 0;
    int                     x          = 0;
    int                     right      = 0;
    int                     boxRight   = 0;



    SetVisible (widget, shown);

    if (!shown)
    {
        return;
    }

    //  The widget is the pane's, so it goes with the pane into a floating
    //  window.
    Move (widget, m_window.GetFindWindow (pane));

    area         = view->GetBounds();
    plate.right  = area.right - px (kScrollDip);
    plate.left   = (std::max) (area.left + px (4), plate.right - px (kWidthDip));
    plate.top    = area.top;
    plate.bottom = area.top + px (kHeightDip);

    widget.plate->Layout (plate, scaler);

    top    = plate.top    + px (4);
    bottom = plate.bottom - px (4);
    x      = plate.left   + px (2);
    right  = plate.right  - px (4);

    //  From the right: Close, find in selection, the arrows and the count.
    widget.closeButton->Layout (RECT { right - button, top, right, bottom }, scaler);  right -= button + px (2);
    widget.selButton->Layout   (RECT { right - button, top, right, bottom }, scaler);  right -= button;
    widget.nextButton->Layout  (RECT { right - button, top, right, bottom }, scaler);  right -= button;
    widget.prevButton->Layout  (RECT { right - button, top, right, bottom }, scaler);  right -= button + px (4);
    widget.status->Layout      (RECT { right - px (kCountDip), top, right, bottom }, scaler);
    right -= px (kCountDip) + px (4);

    //  From the left: the chevron, then the box, with the toggles inside it.
    widget.chevron->Layout (RECT { x, top, x + px (16), bottom }, scaler);  x += px (16) + px (2);

    boxRight = (std::max) (x + button * 3, right);

    //  The box measures its text to place the caret under a click and to drag
    //  a selection, which requires the window's renderer, made after the box.
    widget.box->SetTextRenderer (m_host.GetPaneTextRenderer());

    widget.box->Layout         (RECT { x, top, boxRight, bottom }, scaler);
    widget.regexButton->Layout (RECT { boxRight - px (2) - button,     top + px (2), boxRight - px (2),              bottom - px (2) }, scaler);
    widget.wordButton->Layout  (RECT { boxRight - px (2) - button * 2, top + px (2), boxRight - px (2) - button,     bottom - px (2) }, scaler);
    widget.caseButton->Layout  (RECT { boxRight - px (2) - button * 3, top + px (2), boxRight - px (2) - button * 2, bottom - px (2) }, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::OnFloatClosed
//
//  Each widget in the closed window goes back to the main window, in the
//  pane it searches.
//
////////////////////////////////////////////////////////////////////////////////

void FindWidgetSet::OnFloatClosed (const DxuiWindow * window)
{
    for (auto & [pane, widget] : m_widgets)
    {
        if (widget.host == window)
        {
            Move (widget, &m_window.GetMainWindow());
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::Open
//
//  The pane comes forward with the keys in the box, its text selected so
//  typing replaces it. A selection in the pane's text on one line becomes
//  the text to find, as it does in Visual Studio. Each pane has its own
//  widget, so one open in another pane stays open.
//
////////////////////////////////////////////////////////////////////////////////

void FindWidgetSet::Open (const std::wstring & pane)
{
    FindWidget         & widget   = Activate (pane);
    DxuiTextView       * view     = m_window.GetFindView (pane);
    DebuggerPaneFrame  * frame    = m_window.GetFindFrame (pane);
    std::wstring         selected = (view != nullptr) ? view->GetSelectionText() : std::wstring();



    widget.box->SetPlaceholder ((pane == DebuggerLayout::kConsole) ? L"Find in the console" : L"Find in the source");

    if (!selected.empty() && selected.find (L'\n') == std::wstring::npos)
    {
        widget.box->SetText (selected);
    }

    widget.isOpen = true;

    m_window.BringPaneForward (pane);

    if (frame != nullptr)
    {
        frame->Relayout();
    }

    Place();
    widget.box->SelectAll();

    //  A floating pane has its own focus.
    m_window.FocusInPane (pane, widget.box);
    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::Close
//
//  The match remains selected in the pane's text; the keys go back to the
//  command line from the console's widget, and to the text from a source
//  document's.
//
////////////////////////////////////////////////////////////////////////////////

void FindWidgetSet::Close()
{
    FindWidget         & widget = *m_active;
    DxuiTextView       * view   = m_window.GetFindView (m_activePane);
    DebuggerPaneFrame  * frame  = m_window.GetFindFrame (m_activePane);



    widget.isOpen = false;
    widget.statusText.clear();
    widget.status->SetText (widget.statusText);

    if (view != nullptr)
    {
        view->ClearFindHighlights();
    }

    if (frame != nullptr)
    {
        frame->Relayout();
    }

    Place();

    m_window.FocusInPane (m_activePane, (m_activePane == DebuggerLayout::kConsole) ? m_window.GetCommandLine() : (IDxuiControl *) view);
    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::RefocusBox
//
//  A button the pointer pressed puts the keys back in the box, so the next
//  Enter searches again; one pressed from the keyboard does not, as Visual
//  Studio Code's do not.
//
////////////////////////////////////////////////////////////////////////////////

void FindWidgetSet::RefocusBox()
{
    if (!m_isKeyClick)
    {
        m_host.FocusControl (m_active->box);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::SetOptions
//
//  With none on, the text is found as a plain substring.
//
////////////////////////////////////////////////////////////////////////////////

void FindWidgetSet::SetOptions (bool matchCase, bool wholeWord, bool isRegex)
{
    m_active->matchCase = matchCase;
    m_active->wholeWord = wholeWord;
    m_active->isRegex   = isRegex;
    Save();

    ShowOptions (*m_active);

    RefindAfterOptionChange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::SetInSelection
//
//  On, the search is limited to what is selected in the pane's text when it
//  is turned on, as Visual Studio Code's is; with nothing selected, the
//  whole text.
//
////////////////////////////////////////////////////////////////////////////////

void FindWidgetSet::SetInSelection (bool on)
{
    DxuiTextView  * view = m_window.GetFindView (m_activePane);



    m_active->inSelection = on;
    m_active->selButton->SetEmphasis (on);

    if (view != nullptr && on)
    {
        view->SetFindScopeToSelection();
    }
    else if (view != nullptr)
    {
        view->ClearFindScope();
    }

    RefindAfterOptionChange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::RecordHistory
//
//  The text just searched for goes to the end of its pane's history, moved
//  there if it was already in it, and the oldest goes once there are more
//  than kHistoryMax.
//
////////////////////////////////////////////////////////////////////////////////

void FindWidgetSet::RecordHistory()
{
    std::vector<std::wstring>  & history = m_active->history;
    const std::wstring         & text    = m_active->box->GetText();



    if (text.empty())
    {
        return;
    }

    std::erase (history, text);
    history.push_back (text);

    if (history.size() > kHistoryMax)
    {
        history.erase (history.begin());
    }

    m_active->historyAt = -1;
    Save();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::StepHistory
//
//  Up and Down in the box, as in Visual Studio Code's: a step back (-1) puts
//  the text searched for before in the box, a step forward (+1) the one
//  after, and stepping forward past the newest empties the box. False with
//  no history to step through.
//
////////////////////////////////////////////////////////////////////////////////

bool FindWidgetSet::StepHistory (int step)
{
    const std::vector<std::wstring>  & history = m_active->history;
    int                                count   = (int) history.size();
    int                                at      = (m_active->historyAt < 0) ? count : m_active->historyAt;



    if (count == 0)
    {
        return false;
    }

    at = std::clamp (at + step, 0, count);

    m_active->historyAt = (at == count) ? -1 : at;
    m_active->box->SetText ((at == count) ? std::wstring() : history[(size_t) at]);
    m_active->box->SelectAll();

    m_host.Repaint();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::Find
//
//  With nothing to find yet, this opens the widget for it. Otherwise the
//  next or previous match is selected in the active pane, whether or not
//  the widget is open, as F3 finds again in Visual Studio after its find is
//  closed.
//
////////////////////////////////////////////////////////////////////////////////

void FindWidgetSet::Find (bool forward)
{
    std::wstring              target = GetTarget();
    std::wstring              needle;
    DxuiTextView            * view   = nullptr;
    DxuiTextView::FindResult  result = DxuiTextView::FindResult::NotFound;
    int                       index  = 0;
    int                       count  = 0;



    //  F3 searches the text with the keys, if it can; from elsewhere, the
    //  pane whose widget is open.
    if (m_active == nullptr || (target != m_activePane && (!m_active->isOpen || m_window.GetFocusPane() == target)))
    {
        (void) Activate (target);
    }

    needle = m_active->box->GetText();

    if (needle.empty())
    {
        Open (GetTarget());
        return;
    }

    RecordHistory();

    view = m_window.GetFindView (m_activePane);
    view->SetFindColors (m_host.GetPcRowArgb(), m_host.GetPcMarkerArgb());

    result               = view->SelectMatch (needle, m_active->matchCase, m_active->wholeWord, m_active->isRegex, forward, index, count);
    m_active->statusText = GetStatusText (result, index, count);
    m_active->status->SetText (m_active->statusText);

    //  With the widget closed the marks have no count beside them, so they
    //  are cleared.
    if (!m_active->isOpen)
    {
        view->ClearFindHighlights();
    }

    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::SearchAsTyped
//
//  Each change to the box searches again, as Visual Studio Code's does:
//  every match is highlighted and the first on screen selected, or, with
//  none on screen, the first after it, scrolled into view. An empty box
//  clears the marks and the count.
//
////////////////////////////////////////////////////////////////////////////////

void FindWidgetSet::SearchAsTyped()
{
    DxuiTextView              * view   = m_window.GetFindView (m_activePane);
    std::wstring                needle = m_active->box->GetText();
    DxuiTextView::FindResult    result = DxuiTextView::FindResult::NotFound;
    int                         index  = 0;
    int                         count  = 0;



    if (view == nullptr)
    {
        return;
    }

    //  The marks are the PC row's colors, the one the search is at the PC
    //  arrow's yellow.
    view->SetFindColors (m_host.GetPcRowArgb(), m_host.GetPcMarkerArgb());

    if (needle.empty())
    {
        view->ClearFindHighlights();
        m_active->statusText.clear();
    }
    else
    {
        result               = view->SelectMatchInView (needle, m_active->matchCase, m_active->wholeWord, m_active->isRegex, index, count);
        m_active->statusText = GetStatusText (result, index, count);
    }

    m_active->status->SetText (m_active->statusText);
    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::RefindAfterOptionChange
//
//  A changed option changes what matches, so with find open and text in the
//  box the search runs again at once, as it does when the text changes.
//  Otherwise the last count is no longer right and is cleared.
//
////////////////////////////////////////////////////////////////////////////////

void FindWidgetSet::RefindAfterOptionChange()
{
    if (m_active->isOpen && !m_active->box->GetText().empty())
    {
        SearchAsTyped();
        return;
    }

    m_active->statusText.clear();
    m_active->status->SetText (m_active->statusText);

    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::GetStatusText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FindWidgetSet::GetStatusText (DxuiTextView::FindResult result, int index, int count)
{
    if (result == DxuiTextView::FindResult::NotFound || count == 0)
    {
        return L"No results";
    }

    return std::format (L"{} of {}", index, count);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::RouteKey
//
//  The keys act on the widget they are in, whichever pane's it is. While a
//  widget has the keys, Enter finds the next match, Shift+Enter the one
//  before, and Escape closes it. Every other key the box takes -- letters,
//  Space, editing keys -- goes to the box, so typing a search never steps
//  the machine; function keys and chords go to the scheme, which is how F3
//  and Shift+F3 work from here.
//
////////////////////////////////////////////////////////////////////////////////

bool FindWidgetSet::RouteKey (const DxuiKeyEvent & ev, bool & handled)
{
    IDxuiControl  * focused = m_window.GetFindFocus();
    std::wstring    owner   = GetPaneOfControl (focused);
    bool            isDown  = ev.kind == DxuiKeyEventKind::Down;
    FindWidget    * widget  = nullptr;



    if (owner.empty())
    {
        return false;
    }

    widget = &Activate (owner);

    if (!widget->isOpen)
    {
        return false;
    }

    if (isDown && ev.vk == VK_ESCAPE)
    {
        Close();
        handled = true;
        return true;
    }

    //  Space or Enter on one of the widget's buttons presses it and does not
    //  move the focus, so the character the key sends next is not typed into
    //  the box.
    if (isDown && focused != widget->box && (ev.vk == VK_SPACE || ev.vk == VK_RETURN) && !ev.ctrl && !ev.alt)
    {
        m_isKeyClick = true;
        handled      = focused->OnKey (ev);
        m_isKeyClick = false;

        if (handled)
        {
            return true;
        }
    }

    if (focused != widget->box)
    {
        return false;
    }

    //  The box's own editing chords -- select all, the clipboard, undo -- go
    //  to the box ahead of the window's commands.
    if (isDown && DxuiCommandRouter::TranslateKey (ev.vk, ev.ctrl, ev.alt, ev.shift) != DxuiStandardCommand::None)
    {
        (void) widget->box->OnKey (ev);
        handled = true;
        return true;
    }

    if (isDown && ev.vk == VK_RETURN)
    {
        Find (!ev.shift);
        handled = true;
        return true;
    }

    if (ev.kind == DxuiKeyEventKind::Char)
    {
        //  Enter and Escape arrive as characters too, after the key-down
        //  above has acted on them; each character has its key's code.
        handled = (ev.vk == VK_RETURN || ev.vk == VK_ESCAPE) || widget->box->OnKey (ev);
        return true;
    }

    //  Up and Down step through the pane's find history.
    if (isDown && (ev.vk == VK_UP || ev.vk == VK_DOWN) && !ev.ctrl && !ev.alt)
    {
        (void) StepHistory ((ev.vk == VK_UP) ? -1 : 1);
        handled = true;
        return true;
    }

    if (isDown && ev.vk != VK_TAB && DebuggerKeySchemes::DoesBoxKeepKey (ev.vk, ev.ctrl, ev.alt, true, false, false))
    {
        (void) widget->box->OnKey (ev);
        handled = true;
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::OnMouseMove
//
//  Each widget moves with its pane; its box takes the whole move, so a
//  press in it drags a selection.
//
////////////////////////////////////////////////////////////////////////////////

void FindWidgetSet::OnMouseMove (const DxuiMouseEvent & ev, bool isLeftDown)
{
    int  x = ev.positionDip.x;
    int  y = ev.positionDip.y;



    for (const auto & [pane, widget] : m_widgets)
    {
        if (widget.box == nullptr || !m_host.IsRoutable (widget.box))
        {
            continue;
        }

        (void) widget.box->OnMouse (ev);

        for (DxuiButton * button : widget.GetButtons())
        {
            button->SetMouse (x, y, button->HitTest (x, y) && isLeftDown);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::IsOverPlate
//
////////////////////////////////////////////////////////////////////////////////

bool FindWidgetSet::IsOverPlate (POINT clientPx) const
{
    return std::ranges::any_of (m_widgets, [&] (const auto & entry)
    {
        const FindWidgetPlate  * plate = entry.second.plate;

        return plate != nullptr && m_host.IsRoutable (plate) && plate->IsVisible() && DxuiDockSite::Contains (plate->GetBounds(), clientPx);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::GetTip
//
//  What each of the widget's controls does, as every toolbar control's tip
//  has it. The toggles lie inside the box, so the box is checked last.
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * FindWidgetSet::GetTip (POINT clientPx, RECT & anchor) const
{
    for (const auto & [pane, widget] : m_widgets)
    {
        const std::pair<IDxuiControl *, const wchar_t *>  tips[] =
        {
            { widget.caseButton,  L"Find only text in the same case"      },
            { widget.wordButton,  L"Find only whole words"                },
            { widget.regexButton, L"Find by regular expression"           },
            { widget.prevButton,  L"Find the previous match"              },
            { widget.nextButton,  L"Find the next match"                  },
            { widget.selButton,   L"Find in selection"                    },
            { widget.chevron,     L"Replace is not available: this text is read-only" },
            { widget.closeButton, L"Close the find bar"                   },
            { widget.box,         L"Text to find in this pane"             },
        };

        for (const auto & [control, tip] : tips)
        {
            if (control != nullptr && m_host.IsRoutable (control) && control->IsVisible() && DxuiDockSite::Contains (control->GetBounds(), clientPx))
            {
                anchor = control->GetBounds();
                return tip;
            }
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::GetCursor
//
//  The I-beam over a widget's box, and the arrow over its buttons and the
//  rest of it, not the text's I-beam beneath.
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR FindWidgetSet::GetCursor (POINT clientPx) const
{
    for (const auto & [pane, widget] : m_widgets)
    {
        if (widget.plate == nullptr || !m_host.IsRoutable (widget.plate) || !widget.plate->IsVisible() || !DxuiDockSite::Contains (widget.plate->GetBounds(), clientPx))
        {
            continue;
        }

        bool  onButton = std::ranges::any_of (std::initializer_list<const DxuiButton *> { widget.chevron, widget.caseButton, widget.wordButton, widget.regexButton, widget.prevButton, widget.nextButton, widget.selButton, widget.closeButton },
                                              [clientPx] (const DxuiButton * button) { return button->IsVisible() && DxuiDockSite::Contains (button->GetBounds(), clientPx); });

        return (!onButton && DxuiDockSite::Contains (widget.box->GetBounds(), clientPx)) ? IDC_IBEAM : IDC_ARROW;
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::GetPaneOfControl
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FindWidgetSet::GetPaneOfControl (const IDxuiControl * control) const
{
    if (control == nullptr)
    {
        return {};
    }

    for (const auto & [pane, widget] : m_widgets)
    {
        std::vector<IDxuiControl *>  parts = widget.GetControls();

        if (std::ranges::find (parts, control) != parts.end())
        {
            return pane;
        }
    }

    return {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::GetPressTargets
//
//  Every pane's widget, open or not, its buttons before its box: the option
//  toggles lie inside the box, and a press on one is the toggle's.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IDxuiControl *> FindWidgetSet::GetPressTargets() const
{
    std::vector<IDxuiControl *>  targets;



    for (const auto & [pane, widget] : m_widgets)
    {
        if (widget.box != nullptr)
        {
            targets.insert (targets.end(), { widget.caseButton, widget.wordButton, widget.regexButton, widget.prevButton, widget.nextButton, widget.selButton, widget.closeButton, widget.box });
        }
    }

    return targets;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::Overlaps
//
//  Whether a visible widget's plate is under the point or overlaps the rect.
//
////////////////////////////////////////////////////////////////////////////////

bool FindWidgetSet::Overlaps (POINT clientPx, const RECT & rect) const
{
    for (const auto & [pane, widget] : m_widgets)
    {
        RECT  both  = {};
        RECT  plate = (widget.plate != nullptr) ? widget.plate->GetBounds() : RECT {};

        if (widget.plate != nullptr && m_host.IsRoutable (widget.plate) && widget.plate->IsVisible() &&
            (DxuiDockSite::Contains (plate, clientPx) || IntersectRect (&both, &plate, &rect) != FALSE))
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::GetLayerControls
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IDxuiControl *> FindWidgetSet::GetLayerControls (const std::wstring & slidPane) const
{
    std::vector<IDxuiControl *>  layer;



    for (const auto & [pane, widget] : m_widgets)
    {
        if (widget.plate == nullptr || !widget.plate->IsVisible() || (!slidPane.empty() && pane != slidPane))
        {
            continue;
        }

        std::ranges::copy (widget.GetControls(), std::back_inserter (layer));
    }

    return layer;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::LoadSaved
//
//  A line per pane: its id, the options it had on, and its history, split
//  by unit separators.
//
////////////////////////////////////////////////////////////////////////////////

void FindWidgetSet::LoadSaved (const std::wstring & text)
{
    std::wistringstream  finds (text);
    std::wstring         line;



    m_saved.clear();

    while (std::getline (finds, line))
    {
        std::vector<std::wstring>  parts;
        size_t                     at = 0;
        Saved                      saved;

        for (size_t next = line.find (L'\x1F'); ; next = line.find (L'\x1F', at))
        {
            parts.push_back (line.substr (at, (next == std::wstring::npos) ? std::wstring::npos : next - at));

            if (next == std::wstring::npos)
            {
                break;
            }

            at = next + 1;
        }

        if (parts.size() < 2 || parts[0].empty())
        {
            continue;
        }

        saved.matchCase = parts[1].find (L'C') != std::wstring::npos;
        saved.wholeWord = parts[1].find (L'W') != std::wstring::npos;
        saved.isRegex   = parts[1].find (L'R') != std::wstring::npos;
        saved.history.assign (parts.begin() + 2, parts.end());
        std::erase (saved.history, std::wstring());

        m_saved[parts[0]] = std::move (saved);
    }

    for (auto & [pane, widget] : m_widgets)
    {
        ApplySaved (pane, widget);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet::Save
//
//  Every pane's, with those a project brought back for widgets not made
//  yet.
//
////////////////////////////////////////////////////////////////////////////////

void FindWidgetSet::Save()
{
    std::map<std::wstring, Saved>  all  = m_saved;
    IDebuggerWindowHost          * host = m_host.GetWindowHost();
    std::wstring                   text;



    for (const auto & [pane, widget] : m_widgets)
    {
        all[pane] = Saved { widget.matchCase, widget.wholeWord, widget.isRegex, widget.history };
    }

    for (const auto & [pane, saved] : all)
    {
        text += pane + L"\x1F" + (saved.matchCase ? L"C" : L"") + (saved.wholeWord ? L"W" : L"") + (saved.isRegex ? L"R" : L"");

        for (const std::wstring & each : saved.history)
        {
            text += L"\x1F" + each;
        }

        text += L"\n";
    }

    if (host != nullptr)
    {
        host->SetDebuggerWindowState ("find", SourcePathList::WideToUtf8 (text));
    }
}
