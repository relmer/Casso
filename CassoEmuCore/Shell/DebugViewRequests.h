#pragma once

class DebuggerViewState;
class HeatMapOptions;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewRequests
//
//  The debugger window's changes to what its panes show, carried out on the
//  CPU thread: the code views, the memory windows, the trace and the heat
//  map. Each takes the view state lock a build reads under, and a change
//  calls the function given, which marks the view for building again.
//
////////////////////////////////////////////////////////////////////////////////

class DebugViewRequests
{
public:
    DebugViewRequests (DebuggerViewState & state, std::mutex & lock, std::function<void()> onChanged);

    void  SetFollowView       (int view);
    void  CloseCodeView       (int view);
    void  OpenCodeViewAt      (int view, Word top);
    void  SetCodeLines        (int lines, int view);
    void  SetCodeShown        (int lines, int view);
    void  ShowCode            (int view, std::optional<Word> address);
    void  ScrollCode          (int lines, int view);
    void  HoldCode            (bool isHeld, int view);
    void  SelectCode          (int view, Word address);
    void  SetMemoryWindow     (int id, std::optional<Word> address);
    void  SetTraceView        (std::optional<uint64_t> first);
    void  SetTraceShown       (bool shown);
    void  SetHeatMapShown     (bool shown);
    void  SetHeatColumnsShown (bool shown);
    void  SetHeatMapOptions   (const HeatMapOptions & options);
    void  SetHeatMapHover     (std::optional<Word> address);
    void  SetBasicShown       (bool isVisible);

private:
    static bool  IsCodeView   (int view);

    DebuggerViewState      & m_state;
    std::mutex             & m_lock;
    std::function<void()>    m_onChanged;
};
