#pragma once

#include "Pch.h"

#include "CassoExplorer/Window/Panes/Preview/PreviewView.h"
#include "Seams/Win32ShellPreview.h"

struct PreviewContent;





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewShellView
//
//  Windows' preview of a host file, from the file type's preview handler,
//  hosted over the pane. A handler's thumbnail is drawn in the picture view.
//
////////////////////////////////////////////////////////////////////////////////

class PreviewShellView : public PreviewView
{
public:
    PreviewShellView (ExplorerPreviewManager & manager, ExplorerContext & context);

    static constexpr UINT  kShellPreviewTimeoutMs = 5000;   // how long a shell preview has before the bytes replace it

    void  Create    () override {}
    void  Configure () override;
    void  Layout    (const RECT & content) override;
    void  Hide      () override;
    void  Update    (const PreviewContent & preview, const RECT & content);
    void  ShowBytes ();
    bool  IsPicture () const                                 { return m_isPicture; }

    void  HandleResult        ();
    bool  HandleHelperMessage (UINT msg, WPARAM wParam, LPARAM lParam) { return m_shellPreview.OnHelperMessage (msg, wParam, lParam); }

private:
    void  Unload ();

    Win32ShellPreview  m_shellPreview;
    std::wstring       m_path;
    RECT               m_pictureRect  = {};
    bool               m_isPicture    = false;   // the shell's preview is a thumbnail in the picture view
};
