#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/CrtPictureRenderer.h"
#include "CassoExplorer/Window/ExplorerPane.h"
#include "CassoExplorer/Window/ExplorerPreviewArchive.h"
#include "CassoExplorer/Window/ExplorerPreviewFind.h"
#include "CassoExplorer/Window/PreviewHexSources.h"
#include "Seams/Win32ShellPreview.h"
#include "CassoExplorer/CassoExplorerCommands.h"

class CassoExplorerWindow;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview
//
//  The Preview pane and its widgets: the catalog and archive rows, the text,
//  the bytes, the picture through Casso's CRT pass, the shell's preview
//  handler, the pane's message, and its toolbar with the search and go-to
//  boxes. The window routes pointer and key events over the pane here; the
//  pane lays out its widgets and has the hex and text views' settings and
//  its menus.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerPreview
{
public:
    explicit ExplorerPreview (CassoExplorerWindow & window);

    static constexpr const wchar_t *  kSelectToPreview = L"Select a file to preview.";
    static constexpr float            kPromptFontDip   = 12.0f;   // Explorer's, 9 points

    //  Each row is one line of fixed-width text, so a row is the line's height
    //  rather than a file listing's roomier row.
    static constexpr int    kRowHeightDip = 18;

    //  Preview text is drawn this far from the background toward the theme's
    //  foreground, a gray like a terminal's text rather than full white.
    static constexpr float  kTextStrength = 0.8f;

    //  Room around a picture for its glow, which spills past its edges.
    static constexpr int    kPictureInsetDip = 16;

    //  The dark pane over the list, per channel.
    static constexpr uint32_t  kBackgroundLift = 7;

    //  How long a shell preview has before the bytes replace it.
    static constexpr UINT  kShellPreviewTimeoutMs = 5000;

    //  The pane's widgets, as children of the window, and their settings and
    //  handlers once the window has its HWND.
    void  Create    ();
    void  CreateToolbar ();
    void  Configure ();

    //  The pane over `rect`, its toolbar across the top when the preview has
    //  one. Hide unloads a shell preview and the toolbar when the pane goes.
    void           Layout (const RECT & rect);
    void           Hide   ();
    const RECT &   GetRect () const { return m_previewRect; }

    //  Every preview draws on the content surface, a message included, which
    //  has no background of its own.
    void             PaintBackground (IDxuiPainter & painter, const IDxuiTheme & theme) const;
    static uint32_t  GetBackground   (const IDxuiTheme & theme);

    //  Pointer events. The window routes them to the message ahead of its
    //  other panes, since the message's text selects with the mouse, and to
    //  the rest of the pane after them.
    bool     OnMessageMouse       (const DxuiMouseEvent & ev);
    bool     OnMouse              (const DxuiMouseEvent & ev);
    bool     UpdateScrollbarHover (POINT at);
    bool     TickScrollbars       (int64_t nowMs);
    bool     IsOverPane           (POINT point) const;
    LPCWSTR  GetCursorForPoint    (POINT clientPx) const;
    LPCWSTR  GetPictureCursor     (POINT clientPx) const;
    std::vector<DxuiPopupMenuItem>  GetScrollbarMenu (const DxuiMouseEvent & ev) const;

    //  Keys. A focused box has its keys first; the shortcuts for an archive's
    //  files and for a message come before the window's commands; Tab moves
    //  between the hex view's columns before focus moves to the next pane.
    bool     OnBoxKey       (ExplorerPane focus, const DxuiKeyEvent & ev) { return m_find.OnBoxKey (focus, ev); }
    bool     OnShortcutKey  (const DxuiKeyEvent & ev);
    bool     OnTabKey       (const DxuiKeyEvent & ev);
    void     OnFocusEntered (bool forward);
    bool     OnKey          (const DxuiKeyEvent & ev);

    //  Focus moved to `focus`; the pane's control, for a standard command.
    void            OnFocusChanged    (ExplorerPane focus);
    IDxuiControl *  GetFocusedControl () const;
    bool            IsCaretOn         (ExplorerPane focus) const { return m_find.IsCaretOn (focus); }

    //  The toolbar the preview's kind brings, if any, and its focused button.
    DxuiToolbar &  GetToolbar  () const { return *m_previewToolbar; }
    int            GetBarMode  () const { return m_previewBarMode; }
    int            GetBarFocus () const { return m_previewBarFocus; }
    void           SetBarFocus (int entry) { m_previewBarFocus = entry; }
    int            GetPreviewStopIndex (int commandId) const;

    //  The hex view's settings, and find and go to.
    bool  IsShowingValues      () const { return m_hexView->IsShowingValues(); }
    bool  CanFindNext          () const { return m_find.CanFindNext(); }
    bool  FocusGoTo            () { return m_find.FocusGoTo(); }
    bool  FocusFind            () { return m_find.FocusFind(); }
    void  ToggleLineAddresses  ();
    void  SetHexGrouping       (int grouping);
    void  SetHexColumns        (int columns);
    void  SetHexFormat         (const char * format);
    void  SetHexShowValues     (bool show);
    void  SetPreviewZoom       (int percent);
    void  FindNext             () { m_find.FindNext(); }

    //  Bytes or characters selected, as the status bar's detail.
    bool  TryGetSelectionDetail (std::wstring & outDetail) const;

    //  Whether the pane currently displays the hex view, or the text view,
    //  rather than lines, a picture or a message.
    bool  IsHexPreviewShowing  () const;
    bool  IsTextPreviewShowing () const;

    //  The content for the selection, and the Windows preview of a host file,
    //  hosted over the pane.
    void  FillPreview           ();
    void  UpdateShellPreview    (const PreviewContent & preview);
    void  OnShellPreviewResult  ();
    void  ShowShellPreviewBytes ();
    bool  OnHelperMessage       (UINT msg, WPARAM wParam, LPARAM lParam) { return m_shellPreview.OnHelperMessage (msg, wParam, lParam); }

    //  A picture preview on its monitor, through Casso's CRT pass.
    PicturePreview::Monitor   GetPreviewMonitor     () const;
    const ThemeCrtDefaults *  GetPreviewCrtDefaults ();
    void                      UpdateCrtPicture      (bool force);
    void                      SelectMonitor         (PicturePreview::Monitor monitor);
    void                      ShowPictureForm       (bool color);

    //  Whether the file list's location is the inside of an archive.
    bool  IsArchiveLocation () const;

    //  The hex view's character column for a preview's bytes: the host
    //  file's own character set, or Apple text for a file from an image.
    static DxuiHexView::TextEncoding       GetHexEncoding   (bool hostFile, HostCharset charset);
    static DxuiHexView::ValueFormat        ParseHexFormat   (const std::string & name);

private:
    void  ShowTextContextMenu    (int x, int y);
    void  ShowPictureContextMenu (int x, int y);

    CassoExplorerWindow  & m_window;

    friend class ExplorerPreviewArchive;
    friend class ExplorerPreviewFind;
    ExplorerPreviewArchive  m_archive { *this, m_window };
    ExplorerPreviewFind     m_find    { *this, m_window };

    DxuiListView         * m_previewList    = nullptr;
    DxuiTextView         * m_textView       = nullptr;
    DxuiHexView          * m_hexView        = nullptr;
    DxuiFramebufferView  * m_picture        = nullptr;
    DxuiSelectableText   * m_previewMessage = nullptr;
    DxuiToolbar          * m_previewToolbar = nullptr;
    Win32ShellPreview      m_shellPreview;

    //  The toolbar's commands, as one of CassoExplorerCommands' preview bars,
    //  and its focused button.
    int    m_previewBarMode  = CassoExplorerCommands::kPreviewBarNone;
    int    m_previewBarFocus = 0;

    RECT   m_previewRect     = {};   // the pane, its toolbar included
    RECT   m_previewContent  = {};   // below the toolbar

    bool          m_previewArchive  = false;   // the rows are an archive's files, as the file list
    std::wstring  m_shellPreviewPath;
    bool          m_shellPicture    = false;   // the shell's preview is a thumbnail in the picture view

    CrtPictureRenderer  m_crtPicture;
    int                 m_crtWidth      = 0;
    int                 m_crtHeight     = 0;
    std::string         m_crtThemeName;
    LoadedTheme         m_crtTheme;
    bool                m_crtThemeValid = false;
    PreviewBytes        m_previewBytes;
    PreviewFileBytes    m_fileBytes;

};
