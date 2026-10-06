#pragma once

#include "Pch.h"

#include "Ui/Dialogs/ReleaseNotesView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogContent
//
//  The body of the update dialog, top to bottom: the versions line, the
//  release date, the release notes in a scrolling area that takes the
//  height left over, a status line (developer text, progress, or a
//  failure), and a link to the release page. Laid out in physical pixels.
//
//  The notes' real height is known only after they are painted, so the
//  dialog calls SyncNotesHeight from its tick and lays the scroll area out
//  again when the height changed.
//
////////////////////////////////////////////////////////////////////////////////

class UpdateDialogContent : public DxuiPanel
{
public:
    using OpenUrlFn = std::function<void (const std::wstring & url)>;

    UpdateDialogContent();

    UpdateDialogContent             (const UpdateDialogContent &) = delete;
    UpdateDialogContent & operator= (const UpdateDialogContent &) = delete;

    void  SetHeader        (const std::wstring & header, const std::wstring & dateLine);
    void  SetNotesLines    (std::vector<FormattedLine> lines);
    void  SetNotesMessage  (const std::wstring & message);
    void  SetStatus        (const std::wstring & status, bool isError);
    void  SetPageUrl       (const std::wstring & url);
    void  SetOnOpenUrl     (OpenUrlFn fn) { m_onOpenUrl = std::move (fn); }

    bool  SyncNotesHeight  ();

    void  Layout           (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;

private:
    void  LayoutNotes      ();

    DxuiLabel         m_header;
    DxuiLabel         m_date;
    DxuiScrollPanel   m_scroll;
    ReleaseNotesView  m_notes;
    DxuiLabel         m_status;
    DxuiButton        m_pageLink;
    OpenUrlFn         m_onOpenUrl;
    std::wstring      m_pageUrl;
    DxuiDpiScaler     m_scaler;
    RECT              m_notesViewportPx = {};
    int               m_placedHeightPx  = 0;
    bool              m_isLaidOut       = false;
};
