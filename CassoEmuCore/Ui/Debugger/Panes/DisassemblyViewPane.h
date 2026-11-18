#pragma once

#include "Ui/Debugger/DisassemblyCells.h"
#include "Ui/Debugger/DisassemblyOptions.h"
#include "Ui/Debugger/PaneBarSlot.h"
#include "Ui/Debugger/Panes/IDebuggerPane.h"

class CodeViewFind;
class DebuggerPaneFrame;
class DisassemblyGroup;
class DxuiInfoBanner;
class HistoryBand;





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyViewPane
//
//  One disassembly view: its toolbar of viewing options over its lines, and
//  for the first view, the history band between them. The machine opens and
//  closes the views after the first, as the snapshot lists them.
//
////////////////////////////////////////////////////////////////////////////////

class DisassemblyViewPane : public IDebuggerPane
{
public:
    DisassemblyViewPane (IDebuggerPaneHost & host, DisassemblyGroup & group, int view);
    ~DisassemblyViewPane() override;

    DxuiListView                   * GetList        () const { return m_list; }
    DxuiToolbar                    * GetBar         () const { return m_bar.GetBar(); }
    HistoryBand                    * GetHistoryBand () const { return m_historyBand; }
    DebuggerPaneFrame              * GetFrame       () const { return m_frame.get(); }

    //  The view's rows, each a code line or a source line above it.
    const std::vector<DisassemblyOptions::Row>  & GetRows () const { return m_rows; }
    void                                          SetRows (std::vector<DisassemblyOptions::Row> rows) { m_rows = std::move (rows); }

    //  Where each row's parts are in its cells, as its source row was filled;
    //  the default for an instruction's row.
    const std::vector<DisassemblyCells::SourceParts>  & GetSourceParts () const { return m_sourceParts; }
    void                                                SetSourceParts (std::vector<DisassemblyCells::SourceParts> parts) { m_sourceParts = std::move (parts); }

    //  The banner over the lines while a loaded file's program isn't in
    //  memory: its text, or empty for none.
    void              SetStaleText  (const std::wstring & text);
    DxuiInfoBanner  * GetBanner     () const { return m_banner; }
    bool              IsBannerShown () const { return m_isBannerShown; }

    //  What the banner reads for the files whose program isn't in memory,
    //  the debug file first when isDebugFileStale is set; empty for none.
    static std::wstring              GetStaleText   (const std::vector<std::string> & names, bool isDebugFileStale);

    const std::wstring               & GetId            () const override;
    std::wstring                       GetTitle         () const override;
    std::wstring                       GetTabTitle      () const override;
    int                                GetStopFocusRank () const override;
    void                               CreateControls   () override;
    void                               Configure        () override;
    bool                               TryGetOpened     (bool & isOpened) const override;
    bool                               RequestOpen      () override;
    bool                               RequestClose     () override;
    bool                               IsFixed          () const override { return m_view == 0; }
    void                               AddToDock        (DxuiDockSite & dock) override;
    IDxuiControl                     * GetContent       () const override;
    IDxuiFindTarget                  * GetFindView      () const override;
    DebuggerPaneFrame                * GetFindFrame     () const override { return m_frame.get(); }
    std::vector<IDxuiControl *>        GetControls      () const override;
    std::vector<DxuiListView *>        GetLists         () const override { return { m_list }; }
    std::vector<DxuiToolbar *>         GetBars          () const override { return { m_bar.GetBar() }; }
    bool                               HasTextLists     () const override { return false; }
    std::optional<PaneColorKeySpec>    GetColorKey      () const override;
    void                               PlaceBar         () override { m_bar.Place(); }
    bool                               RouteBarMouse    (const DxuiMouseEvent & ev) override;
    bool                               AddMenuItems     (DxuiListView * list, int row, PaneMenuItems & items) override;

private:
    std::shared_ptr<const std::vector<DebuggerViewSnapshot::CodeLine>>  GetFindLines () const;
    std::optional<Word>                                                 GetFindStart (bool isTop) const;
    void                                                                SelectFound  (Word address);

    DisassemblyGroup                            & m_group;
    int                                           m_view          = 0;
    std::wstring                                  m_id;
    PaneBarSlot                                   m_bar;
    DxuiListView                                * m_list          = nullptr;
    HistoryBand                                 * m_historyBand   = nullptr;
    DxuiInfoBanner                              * m_banner        = nullptr;
    bool                                          m_isBannerShown = false;
    std::unique_ptr<DebuggerPaneFrame>            m_frame;
    std::unique_ptr<CodeViewFind>                 m_find;
    std::vector<DisassemblyOptions::Row>          m_rows;
    std::vector<DisassemblyCells::SourceParts>    m_sourceParts;
};
