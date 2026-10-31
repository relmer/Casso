#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoExplorerCommands.h"
#include "CassoExplorer/Window/Panes/ExplorerPane.h"
#include "CassoExplorer/Window/Panes/Preview/PreviewFindBar.h"
#include "CassoExplorer/Window/Panes/Preview/PreviewHexView.h"
#include "CassoExplorer/Window/Panes/Preview/PreviewListingView.h"
#include "CassoExplorer/Window/Panes/Preview/PreviewPictureView.h"
#include "CassoExplorer/Window/Panes/Preview/PreviewShellView.h"
#include "CassoExplorer/Window/Panes/Preview/PreviewTextView.h"

struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewManager
//
//  The Preview pane: a view for each kind of preview, one of them visible at
//  a time, and the pane's message and toolbar, with the hex view's search
//  and go-to boxes. The window routes pointer and key
//  events over the pane here, and the manager routes them to its views.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerPreviewManager
{
public:
    explicit ExplorerPreviewManager (ExplorerContext & context);

    static constexpr const wchar_t *  kSelectToPreview = L"Select a file to preview.";
    static constexpr uint32_t         kBackgroundLift  = 7;       // the dark pane over the list, per channel

    void                              Create                 ();
    void                              CreateToolbar          ();
    void                              Configure              ();
    void                              Layout                 (const RECT & rect);
    void                              Hide                   ();
    void                              FillPreview            ();
    void                              SetPinned              (bool pinned);
    static bool                       IsKnownPreview         (const PreviewContent & preview);
    void                              PaintBackground        (IDxuiPainter & painter, const IDxuiTheme & theme) const;
    static uint32_t                   GetBackground          (const IDxuiTheme & theme);
    bool                              UpdateScrollbarHover   (POINT at);
    bool                              TickScrollbars         (int64_t nowMs);
    bool                              IsOverPane             (POINT point) const;
    LPCWSTR                           GetCursorForPoint      (POINT clientPx) const;
    std::vector<DxuiPopupMenuItem>    GetScrollbarMenu       (const DxuiMouseEvent & ev) const;
    IDxuiControl                    * GetFocusedControl      () const;
    bool                              TryGetSelectionDetail  (std::wstring & outDetail) const;
    void                              SetPreviewZoom         (int percent);
    void                              HandleAutoPreviewTimer ();
    int                               GetShownZoom           () const;
    bool                              IsArchiveLocation      () const;
    int                               GetPreviewStopIndex    (int commandId) const;
    const RECT                      & GetRect                () const      { return m_rect; }
    const RECT                      & GetContentRect         () const      { return m_content; }
    DxuiToolbar                     & GetToolbar             () const      { return *m_toolbar; }
    int                               GetBarMode             () const      { return m_barMode; }
    int                               GetBarFocus            () const      { return m_barFocus; }
    void                              SetBarFocus            (int entry)   { m_barFocus = entry; }
    PreviewListingView              & GetListing             ()            { return m_listing; }
    PreviewTextView                 & GetText                ()            { return m_text; }
    PreviewHexView                  & GetHex                 ()            { return m_hex; }
    PreviewPictureView              & GetPicture             ()            { return m_picture; }
    PreviewShellView                & GetShell               ()            { return m_shell; }
    PreviewFindBar                  & GetFind                ()            { return m_find; }

    bool                            HandleMessageMouse    (const DxuiMouseEvent & ev);
    bool                            HandleMouse           (const DxuiMouseEvent & ev);
    bool                            HandleShortcutKey     (const DxuiKeyEvent & ev);
    bool                            HandleTabKey          (const DxuiKeyEvent & ev) { return m_hex.HandleTabKey (ev); }
    void                            HandleFocusEntered    (bool forward)            { m_hex.HandleFocusEntered (forward); }
    bool                            HandleKey             (const DxuiKeyEvent & ev);
    void                            HandleFocusChanged    (ExplorerPane focus);

private:
    ExplorerContext               & m_context;
    PreviewListingView              m_listing     { *this, m_context };
    PreviewPictureView              m_picture     { *this, m_context };
    PreviewHexView                  m_hex         { *this, m_context };
    PreviewTextView                 m_text        { *this, m_context };
    PreviewShellView                m_shell       { *this, m_context };
    PreviewFindBar                  m_find        { *this, m_context };
    std::array<PreviewView *, 5>    m_views       { &m_listing, &m_picture, &m_hex, &m_text, &m_shell };   // in creation order, which is drawing order
    DxuiSelectableText            * m_message     = nullptr;
    DxuiToolbar                   * m_toolbar     = nullptr;
    int                             m_barMode     = CassoExplorerCommands::kPreviewBarNone;   // one of CassoExplorerCommands' preview bars
    int                             m_barFocus    = 0;   // the toolbar's focused button
    RECT                            m_rect        = {};   // the pane, its toolbar included
    RECT                            m_content     = {};   // below the toolbar
    bool                            m_closeDue    = false;
    int                             m_pictureZoom = 100;
    PreviewContent::Kind            m_lastKind    = PreviewContent::Kind::Text;
    bool                            m_pinned      = false;   // turned on manually, so automatic preview doesn't close it
    std::wstring                    m_autoKey;   // the file automatic preview last checked

    std::wstring  GetAutoKey        () const;
    bool          UpdateAutoPreview ();
};
