#pragma once

#include "Pch.h"

struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerRename
//
//  Rename in place, as Explorer's F2 starts it: an edit box laid over a list
//  row's name or a tree node's label, and the rename it commits, with an undo.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerRename
{
public:
    explicit ExplorerRename (ExplorerContext & context);

    static constexpr int  kMaxCatalogName = 30;

    void  Create            ();
    bool  IsRenaming        () const { return m_renameRow >= 0 || !m_renameTreeId.empty(); }
    int   GetRenameRow      () const { return m_renameRow; }
    void  BeginRename       ();
    void  BeginTreeRename   (const std::wstring & id);
    void  EndRename         (bool commit);
    bool  HandleRenameMouse (const DxuiMouseEvent & ev);
    void  HandleRenameKey   (const DxuiKeyEvent & ev);

private:
    ExplorerContext  & m_context;

    //  The box over the name, and the list row being renamed, or -1, or the
    //  tree node.
    DxuiTextInput    * m_renameBox    = nullptr;
    int                m_renameRow    = -1;
    std::wstring       m_renameHiddenExtension;   // a shortcut's .lnk, left out of the box
    std::wstring       m_renameTreeId;
};