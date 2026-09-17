#include "Pch.h"

#include "Cassque/CassqueWindow.h"
#include "Cassque/CassqueAbout.h"
#include "Cassque/CassqueDragOut.h"
#include "Cassque/CassqueNewDiskDialog.h"
#include "Cassque/CassquePromptDialog.h"
#include "Cassque/CassqueShell.h"
#include "Cassque/Model/KnownFolderStore.h"
#include "Cassque/Model/LaunchCommand.h"
#include "Core/TextEncoding.h"
#include "Widgets/DxuiContextMenu.h"
#include "Core/DxuiClipboard.h"
#include "Theme/DxuiDwm.h"
#include "Theme/DxuiWindowsThemeColors.h"
#include "Window/DxuiMessageBox.h"
#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::CassqueWindow
//
////////////////////////////////////////////////////////////////////////////////

CassqueWindow::CassqueWindow (CassqueBrowser & browser, CassqueActions & actions, CassquePrefs & prefs, Context context)
    : m_browser  (browser),
      m_actions  (actions),
      m_prefs    (prefs),
      m_context  (std::move (context)),
      m_commands (CassqueCommands::Handlers {
                      [this] (int id)       { Dispatch (id); },
                      [this] (int id)       { return IsEnabled (id); },
                      [this] (int id)       { return IsChecked (id); } })
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::~CassqueWindow
//
////////////////////////////////////////////////////////////////////////////////

CassqueWindow::~CassqueWindow()
{
    m_dropTarget.Shutdown();
    DestroyBackend();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::Open
//
//  The class name is the one a second launch looks for, so it is fixed. A
//  remembered placement still on a monitor is applied before the first show.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassqueWindow::Open (HINSTANCE instance, const std::wstring & title, int showCommand)
{
    HRESULT                   hr     = S_OK;
    DxuiWindow::CreateParams  params;
    WINDOWPLACEMENT           place  = { sizeof (place) };
    RECT                      rect   = {};
    BOOL                      result = FALSE;



    AdoptSystemColors();

    m_theme = &CassqueShell::ChooseTheme (m_prefs.theme, DxuiWindowsThemeColors::Instance().IsDarkMode(), m_lightTheme, m_darkTheme);

    //  Before the first layout, which the window's creation can bring on. The
    //  tabs run across the top, above the bars each tab's location drives, as
    //  a browser's do.
    m_dock.SetDock (m_tabBand,     DxuiDock::Top);
    m_dock.SetDock (m_menuBand,    DxuiDock::Top);
    m_dock.SetDock (m_toolbarBand, DxuiDock::Top);
    m_dock.SetDock (m_statusBand,  DxuiDock::Bottom);
    m_dock.SetDock (m_bodyBand,    DxuiDock::Fill);

    params.title                    = title;
    params.hInstance                = instance;
    params.initialSizeDip           = { CassqueShell::kDefaultWidthDip, CassqueShell::kDefaultHeightDip };
    params.minSizeDip               = { 640, 400 };
    params.resizable                = true;
    params.insetContentBelowCaption = true;
    params.classNameOverride        = CassqueShell::kWindowClass;
    params.appIconBig               = LoadIconW (instance, MAKEINTRESOURCEW (IDI_CASSQUE));
    params.appIconSmall             = params.appIconBig;

    hr = DxuiWindow::Create (params);
    CHR (hr);

    //  The caption draws the app's own icon, as Casso's does; the taskbar
    //  already has it from the create parameters. A failure leaves the caption
    //  without one rather than failing the window.
    {
        HICON          captionIcon = (HICON) LoadImageW (instance, MAKEINTRESOURCEW (IDI_CASSQUE), IMAGE_ICON,
                                                         kCaptionIconPx, kCaptionIconPx, LR_DEFAULTCOLOR);
        DxuiIconImage  image;
        HRESULT        hrIcon      = E_FAIL;

        if (captionIcon != nullptr)
        {
            hrIcon = DxuiIconImage::FromHicon (captionIcon, kCaptionIconPx, image);
            DestroyIcon (captionIcon);
        }

        if (SUCCEEDED (hrIcon))
        {
            GetPopupHost()->SetCaptionIcon (std::move (image.bgraPremul), image.width, image.height);
        }
    }

    ApplyTheme();

    //  The tooltip's dwell runs on a timer: nothing else ticks this window.
    SetTimer (GetHwnd(), kTooltipTimerId, kTooltipTickMs, nullptr);

    //  Files and folders dropped on the list or the tree go into the image or
    //  directory under the pointer, and entries dragged out of another image
    //  go in with their types. A failure to register leaves the window
    //  without drops, not without a window.
    {
        HRESULT  hrDrop = m_dropTarget.Initialize (GetHwnd(), &m_dropHits,
                                                   [this] (int, const std::wstring & path) { OnDropFile (path); });

        m_dropTarget.SetDataHandlers ([this] (IDataObject * data, int tag, POINT screen) { return GetDropEffect (data, tag, screen); },
                                      [this] (IDataObject * data, int tag, POINT screen) { OnDrop (data, tag, screen); });

        IGNORE_RETURN_VALUE (hrDrop, S_OK);
    }

    if (m_prefs.placement.valid && m_prefs.placement.w > 0 && m_prefs.placement.h > 0)
    {
        rect = RECT { m_prefs.placement.x, m_prefs.placement.y,
                      m_prefs.placement.x + m_prefs.placement.w, m_prefs.placement.y + m_prefs.placement.h };

        if (MonitorFromRect (&rect, MONITOR_DEFAULTTONULL) != nullptr)
        {
            place.rcNormalPosition = rect;
            place.showCmd          = SW_HIDE;
            result = SetWindowPlacement (GetHwnd(), &place);
            IGNORE_RETURN_VALUE (result, TRUE);

            if (m_prefs.placement.maximized && showCommand != SW_SHOWMINIMIZED && showCommand != SW_SHOWMINNOACTIVE)
            {
                showCommand = SW_SHOWMAXIMIZED;
            }
        }
    }

    result = ShowWindow (GetHwnd(), showCommand);
    IGNORE_RETURN_VALUE (result, FALSE);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::StorePlacement
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::StorePlacement()
{
    WINDOWPLACEMENT  place = { sizeof (place) };



    if (GetHwnd() == nullptr || !GetWindowPlacement (GetHwnd(), &place))
    {
        return;
    }

    m_prefs.placement.x         = place.rcNormalPosition.left;
    m_prefs.placement.y         = place.rcNormalPosition.top;
    m_prefs.placement.w         = place.rcNormalPosition.right - place.rcNormalPosition.left;
    m_prefs.placement.h         = place.rcNormalPosition.bottom - place.rcNormalPosition.top;
    m_prefs.placement.maximized = place.showCmd == SW_SHOWMAXIMIZED;
    m_prefs.placement.valid     = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::OnCreate
//
//  Children are created in paint order: panes first, then the splitters over
//  their edges, the status bar, and the menu bar last so its strip is on top.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::OnCreate()
{
    CassqueNamedControl<DxuiTreeView>         * tree            = CreateChild<CassqueNamedControl<DxuiTreeView>>();
    CassqueNamedControl<DxuiListView>         * list            = CreateChild<CassqueNamedControl<DxuiListView>>();
    CassqueNamedControl<DxuiListView>         * previewList     = nullptr;
    CassqueNamedControl<DxuiFramebufferView>  * picture         = nullptr;
    CassqueNamedControl<DxuiHexView>          * hexView         = nullptr;
    CassqueNamedControl<DxuiSplitter>         * treeSplitter    = nullptr;
    CassqueNamedControl<DxuiSplitter>         * previewSplitter = nullptr;



    tree->SetAccessibleName (L"Folders and disk images");
    list->SetAccessibleName (L"Files");

    m_tree            = tree;
    m_list            = list;
    m_listMessage     = CreateChild<DxuiLabel> (L"", DxuiTextRole::Muted, DxuiTextHAlign::Center, DxuiTextVAlign::Center);
    previewList       = CreateChild<CassqueNamedControl<DxuiListView>>();
    picture           = CreateChild<CassqueNamedControl<DxuiFramebufferView>>();
    hexView           = CreateChild<CassqueNamedControl<DxuiHexView>>();
    m_textView        = CreateChild<DxuiTextView>();
    m_previewMessage  = CreateChild<DxuiLabel> (L"", DxuiTextRole::Muted, DxuiTextHAlign::Center, DxuiTextVAlign::Center);
    treeSplitter      = CreateChild<CassqueNamedControl<DxuiSplitter>>();
    previewSplitter   = CreateChild<CassqueNamedControl<DxuiSplitter>>();

    previewList->SetAccessibleName     (L"Preview");
    picture->SetAccessibleName         (L"Picture preview");
    hexView->SetAccessibleName         (L"Bytes");
    treeSplitter->SetAccessibleName    (L"Folder pane width");
    previewSplitter->SetAccessibleName (L"Preview pane width");

    m_previewList     = previewList;
    m_hexView         = hexView;
    m_picture         = picture;
    m_treeSplitter    = treeSplitter;
    m_previewSplitter = previewSplitter;
    m_status          = CreateChild<DxuiStatusBar>();
    m_tabs            = CreateChild<DxuiTabStrip>();
    m_toolbar         = CreateChild<DxuiToolbar>();
    m_address         = CreateChild<DxuiAddressBar>();
    m_menuBar         = CreateChild<DxuiMenuBar>();

    m_renameBox       = CreateChild<DxuiTextInput>();

    m_renameBox->SetVisible      (false);
    m_renameBox->SetOverText     (true);
    m_renameBox->SetHwnd         (GetHwnd());
    m_renameBox->SetTextRenderer (GetTextRenderer());

    m_toolbar->SetTextRenderer (GetTextRenderer());
    m_address->SetTextRenderer (GetTextRenderer());
    m_address->SetIconFace     (DxuiTextRenderer::IsFontFamilyInstalled (DxuiToolbar::kFluentIconFace)
                                ? DxuiToolbar::kFluentIconFace
                                : DxuiToolbar::kMdl2IconFace);
    m_address->SetFont         (DxuiTextRenderer::IsFontFamilyInstalled (DxuiAddressBar::kVariableTextFace)
                                ? DxuiAddressBar::kVariableTextFace
                                : DxuiTheme::kBodyFace,
                                DxuiAddressBar::kFontDip);
    m_toolbar->SetPopupHost    (GetPopupHost());
    m_toolbar->SetEntries      (m_commands.BuildToolbarEntries());
    m_tooltip.SetPopupHost     (GetPopupHost());

    //  Explorer's navigation glyphs are in Windows 11's Segoe Fluent Icons.
    //  Without that font, use MDL2, which has the same code points; text in a
    //  missing font renders as nothing.
    m_toolbar->SetIconFace (DxuiTextRenderer::IsFontFamilyInstalled (DxuiToolbar::kFluentIconFace)
                            ? DxuiToolbar::kFluentIconFace
                            : DxuiToolbar::kMdl2IconFace);
    m_toolbar->SetIconDip  (kNavIconDip);

    m_previewToolbar = CreateChild<DxuiToolbar>();
    m_previewToolbar->SetTextRenderer (GetTextRenderer());
    m_previewToolbar->SetPopupHost    (GetPopupHost());
    m_previewToolbar->SetIconDip      (kNavIconDip);
    m_previewToolbar->SetVisible      (false);

    m_goToBox.SetHint         (L"Go to");
    m_goToBox.SetWidthDip     (130);
    m_goToBox.SetTooltip      (L"Go to an address (e.g., $08FF) or offset (e.g., +10, +$A0), or select a range (e.g., $0803-$0810, $0803,+10). Numbers are hex; # marks decimal, as in -#16.");
    m_goToBox.SetHwnd         (GetHwnd());
    m_goToBox.SetTextRenderer (GetTextRenderer());
    m_goToBox.SetOnChange     ([this] (const std::wstring &) { m_goToBox.SetError (false); });
    m_goToBox.SetOnSubmit     ([this] () { GoToTyped (m_goToBox.GetText()); });

    m_goToBox.SetOnCancel ([this] ()
    {
        m_goToBox.SetText  (L"");
        m_goToBox.SetError (false);
        SetFocusPane (Pane::Preview);
    });

    m_goToBox.SetOnFocusRequest ([this] ()
    {
        m_previewBarFocus = GetPreviewStopIndex (CassqueCommands::kGoToOffset);
        SetFocusPane (Pane::GoTo);
    });

    m_searchBox.SetGlyph        (s_kpszMdl2Search);
    m_searchBox.SetHint         (L"Search");
    m_searchBox.SetHwnd         (GetHwnd());
    m_searchBox.SetTextRenderer (GetTextRenderer());
    m_searchBox.SetOnChange     ([this] (const std::wstring & text) { OnSearchChanged (text); });
    m_searchBox.SetOnSubmit     ([this] () { FindNext(); });

    m_searchBox.SetOnCancel ([this] ()
    {
        m_searchBox.SetText (L"");
        m_findBytes.clear();
        SetFocusPane (Pane::Preview);
    });

    m_searchBox.SetOnFocusRequest ([this] ()
    {
        m_previewBarFocus = GetPreviewStopIndex (CassqueCommands::kFind);
        SetFocusPane (Pane::Search);
    });

    m_menuBar->SetPopupHost (GetPopupHost());
    m_menuBar->SetTextRendererForMeasure (GetTextRenderer());

    //  Drawn by the shell at the pixel size a row's icon is laid out at, and
    //  handed to the browser before the first nodes and rows are built.
    m_shellIcons.SetSizePx (MulDiv (DxuiTreeView::s_kIconDip, (int) GetDpiForWindow (GetHwnd()), (int) DxuiDpiScaler::kBaseDpi));
    m_browser.SetShellIcons (&m_shellIcons);

    ConfigureWidgets();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ConfigureWidgets
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::ConfigureWidgets()
{
    std::vector<DxuiTreeNode>  roots;
    bool                       grouped = false;



    m_menuBar->SetItems (m_commands.BuildMenuItems());
    m_addressRoot = GetProfileRoot();

    m_browser.GetTreeRoots (roots);

    m_tree->SetShowCheckboxes (false);
    m_tree->SetNodes (std::move (roots));
    m_tree->SetChildProvider ([this] (const std::wstring & id) { return m_browser.GetTreeChildren (id); });

    m_tree->SetOnSelect ([this] (const std::wstring & id)
    {
        HRESULT  hr = m_browser.SelectTreeNode (id);

        IGNORE_RETURN_VALUE (hr, S_OK);
        FillList();
    });

    //  Opening or closing a folder changes which folders are on screen.
    m_tree->SetOnExpand ([this] (const std::wstring &, bool)
    {
        //  A refresh re-opens folders one at a time; it updates the watches
        //  once when it is done instead.
        if (!m_refreshingTree)
        {
            UpdateWatchedFolders();
        }
    });


    m_list->SetShowHeader (true);
    m_list->SetColumns (CassqueBrowser::GetColumns());
    m_list->SetPreciseAutoFit (true);

    //  Columns keep the widths they are given, and a pane too narrow for them
    //  scrolls, the way Explorer's details view does, rather than squeezing
    //  every column to fit.
    m_list->SetHorizontalScrollEnabled (true);
    m_list->SetMultiSelect (true);
    m_list->SetKeyboardColumnNav (true);
    m_list->SetActivateOnDoubleClick (true);
    m_list->SetAlwaysShowSelection (true);

    m_list->SetOnSelectionChanged ([this] (int)
    {
        m_browser.SetSelectedRows (m_list->GetSelectedRows());
        FillPreview();
        FillStatus();
    });

    m_list->SetOnSortColumn ([this] (int column)
    {
        m_browser.SortByColumn (column);
        FillList();
    });

    //  A width the user set, by dragging a divider or by double-clicking one
    //  to fit it, is kept for the next run.
    m_list->SetOnColumnResized ([this] (int column, int widthPx)
    {
        if (column < 0)
        {
            return;
        }

        if ((size_t) column >= m_prefs.columnWidthsDip.size())
        {
            m_prefs.columnWidthsDip.resize ((size_t) column + 1, 0);
        }

        m_prefs.columnWidthsDip[(size_t) column] = MulDiv (widthPx, (int) DxuiDpiScaler::kBaseDpi, (int) m_scaler.GetDpi());
    });

    m_list->SetOnActivateRow ([this] (int row)
    {
        if (m_browser.OpenRow (row))
        {
            FillList();
        }
    });

    m_previewList->SetShowHeader (false);
    m_previewList->SetColumns ({ DxuiListView::Column { L"", 0, true } });

    //  A hex dump and a disassembly are columns of fixed-width text, and read
    //  as such only in a monospace face on rows close to the line height.
    m_previewList->SetMonospace (true);
    m_previewList->SetRowHeightDip (kPreviewRowHeightDip);
    m_previewList->SetHorizontalScrollEnabled (true);
    m_previewList->SetPreciseAutoFit (true);
    m_previewList->SetMultiSelect (true);
    m_previewList->SetAlwaysShowSelection (true);
    m_previewList->SetTextSelectionColors (true);
    m_previewList->SetOwnerWindow (GetHwnd());

    m_textView->SetOwnerWindow   (GetHwnd());
    m_textView->SetIconFace      (DxuiTextRenderer::IsFontFamilyInstalled (DxuiToolbar::kFluentIconFace)
                                  ? DxuiToolbar::kFluentIconFace
                                  : DxuiToolbar::kMdl2IconFace);
    m_textView->SetOnContextMenu ([this] (POINT at) { ShowTextContextMenu (at.x, at.y); });

    //  The bytes are Apple text in the column on the right, which is what
    //  the files here hold; the grouping is the user's, kept between runs.
    m_hexView->SetTextEncoding (DxuiHexView::TextEncoding::AppleHighBit);
    m_hexView->SetOwnerWindow  (GetHwnd());
    m_hexView->SetPaddingDip   (6);
    m_hexView->SetColumns      (m_prefs.hexColumns);
    m_hexView->SetValueFormat  (ParseHexFormat (m_prefs.hexFormat));
    m_hexView->SetShowValues   (m_prefs.hexShowValues);
    m_hexView->SetBreakLines   (true);
    m_hexView->SetTextStrength (kPreviewTextStrength);
    m_hexView->SetZoom         ((float) m_prefs.previewZoom / 100.0f);
    m_textView->SetTextStrength (kPreviewTextStrength);
    m_textView->SetZoom         ((float) m_prefs.previewZoom / 100.0f);
    m_picture->SetZoom          ((float) m_prefs.previewZoom / 100.0f);

    grouped = m_hexView->SetGrouping (m_prefs.hexGrouping);
    IGNORE_RETURN_VALUE (grouped, true);

    m_hexView->SetOnSelectionChanged ([this] () { FillStatus(); });

    m_hexView->SetOnContextMenu ([this] (POINT atDip)
    {
        ShowHexContextMenu (atDip.x, atDip.y);
    });

    m_treeSplitter->SetOrientation (DxuiSplitter::Orientation::Vertical);
    m_treeSplitter->SetOnMoved ([this] (int dip)
    {
        m_prefs.treeWidthDip = dip;
        RecomputeLayout();
    });

    m_previewSplitter->SetOrientation (DxuiSplitter::Orientation::Vertical);
    m_previewSplitter->SetOnMoved ([this] (int dip)
    {
        RECT  bounds = m_previewSplitter->GetBounds();
        int   width  = MulDiv (bounds.right - bounds.left, (int) DxuiDpiScaler::kBaseDpi, (int) m_scaler.GetDpi());

        m_prefs.previewWidthDip = width - dip - DxuiSplitter::kSashDip;
        RecomputeLayout();
    });

    m_status->SetFields ({ { L"", 0, true }, { L"", kStatusFreeDip, false }, { L"", kStatusDetailDip, false }, { L"", kStatusZoomDip, false } });

    m_tabs->SetOnChange ([this] (int index) { SwitchToTab ((size_t) index); });
    m_tabs->SetOnMove   ([this] (int from, int to)
    {
        bool  moved = m_browser.MoveTab ((size_t) from, (size_t) to);

        IGNORE_RETURN_VALUE (moved, true);
    });
    m_tabs->SetOnNewTab ([this]() { Dispatch (CassqueCommands::kNewTab); });
    m_tabs->SetOnClose  ([this] (int index)
    {
        //  The last tab stays, as the close command leaves it.
        if (m_browser.GetBrowserModel().GetTabCount() > 1 && m_browser.CloseTab ((size_t) index))
        {
            FillList();
        }
    });
    m_tabs->SetIconFace (DxuiTextRenderer::IsFontFamilyInstalled (DxuiToolbar::kFluentIconFace)
                         ? DxuiToolbar::kFluentIconFace
                         : DxuiToolbar::kMdl2IconFace);

    m_address->SetOnSegment ([this] (int index)
    {
        if (index >= 0 && index < (int) m_addressSegments.size())
        {
            m_browser.NavigateToLocation (m_addressSegments[(size_t) index].location);
            FillList();
        }
    });
    m_address->SetOnSubmit ([this] (const std::wstring & text) { SubmitAddress (text); });
    m_address->SetOnSeparator ([this] (int index, const RECT & anchor) { ShowAddressMenu (index, anchor); });
    m_address->SetOnOverflow  ([this] (const RECT & anchor) { ShowAddressOverflowMenu (anchor); });
    m_address->SetOnHistory   ([this] (const RECT & anchor) { ShowAddressHistoryMenu (anchor); });

    m_browser.GetTypedPaths().Reset (m_prefs.typedPaths);
    ApplyStoredColumnWidths();

    //  Posting is all that happens on the watcher's thread; the settle timer
    //  and the re-read run here.
    if (m_context.watcher != nullptr)
    {
        m_folderWatch = std::make_unique<FolderWatch> (*m_context.watcher);

        m_folderWatch->SetOnChanged ([this] ()
        {
            PostMessageW (GetHwnd(), kFolderChangedMessage, 0, 0);
        });
    }

    m_browser.RestoreTabs (m_prefs.tabs);

    m_tree->OnFocusChanged (true);
    FillList();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::AdoptSystemColors
//
//  The Windows themes take the list surface and the accent from the system
//  each time a theme is chosen, which also picks up an accent changed while
//  the window was away.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::AdoptSystemColors()
{
    const DxuiWindowsThemeColors::SystemColors &  system = DxuiWindowsThemeColors::Instance().GetSystemColors();



    m_lightTheme.ApplySystemColors (system);
    m_darkTheme.ApplySystemColors  (system);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ApplyTheme
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::ApplyTheme()
{
    AdoptSystemColors();

    m_theme = &CassqueShell::ChooseTheme (m_prefs.theme, DxuiWindowsThemeColors::Instance().IsDarkMode(), m_lightTheme, m_darkTheme);

    //  Casso's own themes lend their chrome colors; the skeuomorphic one has
    //  no scene to draw here, so it is colors only.
    if (IsCassoThemeName (m_prefs.theme))
    {
        m_cassoTheme = CassoTheme::MakeByName (m_prefs.theme);
        m_theme      = &m_cassoTheme;
    }

    SetTheme (m_theme);

    if (m_menuBar != nullptr)
    {
        m_menuBar->SetStripColors    (m_theme->navStrip, m_theme->navHover, m_theme->navItemText);
        m_menuBar->SetDropdownColors (m_theme->dropdownBg, m_theme->dropdownHover, m_theme->dropdownItemText,
                                      m_theme->dropdownAccel, m_theme->panelEdge, m_theme->buttonBorder);
    }

    if (m_toolbar != nullptr)
    {
        m_toolbar->SetStripColors (m_theme->navStrip, m_theme->navItemText);
        m_tooltip.SetTheme (*m_theme);
    }

    //  The tabs sit on the caption's color, as Explorer's do, and the selected
    //  one joins the row below it, the menu bar's strip.
    if (m_tabs != nullptr)
    {
        m_tabs->SetStripFill    (m_theme->titleBarTop);
        m_tabs->SetSelectedFill (m_theme->navStrip);
    }

    if (GetHwnd() != nullptr)
    {
        uint32_t  background = m_theme->Background();
        int       luminance  = (int) (((background >> 16) & 0xFF) * 299 + ((background >> 8) & 0xFF) * 587 + (background & 0xFF) * 114) / 1000;

        DxuiDwm::ApplyImmersiveDarkMode (GetHwnd(), luminance < 128);
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::Layout
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);

    m_client = boundsDip;
    m_scaler.SetDpi (scaler.GetDpi());

    RecomputeLayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::RecomputeLayout
//
//  Menu strip and toolbar across the top, status band across the bottom, and
//  the three panes in what the dock leaves between them. The tree splitter
//  spans the whole body so its limits are measured against it; the preview
//  splitter spans everything right of the tree, with its position the list's
//  width.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::RecomputeLayout()
{
    IDxuiControl *  bands[]  = { &m_tabBand, &m_menuBand, &m_toolbarBand, &m_statusBand, &m_bodyBand };
    RECT            body     = {};
    RECT            right    = {};
    RECT            sashRect = {};
    int             rightDip = 0;
    bool            preview  = m_prefs.previewVisible;



    if (m_menuBar == nullptr || m_client.bottom <= m_client.top)
    {
        return;
    }

    //  The toolbar's band thickness follows the responsive mode it plans for
    //  the width, so plan it before the bands are docked.
    m_toolbar->PlanForWidth (m_client.right - m_client.left, m_scaler);

    m_tabBand.SetThickness     (m_scaler.ToPx (kTabHeightDip));
    m_menuBand.SetThickness    (DxuiMenuBar::GetStripHeightPx (m_scaler.GetDpi()));
    m_toolbarBand.SetThickness (m_scaler.ToPx (m_toolbar->GetBandDp()));
    m_statusBand.SetThickness  (m_scaler.ToPx (DxuiStatusBar::GetBandDp()));

    m_dock.Arrange (m_client, m_scaler, bands);

    body = m_bodyBand.GetBounds();

    if (body.bottom <= body.top)
    {
        return;
    }

    m_tabs->Layout (m_tabBand.GetBounds(), m_scaler);
    FillTabs();

    m_menuBar->SetHostClientRect (m_client);
    m_menuBar->Layout (m_menuBand.GetBounds(), m_scaler);

    m_toolbar->SetHostClientRect (m_client);
    m_toolbar->Layout (m_toolbarBand.GetBounds(), m_scaler);
    m_address->Layout (m_toolbar->GetFreeRect(), m_scaler);

    m_tooltip.SetDpi (m_scaler.GetDpi());
    m_tooltip.SetViewportSize (m_client.right - m_client.left, m_client.bottom - m_client.top);
    m_status->Layout (m_statusBand.GetBounds(), m_scaler);

    m_treeSplitter->Layout (body, m_scaler);
    m_treeSplitter->SetLimitsDip (kMinTreeWidthDip, kMinListWidthDip + (preview ? kMinPreviewWidthDip : 0));
    m_treeSplitter->SetPositionDip (m_prefs.treeWidthDip);

    sashRect = m_treeSplitter->GetSashRect();
    m_tree->Layout (RECT { body.left, body.top, sashRect.left, body.bottom }, m_scaler);

    //  The tree has no height until its first layout, so the node revealed at
    //  startup is scrolled into view here.
    if (m_treeRevealPending && m_tree->GetRowCap() > 0)
    {
        m_tree->EnsureRowVisible (m_tree->GetHighlight());
        m_treeRevealPending = false;
    }

    right    = RECT { sashRect.right, body.top, body.right, body.bottom };
    rightDip = MulDiv (right.right - right.left, (int) DxuiDpiScaler::kBaseDpi, (int) m_scaler.GetDpi());

    m_previewSplitter->SetVisible (preview);
    m_previewList->SetVisible (preview && m_previewList->IsVisible());
    m_hexView->SetVisible (preview && m_hexView->IsVisible());
    m_textView->SetVisible (preview && m_textView->IsVisible());
    m_picture->SetVisible (preview && m_picture->IsVisible());
    m_previewMessage->SetVisible (preview && m_previewMessage->IsVisible());

    if (preview)
    {
        m_previewSplitter->Layout (right, m_scaler);
        m_previewSplitter->SetLimitsDip (kMinListWidthDip, kMinPreviewWidthDip);
        m_previewSplitter->SetPositionDip (rightDip - m_prefs.previewWidthDip - DxuiSplitter::kSashDip);

        sashRect = m_previewSplitter->GetSashRect();

        RECT  listRect    = { right.left, right.top, sashRect.left, right.bottom };
        RECT  previewRect = { sashRect.right, right.top, right.right, right.bottom };
        int   barPx       = m_scaler.ToPx (m_previewToolbar->GetBandDp());

        m_previewRect = previewRect;
        m_previewToolbar->SetVisible (m_previewBarMode != 0);

        //  The preview's toolbar sits across its top, and the content below it.
        if (m_previewBarMode != 0)
        {
            m_previewToolbar->PlanForWidth (previewRect.right - previewRect.left, m_scaler);
            barPx = m_scaler.ToPx (m_previewToolbar->GetBandDp());

            m_previewToolbar->SetHostClientRect (m_client);
            m_previewToolbar->Layout (RECT { previewRect.left, previewRect.top, previewRect.right, previewRect.top + barPx }, m_scaler);
            previewRect.top += barPx;
        }

        m_list->Layout           (listRect,    m_scaler);
        m_listMessage->Layout    (listRect,    m_scaler);
        m_previewList->Layout    (previewRect, m_scaler);
        m_textView->Layout       (previewRect, m_scaler);
        m_hexView->Layout        (previewRect, m_scaler);
        m_picture->Layout        (previewRect, m_scaler);
        m_previewMessage->Layout (previewRect, m_scaler);
    }
    else
    {
        m_previewToolbar->SetVisible (false);
        m_list->Layout        (right, m_scaler);
        m_listMessage->Layout (right, m_scaler);
    }

    LayoutStatusFields();

    m_dropHits.Clear();
    m_dropHits.Register (DxuiHitRect { m_list->GetBounds(), DxuiHitSlot::Custom, kDropTagList });
    m_dropHits.Register (DxuiHitRect { m_tree->GetBounds(), DxuiHitSlot::Custom, kDropTagTree });

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::Paint
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT  bounds = GetBounds();



    painter.FillRect ((float) bounds.left, (float) bounds.top,
                      (float) (bounds.right - bounds.left), (float) (bounds.bottom - bounds.top), theme.Background());

    //  Every preview draws on the content surface, a message included, which
    //  has no background of its own.
    if (m_prefs.previewVisible && m_previewRect.right > m_previewRect.left)
    {
        painter.FillRect ((float) m_previewRect.left, (float) m_previewRect.top,
                          (float) (m_previewRect.right - m_previewRect.left), (float) (m_previewRect.bottom - m_previewRect.top),
                          theme.ContentBackground());
    }

    DxuiWindow::Paint (painter, text, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::RevealLocationInTree
//
//  Expands This PC down to the current location's folder and highlights it,
//  as File Explorer's navigation pane does when a window opens on a folder.
//  Each path component is matched against the child labels without regard to
//  case, since a restored path need not match the case on disk.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::RevealLocationInTree()
{
    Location              location = m_browser.GetLocation();
    std::wstring          path     = location.path;
    std::wstring          current  = m_tree->GetHighlightedId();
    int                   casso    = m_tree->FindRowById (TreeModel::kCassoRootId);
    size_t                prefix   = wcslen (TreeModel::kCassoRootId);
    const DxuiTreeNode *  root     = nullptr;
    bool                  wasOpen  = false;
    size_t                known    = 0;
    std::wstring          knownId;
    int                   row      = -1;



    if (location.kind == Location::Kind::None || path.size() < 2 || path[1] != L':')
    {
        return;
    }

    //  A node the user clicked already shows the location, possibly under the
    //  Casso root rather than This PC, so it stays highlighted.
    if (current.size() >= path.size() && _wcsicmp (current.c_str() + current.size() - path.size(), path.c_str()) == 0)
    {
        return;
    }

    //  Casso's known folders come first: the deepest one that holds the
    //  location is where the walk starts. The Casso root is closed again when
    //  none does and it was closed before.
    if (casso >= 0)
    {
        root    = m_tree->GetNodeAt (casso);
        wasOpen = (root != nullptr) && root->expanded;

        m_tree->SetRowExpanded (casso, true);
        root = m_tree->GetNodeAt (m_tree->FindRowById (TreeModel::kCassoRootId));

        for (size_t i = 0; root != nullptr && i < root->children.size(); i++)
        {
            const std::wstring &  id     = root->children[i].id;
            std::wstring          folder = (id.size() > prefix) ? id.substr (prefix) : std::wstring();
            bool                  within = !folder.empty() && folder.size() > known
                                        && _wcsnicmp (path.c_str(), folder.c_str(), folder.size()) == 0
                                        && (path.size() == folder.size() || path[folder.size()] == L'\\' || folder.back() == L'\\');

            if (within)
            {
                known   = folder.size();
                knownId = id;
            }
        }

        if (knownId.empty() && !wasOpen)
        {
            m_tree->SetRowExpanded (m_tree->FindRowById (TreeModel::kCassoRootId), false);
        }
    }

    row = knownId.empty() ? WalkTreeLabels (m_tree->FindRowById (TreeModel::kThisPcRootId), path)
                          : WalkTreeLabels (m_tree->FindRowById (knownId), path.substr (known));

    if (row < 0)
    {
        return;
    }

    m_tree->HighlightRow (row);
    m_treeRevealPending = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::WalkTreeLabels
//
//  Expands down from a row one path component at a time, matching each against
//  the child labels without regard to case, and returns the deepest row
//  reached. A drive's label is its volume name, so a drive is matched by the
//  letter its id ends with instead.
//
////////////////////////////////////////////////////////////////////////////////

int CassqueWindow::WalkTreeLabels (int row, const std::wstring & path)
{
    size_t  start = 0;



    if (row < 0)
    {
        return -1;
    }

    while (start < path.size())
    {
        size_t                slash = path.find (L'\\', start);
        std::wstring          label = path.substr (start, (slash == std::wstring::npos) ? std::wstring::npos : slash - start);
        const DxuiTreeNode *  node  = nullptr;
        int                   next  = -1;

        start = (slash == std::wstring::npos) ? path.size() : slash + 1;

        if (label.empty())
        {
            continue;
        }

        m_tree->SetRowExpanded (row, true);
        node = m_tree->GetNodeAt (row);

        for (size_t i = 0; node != nullptr && i < node->children.size() && next < 0; i++)
        {
            const std::wstring &  childLabel = node->children[i].label;
            bool                  isDrive    = label.size() == 2 && label[1] == L':';

            if (isDrive ? (_wcsnicmp (node->children[i].id.c_str() + wcslen (TreeModel::kThisPcRootId), label.c_str(), 2) == 0)
                        : (_wcsicmp (childLabel.c_str(), label.c_str()) == 0))
            {
                next = m_tree->FindRowById (node->children[i].id);
            }
        }

        if (next < 0)
        {
            break;
        }

        row = next;
    }

    return row;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::FillList
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::FillList()
{
    std::vector<std::vector<DxuiListView::Cell>>    rows;
    const BrowserModel                            & model    = m_browser.GetBrowserModel();
    std::wstring                                    message  = m_browser.GetListError();
    Location                                        location = m_browser.GetLocation();



    //  The rows are about to change under it.
    if (m_renameRow >= 0)
    {
        EndRename (false);
    }

    for (const CatalogRow & row : m_browser.GetRows())
    {
        rows.push_back (CassqueBrowser::ToCells (row, location, &m_shellIcons));
    }

    m_list->SetRows (std::move (rows));

    //  A new location opens at its first row, as Explorer's does, rather than
    //  wherever the list was scrolled for the last one. THE COLUMNS STAY AS
    //  THEY ARE: their widths belong to the view rather than to the folder, so
    //  they neither twitch from one folder to the next nor pay to re-measure
    //  every cell of every row on each navigation.
    if (m_browser.GetLocation() != m_listLocation)
    {
        m_listLocation = m_browser.GetLocation();
        m_list->SetTopRow (0);
        RevealLocationInTree();
        UpdateWatchedFolders();
    }

    m_list->UpdateAutoFitFromRows();

    m_list->SetSelectedRows (m_browser.GetSelectedRows(), m_browser.GetSelectedRows().empty() ? -1 : m_browser.GetSelectedRows()[0]);


    if (model.HasTabs())
    {
        m_list->SetSortIndicator ((int) model.GetActiveTab().sortColumn, model.GetActiveTab().sortDescending);
    }

    if (message.empty() && m_browser.GetRows().empty() && m_browser.GetLocation().kind != Location::Kind::None)
    {
        message = GetEmptyLocationMessage (m_browser.GetLocation().kind);
    }

    m_listMessage->SetText (message);
    m_listMessage->SetVisible (!message.empty());
    m_list->SetVisible (message.empty());

    FillTabs();
    FillAddress();
    FillPreview();
    FillStatus();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::FillPreview
//
//  The preview's kind picks the widget: a picture in the framebuffer view, a
//  catalog as rows under the list's own columns, everything else as lines.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::FillPreview()
{
    const PreviewContent                          & preview  = m_browser.GetPreview();
    std::vector<std::vector<DxuiListView::Cell>>    rows;
    bool                                            visible  = m_prefs.previewVisible;
    bool                                            picture  = preview.kind == PreviewContent::Kind::Picture && !preview.bgra.empty();
    bool                                            hostFile = preview.kind == PreviewContent::Kind::Hex && !preview.hostPath.empty();
    bool                                            opened   = hostFile && (m_fileBytes.GetPath() == preview.hostPath || m_fileBytes.Open (preview.hostPath));
    bool                                            error    = preview.kind == PreviewContent::Kind::Error || (hostFile && !opened);
    bool                                            catalog  = preview.kind == PreviewContent::Kind::Catalog;
    bool                                            hex      = preview.kind == PreviewContent::Kind::Hex && (!preview.bytes.empty() || opened);
    bool                                            columns  = preview.kind == PreviewContent::Kind::Catalog && preview.lines.empty();
    bool                                            program  = preview.kind == PreviewContent::Kind::Listing && !preview.bytes.empty();
    int                                             mode     = !visible ? 0 : hex ? 2 : program ? 1 : 0;



    if (picture)
    {
        m_picture->SetFramebuffer (preview.bgra.data(), preview.width, preview.height);
    }
    else
    {
        m_picture->Clear();
    }

    //  A catalog of any volume but DOS 3.3 or ProDOS arrives as rows under
    //  sortable columns. Everything else shown as text goes to the text view,
    //  where it wraps and selects by character.
    if (columns)
    {
        m_previewList->SetShowHeader (true);
        m_previewList->SetColumns (CassqueBrowser::GetCatalogPreviewColumns());

        for (const CatalogRow & row : preview.rows)
        {
            rows.push_back (CassqueBrowser::ToCatalogPreviewCells (row));
        }
    }

    m_previewList->SetRows (std::move (rows));
    m_previewList->ResetAutoFit();
    m_previewList->UpdateAutoFitFromRows();
    m_previewList->SetTopRow (0);

    m_textView->SetRows (BuildTextRows (preview, m_prefs.lineAddresses));

    //  A new kind of preview brings its own toolbar, which changes the height
    //  left for the content.
    if (mode != m_previewBarMode)
    {
        m_previewBarMode = mode;

        if (mode != 0)
        {
            m_previewToolbar->SetEntries (m_commands.BuildPreviewToolbarEntries (mode == 2, &m_searchBox, &m_goToBox));
        }

        //  The boxes go with the hex view's toolbar, and their focus with it.
        if (mode != 2 && (m_focus == Pane::Search || m_focus == Pane::GoTo))
        {
            SetFocusPane (Pane::Preview);
        }

        RecomputeLayout();
    }

    //  The hex view reads the preview's own bytes where they lie. The source
    //  is re-pointed rather than refilled, so a file of any size costs the
    //  same here as a short one.
    //  A host file is read from the file as the view draws it; an Apple file
    //  is already in memory. Text opens showing only its characters.
    if (!opened)
    {
        m_fileBytes.Close();
    }

    m_previewBytes.SetBytes ((hex && !opened) ? &preview.bytes : nullptr);
    m_hexView->SetSource (!hex ? nullptr : opened ? (const IDxuiHexSource *) &m_fileBytes : (const IDxuiHexSource *) &m_previewBytes);
    m_hexView->SetOriginAddress (preview.origin);
    m_hexView->SetTextEncoding (opened ? DxuiHexView::TextEncoding::Ascii : DxuiHexView::TextEncoding::AppleHighBit);

    if (hex)
    {
        m_hexView->SetShowValues (!(opened ? m_fileBytes.LooksLikeText() : preview.textFile));
    }

    m_previewMessage->SetText (!error                  ? std::wstring()
                               : (hostFile && !opened) ? FormatPreviewError (L"The file could not be read")
                                                       : FormatPreviewError (preview.message));
    m_previewMessage->SetVisible (visible && error);
    m_picture->SetVisible (visible && picture);
    m_previewList->SetVisible (visible && columns);
    m_textView->SetVisible (visible && !picture && !error && !hex && !columns);
    m_hexView->SetVisible (visible && hex);

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::FileBytes::Open
//
//  Shared for reading, writing and deletion, so previewing a file never gets
//  in the way of another program using it.
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueWindow::FileBytes::Open (const std::wstring & path)
{
    LARGE_INTEGER  size = {};



    Close();

    m_file = CreateFileW (path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                          nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (m_file == INVALID_HANDLE_VALUE || !GetFileSizeEx (m_file, &size))
    {
        Close();
        return false;
    }

    m_path = path;
    m_size = (uint64_t) size.QuadPart;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::FileBytes::Close
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::FileBytes::Close()
{
    if (m_file != INVALID_HANDLE_VALUE)
    {
        CloseHandle (m_file);
    }

    m_file       = INVALID_HANDLE_VALUE;
    m_size       = 0;
    m_windowBase = 0;

    m_path.clear();
    m_window.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::FileBytes::LooksLikeText
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueWindow::FileBytes::LooksLikeText() const
{
    std::vector<uint8_t>  sample ((size_t) (std::min) ((uint64_t) kTextSampleBytes, m_size));



    if (sample.empty())
    {
        return false;
    }

    ReadAt (0, sample);

    for (uint8_t byte : sample)
    {
        if ((byte < 0x20 || byte > 0x7E) && byte != '\t' && byte != '\r' && byte != '\n')
        {
            return false;
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::FileBytes::ReadBytes
//
//  The window starts a little before the bytes asked for, so scrolling back
//  a few rows does not move it again straight away.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::FileBytes::ReadBytes (uint64_t offset, std::span<uint8_t> out) const
{
    uint64_t  end = offset + out.size();



    if (out.size() > kWindowBytes / 2)
    {
        ReadAt (offset, out);
        return;
    }

    if (m_window.empty() || offset < m_windowBase || end > m_windowBase + m_window.size())
    {
        m_windowBase = (offset > kWindowBytes / 4) ? (offset - kWindowBytes / 4) : 0;
        m_window.resize ((size_t) (std::min) ((uint64_t) kWindowBytes, m_size - (std::min) (m_windowBase, m_size)));

        ReadAt (m_windowBase, m_window);
    }

    if (end > m_windowBase + m_window.size())
    {
        ReadAt (offset, out);
        return;
    }

    std::copy_n (m_window.begin() + (ptrdiff_t) (offset - m_windowBase), out.size(), out.begin());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::FileBytes::ReadAt
//
//  Bytes the file no longer has, because it shrank while shown, read as zero.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::FileBytes::ReadAt (uint64_t offset, std::span<uint8_t> out) const
{
    LARGE_INTEGER  position = {};
    DWORD          got      = 0;



    std::fill (out.begin(), out.end(), (uint8_t) 0);

    if (m_file == INVALID_HANDLE_VALUE || out.empty())
    {
        return;
    }

    position.QuadPart = (LONGLONG) offset;

    //  A failed seek or read leaves the zeros already there.
    if (SetFilePointerEx (m_file, position, nullptr, FILE_BEGIN) &&
        !ReadFile (m_file, out.data(), (DWORD) out.size(), &got, nullptr))
    {
        return;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::FillStatus
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::FillStatus()
{
    const CassqueBrowser::Status &  status = m_browser.GetStatus();



    std::wstring  detail = status.detail;
    uint64_t      origin = m_hexView->GetOriginAddress();



    //  Bytes selected in the hex view take the detail section: where they
    //  are, and how many.
    if (IsHexPreviewShowing() && m_hexView->HasSelection())
    {
        detail = (m_hexView->GetSelectionCount() == 1)
               ? std::format (L"${:04X}, 1 byte selected", origin + m_hexView->GetSelectionFirst())
               : std::format (L"${:04X}{}${:04X}, {} bytes selected",
                              origin + m_hexView->GetSelectionFirst(),
                              s_kchEnDash,
                              origin + m_hexView->GetSelectionLast(),
                              m_hexView->GetSelectionCount());
    }

    //  Characters selected in a text preview take it the same way.
    if (IsTextPreviewShowing() && m_textView->HasSelection())
    {
        size_t  chars = 0;

        for (wchar_t ch : m_textView->GetSelectionText())
        {
            chars += (ch != L'\r' && ch != L'\n') ? 1 : 0;
        }

        detail = (chars == 1) ? std::wstring (L"1 character selected") : std::format (L"{} characters selected", chars);
    }

    m_status->SetText (0, status.selection);
    m_status->SetText (1, status.freeSpace);
    m_status->SetText (2, detail);
    m_status->SetText (3, std::format (L"{}%", m_prefs.previewZoom));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::LayoutStatusFields
//
//  The item count stretches, and free space follows it. The preview's two
//  fields together are as wide as the preview pane, so they begin at its
//  left edge; with the preview hidden they keep a fixed width.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::LayoutStatusFields()
{
    RECT   band    = m_statusBand.GetBounds();
    bool   preview = m_prefs.previewVisible && (m_previewRect.right > m_previewRect.left);
    RECT   sash    = m_previewSplitter->GetSashRect();
    int    seam    = (int) (DxuiSplitter::GetSeam (sash.left, sash.right, m_scaler) + DxuiSplitter::GetLinePx (m_scaler));
    int    zoomPx  = m_scaler.ToPx (kStatusZoomDip);
    int    detail  = preview ? (std::max) ((int) band.right - seam - zoomPx, 0) : m_scaler.ToPx (kStatusDetailDip);



    //  The detail field starts on the splitter's visible line: the lighter of
    //  the two it draws in the middle of its sash, since the darker one is the
    //  color of the panes beside it. The two dividers then meet.
    m_status->SetFields ({ { L"", 0, true },
                           { L"", kStatusFreeDip, false },
                           { L"", 0, false, detail },
                           { L"", kStatusZoomDip, false } });
    m_status->Layout (band, m_scaler);

    FillStatus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::SetFocusPane
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::SetFocusPane (Pane pane)
{
    if ((pane == Pane::Preview && !m_prefs.previewVisible)
        || (pane == Pane::PreviewToolbar && !m_previewToolbar->IsVisible())
        || ((pane == Pane::Search || pane == Pane::GoTo) && m_previewBarMode != 2))
    {
        pane = Pane::Tree;
    }

    m_focus = pane;

    m_tree->OnFocusChanged        (pane == Pane::Tree);
    m_list->OnFocusChanged        (pane == Pane::List);
    m_previewList->OnFocusChanged (pane == Pane::Preview);
    m_hexView->OnFocusChanged     (pane == Pane::Preview);
    m_tabs->OnFocusChanged        (pane == Pane::Tabs);
    m_address->OnFocusChanged     (pane == Pane::Address);
    m_toolbar->SetFocusIndex      (pane == Pane::Toolbar ? m_toolbarFocus : -1);
    m_previewToolbar->SetFocusIndex (pane == Pane::PreviewToolbar ? m_previewBarFocus : -1);
    m_searchBox.SetFocused          (pane == Pane::Search);
    m_goToBox.SetFocused            (pane == Pane::GoTo);

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::GetFocusStop
//
////////////////////////////////////////////////////////////////////////////////

FocusStop CassqueWindow::GetFocusStop() const
{
    switch (m_focus)
    {
        case Pane::Toolbar: return FocusStop { FocusStop::Kind::ToolbarEntry, m_toolbarFocus };
        case Pane::PreviewToolbar: return FocusStop { FocusStop::Kind::PreviewToolbarEntry, m_previewBarFocus };
        case Pane::Search:         return FocusStop { FocusStop::Kind::PreviewToolbarEntry, GetPreviewStopIndex (CassqueCommands::kFind) };
        case Pane::GoTo:           return FocusStop { FocusStop::Kind::PreviewToolbarEntry, GetPreviewStopIndex (CassqueCommands::kGoToOffset) };
        case Pane::Address: return FocusStop { FocusStop::Kind::Address };
        case Pane::Tabs:    return FocusStop { FocusStop::Kind::Tabs };
        case Pane::List:    return FocusStop { FocusStop::Kind::List };
        case Pane::Preview: return FocusStop { FocusStop::Kind::Preview };
        default:            return FocusStop { FocusStop::Kind::Tree };
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::SetFocusStop
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::SetFocusStop (const FocusStop & stop)
{
    switch (stop.kind)
    {
        case FocusStop::Kind::ToolbarEntry:
            m_toolbarFocus = stop.entry;
            SetFocusPane (Pane::Toolbar);
            break;

        case FocusStop::Kind::PreviewToolbarEntry:
            m_previewBarFocus = stop.entry;
            SetFocusPane ((stop.entry == GetPreviewStopIndex (CassqueCommands::kFind))       ? Pane::Search
                        : (stop.entry == GetPreviewStopIndex (CassqueCommands::kGoToOffset)) ? Pane::GoTo
                                                                                             : Pane::PreviewToolbar);
            break;

        case FocusStop::Kind::Address: m_address->SetFocusCue (true); SetFocusPane (Pane::Address); break;
        case FocusStop::Kind::Tabs:    SetFocusPane (Pane::Tabs);    break;
        case FocusStop::Kind::List:    SetFocusPane (Pane::List);    break;
        case FocusStop::Kind::Preview: SetFocusPane (Pane::Preview); break;
        default:                       SetFocusPane (Pane::Tree);    break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::BuildFocusStops
//
//  A disabled toolbar button, such as Back with no history, is not a stop,
//  as disabled controls are not tab stops in Windows.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<FocusStop> CassqueWindow::BuildFocusStops() const
{
    std::vector<bool>  enabled;
    std::vector<bool>  previewEnabled;



    for (size_t i = 0; i < CassqueCommands::GetToolbarEntryCount(); i++)
    {
        enabled.push_back (IsEnabled (CassqueCommands::GetToolbarCommandId (i)));
    }

    for (int id : (m_previewBarMode != 0) ? CassqueCommands::GetPreviewToolbarCommandIds (m_previewBarMode == 2) : std::vector<int>())
    {
        previewEnabled.push_back (IsEnabled (id));
    }

    return FocusRing::BuildStops (enabled, m_prefs.previewVisible, previewEnabled);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::RouteToolbarKey
//
//  Enter, Space or Down presses the focused button; Left and Right walk the
//  strip's usable buttons and stop at its ends. A picker or a flyout opened
//  from the keyboard owns every key until it closes.
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueWindow::RouteToolbarKey (bool preview, const DxuiKeyEvent & ev)
{
    DxuiToolbar &  toolbar = preview ? *m_previewToolbar : *m_toolbar;



    if (toolbar.OwnsKeyboard())
    {
        return toolbar.HandleKey (ev.vk);
    }

    switch (ev.vk)
    {
        case VK_RETURN:
        case VK_SPACE:
        case VK_DOWN:
            toolbar.ActivateFocused();
            return true;

        case VK_LEFT:
        case VK_RIGHT:
            StepToolbarFocus (preview, ev.vk == VK_RIGHT);
            return true;

        default:
            return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::StepToolbarFocus
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::StepToolbarFocus (bool preview, bool forward)
{
    std::vector<int>  ids;
    int               step = forward ? 1 : -1;
    int               at   = (preview ? m_previewBarFocus : m_toolbarFocus) + step;



    if (preview)
    {
        ids = CassqueCommands::GetPreviewToolbarCommandIds (m_previewBarMode == 2);
    }
    else
    {
        for (size_t i = 0; i < CassqueCommands::GetToolbarEntryCount(); i++)
        {
            ids.push_back (CassqueCommands::GetToolbarCommandId (i));
        }
    }

    while (at >= 0 && at < (int) ids.size())
    {
        if (IsEnabled (ids[(size_t) at]))
        {
            SetFocusStop (FocusStop { preview ? FocusStop::Kind::PreviewToolbarEntry : FocusStop::Kind::ToolbarEntry, at });
            return;
        }

        at += step;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::Contains
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueWindow::Contains (const RECT & rect, POINT point)
{
    return point.x >= rect.left && point.x < rect.right && point.y >= rect.top && point.y < rect.bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ToLocal
//
//  List views take their events relative to their own origin.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMouseEvent CassqueWindow::ToLocal (const DxuiMouseEvent & ev, const RECT & bounds)
{
    DxuiMouseEvent  local = ev;



    local.positionDip.x -= bounds.left;
    local.positionDip.y -= bounds.top;

    return local;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::OnMouse
//
//  The menu bar and the splitters take first refusal, since both reach over
//  the panes. A press in a pane moves focus to it; a list that is mid-drag
//  keeps every event until the button comes up.
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueWindow::OnMouse (const DxuiMouseEvent & ev)
{
    POINT  point = ev.positionDip;
    bool   press = ev.kind == DxuiMouseEventKind::Down;



    //  A five-button mouse's back and forward buttons move through the tab's
    //  history wherever the pointer is, as in Explorer.
    if (ev.kind == DxuiMouseEventKind::Up && (ev.button == DxuiMouseButton::X1 || ev.button == DxuiMouseButton::X2))
    {
        Dispatch (ev.button == DxuiMouseButton::X1 ? CassqueCommands::kBack : CassqueCommands::kForward);
        return true;
    }

    //  A scrollbar widens while the pointer is over it, as Explorer's do, and
    //  narrows again when the pointer moves off it or out of the window.
    if (ev.kind == DxuiMouseEventKind::Move || ev.kind == DxuiMouseEventKind::Leave)
    {
        POINT  at      = (ev.kind == DxuiMouseEventKind::Leave) ? POINT { -1, -1 } : point;
        int    changed = (int) m_hexView->SetScrollbarHover (at)
                       | (int) m_textView->SetScrollbarHover (at)
                       | (int) m_list->SetScrollbarHover (at)
                       | (int) m_previewList->SetScrollbarHover (at)
                       | (int) m_tree->SetScrollbarHover (at)
                       | (int) m_picture->SetScrollbarHover (at);

        if (changed != 0)
        {
            Invalidate();
        }
    }

    if (m_renameRow >= 0)
    {
        if (Contains (m_renameBox->GetBounds(), point) || (ev.kind != DxuiMouseEventKind::Down && ev.kind != DxuiMouseEventKind::Wheel))
        {
            m_renameBox->OnMouse (ev);
            Invalidate();

            if (Contains (m_renameBox->GetBounds(), point))
            {
                return true;
            }
        }
        else
        {
            EndRename (true);
        }
    }

    //  A click on the zoom level puts it back to 100%.
    if (ev.kind == DxuiMouseEventKind::Up && ev.button == DxuiMouseButton::Left
        && Contains (m_status->GetFieldRect (3), point))
    {
        Dispatch (CassqueCommands::kZoomReset);
        return true;
    }

    //  Ctrl with the wheel over the preview zooms it, as in a browser.
    if (ev.kind == DxuiMouseEventKind::Wheel && ev.ctrl && !ev.wheelHorizontal
        && m_prefs.previewVisible && Contains (m_previewRect, point))
    {
        Dispatch ((ev.wheelDelta > 0.0f) ? CassqueCommands::kZoomIn : CassqueCommands::kZoomOut);
        return true;
    }

    if (m_menuBar->OnMouse (ev))
    {
        return true;
    }

    //  The address bar sits in the toolbar's row, so it answers first. Moves
    //  reach it wherever the pointer is, so its hover clears on the way out.
    if (ev.kind == DxuiMouseEventKind::Move && !m_address->IsInteracting() && m_address->OnMouse (ev))
    {
        Invalidate();
    }

    if (ev.kind != DxuiMouseEventKind::Move || m_address->IsInteracting())
    {
        if (m_address->IsInteracting() || Contains (m_address->GetBounds(), point))
        {
            if (press)
            {
                SetFocusPane (Pane::Address);
                m_address->SetFocusCue (false);
            }

            if (m_address->OnMouse (ev))
            {
                Invalidate();
                return true;
            }
        }
    }

    if (m_previewToolbar->IsVisible()
        && (Contains (m_previewToolbar->GetBounds(), point) || ev.kind == DxuiMouseEventKind::Up)
        && RouteToolbarMouse (*m_previewToolbar, ev))
    {
        Invalidate();
        return true;
    }

    if (RouteToolbarMouse (*m_toolbar, ev))
    {
        Invalidate();
        return true;
    }

    if (m_treeSplitter->OnMouse (ev) || (m_previewSplitter->IsVisible() && m_previewSplitter->OnMouse (ev)))
    {
        Invalidate();
        return true;
    }

    //  A tab being dragged keeps the pointer until the button comes up, so the
    //  drag can run past the strip's ends and scroll it.
    if (m_tabs->IsInteracting())
    {
        m_tabs->OnMouse (ev);
        Invalidate();
        return true;
    }

    //  A right-click on a tab opens its menu, as Explorer's does.
    if (press && ev.button == DxuiMouseButton::Right && Contains (m_tabs->GetBounds(), point)
        && m_tabs->HitTest (point.x, point.y) >= 0)
    {
        ShowTabContextMenu (point.x, point.y, m_tabs->HitTest (point.x, point.y));
        return true;
    }

    if (Contains (m_tabs->GetBounds(), point) && m_tabs->OnMouse (ev))
    {
        Invalidate();
        return true;
    }

    //  Mouse input goes to a widget mid-drag until the button is released,
    //  wherever the pointer is. Otherwise a scrollbar thumb dragged out of its
    //  pane transfers the drag to the pane under the pointer.
    if (m_list->IsInteracting())
    {
        m_list->OnMouse (ToLocal (ev, m_list->GetBounds()));
        Invalidate();
        return true;
    }

    //  The tree takes points in the window's coordinates, as its press did, so
    //  a drag that started there keeps the same origin.
    if (m_tree->IsInteracting())
    {
        m_tree->OnMouse (ev);
        Invalidate();
        return true;
    }

    if (Contains (m_tree->GetBounds(), point))
    {
        if (press)
        {
            SetFocusPane (Pane::Tree);
        }

        if (press && ev.button == DxuiMouseButton::Right)
        {
            DxuiMouseEvent  left = ev;

            left.button = DxuiMouseButton::Left;
            left.kind   = DxuiMouseEventKind::Down;
            m_tree->OnMouse (left);
            left.kind   = DxuiMouseEventKind::Up;
            m_tree->OnMouse (left);

            ShowTreeContextMenu (point.x, point.y, m_tree->GetHighlightedId());
            return true;
        }

        m_tree->OnMouse (ev);
        Invalidate();
        return true;
    }

    if (m_list->IsVisible() && Contains (m_list->GetBounds(), point))
    {
        DxuiMouseEvent  local = ToLocal (ev, m_list->GetBounds());

        if (press)
        {
            SetFocusPane (Pane::List);
        }

        if (press && ev.button == DxuiMouseButton::Right)
        {
            int  row = m_list->HitTestRow (local.positionDip.x, local.positionDip.y);

            if (row >= 0 && !m_list->IsRowSelected (row))
            {
                m_list->ClickRow (row, false, false);
            }
            else if (row < 0)
            {
                //  The empty space's menu is the folder's, so nothing stays
                //  selected for it to act on.
                m_list->ClearSelection();
            }

            ShowListContextMenu (point.x, point.y);
            return true;
        }

        if (press && ev.button == DxuiMouseButton::Left)
        {
            m_dragArmed = true;
            m_dragStart = point;
        }
        else if (ev.kind == DxuiMouseEventKind::Up)
        {
            m_dragArmed = false;
        }
        else if (ev.kind == DxuiMouseEventKind::Move && m_dragArmed
              && (abs (point.x - m_dragStart.x) > GetSystemMetrics (SM_CXDRAG)
               || abs (point.y - m_dragStart.y) > GetSystemMetrics (SM_CYDRAG)))
        {
            m_dragArmed = false;
            BeginDragOut();
            return true;
        }

        m_list->OnMouse (local);
        Invalidate();
        return true;
    }

    //  A drag selecting lines keeps the pointer until the button comes up, as
    //  the file list's does.
    if (m_previewList->IsInteracting())
    {
        m_previewList->OnMouse (ToLocal (ev, m_previewList->GetBounds()));
        Invalidate();
        return true;
    }

    if (m_hexView->IsDragging())
    {
        m_hexView->OnMouse (ev);
        Invalidate();
        return true;
    }

    //  A zoomed picture pans by dragging, its scrollbars and the wheel, and
    //  keeps the pointer while a drag is under way.
    if (m_picture->IsInteracting()
        || (m_picture->IsVisible() && Contains (m_picture->GetBounds(), point) && m_picture->OnMouse (ev)))
    {
        if (m_picture->IsInteracting())
        {
            m_picture->OnMouse (ev);
        }

        if (press)
        {
            SetFocusPane (Pane::Preview);
        }

        Invalidate();
        return true;
    }

    if (m_textView->IsInteracting())
    {
        m_textView->OnMouse (ev);
        FillStatus();
        Invalidate();
        return true;
    }

    //  DxuiHexView takes points in the same coordinates as its bounds, so the
    //  event is passed unchanged, not converted to widget-local coordinates as
    //  the older list views require.
    if (m_hexView->IsVisible() && Contains (m_hexView->GetBounds(), point))
    {
        if (press)
        {
            SetFocusPane (Pane::Preview);
        }

        m_hexView->OnMouse (ev);
        Invalidate();
        return true;
    }

    if (m_textView->IsVisible() && Contains (m_textView->GetBounds(), point))
    {
        if (press)
        {
            SetFocusPane (Pane::Preview);
        }

        m_textView->OnMouse (ev);
        FillStatus();
        Invalidate();
        return true;
    }

    if (m_previewList->IsVisible() && Contains (m_previewList->GetBounds(), point))
    {
        if (press)
        {
            SetFocusPane (Pane::Preview);
        }

        m_previewList->OnMouse (ToLocal (ev, m_previewList->GetBounds()));
        Invalidate();
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR CassqueWindow::GetCursorForPoint (POINT clientPx) const
{
    LPCWSTR  cursor = m_treeSplitter->GetCursorForPoint (clientPx);
    RECT     bounds = m_list->GetBounds();



    if (cursor == nullptr && m_previewSplitter->IsVisible())
    {
        cursor = m_previewSplitter->GetCursorForPoint (clientPx);
    }

    if (cursor == nullptr && m_picture->IsVisible() && Contains (m_picture->GetBounds(), clientPx))
    {
        cursor = m_picture->GetCursorForPoint (clientPx);
    }

    if (cursor == nullptr && m_list->IsVisible() && Contains (bounds, clientPx))
    {
        cursor = m_list->GetCursorForPoint (POINT { clientPx.x - bounds.left, clientPx.y - bounds.top });
    }

    return cursor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::OnKey
//
//  Command keys first, then the menu bar's mnemonics and its open dropdown,
//  then Tab between panes, and the rest to the focused pane.
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueWindow::OnKey (const DxuiKeyEvent & ev)
{
    int   command = 0;
    bool  handled = false;



    //  A rename in place takes every key: Enter keeps the name, Escape puts
    //  the old one back.
    if (m_renameRow >= 0)
    {
        if (ev.kind == DxuiKeyEventKind::Down && (ev.vk == VK_RETURN || ev.vk == VK_ESCAPE))
        {
            EndRename (ev.vk == VK_RETURN);
        }
        else
        {
            m_renameBox->OnKey (ev);
        }

        Invalidate();
        return true;
    }

    //  The search box takes keys and characters first while it has focus;
    //  Tab and the keys it has no use for carry on as usual.
    if (m_focus == Pane::Search || m_focus == Pane::GoTo)
    {
        handled = (m_focus == Pane::Search) ? m_searchBox.OnKey (ev) : m_goToBox.OnKey (ev);

        if (handled || ev.kind != DxuiKeyEventKind::Down)
        {
            Invalidate();
            return handled;
        }
    }

    //  While the address bar is being edited, keys and characters go to its
    //  text field first; the keys it leaves, Tab and Ctrl+T among them, carry
    //  on as usual.
    if (m_focus == Pane::Address && m_address->IsEditing())
    {
        handled = m_address->OnKey (ev);

        if (handled || ev.kind != DxuiKeyEventKind::Down)
        {
            Invalidate();
            return handled;
        }
    }

    //  A character typed at the file list jumps to a row. It has to reach the
    //  list before characters are turned away below.
    if (ev.kind == DxuiKeyEventKind::Char && m_focus == Pane::List)
    {
        handled = m_list->OnKey (ev);

        if (handled)
        {
            Invalidate();
        }

        return handled;
    }

    if (ev.kind != DxuiKeyEventKind::Down)
    {
        return false;
    }

    command = CassqueCommands::TranslateKey (ev.vk, ev.ctrl, ev.alt, ev.shift);

    if (command != 0)
    {
        Dispatch (command);
        return true;
    }

    if (m_menuBar->IsOpen() || m_menuBar->HasFocus())
    {
        return m_menuBar->OnKey (ev);
    }

    if (ev.alt && ev.vk >= 0x20 && ev.vk <= 0x7E && m_menuBar->HandleAltKey ((wchar_t) ev.vk))
    {
        return true;
    }

    if ((ev.vk == VK_APPS || (ev.vk == VK_F10 && ev.shift)) && m_focus == Pane::List)
    {
        RECT  bounds = m_list->GetBounds();

        ShowListContextMenu (bounds.left + m_scaler.ToPx (24), bounds.top + m_scaler.ToPx (40));
        return true;
    }

    if (m_focus == Pane::List && ev.ctrl && !ev.alt && !m_browser.IsImageLocation()
        && m_browser.GetLocation().kind == Location::Kind::HostFolder
        && (ev.vk == 'X' || ev.vk == 'C' || ev.vk == 'V'))
    {
        if (ev.vk == 'V')
        {
            RunVerb (CassqueActions::Verb::Paste);
        }
        else if (!m_browser.GetSelectedRows().empty())
        {
            RunVerb (ev.vk == 'X' ? CassqueActions::Verb::Cut : CassqueActions::Verb::Copy);
        }

        return true;
    }

    if (ev.vk == VK_F2 && m_focus == Pane::List && m_browser.GetSelectedRows().size() == 1)
    {
        RunVerb (CassqueActions::Verb::Rename);
        return true;
    }

    if (ev.vk == VK_DELETE && m_focus == Pane::List && !m_browser.GetSelectedRows().empty())
    {
        RunVerb (CassqueActions::Verb::Delete);
        return true;
    }

    //  The hex view's two columns are stops of their own inside the preview
    //  pane: Tab moves between them while it has one left, and only then does
    //  the walk carry on to the next pane.
    if (ev.vk == VK_TAB && !ev.ctrl && m_focus == Pane::Preview && IsHexPreviewShowing()
        && m_hexView->OnKey (ev))
    {
        Invalidate();
        return true;
    }

    //  Tab moves through the enabled toolbar buttons, the address bar, the tab
    //  strip, the tree, the list and the preview, in the order FocusRing
    //  defines.
    if (ev.vk == VK_TAB && !ev.ctrl)
    {
        FocusStop  next = FocusRing::GetNext (BuildFocusStops(), GetFocusStop(), !ev.shift);

        SetFocusStop (next);

        //  A pane with stops inside it starts at the one the walk's direction
        //  calls for.
        if (next.kind == FocusStop::Kind::Preview && IsHexPreviewShowing())
        {
            m_hexView->OnFocusEntered (!ev.shift);
        }

        return true;
    }

    switch (m_focus)
    {
        case Pane::Toolbar: handled = RouteToolbarKey (false, ev); break;
        case Pane::PreviewToolbar: handled = RouteToolbarKey (true, ev); break;
        case Pane::Search:                                               break;
        case Pane::GoTo:                                                 break;
        case Pane::Address: handled = m_address->OnKey (ev);     break;
        case Pane::Tabs:    handled = m_tabs->OnKey (ev);        break;
        case Pane::Tree:    handled = m_tree->OnKey (ev);        break;
        case Pane::List:    handled = m_list->OnKey (ev);        break;
        case Pane::Preview: handled = IsHexPreviewShowing() ? m_hexView->OnKey (ev)
                                                            : IsTextPreviewShowing() ? m_textView->OnKey (ev)
                                                                                     : m_previewList->OnKey (ev);
                            FillStatus();
                            break;
    }

    if (ev.vk == VK_BACK && !handled)
    {
        Dispatch (CassqueCommands::kUp);
        handled = true;
    }

    Invalidate();

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::IsEnabled
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueWindow::IsEnabled (int id) const
{
    const BrowserModel &  model    = m_browser.GetBrowserModel();
    DxuiStandardCommand   standard = CassqueCommands::GetStandardCommand (id);
    bool                  enabled  = false;



    //  A standard command row is enabled when the focused control reports it
    //  available, and grayed when no control handles it.
    if (standard != DxuiStandardCommand::None)
    {
        return DxuiCommandRouter::Query (GetFocusedControl(), standard, enabled) && enabled;
    }

    switch (id)
    {
        case CassqueCommands::kBack:              return model.HasTabs() && model.CanGoBack();
        case CassqueCommands::kForward:           return model.HasTabs() && model.CanGoForward();
        case CassqueCommands::kUp:                return m_browser.CanGoUp();
        case CassqueCommands::kToggleDisassembly: return model.HasTabs();
        case CassqueCommands::kLineAddresses:     return m_previewBarMode == 1;
        case CassqueCommands::kFindNext:          return IsHexPreviewShowing() && !m_findBytes.empty();
        case CassqueCommands::kFind:
        case CassqueCommands::kNoData:
        case CassqueCommands::kFormatHex:
        case CassqueCommands::kFormatSigned:
        case CassqueCommands::kFormatUnsigned:
        case CassqueCommands::kColumns:
        case CassqueCommands::kColumnsAuto:
        case CassqueCommands::kColumns1:
        case CassqueCommands::kColumns2:
        case CassqueCommands::kColumns4:
        case CassqueCommands::kColumns8:
        case CassqueCommands::kColumns16:
        case CassqueCommands::kGoToOffset:
        case CassqueCommands::kGroup1:
        case CassqueCommands::kGroup2:
        case CassqueCommands::kGroup4:
        case CassqueCommands::kGroup8:            return IsHexPreviewShowing();
        case CassqueCommands::kCloseTab:
        case CassqueCommands::kNextTab:
        case CassqueCommands::kPreviousTab:       return model.GetTabCount() > 1;
        default:                                  return true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::IsChecked
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueWindow::IsChecked (int id) const
{
    const BrowserModel &  model = m_browser.GetBrowserModel();



    switch (id)
    {
        case CassqueCommands::kTogglePreview:     return m_prefs.previewVisible;
        case CassqueCommands::kLineAddresses:     return m_prefs.lineAddresses;
        case CassqueCommands::kToggleDisassembly: return model.HasTabs() && model.GetActiveTab().disassemble;
        case CassqueCommands::kThemeLight:        return m_prefs.theme == CassquePrefs::kThemeLight;
        case CassqueCommands::kThemeDark:         return m_prefs.theme == CassquePrefs::kThemeDark;
        case CassqueCommands::kThemeSystem:       return m_prefs.theme != CassquePrefs::kThemeLight && m_prefs.theme != CassquePrefs::kThemeDark
                                                             && !IsCassoThemeName (m_prefs.theme);
        case CassqueCommands::kThemeSkeuomorphic: return m_prefs.theme == CassquePrefs::kThemeSkeuomorphic;
        case CassqueCommands::kThemeDarkModern:   return m_prefs.theme == CassquePrefs::kThemeDarkModern;
        case CassqueCommands::kThemeRetroTerminal: return m_prefs.theme == CassquePrefs::kThemeRetroTerminal;
        case CassqueCommands::kNoData:            return !m_hexView->IsShowingValues();
        case CassqueCommands::kGroup1:            return m_hexView->IsShowingValues() && m_prefs.hexGrouping == 1;
        case CassqueCommands::kGroup2:            return m_hexView->IsShowingValues() && m_prefs.hexGrouping == 2;
        case CassqueCommands::kGroup4:            return m_hexView->IsShowingValues() && m_prefs.hexGrouping == 4;
        case CassqueCommands::kGroup8:            return m_hexView->IsShowingValues() && m_prefs.hexGrouping == 8;
        case CassqueCommands::kFormatHex:         return m_prefs.hexFormat == CassquePrefs::kHexFormatHex;
        case CassqueCommands::kFormatSigned:      return m_prefs.hexFormat == CassquePrefs::kHexFormatSigned;
        case CassqueCommands::kFormatUnsigned:    return m_prefs.hexFormat == CassquePrefs::kHexFormatUnsigned;
        case CassqueCommands::kColumnsAuto:       return m_prefs.hexColumns == 0;
        case CassqueCommands::kColumns1:          return m_prefs.hexColumns == 1;
        case CassqueCommands::kColumns2:          return m_prefs.hexColumns == 2;
        case CassqueCommands::kColumns4:          return m_prefs.hexColumns == 4;
        case CassqueCommands::kColumns8:          return m_prefs.hexColumns == 8;
        case CassqueCommands::kColumns16:         return m_prefs.hexColumns == 16;
        case CassqueCommands::kNamingDescriptive: return m_prefs.hostNaming != CassquePrefs::kNamingCiderPress;
        case CassqueCommands::kNamingCiderPress:  return m_prefs.hostNaming == CassquePrefs::kNamingCiderPress;
        default:                                  return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::GetFocusedControl
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * CassqueWindow::GetFocusedControl() const
{
    switch (m_focus)
    {
        case Pane::Tabs:    return m_tabs;
        case Pane::Tree:    return m_tree;
        case Pane::List:    return m_list;
        case Pane::Preview: return IsHexPreviewShowing()  ? (IDxuiControl *) m_hexView
                                 : IsTextPreviewShowing() ? (IDxuiControl *) m_textView
                                                          : (IDxuiControl *) m_previewList;
        default:            return nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::IsHexPreviewShowing
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueWindow::IsHexPreviewShowing() const
{
    return m_prefs.previewVisible && m_hexView->IsVisible();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::IsTextPreviewShowing
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueWindow::IsTextPreviewShowing() const
{
    return m_prefs.previewVisible && m_textView->IsVisible();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::BuildTextRows
//
//  A listing that is not a valid program ends with a blank row and the
//  warning that says why.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiTextView::Row> CassqueWindow::BuildTextRows (const PreviewContent & preview, bool lineAddresses)
{
    std::vector<DxuiTextView::Row>  rows;
    DxuiTextView::Row               warning;
    std::vector<Word>               addresses;



    if (lineAddresses && preview.kind == PreviewContent::Kind::Listing)
    {
        addresses = GetLineAddresses (preview.bytes, preview.integerBasic);
    }

    if (!addresses.empty())
    {
        for (size_t i = 0; i < preview.lines.size(); i++)
        {
            DxuiTextView::Row  row { SplitLineNumber (preview.lines[i]) };

            row.cells.insert (row.cells.begin(), (i < addresses.size()) ? std::format (L"${:04X}", addresses[i]) : std::wstring());
            rows.push_back (std::move (row));
        }
    }
    else if (preview.kind == PreviewContent::Kind::Details)
    {
        for (const std::pair<std::wstring, std::wstring> & field : preview.details)
        {
            rows.push_back (DxuiTextView::Row { { field.first, field.second } });
        }
    }
    else
    {
        for (const std::wstring & line : preview.lines)
        {
            rows.push_back (DxuiTextView::Row { (preview.kind == PreviewContent::Kind::Listing) ? SplitLineNumber (line)
                                                                                               : std::vector<std::wstring> { line } });
        }
    }

    if (!preview.warning.empty())
    {
        warning.cells.push_back (preview.warning);
        warning.warning = true;

        rows.push_back (DxuiTextView::Row());
        rows.push_back (warning);
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::GetLineAddresses
//
//  Where each line of a BASIC program starts in memory.
//
//  An Applesoft line begins with a pointer to the next one, and a pointer is
//  an absolute address, so the first line's pointer less the second line's
//  offset in the file is the address the program loads at.
//
//  An Integer BASIC line begins with its length, and the program sits against
//  HIMEM, which DOS 3.3 on a 48K machine leaves at $9600. The addresses assume
//  that HIMEM, since the file does not record one.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Word> CassqueWindow::GetLineAddresses (const std::vector<Byte> & program, bool integerBasic)
{
    std::vector<size_t>  starts;
    std::vector<Word>    addresses;
    size_t               offset    = 0;
    Word                 firstLink = 0;
    Word                 base      = 0;



    if (integerBasic)
    {
        while (offset < program.size() && program[offset] != 0 && offset + program[offset] <= program.size())
        {
            starts.push_back (offset);
            offset += program[offset];
        }

        base = (Word) (kIntegerBasicHimem - program.size());
    }

    while (!integerBasic && offset + 4 <= program.size())
    {
        Word    link = (Word) (program[offset] | (program[offset + 1] << 8));
        size_t  end  = offset + 4;

        if (link == 0)
        {
            break;
        }

        while (end < program.size() && program[end] != 0)
        {
            end++;
        }

        if (end >= program.size())
        {
            break;
        }

        if (starts.empty())
        {
            firstLink = link;
        }

        starts.push_back (offset);
        offset = end + 1;

        if (starts.size() == 1)
        {
            base = (Word) (firstLink - offset);
        }
    }

    for (size_t start : starts)
    {
        addresses.push_back ((Word) (base + start));
    }

    return addresses;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::SplitLineNumber
//
//  A listing line's number and the statement after the spaces that follow it,
//  or the whole line as one cell when it does not start with a number.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> CassqueWindow::SplitLineNumber (const std::wstring & line)
{
    size_t  first = line.find_first_not_of (L' ');
    size_t  end   = std::wstring::npos;
    size_t  text  = std::wstring::npos;



    if (first != std::wstring::npos && iswdigit (line[first]))
    {
        end  = line.find_first_not_of (L"0123456789", first);
        text = (end == std::wstring::npos) ? std::wstring::npos : line.find_first_not_of (L' ', end);
    }

    if (text == std::wstring::npos || text == end)
    {
        return { line };
    }

    return { line.substr (0, end), line.substr (text) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::FormatPreviewError
//
//  "No preview", and three lines below it, why. An image refused for its size
//  carries a whole sentence about the file for the command line; the preview
//  says only the reason.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassqueWindow::FormatPreviewError (const std::wstring & message)
{
    std::wstring  reason = message;



    if (message.find (L"800K images") != std::wstring::npos)
    {
        reason = L"800K images are not supported yet";
    }

    return L"No preview\n\n\n" + reason;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::SetPreviewZoom
//
//  The text and hex previews share one zoom, kept within the range the
//  preferences allow.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::SetPreviewZoom (int percent)
{
    int  clamped = (std::max) (CassquePrefs::kMinPreviewZoom, (std::min) (percent, CassquePrefs::kMaxPreviewZoom));



    m_prefs.previewZoom = clamped;

    m_textView->SetZoom ((float) clamped / 100.0f);
    m_hexView->SetZoom  ((float) clamped / 100.0f);
    m_picture->SetZoom  ((float) clamped / 100.0f);

    FillStatus();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::SetHexColumns
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::SetHexColumns (int columns)
{
    m_hexView->SetColumns (columns);
    m_prefs.hexColumns = columns;

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::SetHexFormat
//
//  Choosing a format shows the values it applies to.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::SetHexFormat (const char * format)
{
    m_prefs.hexFormat     = format;
    m_prefs.hexShowValues = true;

    m_hexView->SetShowValues  (true);
    m_hexView->SetValueFormat (ParseHexFormat (m_prefs.hexFormat));

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::SetHexShowValues
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::SetHexShowValues (bool show)
{
    m_prefs.hexShowValues = show;
    m_hexView->SetShowValues (show);

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ParseHexFormat
//
////////////////////////////////////////////////////////////////////////////////

DxuiHexView::ValueFormat CassqueWindow::ParseHexFormat (const std::string & name)
{
    if (name == CassquePrefs::kHexFormatSigned)
    {
        return DxuiHexView::ValueFormat::Signed;
    }

    if (name == CassquePrefs::kHexFormatUnsigned)
    {
        return DxuiHexView::ValueFormat::Unsigned;
    }

    return DxuiHexView::ValueFormat::Hex;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::GetPreviewStopIndex
//
//  A command's place among the hex view toolbar's stops, or -1 while that
//  toolbar is not showing.
//
////////////////////////////////////////////////////////////////////////////////

int CassqueWindow::GetPreviewStopIndex (int commandId) const
{
    std::vector<int>  ids = CassqueCommands::GetPreviewToolbarCommandIds (true);



    if (m_previewBarMode != 2)
    {
        return -1;
    }

    for (size_t i = 0; i < ids.size(); i++)
    {
        if (ids[i] == commandId)
        {
            return (int) i;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::SetHexGrouping
//
//  A grouping the row cannot divide is refused by the view, and a refused
//  one is not written to the preferences either.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::SetHexGrouping (int grouping)
{
    if (!m_hexView->SetGrouping (grouping))
    {
        return;
    }

    m_prefs.hexGrouping   = grouping;
    m_prefs.hexShowValues = true;

    m_hexView->SetShowValues (true);

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::GoToTyped
//
//  An address matches the offset column, which counts from the file's load
//  address when it has one; an offset moves from the caret. A target that
//  does not parse or is outside the file marks the box in error, and one
//  that is inside it moves the caret there and returns focus to the bytes.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::GoToTyped (const std::wstring & text)
{
    int64_t   origin = (int64_t) m_hexView->GetOriginAddress();
    int64_t   caret  = origin + (int64_t) m_hexView->GetCaret();
    int64_t   count  = (m_hexView->GetSource() != nullptr) ? (int64_t) m_hexView->GetSource()->GetByteCount() : 0;
    int64_t   target = 0;
    int64_t   last   = 0;



    if (!IsHexPreviewShowing())
    {
        return;
    }

    if (!CassqueActions::TryParseGoTo (text, caret, target, last) || target < origin || last >= origin + count)
    {
        m_goToBox.SetError (true);
        Invalidate();
        return;
    }

    m_goToBox.SetText  (L"");
    m_goToBox.SetError (false);
    m_hexView->GoToOffset ((uint64_t) (target - origin));

    //  A range selects from its first address to its last.
    if (last > target)
    {
        m_hexView->ExtendSelectionTo ((uint64_t) (last - origin));
        m_hexView->EnsureByteVisible ((uint64_t) (target - origin));
    }

    SetFocusPane (Pane::Preview);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::OnSearchChanged
//
//  Each edit searches again from the start of the current match, so typing
//  more of a term keeps the match it has while it still fits. Hex digits find
//  bytes; other text finds characters with or without the high bit, since
//  Apple II text is stored both ways.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::OnSearchChanged (const std::wstring & text)
{
    if (!CassqueActions::TryParseSearch (text, m_findBytes, m_findIsText))
    {
        m_findBytes.clear();
        Invalidate();
        return;
    }

    m_findTyped = text;
    FindNext (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::FindNext
//
//  Searches forward from just past the selection, wrapping at the end. An
//  incremental search starts at the selection itself and says nothing when
//  the term is not found, since the user is still typing it.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::FindNext (bool incremental)
{
    const IDxuiHexSource *  source = m_hexView->GetSource();
    uint64_t                start  = 0;
    uint64_t                found  = 0;



    if (!IsHexPreviewShowing() || m_findBytes.empty() || source == nullptr)
    {
        return;
    }

    //  The search reads the whole file through the source, not just the part
    //  the view has drawn.
    start = m_hexView->HasSelection() ? m_hexView->GetSelectionFirst() + (incremental ? 0 : 1) : m_hexView->GetCaret();
    found = CassqueActions::FindInSource ([source] (uint64_t offset, std::span<uint8_t> out) { source->ReadBytes (offset, out); },
                                          source->GetByteCount(), m_findBytes, m_findIsText, start);

    if (found == CassqueActions::kNotFoundOffset)
    {
        if (!incremental)
        {
            ShowMessage (std::format (L"{} isn't in this file.", m_findTyped).c_str(), MB_ICONINFORMATION);
        }

        return;
    }

    m_hexView->SelectByte        ((uint64_t) found, m_findIsText ? DxuiHexView::Column::Text : DxuiHexView::Column::Hex);
    m_hexView->ExtendSelectionTo ((uint64_t) (found + m_findBytes.size() - 1));
    m_hexView->EnsureByteVisible ((uint64_t) found);

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ShowHexContextMenu
//
//  Copy, Select all and Go to offset, the commands that apply to a run of
//  bytes. Each item uses the same command object as the Edit menu, so the two
//  menus stay consistent.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::ShowHexContextMenu (int x, int y)
{
    std::vector<DxuiPopupMenuItem>  items;



    static constexpr int  kIds[] = { (int) CassqueCommands::kNoData,
                                     (int) CassqueCommands::kGroup1,
                                     (int) CassqueCommands::kGroup2,
                                     (int) CassqueCommands::kGroup4,
                                     kSeparatorId,
                                     (int) CassqueCommands::kFormatHex,
                                     (int) CassqueCommands::kFormatSigned,
                                     (int) CassqueCommands::kFormatUnsigned,
                                     kSeparatorId,
                                     (int) CassqueCommands::kColumns,
                                     kSeparatorId,
                                     (int) CassqueCommands::kCopy,
                                     (int) CassqueCommands::kGoToOffset };

    static constexpr int  kColumnIds[] = { (int) CassqueCommands::kColumnsAuto,
                                           (int) CassqueCommands::kColumns1,
                                           (int) CassqueCommands::kColumns2,
                                           (int) CassqueCommands::kColumns4,
                                           (int) CassqueCommands::kColumns8,
                                           (int) CassqueCommands::kColumns16 };

    for (int id : kIds)
    {
        std::shared_ptr<const DxuiCommand>  command = (id == kSeparatorId) ? nullptr : m_commands.Find (id);
        std::vector<DxuiPopupMenuItem>      columns;

        if (id == CassqueCommands::kColumns)
        {
            for (int columnId : kColumnIds)
            {
                columns.push_back (DxuiPopupMenuItem::ForCommand (m_commands.Find (columnId)));
            }

            items.push_back (DxuiPopupMenuItem::ForSubmenu (command, std::move (columns)));
        }
        else
        {
            items.push_back ((command != nullptr) ? DxuiPopupMenuItem::ForCommand (command)
                                                  : DxuiPopupMenuItem::ForSeparator());
        }
    }

    DxuiContextMenu::Show (*GetPopupHost(), x, y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ShowTextContextMenu
//
//  Copy and Select all, which reach the text view as the focused control.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::ShowTextContextMenu (int x, int y)
{
    std::vector<DxuiPopupMenuItem>  items;



    for (int id : { (int) CassqueCommands::kCopy, (int) CassqueCommands::kSelectAll })
    {
        std::shared_ptr<const DxuiCommand>  command = m_commands.Find (id);

        if (command != nullptr)
        {
            items.push_back (DxuiPopupMenuItem::ForCommand (command));
        }
    }

    DxuiContextMenu::Show (*GetPopupHost(), x, y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::Dispatch
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::Dispatch (int id)
{
    HRESULT  hr      = S_OK;
    bool     refill  = false;
    bool     routed  = DxuiCommandRouter::Invoke (GetFocusedControl(),
                                                  CassqueCommands::GetStandardCommand (id));



    //  A standard command belongs to whatever has focus, so it is routed
    //  rather than decided here.
    if (CassqueCommands::GetStandardCommand (id) != DxuiStandardCommand::None)
    {
        IGNORE_RETURN_VALUE (routed, true);
        Invalidate();
        return;
    }

    switch (id)
    {
        case CassqueCommands::kExit:
            OnWindowClose();
            break;

        case CassqueCommands::kRefresh:
            m_browser.GetBrowserModel().InvalidateAllCatalogs();
            hr     = m_browser.Refresh();
            refill = true;
            break;

        case CassqueCommands::kTogglePreview:
            m_prefs.previewVisible = !m_prefs.previewVisible;
            FillPreview();
            RecomputeLayout();
            break;

        case CassqueCommands::kToggleDisassembly:
            if (m_browser.GetBrowserModel().HasTabs())
            {
                m_browser.SetDisassemble (!m_browser.GetBrowserModel().GetActiveTab().disassemble);
                FillPreview();
            }

            break;

        case CassqueCommands::kThemeLight:         SelectTheme (CassquePrefs::kThemeLight);         break;
        case CassqueCommands::kThemeDark:          SelectTheme (CassquePrefs::kThemeDark);          break;
        case CassqueCommands::kThemeSystem:        SelectTheme (CassquePrefs::kThemeFollowSystem);  break;
        case CassqueCommands::kThemeSkeuomorphic:  SelectTheme (CassquePrefs::kThemeSkeuomorphic);  break;
        case CassqueCommands::kThemeDarkModern:    SelectTheme (CassquePrefs::kThemeDarkModern);    break;
        case CassqueCommands::kThemeRetroTerminal: SelectTheme (CassquePrefs::kThemeRetroTerminal); break;

        case CassqueCommands::kGoToOffset:
            if (IsHexPreviewShowing())
            {
                m_previewBarFocus = GetPreviewStopIndex (CassqueCommands::kGoToOffset);
                SetFocusPane (Pane::GoTo);
            }

            break;

        case CassqueCommands::kLineAddresses:
            m_prefs.lineAddresses = !m_prefs.lineAddresses;
            m_textView->SetRows (BuildTextRows (m_browser.GetPreview(), m_prefs.lineAddresses));
            Invalidate();
            break;

        case CassqueCommands::kFind:
            if (IsHexPreviewShowing())
            {
                m_previewBarFocus = GetPreviewStopIndex (CassqueCommands::kFind);
                SetFocusPane (Pane::Search);
            }

            break;

        case CassqueCommands::kZoomIn:         SetPreviewZoom (m_prefs.previewZoom + kPreviewZoomStep); break;
        case CassqueCommands::kZoomOut:        SetPreviewZoom (m_prefs.previewZoom - kPreviewZoomStep); break;
        case CassqueCommands::kZoomReset:      SetPreviewZoom (CassquePrefs::kDefaultPreviewZoom);      break;
        case CassqueCommands::kFindNext:       FindNext();                                      break;
        case CassqueCommands::kNoData:         SetHexShowValues (!m_hexView->IsShowingValues()); break;
        case CassqueCommands::kFormatHex:      SetHexFormat (CassquePrefs::kHexFormatHex);      break;
        case CassqueCommands::kFormatSigned:   SetHexFormat (CassquePrefs::kHexFormatSigned);   break;
        case CassqueCommands::kFormatUnsigned: SetHexFormat (CassquePrefs::kHexFormatUnsigned); break;
        case CassqueCommands::kColumns:                                                         break;
        case CassqueCommands::kColumnsAuto:    SetHexColumns (0);                               break;
        case CassqueCommands::kColumns1:       SetHexColumns (1);                               break;
        case CassqueCommands::kColumns2:       SetHexColumns (2);                               break;
        case CassqueCommands::kColumns4:       SetHexColumns (4);                               break;
        case CassqueCommands::kColumns8:       SetHexColumns (8);                               break;
        case CassqueCommands::kColumns16:      SetHexColumns (16);                              break;

        case CassqueCommands::kGroup1: SetHexGrouping (1); break;
        case CassqueCommands::kGroup2: SetHexGrouping (2); break;
        case CassqueCommands::kGroup4: SetHexGrouping (4); break;
        case CassqueCommands::kGroup8: SetHexGrouping (8); break;

        case CassqueCommands::kNamingDescriptive: m_prefs.hostNaming = CassquePrefs::kNamingDescriptive; break;
        case CassqueCommands::kNamingCiderPress:  m_prefs.hostNaming = CassquePrefs::kNamingCiderPress;  break;

        case CassqueCommands::kBack:    refill = m_browser.GoBack();    break;
        case CassqueCommands::kForward: refill = m_browser.GoForward(); break;
        case CassqueCommands::kUp:      refill = m_browser.GoUp();      break;

        case CassqueCommands::kEditAddress:
            SetFocusPane (Pane::Address);
            m_address->BeginEdit();
            break;

        case CassqueCommands::kAddressHistory:
            SetFocusPane (Pane::Address);
            m_address->BeginEdit();
            ShowAddressHistoryMenu (m_address->GetBounds());
            break;

        case CassqueCommands::kAbout:
            ShowAbout();
            break;

        case CassqueCommands::kNewTab:
            m_browser.NewTab();
            refill = true;
            break;

        case CassqueCommands::kCloseTab:
            refill = m_browser.CloseTab (m_browser.GetBrowserModel().GetActiveIndex());
            break;

        case CassqueCommands::kNextTab:
        case CassqueCommands::kPreviousTab:
            if (m_browser.GetBrowserModel().GetTabCount() > 1)
            {
                size_t  count = m_browser.GetBrowserModel().GetTabCount();
                size_t  next  = (m_browser.GetBrowserModel().GetActiveIndex() + (id == CassqueCommands::kNextTab ? 1 : count - 1)) % count;

                SwitchToTab (next);
            }

            break;

        default:
            break;
    }

    IGNORE_RETURN_VALUE (hr, S_OK);

    if (refill)
    {
        FillList();
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ShowAbout
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::ShowAbout()
{
    CassqueAbout::Show (GetHwnd(), m_theme, GetModuleHandleW (nullptr));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::OnWindowClose
//
//  The preferences are saved by the shell once the loop ends; closing only
//  records where the window was and ends the loop.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::OnWindowClose()
{
    m_browser.GetBrowserModel().GetLocations (m_prefs.tabs);
    m_prefs.typedPaths = m_browser.GetTypedPaths().GetEntries();
    StorePlacement();
    Hide();
    PostQuitMessage (0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::GetVerbLabel
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * CassqueWindow::GetVerbLabel (CassqueActions::Verb verb)
{
    switch (verb)
    {
        case CassqueActions::Verb::Open:           return L"&Open";
        case CassqueActions::Verb::Get:            return L"&Copy to folder...";
        case CassqueActions::Verb::Put:            return L"&Put file...";
        case CassqueActions::Verb::Delete:         return L"&Delete";
        case CassqueActions::Verb::Rename:         return L"Re&name...";
        case CassqueActions::Verb::Boot:           return L"Set as &startup program";
        case CassqueActions::Verb::InsertDrive1:   return L"Insert into drive &1";
        case CassqueActions::Verb::InsertDrive2:   return L"Insert into drive &2";
        case CassqueActions::Verb::OpenInNewCasso: return L"Open in &new Casso";
        case CassqueActions::Verb::NewDisk:        return L"&Disk image...";
        case CassqueActions::Verb::NewFolder:      return L"&Folder";
        case CassqueActions::Verb::Format:         return L"&Format disk image...";
        case CassqueActions::Verb::ReadSectors:    return L"Read &sectors to file...";
        case CassqueActions::Verb::WriteSectors:   return L"&Write sectors from file...";
        case CassqueActions::Verb::ReadBlocks:     return L"Read &blocks to file...";
        case CassqueActions::Verb::WriteBlocks:    return L"Write b&locks from file...";
        case CassqueActions::Verb::Refresh:        return L"&Refresh";
        case CassqueActions::Verb::OpenWith:       return L"Open wit&h";
        case CassqueActions::Verb::MoreOptions:    return L"Show more &options";
        case CassqueActions::Verb::Cut:            return L"Cu&t";
        case CassqueActions::Verb::Copy:           return L"&Copy";
        case CassqueActions::Verb::Paste:          return L"&Paste";
        case CassqueActions::Verb::Share:          return L"&Share";
        default:                                   return L"";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::GetVerbGlyph
//
//  The verbs Explorer shows as buttons, with its glyphs; null for the rest.
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * CassqueWindow::GetVerbGlyph (CassqueActions::Verb verb)
{
    switch (verb)
    {
        case CassqueActions::Verb::Cut:    return s_kpszMdl2Cut;
        case CassqueActions::Verb::Copy:   return s_kpszMdl2Copy;
        case CassqueActions::Verb::Paste:  return s_kpszMdl2Paste;
        case CassqueActions::Verb::Rename: return s_kpszMdl2Rename;
        case CassqueActions::Verb::Delete: return s_kpszMdl2Delete;
        case CassqueActions::Verb::Share:  return s_kpszMdl2Share;
        default:                           return nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::GetIconOrder
//
////////////////////////////////////////////////////////////////////////////////

int CassqueWindow::GetIconOrder (CassqueActions::Verb verb)
{
    switch (verb)
    {
        case CassqueActions::Verb::Cut:    return 0;
        case CassqueActions::Verb::Copy:   return 1;
        case CassqueActions::Verb::Paste:  return 2;
        case CassqueActions::Verb::Rename: return 3;
        case CassqueActions::Verb::Share:  return 4;
        default:                           return 5;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ShowListContextMenu
//
//  The rows come from the actions' verb list; each command runs its verb.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::ShowListContextMenu (int x, int y)
{
    std::vector<DxuiPopupMenuItem>                   items;
    Location                                         location;
    std::wstring                                     folderForCasso;
    bool                                             known          = false;
    bool                                             moreOptions    = false;
    std::vector<std::shared_ptr<const DxuiCommand>>  iconCommands;
    std::vector<DxuiPopupMenuItem>                   newChoices;



    m_menuCommands.clear();
    AskCassoToDescribe();

    //  A folder or a disk image opens in a new tab, as Explorer's items do.
    if (m_browser.GetSelectedRows().size() == 1 && m_browser.TryGetRowLocation (m_browser.GetSelectedRows()[0], location))
    {
        AddMenuCommand (items, L"Open in new &tab", [this, location]()
        {
            m_browser.OpenInNewTab (location);
            FillList();
        });
        items.push_back (DxuiPopupMenuItem::ForSeparator());
    }

    for (CassqueActions::Verb verb : m_actions.GetListVerbs())
    {
        std::shared_ptr<DxuiCommand>  command;

        if (verb == CassqueActions::Verb::MoreOptions)
        {
            moreOptions = true;
            continue;
        }

        //  A real file's clipboard, rename and delete are Explorer's buttons
        //  along the menu's edge rather than rows.
        if (!m_browser.IsImageLocation() && GetVerbGlyph (verb) != nullptr)
        {
            command           = std::make_shared<DxuiCommand>();
            command->id       = (int) verb;
            command->label    = GetVerbLabel (verb);
            command->glyph    = GetVerbGlyph (verb);
            command->dispatch = [this, verb]() { RunVerb (verb); };

            if (verb == CassqueActions::Verb::Paste)
            {
                command->isEnabled = [this]() { return m_shellVerbs.ClipboardHasFiles(); };
            }

            iconCommands.push_back (command);
            m_menuCommands.push_back (std::move (command));
            continue;
        }

        if (verb == CassqueActions::Verb::OpenWith)
        {
            AddOpenWithMenu (items);
            continue;
        }

        //  New's choices go in its own submenu, where the Refresh row starts.
        if (verb == CassqueActions::Verb::NewFolder || verb == CassqueActions::Verb::NewDisk)
        {
            command           = std::make_shared<DxuiCommand>();
            command->id       = (int) verb;
            command->label    = GetVerbLabel (verb);
            command->dispatch = [this, verb]() { RunVerb (verb); };

            if (verb == CassqueActions::Verb::NewFolder)
            {
                command->isEnabled = [this]() { return !m_browser.IsImageLocation() || m_browser.GetVolumeKind() == VolumeKind::ProDos; };
            }

            newChoices.push_back (DxuiPopupMenuItem::ForCommand (command));
            m_menuCommands.push_back (std::move (command));
            continue;
        }

        if (verb == CassqueActions::Verb::Refresh && !newChoices.empty())
        {
            std::shared_ptr<DxuiCommand>  parent = std::make_shared<DxuiCommand>();
            bool                          dos33  = m_browser.IsImageLocation() && m_browser.GetVolumeKind() != VolumeKind::ProDos;

            parent->label     = L"Ne&w";
            parent->isEnabled = [dos33]() { return !dos33; };

            if (!items.empty())
            {
                items.push_back (DxuiPopupMenuItem::ForSeparator());
            }

            items.push_back (DxuiPopupMenuItem::ForSubmenu (parent, std::move (newChoices)));
            m_menuCommands.push_back (std::move (parent));
            newChoices.clear();
        }

        if (verb == CassqueActions::Verb::Refresh && !items.empty())
        {
            items.push_back (DxuiPopupMenuItem::ForSeparator());
        }

        command           = std::make_shared<DxuiCommand>();
        command->id       = (int) verb;
        command->label    = GetVerbLabel (verb);
        command->dispatch = [this, verb]() { RunVerb (verb); };

        //  A machine known to have one drive cannot take drive 2; one not
        //  described yet is given the benefit of the doubt.
        if (verb == CassqueActions::Verb::InsertDrive2)
        {
            command->isEnabled = [this]() { return m_cassoDriveCount != 1; };
        }

        if (verb == CassqueActions::Verb::Paste)
        {
            command->isEnabled = [this]() { return m_shellVerbs.ClipboardHasFiles(); };
        }

        items.push_back (DxuiPopupMenuItem::ForCommand (command));
        m_menuCommands.push_back (std::move (command));

        if (verb == CassqueActions::Verb::Format)
        {
            std::vector<DxuiPopupMenuItem>  advanced;
            std::shared_ptr<DxuiCommand>    parent = std::make_shared<DxuiCommand>();

            for (CassqueActions::Verb raw : { CassqueActions::Verb::ReadSectors, CassqueActions::Verb::WriteSectors,
                                              CassqueActions::Verb::ReadBlocks,  CassqueActions::Verb::WriteBlocks })
            {
                std::shared_ptr<DxuiCommand>  child = std::make_shared<DxuiCommand>();

                child->id       = (int) raw;
                child->label    = GetVerbLabel (raw);
                child->dispatch = [this, raw]() { RunRawVerb (raw); };

                advanced.push_back (DxuiPopupMenuItem::ForCommand (child));
                m_menuCommands.push_back (std::move (child));
            }

            parent->label = L"&Advanced";
            items.push_back (DxuiPopupMenuItem::ForSubmenu (parent, std::move (advanced)));
            m_menuCommands.push_back (std::move (parent));
        }
    }

    //  Casso's known folders, from a folder row or from the background of the
    //  folder being shown, as the tree offers them from a node.
    if (m_browser.GetSelectedRows().size() == 1)
    {
        folderForCasso = (m_browser.TryGetRowLocation (m_browser.GetSelectedRows()[0], location) &&
                          location.kind == Location::Kind::HostFolder) ? location.path : std::wstring();
    }
    else if (m_browser.GetSelectedRows().empty() && m_browser.GetLocation().kind == Location::Kind::HostFolder)
    {
        folderForCasso = m_browser.GetLocation().path;
    }

    if (!folderForCasso.empty())
    {
        known = m_browser.IsKnownFolder (folderForCasso);

        AddMenuCommand (items, known ? L"&Remove from Casso" : L"&Add to Casso", [this, folderForCasso, known]()
        {
            ChangeKnownFolder (folderForCasso, !known);
        });
    }

    if (!m_browser.GetSelectedRows().empty())
    {
        items.push_back (DxuiPopupMenuItem::ForSeparator());
        AddMenuCommand (items, L"Copy as &path", [this]() { CopySelectedPaths(); });

        if (m_browser.GetSelectedRows().size() == 1)
        {
            AddMenuCommand (items, L"P&roperties", [this]()
            {
                if (!m_browser.GetSelectedRows().empty())
                {
                    ShowRowProperties (m_browser.GetSelectedRows()[0]);
                }
            });
        }
    }

    //  In Explorer's order, whatever order the verbs came in.
    std::stable_sort (iconCommands.begin(), iconCommands.end(), [] (const auto & a, const auto & b)
    {
        return GetIconOrder ((CassqueActions::Verb) a->id) < GetIconOrder ((CassqueActions::Verb) b->id);
    });

    if (!iconCommands.empty())
    {
        items.insert (items.begin(), DxuiPopupMenuItem::ForSeparator());
        items.insert (items.begin(), DxuiPopupMenuItem::ForIconRow (std::move (iconCommands)));
    }

    //  Last, as Explorer has it: everything else Windows and other programs
    //  offer for these items, in the shell's own menu.
    if (moreOptions)
    {
        POINT  screen = { x, y };

        ClientToScreen (GetHwnd(), &screen);

        items.push_back (DxuiPopupMenuItem::ForSeparator());
        AddMenuCommand (items, GetVerbLabel (CassqueActions::Verb::MoreOptions), [this, screen]()
        {
            std::vector<std::wstring>  paths;
            HRESULT                    hr = S_OK;

            m_browser.GetSelectedHostPaths (paths);

            hr = m_shellVerbs.ShowShellMenu (GetHwnd(), paths, screen);
            IGNORE_RETURN_VALUE (hr, S_OK);
        });
    }

    DxuiContextMenu::Show (*GetPopupHost(), x, y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::AddOpenWithMenu
//
//  The programs Windows recommends for the selected file, then its own
//  dialog for any other.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::AddOpenWithMenu (std::vector<DxuiPopupMenuItem> & items)
{
    std::wstring                          path   = GetSelectedImagePath();
    std::vector<IShellItemVerbs::Handler> handlers;
    std::vector<DxuiPopupMenuItem>        children;
    std::shared_ptr<DxuiCommand>          parent = std::make_shared<DxuiCommand>();
    HRESULT                               hr     = S_OK;
    size_t                                i      = 0;



    if (path.empty())
    {
        return;
    }

    hr = m_shellVerbs.GetOpenWithHandlers (path, handlers);
    IGNORE_RETURN_VALUE (hr, S_OK);

    for (i = 0; i < handlers.size(); i++)
    {
        AddMenuCommand (children, EscapeMnemonics (handlers[i].name).c_str(), [this, path, i]()
        {
            HRESULT  opened = m_shellVerbs.OpenWith (GetHwnd(), path, i);

            IGNORE_RETURN_VALUE (opened, S_OK);
        });
    }

    if (!children.empty())
    {
        children.push_back (DxuiPopupMenuItem::ForSeparator());
    }

    AddMenuCommand (children, L"&Choose another app", [this, path]()
    {
        HRESULT  chosen = m_shellVerbs.ChooseOtherApp (GetHwnd(), path);

        IGNORE_RETURN_VALUE (chosen, S_OK);
    });

    parent->label = GetVerbLabel (CassqueActions::Verb::OpenWith);
    items.push_back (DxuiPopupMenuItem::ForSubmenu (parent, std::move (children)));
    m_menuCommands.push_back (std::move (parent));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::BeginRename
//
//  Over the name, with the name selected up to its extension, as Explorer
//  selects it; an entry inside an image has no extension to leave out.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::BeginRename()
{
    RECT          cell  = {};
    RECT          list  = m_list->GetBounds();
    int           row   = -1;
    std::wstring  name;
    size_t        dot   = std::wstring::npos;
    bool          host  = !m_browser.IsImageLocation();



    if (m_browser.GetSelectedRows().size() != 1)
    {
        return;
    }

    row = m_browser.GetSelectedRows()[0];
    m_list->EnsureVisible (row);

    if (!m_list->GetCellTextRectPx (row, 0, cell))
    {
        return;
    }

    name = m_browser.GetRows()[(size_t) row].name;

    OffsetRect (&cell, list.left, list.top);

    m_renameBox->SetMaxLength (host ? MAX_PATH : kMaxCatalogName);
    m_renameBox->SetText      (name);
    m_renameBox->Layout       (cell, m_scaler);
    m_renameBox->SetVisible   (true);
    m_renameBox->SetFocused   (true);
    m_renameBox->SelectAll();

    dot = host ? name.find_last_of (L'.') : std::wstring::npos;

    //  The stem only, so typing keeps the type.
    if (dot != std::wstring::npos && dot > 0 && !m_browser.GetRows()[(size_t) row].isDirectory)
    {
        m_renameBox->SetSelection (0, dot);
    }

    m_renameRow = row;
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::EndRename
//
//  A name left as it was, or emptied, changes nothing.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::EndRename (bool commit)
{
    HRESULT       hr      = S_OK;
    int           row     = m_renameRow;
    std::wstring  newName = m_renameBox->GetText();
    std::wstring  oldName;
    std::wstring  path;



    m_renameRow = -1;
    m_renameBox->SetFocused (false);
    m_renameBox->SetVisible (false);
    Invalidate();

    if (!commit || row < 0 || row >= (int) m_browser.GetRows().size())
    {
        return;
    }

    oldName = m_browser.GetRows()[(size_t) row].name;

    if (newName.empty() || newName == oldName)
    {
        return;
    }

    if (!m_browser.IsImageLocation())
    {
        if (m_browser.TryGetRowPath (row, path))
        {
            hr = m_shellVerbs.RenameItem (GetHwnd(), path, newName);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        RefreshAfterHostChange();
        return;
    }

    ReportOutcome (m_actions.RenameSelected (newName), L"Rename");
    FillList();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::CreateDiskFromSelection
//
//  The selected host files and folders go onto the new disk. Whether they fit
//  is decided before the image exists, so a refusal leaves nothing behind.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::CreateDiskFromSelection (const CassqueNewDiskDialog::Outcome & newDisk)
{
    std::vector<std::wstring>  paths;
    std::wstring               folder = m_browser.GetLocation().path;
    std::wstring               image  = CassqueBrowser::JoinPath (folder, newDisk.fileName);
    VolumeKind                 kind   = (newDisk.request.formatName == "prodos") ? VolumeKind::ProDos : VolumeKind::Dos33;
    std::wstring               refusal;
    CassqueActions::Outcome    outcome;



    m_browser.GetSelectedHostPaths (paths);

    if (!paths.empty())
    {
        refusal = (newDisk.request.formatName == "none")
                ? std::wstring (L"A disk with no file system cannot hold files.")
                : m_actions.CheckFitsNewDisk (kind, paths);

        if (!refusal.empty())
        {
            ShowMessage (refusal, MB_ICONWARNING);
            return;
        }
    }

    outcome = m_actions.CreateImage (folder, newDisk.fileName, newDisk.request);

    if (outcome.Succeeded() && !paths.empty())
    {
        outcome = m_actions.PutInto (image, kind, "", paths, MakeAddressPrompt());
    }

    ReportOutcome (outcome, L"New disk");
    RefreshAfterHostChange();
    SelectRowNamed (newDisk.fileName);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::TryGetDropLocation
//
//  On the list: the image folder under the pointer, or the image the list
//  shows. On the tree: the image or directory node under the pointer. A host
//  folder takes no drop here; Explorer is for that.
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueWindow::TryGetDropLocation (int tag, POINT screen, Location & outLocation)
{
    POINT                 client = screen;
    int                   row    = -1;
    RECT                  list   = m_list->GetBounds();
    const DxuiTreeNode  * node   = nullptr;



    ScreenToClient (GetHwnd(), &client);

    if (tag == kDropTagTree)
    {
        row  = m_tree->HitTestRow (client.x, client.y);
        node = (row >= 0) ? m_tree->GetNodeAt (row) : nullptr;

        if (node == nullptr || !m_browser.TryGetNodeLocation (node->id, outLocation))
        {
            return false;
        }
    }
    else
    {
        row = m_list->HitTestRow (client.x - list.left, client.y - list.top);

        if (row < 0 || !m_browser.TryGetRowLocation (row, outLocation) ||
            outLocation.kind != Location::Kind::DiskDirectory || !m_browser.IsImageLocation())
        {
            outLocation = m_browser.GetLocation();
        }
    }

    return outLocation.kind == Location::Kind::DiskImage || outLocation.kind == Location::Kind::DiskDirectory;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::GetDropEffect
//
//  A copy where the drag carries files or another image's entries and the
//  image under the pointer can be written; nothing otherwise, so the pointer
//  says so before the button is let go.
//
////////////////////////////////////////////////////////////////////////////////

DWORD CassqueWindow::GetDropEffect (IDataObject * data, int tag, POINT screen)
{
    Location   location;
    bool       readOnly = false;
    HRESULT    hr       = S_OK;
    FORMATETC  hdrop    = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    FORMATETC  entries  = { (CLIPFORMAT) RegisterClipboardFormatA (DragPayload::kPrivateFormatName), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    HRESULT    hasFiles = S_OK;
    HRESULT    hasEntry = S_OK;



    if (data == nullptr || !TryGetDropLocation (tag, screen, location))
    {
        return DROPEFFECT_NONE;
    }

    if (m_context.fs != nullptr)
    {
        hr = m_context.fs->GetReadOnlyAttribute (location.path, readOnly);

        if (SUCCEEDED (hr) && readOnly)
        {
            return DROPEFFECT_NONE;
        }
    }

    hasFiles = data->QueryGetData (&hdrop);
    hasEntry = data->QueryGetData (&entries);

    return (hasFiles == S_OK || hasEntry == S_OK) ? DROPEFFECT_COPY : DROPEFFECT_NONE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::OnDrop
//
//  Another image's entries are copied byte for byte when the drag carries
//  them; otherwise the host files go in by Put's rules.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::OnDrop (IDataObject * data, int tag, POINT screen)
{
    Location                   location;
    FORMATETC                  entries      = { (CLIPFORMAT) RegisterClipboardFormatA (DragPayload::kPrivateFormatName), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM                  medium       = {};
    HRESULT                    hr           = S_OK;
    std::vector<std::wstring>  paths;
    std::string                source;
    VolumeKind                 sourceKind   = VolumeKind::Unknown;
    VolumeKind                 targetKind   = VolumeKind::Unknown;
    std::vector<std::string>   catalogPaths;
    std::string                inner;
    VolumeListing              listing;
    CassqueActions::Outcome    outcome;
    bool                       decoded      = false;



    if (!TryGetDropLocation (tag, screen, location))
    {
        return;
    }

    inner = (location.kind == Location::Kind::DiskDirectory) ? location.innerPath : std::string();

    //  What the target is, by listing where the drop goes.
    if (!m_browser.GetOperations().List (TextEncoding::WideToNarrow (location.path), inner, listing, targetKind).Succeeded())
    {
        ShowMessage (L"The disk image could not be read.", MB_ICONWARNING);
        return;
    }

    hr = data->GetData (&entries, &medium);

    if (SUCCEEDED (hr))
    {
        const char  * bytes = (const char *) GlobalLock (medium.hGlobal);
        size_t        size  = GlobalSize (medium.hGlobal);

        if (bytes != nullptr)
        {
            decoded = DragPayload::DecodeCatalogEntries (std::string (bytes, strnlen (bytes, size)), source, sourceKind, catalogPaths);
            GlobalUnlock (medium.hGlobal);
        }

        ReleaseStgMedium (&medium);
    }

    if (decoded)
    {
        outcome = m_actions.CopyEntriesInto (source, sourceKind, catalogPaths, location.path, targetKind, inner);
    }
    else
    {
        hr = DxuiDragDropTarget::ExtractHDropPaths (data, paths);

        if (FAILED (hr) || paths.empty())
        {
            return;
        }

        outcome = m_actions.PutInto (location.path, targetKind, inner, paths, MakeAddressPrompt());
    }

    ReportOutcome (outcome, L"Put");
    RefreshAfterHostChange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::SelectRowNamed
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::SelectRowNamed (const std::wstring & name)
{
    const std::vector<CatalogRow> &  rows = m_browser.GetRows();



    for (size_t i = 0; i < rows.size(); i++)
    {
        if (_wcsicmp (rows[i].name.c_str(), name.c_str()) == 0)
        {
            m_browser.SetSelectedRows ({ (int) i });
            m_list->SetSelectedRows ({ (int) i }, (int) i);
            m_list->EnsureVisible ((int) i);
            break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::GetPasteFolder
//
//  A single folder row takes the paste; otherwise the folder being shown.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassqueWindow::GetPasteFolder() const
{
    Location  location;



    if (m_browser.GetSelectedRows().size() == 1
     && m_browser.TryGetRowLocation (m_browser.GetSelectedRows()[0], location)
     && location.kind == Location::Kind::HostFolder)
    {
        return location.path;
    }

    return m_browser.GetLocation().path;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::RefreshAfterHostChange
//
//  The folder watcher sees the change too, a moment later; rereading now
//  shows it at once.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::RefreshAfterHostChange()
{
    HRESULT  hr = m_browser.Reload (true);



    IGNORE_RETURN_VALUE (hr, S_OK);
    FillList();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::GetSelectedImagePath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassqueWindow::GetSelectedImagePath() const
{
    std::vector<std::wstring>  paths;



    m_browser.GetSelectedHostPaths (paths);

    return (paths.size() == 1) ? paths[0] : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::RunVerb
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::RunVerb (CassqueActions::Verb verb)
{
    HRESULT                        hr      = S_OK;
    std::filesystem::path          picked;
    bool                           chosen  = false;
    FileDialogSpec                 spec;
    int                            answer  = 0;
    CassqueActions::Outcome        outcome;
    std::vector<FileEntry>         entries;
    std::wstring                   newName;
    std::vector<std::wstring>      hostPaths;
    CassqueNewDiskDialog::Outcome  newDisk;
    HostFileNaming::Style    style   = (m_prefs.hostNaming == CassquePrefs::kNamingCiderPress)
                                     ? HostFileNaming::Style::CiderPress : HostFileNaming::Style::Descriptive;



    switch (verb)
    {
        case CassqueActions::Verb::Open:
            if (!m_browser.GetSelectedRows().empty() && m_browser.OpenRow (m_browser.GetSelectedRows()[0]))
            {
                FillList();
            }
            else if (!GetSelectedImagePath().empty())
            {
                //  Not something to browse: a real file, in its own program.
                hr = m_shellVerbs.Open (GetHwnd(), GetSelectedImagePath());
            }

            break;

        case CassqueActions::Verb::Get:
            hr = m_dialogs.PickFolder (GetHwnd(), picked, chosen);

            if (SUCCEEDED (hr) && chosen)
            {
                ReportOutcome (m_actions.GetSelected (picked.wstring(), style), L"Copy");
            }

            break;

        case CassqueActions::Verb::Put:
            hr = m_dialogs.PickFileToOpen (GetHwnd(), spec, picked, chosen);

            if (SUCCEEDED (hr) && chosen)
            {
                outcome = m_actions.PutFiles ({ picked.wstring() }, MakeAddressPrompt());
                ReportOutcome (outcome, L"Put");
                FillList();
            }

            break;

        case CassqueActions::Verb::Cut:
        case CassqueActions::Verb::Copy:
            m_browser.GetSelectedHostPaths (hostPaths);
            hr = m_shellVerbs.PlaceOnClipboard (GetHwnd(), hostPaths, verb == CassqueActions::Verb::Cut);
            break;

        case CassqueActions::Verb::Share:
            m_browser.GetSelectedHostPaths (hostPaths);
            hr = m_shellVerbs.Share (GetHwnd(), hostPaths);
            break;

        case CassqueActions::Verb::Paste:
            hr = m_shellVerbs.PasteInto (GetHwnd(), GetPasteFolder());
            RefreshAfterHostChange();
            break;

        case CassqueActions::Verb::Delete:
            if (!m_browser.IsImageLocation())
            {
                //  Windows asks, and the Recycle Bin keeps it.
                m_browser.GetSelectedHostPaths (hostPaths);
                hr = m_shellVerbs.Recycle (GetHwnd(), hostPaths);
                RefreshAfterHostChange();
                break;
            }

        {
            std::vector<std::wstring>  plan    = m_actions.DescribeDeletePlan();
            std::wstring               message = std::format (L"Delete {} selected item(s) from this disk image? This cannot be undone.",
                                                              m_browser.GetSelectedRows().size());

            //  A folder goes with everything below it, which the list cannot
            //  show, so the plan's own rows and totals go into the question.
            for (const std::wstring & line : plan)
            {
                message += L"\n" + line;
            }

            answer = DxuiMessageBox (GetHwnd(), m_theme, message.c_str(), L"Delete", MB_YESNO | MB_ICONWARNING);

            if (answer == IDYES)
            {
                ReportOutcome (m_actions.DeleteSelected(), L"Delete");
                FillList();
            }

            break;
        }

        case CassqueActions::Verb::Boot:
            ReportOutcome (m_actions.BootSelected(), L"Set startup program");
            FillList();
            break;

        case CassqueActions::Verb::Rename:
            BeginRename();
            break;

        case CassqueActions::Verb::InsertDrive1:
        case CassqueActions::Verb::InsertDrive2:
            InsertIntoDrive (GetSelectedImagePath(), verb == CassqueActions::Verb::InsertDrive1 ? 1 : 2);
            break;

        case CassqueActions::Verb::OpenInNewCasso:
            OpenInNewCasso (GetSelectedImagePath());
            break;

        case CassqueActions::Verb::NewDisk:
            newDisk = CassqueNewDiskDialog::Ask (GetHwnd(), m_theme, false, [this] (const std::wstring & fileName)
            {
                return m_actions.IsNameTaken (m_browser.GetLocation().path, fileName);
            });

            if (newDisk.confirmed)
            {
                CreateDiskFromSelection (newDisk);
            }

            break;

        case CassqueActions::Verb::NewFolder:
            //  An unused default name, open for renaming at once.
            newName = m_actions.GetNewFolderName();

            if (!m_browser.IsImageLocation())
            {
                hr = m_shellVerbs.CreateFolder (GetHwnd(), m_browser.GetLocation().path, newName);
                RefreshAfterHostChange();
            }
            else
            {
                ReportOutcome (m_actions.CreateFolder (newName), L"New folder");
                FillList();
            }

            SelectRowNamed (newName);
            BeginRename();
            break;

        case CassqueActions::Verb::Format:
            newDisk = CassqueNewDiskDialog::Ask (GetHwnd(), m_theme, true);

            if (newDisk.confirmed)
            {
                answer = DxuiMessageBox (GetHwnd(), m_theme,
                                         (L"Format " + CassqueActions::GetLeafName (m_actions.GetFormatTarget())
                                          + L"? Everything on it will be erased.").c_str(),
                                         L"Format", MB_YESNO | MB_ICONWARNING);

                if (answer == IDYES)
                {
                    ReportOutcome (m_actions.FormatImage (newDisk.request), L"Format");
                    FillList();
                }
            }

            break;

        case CassqueActions::Verb::Refresh:
            Dispatch (CassqueCommands::kRefresh);
            break;

        default:
            break;
    }

    IGNORE_RETURN_VALUE (hr, S_OK);
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ReportOutcome
//
//  A failure shows what the runner said; a success is quiet, since the list
//  already shows the result.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::ReportOutcome (const CassqueActions::Outcome & outcome, const wchar_t * verbName)
{
    if (outcome.Succeeded())
    {
        m_status->SetText (2, std::format (L"{}: {} file(s)", verbName, outcome.written));
        return;
    }

    ShowMessage (outcome.message.empty() ? std::wstring (verbName) + L" did not complete." : outcome.message, MB_ICONERROR);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ShowMessage
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::ShowMessage (const std::wstring & text, UINT icon)
{
    int  result = DxuiMessageBox (GetHwnd(), m_theme, text.c_str(), CassqueShell::kAppName, MB_OK | icon);



    IGNORE_RETURN_VALUE (result, IDOK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::InsertIntoDrive
//
//  The Casso that launched this browser when it is still there, otherwise
//  any running Casso, otherwise a new one with the disk in drive 1. The
//  answer arrives later as a reply message.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::InsertIntoDrive (const std::wstring & imagePath, int drive)
{
    HWND  target = nullptr;
    bool  sent   = false;



    if (imagePath.empty())
    {
        return;
    }

    target = FindCassoTarget();

    if (target == nullptr)
    {
        OpenInNewCasso (imagePath);
        return;
    }

    sent = Win32IntentChannel::SendTo (target, GetHwnd(), Win32IntentChannel::GetMessageId(),
                                       Win32IntentChannel::EncodeInsert (TextEncoding::WideToNarrow (imagePath), drive));

    if (!sent)
    {
        ShowMessage (L"Casso did not answer the request to insert the disk.", MB_ICONWARNING);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::OpenInNewCasso
//
//  The folder is recorded here, since the new Casso learns of the disk from
//  its command line rather than from a hand-off it would record itself.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::OpenInNewCasso (const std::wstring & imagePath)
{
    wchar_t       module[MAX_PATH] = {};
    DWORD         length           = GetModuleFileNameW (nullptr, module, ARRAYSIZE (module));
    std::wstring  exe;
    HRESULT       hr               = S_OK;



    if (imagePath.empty() || length == 0 || length >= ARRAYSIZE (module))
    {
        return;
    }

    exe = LaunchCommand::GetSiblingPath (std::filesystem::path (module).parent_path().wstring(), LaunchCommand::kCassoExe);

    if (!m_launcher.Exists (exe))
    {
        ShowMessage (LaunchCommand::DescribeMissing (exe), MB_ICONERROR);
        return;
    }

    hr = m_launcher.Launch (exe, LaunchCommand::MakeCassoArguments (imagePath, m_context.titlePrefix));

    if (FAILED (hr))
    {
        ShowMessage (L"Casso could not be started.", MB_ICONERROR);
        return;
    }

    if (m_context.fs != nullptr)
    {
        KnownFolderStore  store (*m_context.fs, m_context.baseDir);

        hr = store.Append (std::filesystem::path (imagePath).parent_path().wstring(), (int64_t) _time64 (nullptr));
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::OnCopyData
//
//  A reply is queued and shown after the send returns: Casso is blocked in
//  its send until this handler does, and a dialog opened here would hold it
//  there until the send timed out.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult CassqueWindow::OnCopyData (WPARAM sender, LPARAM data)
{
    const COPYDATASTRUCT *     copy  = (const COPYDATASTRUCT *) data;
    Win32IntentChannel::Reply  reply;
    bool                       ours  = copy != nullptr && copy->dwData == Win32IntentChannel::GetReplyMessageId();



    UNREFERENCED_PARAMETER (sender);

    if (!ours || !Win32IntentChannel::DecodeReply ((const Byte *) copy->lpData, copy->cbData, reply))
    {
        return DxuiMessageResult::NotHandled;
    }

    m_pendingReplies.push_back (reply);
    PostMessageW (GetHwnd(), kReplyMessage, 0, 0);

    return DxuiMessageResult::Handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::OnAppMessage
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult CassqueWindow::OnAppMessage (UINT msg, WPARAM wParam, LPARAM lParam)
{
    std::vector<Win32IntentChannel::Reply>  replies;



    UNREFERENCED_PARAMETER (wParam);
    UNREFERENCED_PARAMETER (lParam);

    //  A change arrived. Restarting the timer rather than re-reading now is
    //  what collapses a burst: a copy of a hundred files re-reads once, when
    //  the copying stops.
    if (msg == kFolderChangedMessage)
    {
        SetTimer (GetHwnd(), kFolderTimerId, kFolderSettleMs, nullptr);

        return DxuiMessageResult::Handled;
    }

    if (msg != kReplyMessage)
    {
        return DxuiMessageResult::NotHandled;
    }

    replies.swap (m_pendingReplies);

    for (const Win32IntentChannel::Reply & reply : replies)
    {
        std::wstring  text = TextEncoding::NarrowToWide (reply.text);

        switch (reply.kind)
        {
            case Win32IntentChannel::ReplyKind::MachineDescription:
                m_cassoDriveCount = reply.driveCount;
                break;

            case Win32IntentChannel::ReplyKind::InsertDone:
                m_status->SetText (2, L"Inserted into Casso");
                break;

            case Win32IntentChannel::ReplyKind::InsertRefused:
                ShowMessage (L"Casso did not insert the disk.\n\n" + text, MB_ICONWARNING);
                break;

            case Win32IntentChannel::ReplyKind::ReloadConflict:
            case Win32IntentChannel::ReplyKind::ReloadRefused:
                ShowMessage (L"Casso did not reload the changed disk.\n\n" + text, MB_ICONWARNING);
                break;

            default:
                break;
        }
    }

    Invalidate();

    return DxuiMessageResult::Handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ShowTreeContextMenu
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::ShowTreeContextMenu (int x, int y, const std::wstring & id)
{
    std::vector<DxuiPopupMenuItem>  items;
    std::wstring                    folder;
    Location                        location;
    bool                            hasFolder   = m_browser.TryGetNodePath (id, folder);
    bool                            hasLocation = m_browser.TryGetNodeLocation (id, location);



    m_menuCommands.clear();

    if (hasLocation)
    {
        AddMenuCommand (items, L"Open in new &tab", [this, location]()
        {
            m_browser.OpenInNewTab (location);
            FillList();
        });
    }

    if (hasFolder && m_browser.CanRemoveFromCasso (id))
    {
        AddMenuCommand (items, L"&Remove from Casso", [this, folder]() { ChangeKnownFolder (folder, false); });
    }
    else if (hasFolder && m_browser.CanAddToCasso (id))
    {
        AddMenuCommand (items, L"&Add to Casso", [this, folder]() { ChangeKnownFolder (folder, true); });
    }

    if (hasLocation)
    {
        items.push_back (DxuiPopupMenuItem::ForSeparator());
        AddMenuCommand (items, L"Copy as &path", [this, location]()
        {
            DxuiClipboard::SetText (GetHwnd(), L"\"" + BrowserModel::FormatAddress (location) + L"\"");
        });
        AddMenuCommand (items, L"P&roperties", [this, location]() { ShowLocationProperties (location); });
    }

    if (!items.empty())
    {
        DxuiContextMenu::Show (*GetPopupHost(), x, y, std::move (items));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ShowTabContextMenu
//
//  Explorer's tab menu. The last tab never closes, and the last tab has no
//  tabs to its right, so those rows are disabled rather than doing nothing.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::ShowTabContextMenu (int x, int y, int index)
{
    std::vector<DxuiPopupMenuItem>  items;
    size_t                          tab   = (size_t) index;
    size_t                          count = m_browser.GetBrowserModel().GetTabCount();



    m_menuCommands.clear();

    AddMenuCommand (items, L"&Close tab", [this, tab]()
    {
        if (m_browser.CloseTab (tab))
        {
            FillList();
        }
    }, L"Ctrl+W");

    AddMenuCommand (items, L"Close &other tabs", [this, tab]()
    {
        if (m_browser.CloseOtherTabs (tab))
        {
            FillList();
        }
    });

    AddMenuCommand (items, L"Close tabs to the &right", [this, tab]()
    {
        if (m_browser.CloseTabsToRight (tab))
        {
            FillList();
        }
    });

    AddMenuCommand (items, L"&Duplicate tab", [this, tab]()
    {
        m_browser.DuplicateTab (tab);
        FillList();
    });

    m_menuCommands[0]->isEnabled = [count]() { return count > 1; };
    m_menuCommands[1]->isEnabled = [count]() { return count > 1; };
    m_menuCommands[2]->isEnabled = [tab, count]() { return tab + 1 < count; };

    DxuiContextMenu::Show (*GetPopupHost(), x, y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::AddMenuCommand
//
//  One row of a context menu, its command kept alive until the next menu.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::AddMenuCommand (std::vector<DxuiPopupMenuItem> & items, const wchar_t * label, std::function<void()> dispatch, const wchar_t * accelerator)
{
    std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();



    command->label       = label;
    command->accelerator = accelerator;
    command->dispatch    = std::move (dispatch);

    items.push_back (DxuiPopupMenuItem::ForCommand (command));
    m_menuCommands.push_back (std::move (command));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::CopySelectedPaths
//
//  Each path in quotes on its own line, as Explorer's Copy as path gives them.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::CopySelectedPaths()
{
    std::wstring  text;
    std::wstring  path;



    for (int row : m_browser.GetSelectedRows())
    {
        if (!m_browser.TryGetRowPath (row, path))
        {
            continue;
        }

        if (!text.empty())
        {
            text += L"\r\n";
        }

        text += L"\"" + path + L"\"";
    }

    if (!text.empty())
    {
        DxuiClipboard::SetText (GetHwnd(), text);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ShowRowProperties
//
//  A host item, a drive among them, has Windows' own Properties sheet. An
//  entry inside an image has none, so its catalog details are shown instead.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::ShowRowProperties (int row)
{
    Location                           location = m_browser.GetLocation();
    std::vector<DxuiListView::Column>  columns  = CassqueBrowser::GetColumns();
    std::vector<DxuiListView::Cell>    cells;
    std::wstring                       path;
    std::wstring                       text;
    size_t                             i        = 0;



    if (!m_browser.TryGetRowPath (row, path))
    {
        return;
    }

    if (location.kind == Location::Kind::HostFolder || location.kind == Location::Kind::None)
    {
        ShowHostProperties (path);
        return;
    }

    cells = CassqueBrowser::ToCells (m_browser.GetRows()[(size_t) row], location);
    text  = path + L"\n\n";

    for (i = 0; i < columns.size() && i < cells.size(); i++)
    {
        if (!cells[i].text.empty() && !columns[i].title.empty())
        {
            text += columns[i].title + L": " + cells[i].text + L"\n";
        }
    }

    ShowMessage (text, MB_ICONINFORMATION);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ShowLocationProperties
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::ShowLocationProperties (const Location & location)
{
    if (location.kind == Location::Kind::DiskDirectory)
    {
        ShowMessage (BrowserModel::FormatAddress (location), MB_ICONINFORMATION);
        return;
    }

    ShowHostProperties (location.path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ShowHostProperties
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::ShowHostProperties (const std::wstring & path)
{
    BOOL  shown = SHObjectProperties (GetHwnd(), SHOP_FILEPATH, path.c_str(), nullptr);



    IGNORE_RETURN_VALUE (shown, TRUE);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ChangeKnownFolder
//
//  The shared file is changed under its lock, then read back whole, so a
//  folder Casso recorded in the meantime shows too.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::ChangeKnownFolder (const std::wstring & folder, bool add)
{
    HRESULT                               hr = S_OK;
    std::vector<KnownFolderStore::Entry>  entries;
    std::vector<std::wstring>             folders;



    if (m_context.fs == nullptr)
    {
        return;
    }

    KnownFolderStore  store (*m_context.fs, m_context.baseDir);

    hr = add ? store.Append (folder, (int64_t) _time64 (nullptr)) : store.Remove (folder);

    if (FAILED (hr))
    {
        ShowMessage (L"Casso's folder list could not be changed.", MB_ICONERROR);
        return;
    }

    hr = store.Load (entries);
    IGNORE_RETURN_VALUE (hr, S_OK);

    folders = KnownFolderStore::ListRootFolders (*m_context.fs, m_context.baseDir, entries);

    m_browser.GetTreeModel().SetKnownFolders (folders);
    RebuildTree();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::RebuildTree
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::RebuildTree()
{
    std::vector<DxuiTreeNode>  roots;



    m_browser.GetTreeModel().InvalidateAll();
    m_browser.GetTreeRoots (roots);
    m_tree->SetNodes (std::move (roots));
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::RefreshTree
//
//  Re-reads the tree without closing what was open. The open folders are
//  opened again in row order, which puts every parent ahead of its children,
//  so each child's row exists by the time it is looked for. The highlighted
//  node and where the tree is looking follow RefreshAnchor, as the list's do.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::RefreshTree()
{
    std::vector<std::wstring>  open;
    std::vector<std::wstring>  keys;
    RefreshAnchor::Before      before;
    RefreshAnchor::After       after;
    std::wstring               highlighted = m_tree->GetHighlightedId();
    int                        row         = 0;



    for (row = 0; row < m_tree->GetVisibleCount(); row++)
    {
        const DxuiTreeNode *  node = m_tree->GetNodeAt (row);
        std::wstring          id   = (node != nullptr) ? node->id : std::wstring();

        before.keys.push_back (id);

        if (node != nullptr && node->expanded)
        {
            open.push_back (id);
        }
    }

    before.topRow   = m_tree->GetTopRow();
    before.capacity = m_tree->GetRowCap();
    before.focused  = m_tree->GetHighlight();

    if (before.focused >= 0)
    {
        before.selected.push_back (before.focused);
    }

    m_refreshingTree = true;

    RebuildTree();

    for (const std::wstring & id : open)
    {
        int  found = m_tree->FindRowById (id);

        if (found >= 0)
        {
            m_tree->SetRowExpanded (found, true);
        }
    }

    m_refreshingTree = false;
    UpdateWatchedFolders();

    for (row = 0; row < m_tree->GetVisibleCount(); row++)
    {
        const DxuiTreeNode *  node = m_tree->GetNodeAt (row);

        keys.push_back ((node != nullptr) ? node->id : std::wstring());
    }

    after = RefreshAnchor::Compute (before, keys);

    if (after.focused >= 0)
    {
        m_tree->HighlightRow (after.focused);
    }

    //  Last, since highlighting scrolls the node into view.
    m_tree->SetTopRow (after.topRow);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::OnActivateApp
//
//  Coming back to the window rereads what it shows, since another window,
//  Casso or Explorer may have changed it meanwhile. The selection is kept by
//  name.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult CassqueWindow::OnActivateApp (bool active)
{
    HRESULT  hr = S_OK;



    //  Follow system has no settings-change message to wait for here, so the
    //  Windows mode is read again whenever the window comes back.
    if (active && m_menuBar != nullptr && !IsChecked (CassqueCommands::kThemeLight) && !IsChecked (CassqueCommands::kThemeDark))
    {
        DxuiWindowsThemeColors::Instance().Refresh();
        ApplyTheme();
    }

    //  NO RE-READ HERE. Coming back to the window used to re-enumerate the
    //  folder and rebuild every row, whether or not anything had changed,
    //  which on a folder of a few thousand files is a stall for nothing. The
    //  watcher says what changed instead, and says it as it happens.
    //
    //  Without a watcher -- an unwatchable share, or a host that supplied
    //  none -- the browser shows what it read when it read it, and the next
    //  navigation picks up the rest.
    if (active && m_list != nullptr && m_folderWatch == nullptr && m_browser.GetBrowserModel().HasTabs())
    {
        hr = m_browser.Reload (true);
        IGNORE_RETURN_VALUE (hr, S_OK);
        FillList();
    }

    return DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::FillAddress
//
//  The segments and the path of where the active tab is.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::FillAddress()
{
    Location                   location = m_browser.GetLocation();
    std::vector<std::wstring>  labels;



    m_addressSegments = BrowserModel::GetAddressSegments (location, m_addressRoot);

    for (const BrowserModel::AddressSegment & segment : m_addressSegments)
    {
        labels.push_back (segment.label);
    }

    m_address->SetSegments (std::move (labels));
    m_address->SetPath (BrowserModel::FormatAddress (location));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::SubmitAddress
//
//  A path that goes nowhere says so and leaves the text as typed.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::SubmitAddress (const std::wstring & text)
{
    if (m_browser.NavigateToAddress (text))
    {
        SetFocusPane (Pane::List);
        FillList();
    }
    else
    {
        ShowMessage (L"Cassque can't find \"" + text + L"\". Check the path and try again.", MB_ICONWARNING);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ShowAddressMenu
//
//  The folders and images inside a segment's location, hung from the
//  separator after it, as Explorer's address bar lists a folder's children.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::ShowAddressMenu (int index, const RECT & anchor)
{
    std::vector<BrowserModel::AddressSegment>  children;
    std::vector<DxuiPopupMenuItem>             items;



    if (index < 0 || index >= (int) m_addressSegments.size())
    {
        return;
    }

    m_browser.GetFolderChildren (m_addressSegments[(size_t) index].location, children);
    m_menuCommands.clear();

    for (const BrowserModel::AddressSegment & child : children)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();
        Location                      target  = child.location;

        command->label = EscapeMnemonics (child.label);

        command->dispatch = [this, target]()
        {
            m_browser.NavigateToLocation (target);
            FillList();
        };

        items.push_back (DxuiPopupMenuItem::ForCommand (command));
        m_menuCommands.push_back (std::move (command));
    }

    if (!items.empty())
    {
        m_address->SetOpenSeparator (index);
        DxuiContextMenu::ShowUnder (*GetPopupHost(), anchor, std::move (items), [this] (bool)
        {
            m_address->SetOpenSeparator (-1);
            Invalidate();
        });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::UpdateWatchedFolders
//
//  The folder the list is showing -- for an image, the folder holding it --
//  and every host folder the tree has open, since those are the folders whose
//  contents are on screen.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::UpdateWatchedFolders()
{
    std::vector<std::wstring>  folders;
    Location                   showing = m_browser.GetLocation();
    int                        row     = 0;



    if (m_folderWatch == nullptr)
    {
        return;
    }

    if (showing.kind == Location::Kind::HostFolder && !showing.path.empty())
    {
        folders.push_back (showing.path);
    }
    else if (!showing.path.empty())
    {
        folders.push_back (CassqueBrowser::GetParentFolder (showing.path));
    }

    for (row = 0; row < m_tree->GetVisibleCount(); row++)
    {
        const DxuiTreeNode *  node = m_tree->GetNodeAt (row);
        Location              at;

        if (node != nullptr && node->expanded
            && m_browser.TryGetNodeLocation (node->id, at)
            && at.kind == Location::Kind::HostFolder && !at.path.empty())
        {
            folders.push_back (at.path);
        }
    }

    m_folderWatch->SetWatched (folders);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::RefreshChangedFolders
//
//  Runs once a burst has settled. The same files stay selected, and the view
//  stays on the same files, by the rules RefreshAnchor sets out: a file added
//  or removed elsewhere in the folder is no reason to lose the selection or to
//  move what is on screen.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::RefreshChangedFolders()
{
    HRESULT                    hr = S_OK;
    std::vector<std::wstring>  changed;
    std::vector<std::wstring>  keys;
    RefreshAnchor::Before      before;
    RefreshAnchor::After       after;



    if (m_folderWatch == nullptr || !m_folderWatch->TakeChanged (changed))
    {
        return;
    }

    RefreshTree();

    if (!m_browser.GetBrowserModel().HasTabs())
    {
        Invalidate();
        return;
    }

    //  The view as it stands, in keys, before the rows under it change.
    m_browser.GetRowKeys (before.keys);
    before.topRow   = m_list->GetTopRow();
    before.capacity = m_list->GetVisibleRowCapacity();
    before.selected = m_list->GetSelectedRows();

    //  The focused item counts only as part of the selection it belongs to.
    before.focused  = m_list->IsRowSelected (m_list->GetSelectedRow()) ? m_list->GetSelectedRow() : -1;

    //  Re-read without the browser's own restore: the anchor decides both the
    //  selection and where the view lands.
    hr = m_browser.Reload (false);
    IGNORE_RETURN_VALUE (hr, S_OK);
    FillList();

    m_browser.GetRowKeys (keys);
    after = RefreshAnchor::Compute (before, keys);

    m_browser.SetSelectedRows (after.selected);
    m_list->SetSelectedRows   (after.selected, after.focused);

    //  Last, since restoring the selection scrolls it into view.
    m_list->SetTopRow (after.topRow);

    FillPreview();
    FillStatus();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ApplyStoredColumnWidths
//
//  The widths from the last run, where there are any. A stored zero means the
//  column was never given a width of its own and still fits itself.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::ApplyStoredColumnWidths()
{
    size_t  count = m_prefs.columnWidthsDip.size();
    size_t  c     = 0;



    for (c = 0; c < count; c++)
    {
        if (m_prefs.columnWidthsDip[c] > 0)
        {
            m_list->SetColumnOverrideWidthPx (c, m_scaler.ToPx (m_prefs.columnWidthsDip[c]));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::GetEmptyLocationMessage
//
//  What an empty list says, in the words the holding file system uses. ProDOS
//  calls them directories, which is what the command line's mkdir and rmdir
//  make, and Windows calls them folders. An empty disk image is worth telling
//  apart from either: it holds nothing at all, where an empty folder says
//  nothing about the rest of the disk.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassqueWindow::GetEmptyLocationMessage (Location::Kind kind)
{
    switch (kind)
    {
        case Location::Kind::HostFolder:    return L"This folder is empty.";
        case Location::Kind::DiskDirectory: return L"This directory is empty.";
        case Location::Kind::DiskImage:     return L"This disk image is empty.";
        default:                            return L"This location is empty.";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ShowAddressHistoryMenu
//
//  The paths typed into the bar, newest first.
//
//  EACH PICK GOES BACK THROUGH THE TEXT, not through the location it parses
//  to. The history records a path where a typed navigation succeeds, so
//  submitting the text is what moves the pick to the top of the list; handing
//  the parsed location straight to the browser would navigate correctly and
//  silently leave the order alone.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::ShowAddressHistoryMenu (const RECT & anchor)
{
    std::vector<DxuiPopupMenuItem>  items;



    m_menuCommands.clear();

    for (const std::wstring & typed : m_browser.GetTypedPaths().GetEntries())
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();
        std::wstring                  text    = typed;

        command->label    = EscapeMnemonics (typed);
        command->dispatch = [this, text]() { SubmitAddress (text); };

        items.push_back (DxuiPopupMenuItem::ForCommand (command));
        m_menuCommands.push_back (std::move (command));
    }

    //  Under the whole bar and as wide as it, as Explorer's history list is,
    //  rather than under the chevron that opened it.
    UNREFERENCED_PARAMETER (anchor);

    if (!items.empty())
    {
        DxuiContextMenu::ShowUnderMatchingWidth (*GetPopupHost(), m_address->GetBounds(), std::move (items));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ShowAddressOverflowMenu
//
//  The segments collapsed behind the address bar's overflow button, nearest
//  first, as Explorer lists them.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::ShowAddressOverflowMenu (const RECT & anchor)
{
    std::vector<DxuiPopupMenuItem>  items;
    int                             i = 0;



    m_menuCommands.clear();

    for (i = (std::min) (m_address->GetFirstShown(), (int) m_addressSegments.size()) - 1; i >= 0; i--)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();
        Location                      target  = m_addressSegments[(size_t) i].location;

        command->label    = EscapeMnemonics (m_addressSegments[(size_t) i].label);
        command->dispatch = [this, target]()
        {
            m_browser.NavigateToLocation (target);
            FillList();
        };

        items.push_back (DxuiPopupMenuItem::ForCommand (command));
        m_menuCommands.push_back (std::move (command));
    }

    if (!items.empty())
    {
        DxuiContextMenu::ShowUnder (*GetPopupHost(), anchor, std::move (items));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ShowHistoryMenu
//
//  Where Back or Forward would go, nearest first; picking one takes as many
//  steps as it is away.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::ShowHistoryMenu (bool forward, const RECT & anchor)
{
    std::vector<DxuiPopupMenuItem>  items;
    std::vector<Location>           stack;
    size_t                          i     = 0;



    if (!m_browser.GetBrowserModel().HasTabs())
    {
        return;
    }

    stack = forward ? m_browser.GetBrowserModel().GetActiveTab().forward
                    : m_browser.GetBrowserModel().GetActiveTab().back;

    m_menuCommands.clear();

    for (i = 0; i < stack.size(); i++)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();
        size_t                        steps   = i + 1;

        command->label    = EscapeMnemonics (CassqueBrowser::GetLocationLabel (stack[stack.size() - 1 - i]));
        command->dispatch = [this, forward, steps]()
        {
            if (forward ? m_browser.GoForwardBy (steps) : m_browser.GoBackBy (steps))
            {
                FillList();
            }
        };

        items.push_back (DxuiPopupMenuItem::ForCommand (command));
        m_menuCommands.push_back (std::move (command));
    }

    if (!items.empty())
    {
        DxuiContextMenu::ShowUnder (*GetPopupHost(), anchor, std::move (items));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::EscapeMnemonics
//
//  A menu label reads an ampersand as a mnemonic marker, so a name's own is
//  doubled.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassqueWindow::EscapeMnemonics (const std::wstring & text)
{
    std::wstring  escaped;



    for (wchar_t ch : text)
    {
        escaped += (ch == L'&') ? L"&&" : std::wstring (1, ch);
    }

    return escaped;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::GetProfileRoot
//
//  The user's profile folder and the name the shell gives it, which is the
//  user's own name; empty when the shell will not say.
//
////////////////////////////////////////////////////////////////////////////////

BrowserModel::AddressRoot CassqueWindow::GetProfileRoot()
{
    BrowserModel::AddressRoot  root;
    IShellItem               * item = nullptr;
    PWSTR                      path = nullptr;
    PWSTR                      name = nullptr;
    HRESULT                    hr   = S_OK;



    //  The profile as the shell's namespace holds it, where its display name
    //  is the user's name. The item for its file system path is displayed as
    //  the folder's own name instead.
    hr = SHCreateItemFromParsingName (L"shell:UsersFilesFolder", nullptr, IID_PPV_ARGS (&item));

    if (SUCCEEDED (hr))
    {
        hr = item->GetDisplayName (SIGDN_FILESYSPATH, &path);
    }

    if (SUCCEEDED (hr))
    {
        hr = item->GetDisplayName (SIGDN_NORMALDISPLAY, &name);
    }

    if (SUCCEEDED (hr))
    {
        root.path  = path;
        root.label = name;
    }

    CoTaskMemFree (name);
    CoTaskMemFree (path);

    if (item != nullptr)
    {
        item->Release();
    }

    return root;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::FillTabs
//
//  Tabs from the left of the strip, labeled by where each tab is. They share
//  the strip's width less the + button, no wider than a full tab and no
//  narrower than the minimum; past that the strip scrolls.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::FillTabs()
{
    const BrowserModel &            model  = m_browser.GetBrowserModel();
    RECT                            strip  = m_tabs->GetBounds();
    int                             width  = m_scaler.ToPx (kTabWidthDip);
    int                             count  = (int) model.GetTabCount();
    std::vector<DxuiTabStrip::Tab>  tabs;
    size_t                          index  = 0;



    if (count > 0)
    {
        width = std::clamp (((int) (strip.right - strip.left) - m_scaler.ToPx (DxuiTabStrip::kNewTabWidthDip)) / count, m_scaler.ToPx (kTabMinWidthDip), width);
    }

    for (index = 0; index < model.GetTabCount(); index++)
    {
        DxuiTabStrip::Tab  tab;
        const Location &   location = model.GetTab (index).location;

        //  Tabs start below the strip's top, as Explorer's do, and reach its
        //  bottom, where the selected one joins the row below.
        tab.label = m_browser.GetTabLabel (index);
        tab.rect  = RECT { strip.left + (int) index * width, strip.top + m_scaler.ToPx (kTabTopDip), strip.left + (int) (index + 1) * width, strip.bottom };

        switch (location.kind)
        {
            case Location::Kind::None:          tab.icon = m_shellIcons.GetForKind (IShellIcons::Kind::ThisPc); break;
            case Location::Kind::DiskDirectory: tab.icon = m_shellIcons.GetForKind (IShellIcons::Kind::Folder); break;
            case Location::Kind::DiskImage:     tab.icon = m_shellIcons.GetForPath (location.path, false);     break;
            default:                            tab.icon = m_shellIcons.GetForPath (location.path, true);      break;
        }

        tabs.push_back (tab);
    }

    m_tabs->SetTabs (std::move (tabs));
    m_tabs->SetSelected ((int) model.GetActiveIndex());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::SwitchToTab
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::SwitchToTab (size_t index)
{
    if (m_browser.SwitchTab (index))
    {
        FillList();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::BeginDragOut
//
//  The drag runs its own loop until the drop or the cancel, so the list is
//  told the button came up afterwards; it saw the press and never the
//  release.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::BeginDragOut()
{
    HRESULT                                  hr      = S_OK;
    DWORD                                    effect  = DROPEFFECT_NONE;
    DxuiMouseEvent                           release;
    HostFileNaming::Style                    style   = (m_prefs.hostNaming == CassquePrefs::kNamingCiderPress)
                                                     ? HostFileNaming::Style::CiderPress : HostFileNaming::Style::Descriptive;
    std::vector<DxuiDragDropSource::Format>  formats = CassqueDragOut::BuildFormats (m_browser, style);



    release.kind   = DxuiMouseEventKind::Up;
    release.button = DxuiMouseButton::Left;
    m_list->OnMouse (release);

    if (formats.empty())
    {
        return;
    }

    hr = DxuiDragDropSource::Begin (std::move (formats), DROPEFFECT_COPY, effect);
    IGNORE_RETURN_VALUE (hr, S_OK);

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::OnDropFile
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::OnDropFile (const std::wstring & path)
{
    ReportOutcome (m_actions.PutFiles ({ path }, MakeAddressPrompt()), L"Put");
    FillList();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::SelectTheme
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::SelectTheme (const char * name)
{
    m_prefs.theme = name;
    ApplyTheme();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::IsCassoThemeName
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueWindow::IsCassoThemeName (const std::string & name)
{
    return name == CassquePrefs::kThemeSkeuomorphic
        || name == CassquePrefs::kThemeDarkModern
        || name == CassquePrefs::kThemeRetroTerminal;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::MakeAddressPrompt
//
//  Asks for a binary's load address, prefilled with the content's
//  suggestion, and asks again after an address that does not parse.
//
////////////////////////////////////////////////////////////////////////////////

CassqueActions::AddressFn CassqueWindow::MakeAddressPrompt()
{
    return [this] (const std::wstring & hostName, Word suggested, Word & outAddress)
    {
        std::wstring  text = std::format (L"${:04X}", suggested);

        for (;;)
        {
            if (!CassquePromptDialog::Ask (GetHwnd(), m_theme, L"Load Address",
                                           hostName + L" is a binary. Load address:", text, 8, text))
            {
                return false;
            }

            if (CassqueActions::TryParseAddress (text, outAddress))
            {
                return true;
            }

            ShowMessage (L"Type an address from $0000 to $FFFF.", MB_ICONWARNING);
        }
    };
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::FindCassoTarget
//
//  The Casso that launched this browser while it is still there, otherwise
//  any running Casso, otherwise none.
//
////////////////////////////////////////////////////////////////////////////////

HWND CassqueWindow::FindCassoTarget() const
{
    if (m_context.owner != nullptr && IsWindow (m_context.owner))
    {
        return m_context.owner;
    }

    return FindWindowExW (nullptr, nullptr, Win32IntentChannel::kWindowClass, nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::AskCassoToDescribe
//
//  The answer arrives as a reply message and updates the drive count the
//  menus read. With no Casso running, a new one opens with the default
//  machine, which has two drives.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::AskCassoToDescribe()
{
    HWND  target = FindCassoTarget();
    bool  sent   = false;



    if (target == nullptr)
    {
        m_cassoDriveCount = Win32IntentChannel::kMaxDriveCount;
        return;
    }

    sent = Win32IntentChannel::SendTo (target, GetHwnd(), Win32IntentChannel::GetMessageId(), Win32IntentChannel::EncodeDescribe());
    IGNORE_RETURN_VALUE (sent, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::RunRawVerb
//
//  A read asks where the bytes start and how many, then where to save them;
//  a write asks for the file, then where it goes, then confirms, since it
//  overwrites whatever is there with no file system to stop it.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::RunRawVerb (CassqueActions::Verb verb)
{
    HRESULT                  hr       = S_OK;
    bool                     sectors  = verb == CassqueActions::Verb::ReadSectors || verb == CassqueActions::Verb::WriteSectors;
    bool                     reading  = verb == CassqueActions::Verb::ReadSectors || verb == CassqueActions::Verb::ReadBlocks;
    std::wstring             text     = sectors ? L"17 0 1" : L"2 1";
    std::wstring             prompt;
    std::vector<int>         numbers;
    std::filesystem::path    picked;
    bool                     chosen   = false;
    FileDialogSpec           spec;
    int                      answer   = 0;
    CassqueActions::Outcome  outcome;



    if (m_actions.GetFormatTarget().empty())
    {
        return;
    }

    if (!reading)
    {
        hr = m_dialogs.PickFileToOpen (GetHwnd(), spec, picked, chosen);

        if (FAILED (hr) || !chosen)
        {
            return;
        }

        text = sectors ? L"17 0" : L"2";
    }

    prompt = sectors ? (reading ? L"Track, sector and count:" : L"Track and sector to write at:")
                     : (reading ? L"Block and count:"         : L"Block to write at:");

    do
    {
        if (!CassquePromptDialog::Ask (GetHwnd(), m_theme, GetVerbLabel (verb), prompt, text, 16, text))
        {
            return;
        }
    }
    while (!CassqueActions::TryParseNumbers (text, sectors ? 2 : 1, numbers));

    numbers.resize (3, 1);

    if (reading)
    {
        spec.defaultFileName  = sectors ? L"sectors.bin" : L"blocks.bin";
        spec.defaultExtension = L"bin";

        hr = m_dialogs.PickFileToSave (GetHwnd(), spec, picked, chosen);

        if (FAILED (hr) || !chosen)
        {
            return;
        }

        outcome = sectors ? m_actions.ReadSectors (numbers[0], numbers[1], numbers[2], picked.wstring())
                          : m_actions.ReadBlocks  (numbers[0], numbers[1], picked.wstring());

        ReportOutcome (outcome, reading ? L"Read" : L"Write");
        return;
    }

    answer = DxuiMessageBox (GetHwnd(), m_theme,
                             L"Write the file's bytes straight onto the disk image? What is there now is overwritten.",
                             GetVerbLabel (verb), MB_YESNO | MB_ICONWARNING);

    if (answer != IDYES)
    {
        return;
    }

    outcome = sectors ? m_actions.WriteSectors (numbers[0], numbers[1], picked.wstring())
                      : m_actions.WriteBlocks  (numbers[0], picked.wstring());

    ReportOutcome (outcome, L"Write");
    FillList();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::GetNowMs
//
////////////////////////////////////////////////////////////////////////////////

int64_t CassqueWindow::GetNowMs()
{
    return (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
               std::chrono::steady_clock::now().time_since_epoch()).count();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::RouteToolbarMouse
//
//  The toolbar takes its pointer input through its own calls rather than
//  OnMouse. A move always reaches it, so its hover clears when the pointer
//  leaves; it is consumed only over the strip. The tooltip follows the hover.
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueWindow::RouteToolbarMouse (DxuiToolbar & toolbar, const DxuiMouseEvent & ev)
{
    int              x      = ev.positionDip.x;
    int              y      = ev.positionDip.y;
    bool             took   = false;
    RECT             anchor = {};
    const wchar_t *  tip    = nullptr;
    int64_t          nowMs  = GetNowMs();



    switch (ev.kind)
    {
        case DxuiMouseEventKind::Move:
            took = toolbar.OnToolbarMouseMove (x, y);
            tip  = took ? toolbar.GetTooltipAt (x, y, anchor) : nullptr;

            if (tip != nullptr && *tip != L'\0')
            {
                m_tooltip.RequestShow (anchor, tip, nowMs);
            }
            else
            {
                m_tooltip.RequestHide (nowMs);
            }

            break;

        case DxuiMouseEventKind::Down:
            took = ev.button == DxuiMouseButton::Left && toolbar.OnToolbarLButtonDown (x, y);

            //  A right-click on Back or Forward lists where each would go, as
            //  Explorer's does.
            if (ev.button == DxuiMouseButton::Right && toolbar.TryGetEntryRect (CassqueCommands::kBack, anchor) && Contains (anchor, ev.positionDip))
            {
                ShowHistoryMenu (false, anchor);
                took = true;
            }
            else if (ev.button == DxuiMouseButton::Right && toolbar.TryGetEntryRect (CassqueCommands::kForward, anchor) && Contains (anchor, ev.positionDip))
            {
                ShowHistoryMenu (true, anchor);
                took = true;
            }

            if (took)
            {
                m_tooltip.HideImmediate();
            }

            break;

        case DxuiMouseEventKind::Up:
            took = ev.button == DxuiMouseButton::Left && toolbar.OnToolbarLButtonUp (x, y);
            break;

        case DxuiMouseEventKind::Leave:
            toolbar.OnToolbarMouseLeave();
            m_tooltip.RequestHide (nowMs);
            break;

        default:
            break;
    }

    return took;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::OnTimer
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult CassqueWindow::OnTimer (UINT_PTR timerId)
{
    if (timerId == kFolderTimerId)
    {
        KillTimer (GetHwnd(), kFolderTimerId);
        RefreshChangedFolders();

        return DxuiMessageResult::Handled;
    }

    if (timerId != kTooltipTimerId)
    {
        return DxuiMessageResult::NotHandled;
    }

    if (m_tooltip.WantsTick())
    {
        m_tooltip.Tick (GetNowMs());
    }

    if (m_address->WantsTick())
    {
        m_address->Tick (GetNowMs());
        Invalidate();
    }

    //  Menus slide open and submenus wait out a delay, both on ticks the host
    //  supplies; Casso supplies them from its frame loop, and this window from
    //  its timer.
    if (m_menuBar->WantsTick() || m_toolbar->WantsTick() || m_previewToolbar->WantsTick() || GetPopupHost()->GetContextMenu().WantsTick())
    {
        m_menuBar->TickMenus (GetNowMs());
        m_toolbar->TickMenus (GetNowMs());
        m_previewToolbar->TickMenus (GetNowMs());
        GetPopupHost()->GetContextMenu().Tick (GetNowMs());
        Invalidate();
    }

    //  Scrollbars widen and narrow over a few frames as the pointer comes and
    //  goes.
    if (((int) m_hexView->TickScrollbars (GetNowMs())
       | (int) m_textView->TickScrollbars (GetNowMs())
       | (int) m_list->TickScrollbars (GetNowMs())
       | (int) m_previewList->TickScrollbars (GetNowMs())
       | (int) m_tree->TickScrollbars (GetNowMs())
       | (int) m_picture->TickScrollbars (GetNowMs())) != 0)
    {
        Invalidate();
    }

    //  The search box's caret blinks on the frames this asks for.
    if (m_focus == Pane::Search || m_focus == Pane::GoTo)
    {
        Invalidate();
    }

    return DxuiMessageResult::Handled;
}
