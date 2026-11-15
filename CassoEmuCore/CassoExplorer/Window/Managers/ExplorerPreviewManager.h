#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoExplorerCommands.h"
#include "CassoExplorer/Window/Panes/ExplorerPane.h"
#include "CassoExplorer/Window/Panes/Preview/PreviewFindBar.h"
#include "CassoExplorer/Window/Panes/Preview/PreviewHexView.h"
#include "CassoExplorer/Window/Panes/Preview/PreviewListingView.h"
#include "CassoExplorer/Window/Panes/Preview/PreviewPictureView.h"
#include "CassoExplorer/Window/Panes/Preview/PreviewDiskThumbnail.h"
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

    static constexpr uint32_t         kBackgroundLift  = 7;       // the dark pane over the list, per channel
    static constexpr int              kSummaryShare    = 2;       // a WOZ image's details above its catalog take at most 1/2 of the pane
    static constexpr int              kRuleBandDip     = 13;      // the band between those details and the catalog, with a rule across it
    static constexpr int              kRuleInsetDip    = 8;

    //  The longest record length that can be typed, and its digits.
    static constexpr size_t           kRecordLengthMax    = 65535;
    static constexpr int              kRecordLengthDigits = 5;

    //  A prompt for the record length of a random-access text file, and the
    //  text typed in it read as a length: decimal digits, 0 to 65535, empty
    //  for 0.
    void                              ChooseRecordLength     ();
    static bool                       TryParseRecordLength   (const std::wstring & text, size_t & outLength);

    void                              Create                 ();
    void                              CreateToolbar          ();
    void                              Configure              ();
    void                              Layout                 (const RECT & rect);
    void                              Hide                   ();
    void                              FillPreview            ();
    void                              TakeThumbnail          ();
    void                              HandlePicturesReady    ();
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
    bool                              IsArchiveLocation      () const;
    int                               GetPreviewStopIndex    (int commandId) const;
    bool                              IsShown                () const      { return m_shown; }
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
    PreviewDiskThumbnail            & GetThumbnail           ()            { return m_thumbnail; }

    //  The preview over the whole tab, below the tab strip, and its steps to
    //  the previous and next file.
    bool  IsExpanded             () const;
    void  SetExpanded            (bool expanded);
    bool  CanStep                (bool forward) const;
    void  Step                   (bool forward);
    void  HandleSelectionChanged ();
    bool  HandleExpandedKey      (const DxuiKeyEvent & ev, ExplorerPane focus);
    bool  HandleEnterKey         (const DxuiKeyEvent & ev);

    void  HandleAutoPreviewTimer ();
    void  SaveTabState           ();
    void  RestoreTabState        ();
    int   GetShownZoom           () const;

    bool                            HandleMessageMouse    (const DxuiMouseEvent & ev);
    bool                            HandleShortcutKey     (const DxuiKeyEvent & ev);
    bool                            HandleTabKey          (const DxuiKeyEvent & ev) { return m_hex.HandleTabKey (ev); }
    void                            HandleFocusEntered    (bool forward)            { m_hex.HandleFocusEntered (forward); }
    bool                            HandleKey             (const DxuiKeyEvent & ev);
    void                            HandleFocusChanged    (ExplorerPane focus);

private:
    ExplorerContext               & m_context;
    PreviewListingView              m_listing       { *this, m_context };
    PreviewPictureView              m_picture       { *this, m_context };
    PreviewHexView                  m_hex           { *this, m_context };
    PreviewTextView                 m_text          { *this, m_context };
    PreviewShellView                m_shell         { *this, m_context };
    PreviewFindBar                  m_find          { *this, m_context };
    PreviewDiskThumbnail            m_thumbnail     { m_context };
    std::array<PreviewView *, 5>    m_views         { &m_listing, &m_picture, &m_hex, &m_text, &m_shell };   // in creation order, which is drawing order
    DxuiSelectableText            * m_message       = nullptr;
    DxuiTextView                  * m_summary       = nullptr;   // a WOZ image's details, above its catalog
    RECT                            m_ruleBand      = {};   // between them, empty without them
    DxuiToolbar                   * m_toolbar       = nullptr;
    int                             m_barMode       = CassoExplorerCommands::kPreviewBarNone;   // one of CassoExplorerCommands' preview bars
    int                             m_barFocus      = 0;   // the toolbar's focused button
    RECT                            m_rect          = {};   // the pane, its toolbar included
    RECT                            m_content       = {};   // below the toolbar
    int                             m_pictureZoom   = 100;
    PreviewContent::Kind            m_lastKind      = PreviewContent::Kind::Text;
    bool                            m_pinned        = false;   // turned on manually, so automatic preview doesn't close it
    bool                            m_shown         = false;   // on screen: turned on, with a file selected
    bool                            m_isBarBuilt    = false;   // the toolbar's entries set at least once
    bool                            m_isBarExpanded = false;   // whether they have previous and next
    bool                            m_isStepping    = false;   // set during a step's selection, so the step set stays
    std::vector<std::wstring>       m_stepSet       = {};   // the keys of the files a step goes through; empty for every file
    std::wstring                    m_autoKey;   // the file automatic preview last checked
    std::wstring                    m_autoAddress;

    int           GetStepTarget     (bool forward) const;
    void          SelectStep        (int row);
    std::wstring  GetAutoKey        () const;
    bool          UpdateAutoPreview ();
    bool          UpdateShown       ();
};
