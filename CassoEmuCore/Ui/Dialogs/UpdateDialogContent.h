#pragma once

#include "Pch.h"

#include "Ui/Dialogs/ReleaseNotesView.h"
#include "Update/UpdateDialogModel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDialogContent
//
//  The body of the update dialog, top to bottom: the versions sentence with
//  the release date, a tab strip when the notes have both a What's new and
//  a Changelog tab, the selected tab's notes in a scrolling area that takes
//  the height left over (each tab keeps its own scroll position), a status
//  line (developer text, progress, or a
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

    void  SetOpener        (const std::wstring & opener);
    void  SetHeader        (const std::wstring & header);
    void  SetNotes         (const ReleaseNotes & notes);
    void  SelectTab        (NotesTab tab);
    NotesTab  GetSelectedTab () const { return m_selected; }
    bool  IsTabStripShown  () const { return UpdateDialogModel::ShowsTabStrip (m_tabs); }
    void  SetNotesMessage  (const std::wstring & message);
    void  SetStatus        (const std::wstring & status, bool isError);
    void  SetPageUrl       (const std::wstring & url);
    void  SetOnOpenUrl     (OpenUrlFn fn) { m_onOpenUrl = std::move (fn); }

    bool  SyncNotesHeight  ();
    void  SetNotesImage    (const std::string & src, std::shared_ptr<const NotesImage> image);
    std::vector<std::string>  GetImageSources () const;

    void  Layout           (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;

private:
    static constexpr size_t  kTabCount = 2;

    //  One tab's notes, in their own scroll area.
    struct NotesPane
    {
        DxuiScrollPanel   scroll;
        ReleaseNotesView  view;
        int               placedHeightPx = 0;
    };

    void         SetPaneLines     (NotesTab tab, std::vector<FormattedLine> lines);
    void         SetTabs          (std::vector<NotesTab> tabs);
    void         ShowSelectedPane ();
    void         LayoutTabStrip   ();
    void         LayoutNotes      (NotesPane & pane);
    NotesPane &  GetPane          (NotesTab tab) { return m_panes[(size_t) tab]; }

    DxuiLabel                         m_opener;
    DxuiLabel                         m_header;
    DxuiTabStrip                      m_tabStrip;
    std::array<NotesPane, kTabCount>  m_panes;
    std::vector<NotesTab>             m_tabs            = { NotesTab::Changelog };
    NotesTab                          m_selected        = NotesTab::Changelog;
    DxuiLabel                         m_status;
    DxuiButton                        m_pageLink;
    OpenUrlFn                         m_onOpenUrl;
    std::wstring                      m_pageUrl;
    DxuiDpiScaler                     m_scaler;
    RECT                              m_tabStripPx      = {};
    RECT                              m_notesViewportPx = {};
    bool                              m_isLaidOut       = false;
    bool                              m_hasStatus       = false;
};
