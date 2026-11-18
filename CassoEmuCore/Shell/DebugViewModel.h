#pragma once

#include "Ui/Debugger/DebuggerViewState.h"

class HeatMapOptions;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewModel
//
//  The debugger window's view state on the CPU thread, the lock every change
//  to it and every build that reads it are made under, and whether it
//  changed since the views were last built. The window's changes to its code
//  views, memory windows, trace and heat map are made here, each under the
//  lock.
//
////////////////////////////////////////////////////////////////////////////////

class DebugViewModel
{
public:
    DebuggerViewState  & GetState    ()       { return m_state; }
    std::mutex         & GetLock     ()       { return m_lock; }
    void                 MarkDirty   ()       { m_isDirty = true; }
    void                 ClearDirty  ()       { m_isDirty = false; }
    bool                 IsDirty     () const { return m_isDirty; }

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

    DebuggerViewState  m_state;
    std::mutex         m_lock;
    bool               m_isDirty = true;
};
