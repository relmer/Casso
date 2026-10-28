#pragma once

#include "Pch.h"

struct ExplorerContext;
class PreviewListingView;





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewArchiveView
//
//  An archive's files in the Preview pane, handled as the file list's are: a
//  press that moves drags them out for the shell to extract where they are
//  dropped, a right-click opens the shell's menu for them, a double-click
//  opens one, and they copy, cut and delete as files. The preview list has
//  the selection.
//
////////////////////////////////////////////////////////////////////////////////

class PreviewArchiveView
{
public:
    PreviewArchiveView (PreviewListingView & listing, ExplorerContext & context);

    //  True when the event is taken here.
    bool  HandleMouse         (const DxuiMouseEvent & ev, POINT point);
    void  OpenRow         (int row);
    void  CopySelection   (bool cut);
    void  DeleteSelection ();

private:
    //  The selected files, as the shell's ids for them.
    std::vector<std::wstring>  GetSelectedIds  () const;
    void                       ShowContextMenu (int x, int y);

    PreviewListingView  & m_listing;
    ExplorerContext     & m_context;

    bool   m_dragArmed = false;   // a left press on a file, which may become a drag
    bool   m_menuArmed = false;   // a right press on one, whose menu opens on release
    POINT  m_pressAt   = {};
};