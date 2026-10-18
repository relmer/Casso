#pragma once

#include "Debugger/HeatMapRangeSets.h"
#include "Debugger/HeatMapSymbols.h"
#include "Ui/Debugger/PaneBarSlot.h"
#include "Ui/Debugger/Panes/IDebuggerPane.h"

class HeatMapPane;
class HeatMapRangeBarCommands;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatRangesPane
//
//  The heat map's sets of ranges and the pane that edits them: every set and
//  the set on the map, the set the pane edits, the symbols they read
//  against, and each list row's range in the set. The pane is its bar over a
//  line with the reason an edit failed, over its list, and an edit opens
//  a box over a cell or over the set's name on the bar. It is closed until
//  first opened; its setting, written then, opens it from then on.
//
////////////////////////////////////////////////////////////////////////////////

class HeatRangesPane : public IDebuggerPane
{
public:
    HeatRangesPane (IDebuggerPaneHost & host, HeatMapPane & heatMap);
    ~HeatRangesPane() override;

    const HeatMapRangeSets &   GetSets          () const { return m_sets; }
    const HeatMapSymbols &     GetSymbols       () const;
    const std::string &        GetEditedSetName () const { return m_editedSet; }
    DxuiEditableGrid *         GetList          () const { return m_list; }
    DxuiToolbar *              GetBar           () const { return m_bar.GetBar(); }
    DxuiTextInput *            GetEditor        () const;
    std::wstring               GetError         () const;
    bool                       IsEditing        () const;

    //  The set on the map and the set the pane edits, the bar's entries,
    //  an edit in place over a cell or the set's name, and a range's check.
    void                       ShowRangeSet     (const std::string & name);
    void                       EditRangeSet     (const std::string & name);
    void                       RunBarEntry      (int id);
    bool                       IsBarEnabled     (int id) const;
    void                       BeginEdit        (int row, HeatMapRangeSets::Field field);
    void                       EndEdit          (bool commit);
    void                       ToggleRange      (int row, bool included);
    void                       ApplyRanges      ();
    void                       SetMenus         ();

    //  Edit ranges on the heat map's strip.
    void                       OpenForShownSet  ();

    const std::vector<std::shared_ptr<DxuiCommand>> &  GetRangeSetCommands () const { return m_rangeSetCommands; }

    std::wstring                 GetId             () const override;
    std::wstring                 GetTitle          () const override { return L"Heat map ranges"; }
    void                         CreateControls    () override;
    void                         CreateOverlays    () override;
    void                         Configure         () override;
    void                         AddToDock         (DxuiDockSite & dock) override;
    void                         OnDocked          () override { ApplyRanges(); }
    bool                         TryGetOpened      (bool & isOpened) const override { isOpened = m_isOpened; return true; }
    void                         Open              () override;
    IDxuiControl               * GetContent        () const override;
    std::vector<IDxuiControl *>  GetControls       () const override;
    std::vector<DxuiListView *>  GetLists          () const override;
    std::vector<DxuiToolbar *>   GetBars           () const override { return { m_bar.GetBar() }; }
    void                         Apply             (const DebuggerViewSnapshot & snapshot) override;
    void                         OnProjectSwitched () override;
    void                         PlaceBar          () override { m_bar.Place(); }
    bool                         RouteEditMouse    (const DxuiMouseEvent & ev) override;
    bool                         RouteBarMouse     (const DxuiMouseEvent & ev) override { return m_bar.RouteMouse (ev); }
    bool                         RouteEditKey      (const DxuiKeyEvent & ev, IDxuiControl * focused) override;
    const wchar_t              * GetBarTip         (POINT clientPx, RECT & anchor) const override { return m_bar.GetTip (clientPx, anchor); }
    bool                         AddMenuItems      (DxuiListView * list, int row, PaneMenuItems & items) override;

private:
    struct Edit
    {
        bool  isNew     = false;
        bool  isSetName = false;
    };

    static constexpr int  kEditMaxChars = 64;
    static constexpr int  kNameDip      = 150;
    static constexpr int  kSpanDip      = 130;
    static constexpr int  kSizeDip      = 70;
    static constexpr int  kErrorDip     = 20;

    void                     LoadSets        ();
    std::wstring             GetBarLabel     (int id) const;
    void                     ApplyRows       ();
    void                     Save            ();
    void                     SetError        (const std::string & error);
    void                     BeginSetNameEdit (bool isNew);
    void                     MoveRange       (int delta);
    void                     MoveRow         (int row, int to);
    bool                     AddRange        (const std::vector<std::wstring> & texts, std::wstring & outError);
    bool                     CommitCell      (int row, int column, const std::wstring & text, std::wstring & outError);
    void                     RemoveRange     (int row);
    bool                     ConfirmDeleteSet ();
    HeatMapRangeSet *        GetEditedSet    ();
    const HeatMapRangeSet *  GetEditedSet    () const;
    std::optional<size_t>    GetSelectedRange () const;

    HeatMapPane                                & m_heatMap;
    PaneBarSlot                                  m_bar;
    HeatMapRangeSets                             m_sets;
    std::string                                  m_editedSet;
    std::shared_ptr<const HeatMapSymbols>        m_symbols;
    std::vector<size_t>                          m_rows;
    DxuiEditableGrid                           * m_list              = nullptr;
    DxuiTextInput                              * m_editor            = nullptr;
    DxuiLabel                                  * m_error             = nullptr;
    std::unique_ptr<DebuggerPaneFrame>           m_frame;
    std::unique_ptr<HeatMapRangeBarCommands>     m_commands;
    std::vector<std::shared_ptr<DxuiCommand>>    m_rangeSetCommands;
    std::vector<std::shared_ptr<DxuiCommand>>    m_editedSetCommands;
    Edit                                         m_edit;
    bool                                         m_isOpened          = false;
};
