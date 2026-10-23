#pragma once

#include "Ui/Debugger/Panes/IDebuggerPane.h"

class DebuggerPaneFrame;
class FindWidgetPlate;





////////////////////////////////////////////////////////////////////////////////
//
//  IFindWidgetHost
//
//  The window's parts the find widgets use beyond a pane's: the text and
//  frame of a searchable pane, the window a pane is in, the focus of the
//  window and of its floating windows, the dock, and the top layer the
//  widgets paint in.
//
////////////////////////////////////////////////////////////////////////////////

class IFindWidgetHost
{
public:
    virtual ~IFindWidgetHost() = default;

    //  The text a pane's widget searches and the frame it lies in; none for
    //  a pane that is not text to search.
    virtual DxuiTextView         * GetFindView      (const std::wstring & pane) const = 0;
    virtual DebuggerPaneFrame    * GetFindFrame     (const std::wstring & pane) const = 0;

    //  The window a pane is in now, and the main window.
    virtual DxuiWindow           * GetFindWindow    (const std::wstring & pane) = 0;
    virtual DxuiWindow           & GetMainWindow    () = 0;

    //  A control leaving a window: the focus on it goes to the command line
    //  there, and a floating window's focus on it is cleared.
    virtual void                   ReleaseFocusOf   (IDxuiControl * control) = 0;

    //  The dock site, then the boxes laid over cells, as the main window's
    //  last children, so they paint over the panes and the widgets.
    virtual void                   KeepDockLast     () = 0;
    virtual void                   KeepOverlaysLast () = 0;

    //  The focus put on a control in the window a pane is in; the control
    //  with the focus in the window the event came from; and the pane it is
    //  in.
    virtual void                   FocusInPane      (const std::wstring & pane, IDxuiControl * control) = 0;
    virtual IDxuiControl         * GetFindFocus     () const = 0;
    virtual std::wstring           GetFocusPane     () const = 0;

    //  The console's command line, where the keys go when its widget closes.
    virtual IDxuiControl         * GetCommandLine   () const = 0;

    //  A pane brought forward, even from an edge.
    virtual void                   BringPaneForward (const std::wstring & pane) = 0;

    //  The controls painted over the page, the widgets open among them,
    //  gathered again.
    virtual void                   UpdateFindLayer  () = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidget
//
//  A searchable pane's own find widget, as Visual Studio Code's: a box for
//  the text, whether case matters, whether only whole words count, whether
//  it is a regular expression, a button each way, find in selection, Close,
//  and a line with what the last search found. It floats over its pane's
//  text, so it goes with the pane into a floating window.
//
////////////////////////////////////////////////////////////////////////////////

struct FindWidget
{
    FindWidgetPlate          * plate       = nullptr;
    DxuiButton               * chevron     = nullptr;
    DxuiTextInput            * box         = nullptr;
    DxuiButton               * caseButton  = nullptr;
    DxuiButton               * wordButton  = nullptr;
    DxuiButton               * regexButton = nullptr;
    DxuiLabel                * status      = nullptr;
    DxuiButton               * prevButton  = nullptr;
    DxuiButton               * nextButton  = nullptr;
    DxuiButton               * selButton   = nullptr;
    DxuiButton               * closeButton = nullptr;
    DxuiWindow               * host        = nullptr;
    bool                       isOpen      = false;
    bool                       matchCase   = false;
    bool                       wholeWord   = false;
    bool                       isRegex     = false;
    bool                       inSelection = false;
    int                        historyAt   = -1;
    std::wstring               statusText;
    std::vector<std::wstring>  history;

    //  Every control, the plate first; and those that take a press or the
    //  keys, the box last since the toggles lie inside it.
    std::vector<IDxuiControl *>  GetControls () const;
    std::vector<DxuiButton *>    GetButtons  () const;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FindWidgetSet
//
//  The find widgets of the searchable panes -- the console and the source
//  documents -- each made the first time find opens in its pane, and the
//  one the keys and the search act on. Each widget has its own text,
//  options and history, so two panes can each have theirs open. The options
//  and the history of each pane are saved with the debug project.
//
////////////////////////////////////////////////////////////////////////////////

class FindWidgetSet
{
public:
    FindWidgetSet (IDebuggerPaneHost & host, IFindWidgetHost & window);

    FindWidgetSet (const FindWidgetSet &)             = delete;
    FindWidgetSet & operator= (const FindWidgetSet &) = delete;

    //  The pane the keys and the search act on, and its widget, which is
    //  made first if the pane has none yet.
    const std::wstring  & GetActivePane  () const { return m_activePane; }
    FindWidget          & Activate       (const std::wstring & pane);

    //  The active widget's state, for the window's tests.
    bool                 IsOpen         () const { return m_active != nullptr && m_active->isOpen; }
    bool                 IsOpenIn       (const std::wstring & pane) const;
    DxuiTextInput      * GetBox         () const { return (m_active != nullptr) ? m_active->box : nullptr; }
    DxuiTextInput      * GetBoxOf       (const std::wstring & pane) const;
    DxuiButton         * GetWordButton  () const { return (m_active != nullptr) ? m_active->wordButton : nullptr; }
    std::wstring         GetStatus      () const { return (m_active != nullptr) ? m_active->statusText : std::wstring(); }
    bool                 IsInSelection  () const { return m_active != nullptr && m_active->inSelection; }
    RECT                 GetBounds      () const;
    std::vector<IDxuiControl *>  GetActiveControls () const;
    std::vector<std::wstring>  GetHistory (const std::wstring & pane) const;

    //  The pane find opens in: the one with the keys when it is text to
    //  search, and the console otherwise.
    std::wstring         GetTarget      () const;

    //  Opens a pane's widget, as Ctrl+F or the search button on its toolbar
    //  does; closes the active one; and the next or previous match, as F3
    //  and Shift+F3 find.
    void                 Open           (const std::wstring & pane);
    void                 Close          ();
    void                 Find           (bool forward);

    //  The options, each emphasized while on; find in selection; the keys
    //  back to the box after a button the pointer pressed; and a step
    //  through the history.
    void                 SetOptions     (bool matchCase, bool wholeWord, bool isRegex);
    void                 SetInSelection (bool on);
    void                 RefocusBox     ();
    bool                 StepHistory    (int step);

    //  The widgets placed over their panes' text, and those in a floating
    //  window that closed moved back to the main window.
    void                 Place          ();
    void                 OnFloatClosed  (const DxuiWindow * window);

    //  The keys for a widget with the focus, true when acted on here; a move
    //  of the pointer over the widgets; and whether a widget's plate is under
    //  a point, which a press on it takes from the text beneath.
    bool                 RouteKey       (const DxuiKeyEvent & ev, bool & handled);
    void                 OnMouseMove    (const DxuiMouseEvent & ev, bool isLeftDown);
    bool                 IsOverPlate    (POINT clientPx) const;

    //  The tip of a widget's control under a point; the cursor over a
    //  widget, none where no widget is; and the pane whose widget has a
    //  control, empty for none.
    const wchar_t      * GetTip         (POINT clientPx, RECT & anchor) const;
    LPCWSTR              GetCursor      (POINT clientPx) const;
    std::wstring         GetPaneOfControl (const IDxuiControl * control) const;

    //  Every widget's controls that take a press, its buttons before its
    //  box; whether a visible widget overlaps a rect or is under a point; and the
    //  controls of each open widget to paint over the page, only the slid
    //  pane's while a pane is slid out.
    std::vector<IDxuiControl *>  GetPressTargets () const;
    bool                 Overlaps       (POINT clientPx, const RECT & rect) const;
    std::vector<IDxuiControl *>  GetLayerControls (const std::wstring & slidPane) const;

    //  The options and histories saved with the project, read back and
    //  given to the widgets made; and every pane's saved.
    void                 LoadSaved      (const std::wstring & text);
    void                 Save           ();

private:
    struct Saved
    {
        bool                       matchCase = false;
        bool                       wholeWord = false;
        bool                       isRegex   = false;
        std::vector<std::wstring>  history;
    };

    static constexpr size_t  kHistoryMax      = 20;
    static constexpr int     kWidthDip        = 460;
    static constexpr int     kHeightDip       = 34;
    static constexpr int     kScrollDip       = 18;
    static constexpr int     kCountDip        = 74;

    void                 Create         (const std::wstring & pane, FindWidget & widget);
    void                 ApplySaved     (const std::wstring & pane, FindWidget & widget) const;
    static void          ShowOptions    (FindWidget & widget);
    void                 SetVisible     (FindWidget & widget, bool shown);
    void                 Move           (FindWidget & widget, DxuiWindow * to);
    void                 PlaceOne       (const std::wstring & pane, FindWidget & widget);
    void                 RecordHistory  ();
    void                 SearchAsTyped  ();
    void                 RefindAfterOptionChange ();

    static std::wstring  GetStatusText  (DxuiTextView::FindResult result, int index, int count);

    IDebuggerPaneHost                     & m_host;
    IFindWidgetHost                       & m_window;
    std::map<std::wstring, FindWidget>      m_widgets;
    std::map<std::wstring, Saved>           m_saved;
    std::wstring                            m_activePane;
    FindWidget                            * m_active     = nullptr;
    bool                                    m_isKeyClick = false;
};
