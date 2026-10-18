#pragma once

#include "Ui/Debugger/PaneBarSlot.h"
#include "Ui/Debugger/Panes/IDebuggerPane.h"
#include "Ui/Debugger/Panes/SourceDocuments.h"

class DebuggerPaneFrame;
class SourceDocumentGroup;
class SourcePane;





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentPane
//
//  One source document (FR-054): its toolbar and banner over its text, its
//  tab titled with its file's name. It is visible only while it has a file,
//  which the group assigns.
//
////////////////////////////////////////////////////////////////////////////////

class SourceDocumentPane : public IDebuggerPane
{
public:
    SourceDocumentPane (IDebuggerPaneHost & host, SourceDocumentGroup & group, int slot);
    ~SourceDocumentPane() override;

    SourcePane                     & GetModel       () const { return *m_model; }
    DxuiTextView                   * GetView        () const { return m_view; }
    DxuiToolbar                    * GetBar         () const { return m_bar.GetBar(); }
    DxuiActionBanner               * GetBanner      () const { return m_banner; }
    DebuggerPaneFrame              * GetFrame       () const { return m_frame.get(); }
    bool                             IsShown        () const { return m_isShown; }
    bool                             IsBannerShown  () const { return m_isBannerShown; }

    //  Takes the snapshot with the file, the macro level and the heat the
    //  group has now, and updates the tab; true when the document or its
    //  banner came or went, which the dock lays out again for.
    bool                             ApplyDocument  (const DebuggerViewSnapshot & snapshot, int fileId, int macroLevel, const std::wstring & title);

    //  Whether the text or the banner is under a point while visible.
    bool                             Contains       (POINT atDip) const;

    std::wstring                     GetId          () const override;
    std::wstring                     GetTitle       () const override { return m_title; }
    void                             CreateControls () override;
    void                             Configure      () override;
    bool                             TryGetOpened   (bool & isOpened) const override { isOpened = m_isShown; return true; }
    bool                             IsFixed        () const override { return false; }
    void                             AddToDock      (DxuiDockSite & dock) override;
    IDxuiControl                   * GetContent     () const override;
    std::vector<IDxuiControl *>      GetControls    () const override;
    std::vector<DxuiToolbar *>       GetBars        () const override { return { m_bar.GetBar() }; }
    std::optional<PaneColorKeySpec>  GetColorKey    () const override;
    void                             SetTextZoom    (float zoom) override;
    void                             PlaceBar       () override { m_bar.Place(); }
    bool                             RouteBarMouse  (const DxuiMouseEvent & ev) override { return m_bar.RouteMouse (ev); }
    const wchar_t                  * GetBarTip      (POINT clientPx, RECT & anchor) const override { return m_bar.GetTip (clientPx, anchor); }

private:
    SourceDocumentGroup                 & m_group;
    int                                   m_slot          = 0;
    PaneBarSlot                           m_bar;
    DxuiTextView                        * m_view          = nullptr;
    DxuiActionBanner                    * m_banner        = nullptr;
    std::unique_ptr<SourcePane>           m_model;
    std::unique_ptr<DebuggerPaneFrame>    m_frame;
    bool                                  m_isShown       = false;
    bool                                  m_isBannerShown = false;
    std::wstring                          m_bannerKey;
    std::wstring                          m_title         = L"Source";
};





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentGroup
//
//  What the source documents share: which file each has, the file the PC
//  is in and the macro body level every document follows, the files opened
//  with no debug file record, the documents saved open that wait for their
//  debug file, and the document last used. The group routes the mouse
//  across every document, since a drag in one has the mouse until it ends.
//  Its documents are in the pane list; the group itself is not.
//
////////////////////////////////////////////////////////////////////////////////

class SourceDocumentGroup
{
public:
    explicit SourceDocumentGroup (IDebuggerPaneHost & host);
    ~SourceDocumentGroup();

    SourceDocumentGroup (const SourceDocumentGroup &)             = delete;
    SourceDocumentGroup & operator= (const SourceDocumentGroup &) = delete;

    //  The documents, for the pane list.
    std::vector<std::unique_ptr<IDebuggerPane>>  CreateDocuments ();

    SourceDocumentPane             & GetDocument    (int slot) const { return *m_documents[(size_t) slot]; }
    bool                             IsOpen         (int slot) const { return m_files.IsOpen (slot); }
    void                             SetActive      (int slot) { m_activeSlot = slot; }

    //  The documents take the snapshot, and the dock lays out again when a
    //  document or its banner comes or goes, or a banner's text or actions
    //  change: a new action has no place until it is laid out. Another debug
    //  file closes every document, since a file's id is valid only in the
    //  debug file it came from.
    void                             Apply          ();

    //  The file's document, opened if it is not, brought to the front of its
    //  group with `activate`, and scrolled so `line` is at its top when one
    //  is given; and a document closed.
    void                             Open           (int fileId, int line, bool activate);
    void                             Close          (int slot);

    //  A disassembly line was selected: its source line is selected in its
    //  file's document, opened behind the others if it is not open (FR-054).
    void                             ShowLine       (int fileId, int line);

    //  The banner's actions, then the text, of the document under the
    //  pointer or the one a drag began in.
    bool                             RouteMouse     (const DxuiMouseEvent & ev);

    //  Scrolls each document to its marked line once it is laid out.
    void                             FollowMarkedLines ();

    //  The documents open, in the open views' text, with those waiting for
    //  their debug file; and the waiting ones read back from it.
    std::string                      FormatOpen     (const std::function<std::string (int fileId)> & nameOf);
    void                             SetSaved       (std::vector<SourceDocuments::Saved> saved) { m_saved = std::move (saved); }

    //  A source to match against the debug file once it loads.
    void                             SetPendingLoose (const std::wstring & path) { m_pendingLoose = path; }

    //  A source file the user opens; a file with no debug file record; a
    //  dropped source matched against the loaded debug file; and a loose
    //  document's Load symbols button.
    void                             OpenSourcePath (const std::wstring & path);
    void                             OpenLooseFile  (const std::wstring & path, const std::string & text, bool isSource);
    void                             ShowDropped    (const std::wstring & path);
    void                             LoadSymbolsFor (int slot);
    bool                             IsLoose        (int slot) const;

    //  One choice for every document: the body's file comes forward, or the
    //  invocation's again.
    void                             ToggleMacroBody ();

private:
    static constexpr int  kFirstLooseFileId = 1 << 20;

    void                             FollowPc       (const DebuggerViewSnapshot & snapshot);
    void                             Restore        (const DebuggerViewSnapshot & snapshot);
    int                              GetSlotAt      (POINT atDip) const;

    IDebuggerPaneHost                         & m_host;
    std::vector<SourceDocumentPane *>           m_documents;
    SourceDocuments                             m_files;
    std::vector<SourceDocuments::Saved>         m_saved;
    std::wstring                                m_loadedFor;
    std::pair<int, int>                         m_pcPlace      = { -1, 0 };
    int                                         m_macroLevel   = 0;
    int                                         m_activeSlot   = 0;
    std::map<int, std::wstring>                 m_looseFiles;
    int                                         m_nextLooseId  = kFirstLooseFileId;
    std::wstring                                m_pendingLoose;
    uint64_t                                    m_clickMs      = 0;
    POINT                                       m_clickAt      = {};
};
