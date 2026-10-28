#pragma once

#include "Pch.h"

#include "Ui/Debugger/Panes/EventLogControllers.h"

class CommLogPane;
class CommSignalGraph;





////////////////////////////////////////////////////////////////////////////////
//
//  CommLogController
//
//  The serial log or the network log: the graph across the top and the
//  data view under it. The bar has a toggle for each of the graph's lanes
//  and Zoom in and Zoom out. The graph covers a span of cycles ending at the
//  newest row, or centered on the row the keyboard is on; the serial graph
//  is of that row's port, or of the newest row's.
//
////////////////////////////////////////////////////////////////////////////////

class CommLogController : public EventLogController
{
public:
    static constexpr uint64_t  kDefaultSpan = 20'000;
    static constexpr uint64_t  kMinSpan     = 500;
    static constexpr uint64_t  kMaxSpan     = 20'000'000;

    CommLogController (IDebuggerPaneHost & host, DebugLogKind kind);
    ~CommLogController() override;

    CommLogPane                 & GetModel       () const { return *m_model; }
    CommSignalGraph             & GetGraph       () const { return *m_graph; }
    uint64_t                      GetSpan        () const { return m_span; }

    void                          CreateControls () override;
    void                          Configure      () override;
    void                          AddToDock      (DxuiDockSite & dock) override;
    std::vector<IDxuiControl *>   GetControls    () const override;
    void                          ApplyLogDeltas (const DebugLogDeltas & deltas, bool isWindowVisible) override;
    bool                          TryGetTip      (POINT clientPx, PaneTip & tip) override;

    void                          Zoom           (bool isIn);
    void                          ShowLane       (size_t lane, bool isShown);

protected:
    std::wstring                  GetModelText   (bool isSelectedOnly) const override;
    std::string                   GetOptionsText () const override;
    void                          SetOptionsText (const std::string & text) override;
    void                          SetModelShown  (bool isShown) override;
    size_t                        GetTextColumn  () const override;
    void                          SetMenus       () override {}

private:
    bool          IsNetwork      () const { return m_kind == DebugLogKind::Network; }
    void          SetBarEntries  ();
    void          UpdateGraph    ();

    std::unique_ptr<CommLogPane>  m_model;
    CommSignalGraph             * m_graph         = nullptr;
    uint64_t                      m_span          = kDefaultSpan;
    int                           m_graphedRow    = -2;          // the keyboard's row when the graph was last built
    size_t                        m_graphedCount  = SIZE_MAX;    // the rows then
    uint64_t                      m_graphedBefore = UINT64_MAX;  // and the position the machine was taken back to
};
