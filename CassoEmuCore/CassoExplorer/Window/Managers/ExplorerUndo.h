#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/Window/Managers/ExplorerUndoStep.h"

struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUndo
//
//  The file operations Ctrl+Z can undo, newest last, and undoing the newest.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerUndo
{
public:
    explicit ExplorerUndo (ExplorerContext & context);

    static constexpr int  kRestoreRetries = 5;

    void                       Push             (ExplorerUndoStep step);
    void                       PushPut          (const CassoExplorerActions::Outcome & outcome, const std::wstring & image, const std::string & inner);
    void                       UndoLast         ();

    //  A file a step made, replaced since by another: its steps undo the
    //  replacement instead.
    void                       ReplacePath      (const std::wstring & from, const std::wstring & to)  { ReplacePathIn (m_undo, from, to); }
    static void                ReplacePathIn    (std::vector<ExplorerUndoStep> & ioSteps, const std::wstring & from, const std::wstring & to);

    bool                       CanUndo          () const { return !m_undo.empty(); }
    const ExplorerUndoStep   & GetLast          () const { return m_undo.back(); }
    static const wchar_t     * GetLabel         (ExplorerUndoStep::Kind kind);

    //  After a Recycle Bin restore, the folder is listed again on a timer
    //  until the files appear; true when the timer stops.
    bool                       CountDownRestore () { return --m_restoreTicks <= 0; }

private:
    HRESULT  UndoRecycle    (const ExplorerUndoStep & step);
    HRESULT  UndoHostRename (const ExplorerUndoStep & step);
    HRESULT  UndoHostCreate (const ExplorerUndoStep & step);
    HRESULT  UndoHostMove   (const ExplorerUndoStep & step);
    bool     UndoInImage    (const ExplorerUndoStep & step);

    ExplorerContext                & m_context;
    std::vector<ExplorerUndoStep>    m_undo;   // newest last
    int                              m_restoreTicks = 0;
};