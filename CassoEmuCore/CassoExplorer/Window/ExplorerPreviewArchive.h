#pragma once

#include "Pch.h"

class CassoExplorerWindow;
class ExplorerPreview;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewArchive
//
//  An archive's files in the Preview pane, handled as the file list's are: a
//  press that moves drags them out for the shell to extract where they are
//  dropped, a right-click opens the shell's menu for them, a double-click
//  opens one, and they copy, cut and delete as files. The preview list has
//  the selection.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerPreviewArchive
{
public:
    ExplorerPreviewArchive (ExplorerPreview & preview, CassoExplorerWindow & window);

    //  True when the event is taken here.
    bool  OnMouse         (const DxuiMouseEvent & ev, POINT point);
    void  OpenRow         (int row);
    void  CopySelection   (bool cut);
    void  DeleteSelection ();

private:
    //  The selected files, as the shell's ids for them.
    std::vector<std::wstring>  GetSelectedIds  () const;
    void                       ShowContextMenu (int x, int y);

    ExplorerPreview      & m_preview;
    CassoExplorerWindow  & m_window;

    bool   m_dragArmed = false;   // a left press on a file, which may become a drag
    bool   m_menuArmed = false;   // a right press on one, whose menu opens on release
    POINT  m_pressAt   = {};
};