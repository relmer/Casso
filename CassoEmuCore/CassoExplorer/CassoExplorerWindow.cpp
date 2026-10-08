#include "Pch.h"

#include "CassoExplorer/CassoExplorerIcons.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "CassoExplorer/CassoExplorerAbout.h"
#include "CassoExplorer/CassoExplorerDragOut.h"
#include "CassoExplorer/CassoExplorerNewDiskDialog.h"
#include "CassoExplorer/CassoExplorerProperties.h"
#include "CassoExplorer/CassoExplorerRawDialog.h"
#include "CassoExplorer/CassoExplorerOptionsDialog.h"
#include "CassoExplorer/Model/CassoTargeting.h"
#include "CassoExplorer/CassoExplorerPromptDialog.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/Model/KnownFolderStore.h"
#include "CassoExplorer/Model/LaunchCommand.h"
#include "AssetBootstrap.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "Config/FileAssociations.h"
#include "Config/GlobalUserPrefs.h"
#include "Seams/Win32UserClasses.h"
#include "Config/Win32FileSystem.h"
#include "Core/MachineConfig.h"
#include "Core/MachineScanner.h"
#include "Core/PathResolver.h"
#include "Core/TextEncoding.h"
#include "resource.h"





//  The Theme menu's rows, in order, by the theme each one selects.
static constexpr const char *  s_kpszThemeRows[] =
{
    CassoExplorerPrefs::kThemeLight,
    CassoExplorerPrefs::kThemeDark,
    CassoExplorerPrefs::kThemeFollowSystem,
    CassoExplorerPrefs::kThemeSkeuomorphic,
    CassoExplorerPrefs::kThemeDarkModern,
    CassoExplorerPrefs::kThemeRetroTerminal,
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::CassoExplorerWindow
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerWindow::CassoExplorerWindow (CassoExplorerBrowser & browser, CassoExplorerActions & actions, CassoExplorerPrefs & prefs, Context context)
    : m_browser  (browser),
      m_actions  (actions),
      m_prefs    (prefs),
      m_context  (std::move (context)),
      m_commands (CassoExplorerCommands::Handlers {
                      [this] (int id)       { Dispatch (id); },
                      [this] (int id)       { return IsEnabled (id); },
                      [this] (int id)       { return IsChecked (id); },
                      [this] (int id)       { return GetCommandLabel (id); } })
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::~CassoExplorerWindow
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerWindow::~CassoExplorerWindow()
{
    m_shellVerbs.UnwatchRecycleBin();
    m_dropTarget.Shutdown();
    m_labelTip.Hide();
    DestroyBackend();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::Open
//
//  The class name is the one a second launch looks for, so it is fixed. A
//  remembered placement still on a monitor is applied before the first show.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoExplorerWindow::Open (HINSTANCE instance, const std::wstring & title, int showCommand)
{
    HRESULT                   hr     = S_OK;
    DxuiWindow::CreateParams  params;
    WINDOWPLACEMENT           place  = { sizeof (place) };
    RECT                      rect   = {};
    BOOL                      result = FALSE;



    AdoptSystemColors();

    m_theme = &CassoExplorerShell::ChooseTheme (m_prefs.theme, DxuiWindowsThemeColors::Instance().IsDarkMode(), m_lightTheme, m_darkTheme);

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
    params.initialSizeDip           = { CassoExplorerShell::kDefaultWidthDip, CassoExplorerShell::kDefaultHeightDip };
    params.minSizeDip               = { kMinWindowWidthDip, 400 };
    params.resizable                = true;
    params.insetContentBelowCaption = true;
    params.classNameOverride        = CassoExplorerShell::kWindowClass;
    params.appIconBig               = LoadIconW (instance, MAKEINTRESOURCEW (IDI_CASSO_EXPLORER));
    params.appIconSmall             = params.appIconBig;

    hr = DxuiWindow::Create (params);
    CHR (hr);

    //  The caption draws the app's own icon, as Casso's does; the taskbar
    //  already has it from the create parameters. A failure leaves the caption
    //  without one rather than failing the window.
    {
        HICON          captionIcon = (HICON) LoadImageW (instance, MAKEINTRESOURCEW (IDI_CASSO_EXPLORER), IMAGE_ICON,
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

    //  The animation tick starts with the first input; see ArmTick.

    //  Files and folders dropped on the list or the tree go into the image or
    //  directory under the pointer, and entries dragged out of another image
    //  go in with their types. A failure to register leaves the window
    //  without drops, not without a window.
    {
        HRESULT  hrDrop = m_dropTarget.Initialize (GetHwnd(), &m_dropHits,
                                                   [this] (int, const std::wstring & path) { OnDropFile (path); });

        m_dropTarget.SetDataHandlers ([this] (IDataObject * data, int tag, POINT screen) { return GetDropEffect (data, tag, screen); },
                                      [this] (IDataObject * data, int tag, POINT screen) { OnDrop (data, tag, screen); },
                                  [this]                                          { ClearDropTarget(); });

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

            //  Twice. Windows counts a window as on the monitor that holds most
            //  of it, and placing it can change which one that is, and so its
            //  DPI -- which scales it by the ratio of the two. The second time
            //  its DPI is already the saved one, so the size is kept as saved.
            result = SetWindowPlacement (GetHwnd(), &place);
            IGNORE_RETURN_VALUE (result, TRUE);
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

    m_opened = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::StorePlacement
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::StorePlacement()
{
    WINDOWPLACEMENT  place = { sizeof (place) };



    if (GetHwnd() == nullptr || !GetWindowPlacement (GetHwnd(), &place))
    {
        return;
    }

    //  A snapped window's normal position is where it was before the snap,
    //  which is not where it was left. Its own rectangle is, moved into the
    //  work-area terms the normal position uses.
    if (IsWindowArranged (GetHwnd()) && !IsZoomed (GetHwnd()))
    {
        MONITORINFO  monitor = { sizeof (monitor) };
        RECT         window  = {};

        if (GetWindowRect (GetHwnd(), &window)
            && GetMonitorInfoW (MonitorFromWindow (GetHwnd(), MONITOR_DEFAULTTONEAREST), &monitor))
        {
            OffsetRect (&window, monitor.rcMonitor.left - monitor.rcWork.left, monitor.rcMonitor.top - monitor.rcWork.top);
            place.rcNormalPosition = window;
        }
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
//  CassoExplorerWindow::OnCreate
//
//  Children are created in paint order: panes first, then the splitters over
//  their edges, the status bar, and the menu bar last so its strip is on top.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::OnCreate()
{
    CassoExplorerNamedControl<DxuiTreeView>         * tree            = CreateChild<CassoExplorerNamedControl<DxuiTreeView>>();
    CassoExplorerNamedControl<DxuiListView>         * list            = CreateChild<CassoExplorerNamedControl<DxuiListView>>();
    CassoExplorerNamedControl<DxuiListView>         * previewList     = nullptr;
    CassoExplorerNamedControl<DxuiFramebufferView>  * picture         = nullptr;
    CassoExplorerNamedControl<DxuiHexView>          * hexView         = nullptr;
    CassoExplorerNamedControl<DxuiSplitter>         * treeSplitter    = nullptr;
    CassoExplorerNamedControl<DxuiSplitter>         * previewSplitter = nullptr;



    tree->SetAccessibleName (L"Folders and disk images");
    list->SetAccessibleName (L"Files");

    m_tree            = tree;
    m_list            = list;
    m_listMessage     = CreateChild<DxuiLabel> (L"", DxuiTextRole::Muted, DxuiTextHAlign::Center, DxuiTextVAlign::Center);
    previewList       = CreateChild<CassoExplorerNamedControl<DxuiListView>>();
    picture           = CreateChild<CassoExplorerNamedControl<DxuiFramebufferView>>();
    hexView           = CreateChild<CassoExplorerNamedControl<DxuiHexView>>();
    m_textView        = CreateChild<DxuiTextView>();
    m_previewMessage  = CreateChild<DxuiSelectableText>();
    m_previewMessage->SetTextRole (DxuiTextRole::Muted);
    m_previewMessage->SetAlign    (DxuiTextHAlign::Center, DxuiTextVAlign::Center);
    treeSplitter      = CreateChild<CassoExplorerNamedControl<DxuiSplitter>>();
    previewSplitter   = CreateChild<CassoExplorerNamedControl<DxuiSplitter>>();

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
    m_findBox         = CreateChild<DxuiTextInput>();
    m_menuBar         = CreateChild<DxuiMenuBar>();

    //  Explorer's status bar runs on from the list above it, with no lines.
    m_status->SetDividers (false);

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
                                : DxuiTheme::GetUiFace(),
                                DxuiAddressBar::kFontDip);
    m_toolbar->SetPopupHost    (GetPopupHost());
    m_toolbar->SetEntries      (m_commands.BuildToolbarEntries());

    m_commandBar = CreateChild<DxuiToolbar>();
    m_commandBar->SetTextRenderer (GetTextRenderer());
    m_commandBar->SetPopupHost    (GetPopupHost());

    //  Explorer's flat buttons, on this bar and the others.
    m_toolbar->SetFlatStyle    (true);
    m_commandBar->SetFlatStyle (true);

    m_commandBar->EnableSeeMore   (s_kpszMdl2More, L"See more", &CassoExplorerIcons::s_kMore);
    m_commandBar->SetEntries      (m_commands.BuildCommandBarEntries());
    m_commandBar->SetIconFace     (DxuiTextRenderer::IsFontFamilyInstalled (DxuiToolbar::kFluentIconFace)
                                   ? DxuiToolbar::kFluentIconFace
                                   : DxuiToolbar::kMdl2IconFace);
    m_commandBar->SetIconDip      (kCommandBarIconDip);
    m_commandBar->SetChevronOnIcons (true);
    m_commandBar->SetGroupSeparators (true);
    m_commandBar->SetButtonPadDip    (kCommandBarPadDip);
    m_commandBar->SetLabelFontDip    (kCommandBarLabelDip);
    m_commandBar->SetGroupGapDp      (kCommandBarGroupGapDp);
    m_commandBar->SetBarPadDp        (kCommandBarPadXDp);
    SetCommandBarDropDowns();
    m_tooltip.SetPopupHost     (GetPopupHost());
    m_labelTip.SetPopupHost    (GetPopupHost());
    m_tooltip.SetFollowPointer (true);

    //  The buttons along a context menu's edge have only a word under their
    //  icons, so each has a tip, as Explorer's do.
    GetPopupHost()->GetContextMenu().SetOnIconHover ([this] (const DxuiCommand * command)
    {
        POINT  pointer = {};
        RECT   anchor  = {};

        if (command == nullptr || !GetCursorPos (&pointer) || !ScreenToClient (GetHwnd(), &pointer))
        {
            HideHoverTip (TipOwner::Menu);
            return;
        }

        anchor = { pointer.x, pointer.y, pointer.x + 1, pointer.y + 1 };
        ShowHoverTip (TipOwner::Menu, anchor, GetIconButtonTip (*command));
    });

    //  Explorer's navigation glyphs are in Windows 11's Segoe Fluent Icons.
    //  Without that font, use MDL2, which has the same code points; text in a
    //  missing font renders as nothing.
    m_toolbar->SetIconFace (DxuiTextRenderer::IsFontFamilyInstalled (DxuiToolbar::kFluentIconFace)
                            ? DxuiToolbar::kFluentIconFace
                            : DxuiToolbar::kMdl2IconFace);
    m_toolbar->SetIconDip  (kNavIconDip);

    m_previewToolbar = CreateChild<DxuiToolbar>();
    m_previewToolbar->SetFlatStyle (true);
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
        m_previewBarFocus = GetPreviewStopIndex (CassoExplorerCommands::kGoToOffset);
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
        m_previewBarFocus = GetPreviewStopIndex (CassoExplorerCommands::kFind);
        SetFocusPane (Pane::Search);
    });

    m_menuBar->SetPopupHost (GetPopupHost());
    m_menuBar->SetTextRendererForMeasure (GetTextRenderer());

    //  Drawn by the shell at the pixel size a row's icon is laid out at, and
    //  handed to the browser before the first nodes and rows are built.
    m_shellIcons.SetSizePx (MulDiv (DxuiTreeView::s_kIconDip, (int) GetDpiForWindow (GetHwnd()), (int) DxuiDpiScaler::kBaseDpi));
    m_browser.SetShellIcons (&m_shellIcons);
    m_browser.SetShellVerbs (&m_shellVerbs);
    m_explorerOptions = FolderOptions::ReadFromShell();
    m_browser.SetFolderOptions (m_explorerOptions);
    ApplyNavPaneOptions();
    SizeListIcons();
    m_listIcons.LoadInBackground (GetHwnd(), kIconsLoadedMessage);
    m_infoTips.Start (GetHwnd(), kInfoTipMessage);

    ConfigureWidgets();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ConfigureWidgets
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ConfigureWidgets()
{
    std::vector<DxuiTreeNode>  roots;
    bool                       grouped = false;
    bool                       opened  = false;



    //  CassoExplorer has no menu bar: Explorer's command bar takes its place. The
    //  strip stays, empty and hidden, so its Alt handling finds nothing.
    m_menuBar->SetItems   ({});
    m_menuBar->SetVisible (false);
    m_addressRoot = GetProfileRoot();

    m_browser.GetTreeRoots (roots);

    m_tree->SetShowCheckboxes (false);
    m_tree->SetFontDip (kProseFontDip);
    m_tree->SetIndentDip (kTreeIndentDip);
    m_tree->SetHorizontalScrollEnabled (false);
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
        UpdateWatchedFolders();
    });

    //  The Casso places and This PC open on their first level; the shell's
    //  other roots stay closed, as Explorer's do. Network in particular takes
    //  as long as the network takes to answer.
    for (const std::wstring & id : { std::wstring (TreeModel::kCassoRootId), std::wstring (TreeModel::kThisPcRootId) })
    {
        opened = m_tree->SetRowExpanded (m_tree->FindRowById (id), true);
        IGNORE_RETURN_VALUE (opened, true);
    }


    m_list->SetShowHeader (true);
    m_list->SetIconColumn (0);
    m_list->SetExplorerDetails (true);
    m_list->SetFontDip (kProseFontDip);
    m_list->SetRowHeightPxFn (&CassoExplorerWindow::GetListRowHeightPx);
    m_list->SetColumns (CassoExplorerBrowser::GetColumns());
    m_listColumnChosen.assign (CassoExplorerBrowser::GetColumns().size(), true);

    //  A header dragged into a new place stays there for the next run.
    ApplyColumnOrder();

    m_list->SetOnColumnsReordered ([this] (const std::vector<size_t> & order)
    {
        //  The Recycle Bin's order is its own, as Explorer's is.
        if (m_browser.GetLocation().kind == Location::Kind::RecycleBin)
        {
            Invalidate();
            return;
        }

        m_prefs.columnOrder.clear();

        for (size_t column : order)
        {
            m_prefs.columnOrder.push_back ((int) column);
        }

        //  The folder keeps its own, and the latest is what others follow.
        m_folderColumnOrder = m_prefs.columnOrder;
        RememberFolderColumns();
        Invalidate();
    });
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

    m_list->SetView (m_listView);

    m_list->SetOnSortColumn ([this] (int column)
    {
        m_browser.SortByColumn (column);
        RememberFolderSort();
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

        //  Rounded up, so the width read back is never narrower than the one
        //  the text was fitted to. The latest is the default for folders with
        //  none of their own, and this folder keeps its own, as Explorer's do.
        m_prefs.columnWidthsDip[(size_t) column] = (widthPx * (int) DxuiDpiScaler::kBaseDpi + (int) m_list->GetDpi() - 1) / (int) m_list->GetDpi();
        RememberFolderColumnWidths (column, m_prefs.columnWidthsDip[(size_t) column]);
    });

    m_list->SetOnActivateRow ([this] (int row)
    {
        //  A deleted item opens its properties, as Explorer's does.
        if (m_browser.GetLocation().kind == Location::Kind::RecycleBin)
        {
            RunRecycleBinVerb (CassoExplorerActions::Verb::Open);
        }
        else if (m_browser.GetSelectedRows().size() > 1)
        {
            //  Enter on several opens each, as Explorer's does.
            OpenEachSelected();
        }
        else if (m_browser.OpenRow (row))
        {
            FillList();
        }
        else if (m_browser.IsImageLocation())
        {
            //  A file in an image opens in its own program, from a copy.
            OpenSelectedEntries();
        }
        else if (m_browser.GetLocation().kind == Location::Kind::ShellFolder && row >= 0 && (size_t) row < m_browser.GetRows().size())
        {
            //  Anything else a shell folder holds does what Explorer's
            //  double-click does with it.
            const CatalogRow &  item = m_browser.GetRows()[(size_t) row];
            HRESULT             hr   = item.hostPath.empty() ? m_shellVerbs.OpenShellItem (GetHwnd(), item.shellId)
                                                             : m_shellVerbs.Open (GetHwnd(), item.hostPath);

            IGNORE_RETURN_VALUE (hr, S_OK);
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

    //  The hex view's menu is its own; Go to is the browser's, added to it,
    //  and the view's choices are kept for the next run.
    m_hexView->SetOnBuildContextMenu ([this] (std::vector<DxuiPopupMenuItem> & items)
    {
        std::shared_ptr<const DxuiCommand>  goTo = m_commands.Find (CassoExplorerCommands::kGoToOffset);

        if (goTo != nullptr)
        {
            items.push_back (DxuiPopupMenuItem::ForCommand (goTo));
        }
    });

    m_hexView->SetOnSettingsChanged ([this]()
    {
        m_prefs.hexShowValues = m_hexView->IsShowingValues();
        m_prefs.hexGrouping   = m_hexView->GetGrouping();
        m_prefs.hexColumns    = m_hexView->GetColumns();
        m_prefs.hexFormat     = (m_hexView->GetValueFormat() == DxuiHexView::ValueFormat::Signed)   ? CassoExplorerPrefs::kHexFormatSigned
                              : (m_hexView->GetValueFormat() == DxuiHexView::ValueFormat::Unsigned) ? CassoExplorerPrefs::kHexFormatUnsigned
                                                                                                     : CassoExplorerPrefs::kHexFormatHex;
        Invalidate();
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

    m_status->SetFields ({ { L"", 0, false, -1, true }, { L"", 0, false, -1, true }, { L"", 0, true },
                           { L"", kStatusFreeDip, false }, { L"", kStatusDetailDip, false }, { L"", kStatusZoomDip, false } });

    //  The tabs are drawn as File Explorer's are, and its tab close glyph is
    //  Segoe Fluent Icons', thinner than MDL2's.
    m_tabs->SetStyle    (DxuiTabStripStyle::Explorer);
    m_tabs->SetIconFace (DxuiTextRenderer::IsFontFamilyInstalled (DxuiToolbar::kFluentIconFace)
                         ? DxuiToolbar::kFluentIconFace
                         : DxuiToolbar::kMdl2IconFace);

    m_tabs->SetOnChange ([this] (int index) { SwitchToTab ((size_t) index); });
    m_tabs->SetOnMove   ([this] (int from, int to)
    {
        bool  moved = m_browser.MoveTab ((size_t) from, (size_t) to);

        IGNORE_RETURN_VALUE (moved, true);
    });
    m_tabs->SetOnNewTab ([this]() { Dispatch (CassoExplorerCommands::kNewTab); });
    m_tabs->SetOnClose  ([this] (int index)
    {
        //  The last tab stays, as the close command leaves it.
        if (m_browser.GetBrowserModel().GetTabCount() > 1 && m_browser.CloseTab ((size_t) index))
        {
            FillList();
        }
    });

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
    m_address->SetOnRoots     ([this] (const RECT & anchor) { ShowAddressRootsMenu (anchor); });

    m_findBox->SetTextRenderer (GetTextRenderer());
    m_findBox->SetHwnd         (GetHwnd());
    m_findBox->SetFont         (DxuiAddressBar::kVariableTextFace, DxuiAddressBar::kFontDip);
    m_address->SetOnHistory   ([this] (const RECT & anchor) { ShowAddressHistoryMenu (anchor); });
    m_address->SetHistoryChevron (false);

    //  Explorer drops its history as the address opens for editing, and
    //  turns it into completions as a path is typed.
    m_suggest.SetPopupHost (GetPopupHost());
    m_previewMessage->SetTextRenderer (GetTextRenderer());
    m_previewMessage->SetOwnerWindow  (GetHwnd());
    m_suggest.SetOnPick    ([this] (const std::wstring & text) { SubmitAddress (text); });
    m_address->SetOnEditState ([this] (bool editing)
    {
        m_addressTyped.clear();

        if (editing)
        {
            ShowAddressSuggestions (std::wstring());
        }
        else
        {
            m_suggest.Close();
        }
    });
    m_address->SetOnEditText ([this] (const std::wstring & text)
    {
        m_addressTyped = text;
        ShowAddressSuggestions (text);
    });
    m_address->SetOnEditKey ([this] (WPARAM vk) { return OnAddressKey (vk); });

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

    //  Deletions made anywhere reach the bin, so its list follows them.
    m_shellVerbs.WatchRecycleBin (GetHwnd(), kRecycleBinMessage);

    //  Slow shell folders read on threads of their own and say when done.
    m_browser.GetShellListings().SetTarget (GetHwnd(), kShellListedMessage);

    m_browser.RestoreTabs (m_prefs.tabs);
    m_browser.LeaveMissingLocation();

    m_tree->OnFocusChanged (true);

    if (!m_context.openPath.empty())
    {
        OpenPathInNewTab (m_context.openPath);
    }

    FillList();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::AdoptSystemColors
//
//  The Windows themes take the list surface and the accent from the system
//  each time a theme is chosen, which also picks up an accent changed while
//  the window was away.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::AdoptSystemColors()
{
    const DxuiWindowsThemeColors::SystemColors &  system = DxuiWindowsThemeColors::Instance().GetSystemColors();



    m_lightTheme.ApplySystemColors (system);
    m_darkTheme.ApplySystemColors  (system);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ApplyTheme
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ApplyTheme()
{
    AdoptSystemColors();

    m_theme = &CassoExplorerShell::ChooseTheme (m_prefs.theme, DxuiWindowsThemeColors::Instance().IsDarkMode(), m_lightTheme, m_darkTheme);

    //  Casso's own themes lend their chrome colors; the skeuomorphic one has
    //  no scene to draw here, so it is colors only.
    if (IsCassoThemeName (m_prefs.theme))
    {
        m_cassoTheme = CassoTheme::MakeByName (m_prefs.theme);
        m_theme      = &m_cassoTheme;
    }

    //  The menus' icons come in a set per theme.
    ApplyMenuSvgs();

    SetTheme (m_theme);

    if (m_menuBar != nullptr)
    {
        m_menuBar->SetStripColors    (m_theme->navStrip, m_theme->navHover, m_theme->navItemText);
        m_menuBar->SetDropdownColors (m_theme->dropdownBg, m_theme->dropdownHover, m_theme->dropdownItemText,
                                      m_theme->dropdownAccel, m_theme->panelEdge, m_theme->buttonBorder);
    }

    if (m_toolbar != nullptr)
    {
        //  Explorer's command bar is the list body's color, not the strip's
        //  above it, with a border line above and below it.
        m_toolbar->SetStripColors (m_theme->navStrip, m_theme->navItemText);
        m_toolbar->SetEdgeColor   (m_theme->Border());

        if (m_commandBar != nullptr)
        {
            m_commandBar->SetStripColors (m_theme->ContentBackground(), m_theme->navItemText);
            m_commandBar->SetEdgeColor   (m_theme->Border());
        }

        m_tooltip.SetTheme (*m_theme);
        m_suggest.SetTheme (m_theme);
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
//  CassoExplorerWindow::Layout
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);

    m_client = boundsDip;
    m_scaler.SetDpi (scaler.GetDpi());

    RecomputeLayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::RecomputeLayout
//
//  Menu strip and toolbar across the top, status band across the bottom, and
//  the three panes in what the dock leaves between them. The tree splitter
//  spans the whole body so its limits are measured against it; the preview
//  splitter spans everything right of the tree, with its position the list's
//  width.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::RecomputeLayout()
{
    IDxuiControl  * bands[]     = { &m_tabBand, &m_toolbarBand, &m_menuBand, &m_statusBand, &m_bodyBand };
    RECT            body        = {};
    RECT            right       = {};
    RECT            sashRect    = {};
    int             rightDip    = 0;
    bool            preview     = m_prefs.previewVisible;
    PaneWidths      panes;
    RECT            addressRect = {};
    RECT            findRect    = {};
    int             findWidth   = 0;



    if (m_menuBar == nullptr || m_client.bottom <= m_client.top)
    {
        return;
    }

    //  The toolbar's band thickness follows the responsive mode it plans for
    //  the width, so plan it before the bands are docked.
    m_toolbar->PlanForWidth    (m_client.right - m_client.left, m_scaler);
    m_commandBar->PlanForWidth (m_client.right - m_client.left, m_scaler);

    m_tabBand.SetThickness     (m_scaler.ToPx (kTabHeightDip));
    m_menuBand.SetThickness    ((int) std::floor (m_scaler.ToPxf (kCommandBarDip)));
    m_toolbarBand.SetThickness ((int) std::floor (m_scaler.ToPxf (kNavStripFillDip))   + DxuiToolbar::GetEdgePx (m_scaler));
    m_statusBand.SetThickness  (m_scaler.ToPx (DxuiStatusBar::GetBandDp()));

    m_dock.Arrange (m_client, m_scaler, bands);

    body = m_bodyBand.GetBounds();

    if (body.bottom <= body.top)
    {
        return;
    }

    m_tabs->Layout (m_tabBand.GetBounds(), m_scaler);
    FillTabs();

    m_commandBar->SetHostClientRect (m_client);
    m_commandBar->Layout (m_menuBand.GetBounds(), m_scaler);

    m_toolbar->SetHostClientRect (m_client);
    m_toolbar->Layout (m_toolbarBand.GetBounds(), m_scaler);
    //  The search box takes the row's right end, a quarter of it within
    //  Explorer's bounds, and the address the rest.
    addressRect = GetAddressRect (m_toolbar->GetFreeRect(), m_toolbarBand.GetBounds(), m_scaler);
    findWidth   = std::clamp ((int) (addressRect.right - addressRect.left) / 4, m_scaler.ToPx (kFindBoxMinDip), m_scaler.ToPx (kFindBoxMaxDip));
    findRect    = RECT { addressRect.right - findWidth, addressRect.top, addressRect.right, addressRect.bottom };

    addressRect.right = findRect.left - m_scaler.ToPx (kFindBoxGapDip);
    m_address->Layout (addressRect, m_scaler);
    m_findBox->Layout (findRect, m_scaler);

    m_tooltip.SetDpi (m_scaler.GetDpi());
    m_tooltip.SetViewportSize (m_client.right - m_client.left, m_client.bottom - m_client.top);
    m_status->Layout (m_statusBand.GetBounds(), m_scaler);

    panes = FitPanes (MulDiv (body.right - body.left, (int) DxuiDpiScaler::kBaseDpi, (int) m_scaler.GetDpi()),
                      m_prefs.treeWidthDip, preview ? m_prefs.previewWidthDip : 0);

    //  A narrow window has already taken each pane below its minimum, so
    //  the sash can go no further than where the fit put it.
    m_treeSplitter->Layout (body, m_scaler);
    m_treeSplitter->SetLimitsDip ((std::min) (kMinTreeWidthDip, panes.tree),
                                  (std::min) (kMinListWidthDip, panes.list) + (preview ? (std::min) (kMinPreviewWidthDip, panes.preview) + DxuiSplitter::kSashDip : 0));
    m_treeSplitter->SetPositionDip (panes.tree);

    sashRect = m_treeSplitter->GetSashRect();
    m_tree->Layout (RECT { body.left, body.top, sashRect.left, body.bottom }, m_scaler);

    //  The tree has no height until its first layout, so the node revealed at
    //  startup is scrolled into view here: to the middle, since the layouts
    //  that follow as the window settles can make the tree shorter.
    if (m_treeRevealPending && m_tree->GetRowCap() > 0)
    {
        m_tree->SetTopRow (m_tree->GetHighlight() - m_tree->GetRowCap() / 2);
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
        m_previewSplitter->SetLimitsDip ((std::min) (kMinListWidthDip, panes.list), (std::min) (kMinPreviewWidthDip, panes.preview));
        m_previewSplitter->SetPositionDip (rightDip - panes.preview - DxuiSplitter::kSashDip);

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
        m_listMessage->Layout    (GetMessageRect (listRect),    m_scaler);
        m_previewList->Layout    (previewRect, m_scaler);
        m_textView->Layout       (previewRect, m_scaler);
        m_hexView->Layout        (previewRect, m_scaler);
        m_picture->Layout        (previewRect, m_scaler);
        m_previewMessage->Layout (GetMessageRect (previewRect), m_scaler);
    }
    else
    {
        m_previewToolbar->SetVisible (false);
        m_list->Layout        (right, m_scaler);
        m_listMessage->Layout (GetMessageRect (right), m_scaler);
    }

    LayoutStatusFields();

    m_dropHits.Clear();
    m_dropHits.Register (DxuiHitRect { m_list->GetBounds(), DxuiHitSlot::Custom, kDropTagList });
    m_dropHits.Register (DxuiHitRect { m_tree->GetBounds(), DxuiHitSlot::Custom, kDropTagTree });
    m_dropHits.Register (DxuiHitRect { m_tabs->GetBounds(), DxuiHitSlot::Custom, kDropTagTabs });

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::Paint
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
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
//  CassoExplorerWindow::OnThemeChanged
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::OnThemeChanged()
{
    DxuiWindow::OnThemeChanged();

    //  Applying a theme announces a theme change to the controls, this one
    //  included, which must not apply it again.
    if (m_applyingTheme)
    {
        return;
    }

    m_applyingTheme = true;

    DxuiWindowsThemeColors::Instance().Refresh();
    ApplyTheme();
    Invalidate();

    m_applyingTheme = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::RevealLocationInTree
//
//  Expands This PC down to the current location's folder and highlights it,
//  as File Explorer's navigation pane does when a window opens on a folder.
//  Each path component is matched against the child labels without regard to
//  case, since a restored path need not match the case on disk.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::RevealLocationInTree()
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



    //  A root is a node of its own.
    if (location.kind == Location::Kind::Root)
    {
        row = m_tree->FindRowById (location.path);

        if (row >= 0 && current != location.path)
        {
            m_tree->HighlightRow (row);
        }

        return;
    }

    if (location.kind == Location::Kind::None || path.size() < 2 || path[1] != L':')
    {
        return;
    }

    //  A node the user clicked already shows the location, possibly under the
    //  Casso root rather than This PC, so it stays highlighted, and in view.
    if (current.size() >= path.size() && _wcsicmp (current.c_str() + current.size() - path.size(), path.c_str()) == 0)
    {
        ScrollTreeToRow (m_tree->GetHighlight());
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
    ScrollTreeToRow (row);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ScrollTreeToRow
//
//  Into view now, or at the first layout when the tree has no height yet.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ScrollTreeToRow (int row)
{

    if (m_tree->GetRowCap() > 0)
    {
        m_tree->EnsureRowVisible (row);
    }
    else
    {
        m_treeRevealPending = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::WalkTreeLabels
//
//  Expands down from a row one path component at a time, matching each against
//  the child labels without regard to case, and returns the deepest row
//  reached. A drive's label is its volume name, so a drive is matched by the
//  letter its id ends with instead.
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerWindow::WalkTreeLabels (int row, const std::wstring & path)
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
//  CassoExplorerWindow::FillList
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::FillList()
{
    const BrowserModel                            & model    = m_browser.GetBrowserModel();
    std::wstring                                    message  = m_browser.GetListError();
    size_t                                          count    = m_browser.GetRows().size();



    //  The rows are about to change under it.
    if (m_renameRow >= 0)
    {
        EndRename (false);
    }

    //  Before the cells are built, since their icons are fetched at the
    //  view's size.
    ApplyFolderView();

    //  The Recycle Bin has buttons of its own on the command bar.
    if ((m_browser.GetLocation().kind == Location::Kind::RecycleBin) != m_commandBarForBin)
    {
        m_commandBarForBin = !m_commandBarForBin;

        //  Explorer's own icons for the bin's two buttons, in the theme's set.
        m_commands.SetSvg (CassoExplorerCommands::kEmptyRecycleBin, GetMenuSvg (L"windows.recyclebin.empty"));
        m_commands.SetSvg (CassoExplorerCommands::kRestoreItems,    GetMenuSvg (L"windows.recyclebin.restoreall"));
        m_commandBar->SetEntries (m_commands.BuildCommandBarEntries (m_commandBarForBin));
        m_commandBar->Layout     (m_menuBand.GetBounds(), m_scaler);
    }

    //  A file's size or date may have changed with the listing.
    m_infoTips.Clear();

    //  The rows' cells are built as the list first asks for each, so a folder
    //  of thousands shows as fast as one of ten.
    m_rowCells.assign    (count, {});
    m_rowBuilt.assign    (count, false);
    m_rowProblems.assign (count, std::wstring());
    m_rowLocation = m_browser.GetLocation();
    m_rowDark     = DxuiColor::ComputeRelativeLuminance (m_theme->Background()) < 0.5f;

    m_list->SetRowSource ((int) count, [this] (int row) -> const std::vector<DxuiListView::Cell> & { return GetListRowCells (row); });

    //  Small icons and List measure every name; this reads them without
    //  building the rows' cells.
    m_list->SetRowNameSource ([this] (int row)
    {
        return (row >= 0 && (size_t) row < m_browser.GetRows().size()) ? m_browser.GetRows()[(size_t) row].name : std::wstring();
    });
    m_list->SetGroups    (m_browser.GetListGroups());

    //  A new location opens at its top-left corner, as Explorer's does, rather
    //  than wherever the list was scrolled for the last one. THE COLUMNS STAY AS
    //  THEY ARE: their widths belong to the view rather than to the folder, so
    //  they neither twitch from one folder to the next nor pay to re-measure
    //  every cell of every row on each navigation.
    if (m_browser.GetLocation() != m_listLocation)
    {
        m_listLocation = m_browser.GetLocation();
        m_address->SetLeadIcon (CassoExplorerBrowser::GetLocationIcon (m_listLocation, m_shellIcons));
        SetCommandBarDropDowns();

        //  Explorer's box says what it will search, and empties on leaving
        //  a search's results.
        if (!SearchQuery::IsId (m_listLocation.path))
        {
            std::vector<BrowserModel::AddressSegment>  segments = BrowserModel::GetAddressSegments (m_listLocation, m_addressRoot);

            m_findBox->SetText        (L"");
            m_findBox->SetPlaceholder (L"Search " + (segments.empty() ? std::wstring() : segments.back().label));
        }

        m_list->SetTopRow (0);
        m_list->SetLeftPx (0);

        if (IsNavOptionOn (&CassoExplorerPrefs::navExpandToCurrent))
        {
            RevealLocationInTree();
        }

        UpdateWatchedFolders();
    }

    //  Address and Locked are a disk catalog's; a host folder has neither, so
    //  its list shows only Explorer's four columns. The Recycle Bin adds its
    //  own two.
    ApplyColumnOrder();

    //  Explorer's Recycle Bin titles the type column Item type.
    m_list->SetColumnTitle ((size_t) CatalogModel::Column::Type, (m_browser.GetLocation().kind == Location::Kind::RecycleBin) ? L"Item type" : L"Type");

    for (size_t c = 0; c < m_listColumnChosen.size(); c++)
    {
        m_list->SetColumnVisible (c, IsListColumnShown (c, m_listColumnChosen[c], m_browser.GetLocation().kind, SearchQuery::IsId (m_browser.GetLocation().path)));
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
//  CassoExplorerWindow::GetListRowCells
//
//  A row's cells, built the first time the list asks for them.
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<DxuiListView::Cell> & CassoExplorerWindow::GetListRowCells (int row)
{
    static const std::vector<DxuiListView::Cell>  s_kNone;
    const std::vector<CatalogRow>               & rows = m_browser.GetRows();
    std::wstring                                  path;



    if (row < 0 || (size_t) row >= m_rowCells.size() || (size_t) row >= rows.size())
    {
        return s_kNone;
    }

    if (!m_rowBuilt[(size_t) row])
    {
        std::vector<DxuiListView::Cell> & cells = m_rowCells[(size_t) row];

        cells         = CassoExplorerBrowser::ToCells (rows[(size_t) row], m_rowLocation, &m_listIcons);
        cells[0].argb = CassoExplorerBrowser::GetNameArgb (rows[(size_t) row], m_browser.GetFolderOptions(), m_rowDark);

        //  Cut and not yet pasted, it draws dimmed, as Explorer's does.
        if (!m_cutPaths.empty() && m_browser.TryGetRowPath (row, path) &&
            std::any_of (m_cutPaths.begin(), m_cutPaths.end(), [&] (const std::wstring & cut) { return IsSameFolder (cut, path); }))
        {
            cells[0].iconGhosted = true;
        }

        //  A disk image that is not one says so on its icon.
        if (rows[(size_t) row].isDiskImage && m_rowLocation.kind == Location::Kind::HostFolder && m_browser.TryGetRowPath (row, path))
        {
            m_rowProblems[(size_t) row] = m_browser.GetImageProblem (path);
            cells[0].iconBroken         = !m_rowProblems[(size_t) row].empty();
        }

        m_rowBuilt[(size_t) row] = true;
    }

    return m_rowCells[(size_t) row];
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::RefreshListIcons
//
//  The rows' icons again, now that more have loaded; nothing else about the
//  rows changes, so a rename in progress carries on.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::RefreshListIcons()
{
    const std::vector<CatalogRow> & rows = m_browser.GetRows();



    for (size_t at = 0; at < m_rowCells.size() && at < rows.size(); at++)
    {
        if (m_rowBuilt[at] && !m_rowCells[at].empty())
        {
            m_rowCells[at][0].icon = CassoExplorerBrowser::GetRowIcon (rows[at], m_rowLocation, m_listIcons);
        }
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::FillPreview
//
//  The preview's kind picks the widget: a picture in the framebuffer view, a
//  catalog as rows under the list's own columns, everything else as lines.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::FillPreview()
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
        m_previewList->SetColumns (CassoExplorerBrowser::GetCatalogPreviewColumns());

        for (const CatalogRow & row : preview.rows)
        {
            rows.push_back (CassoExplorerBrowser::ToCatalogPreviewCells (row));
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
//  CassoExplorerWindow::FileBytes::Open
//
//  Shared for reading, writing and deletion, so previewing a file never gets
//  in the way of another program using it.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::FileBytes::Open (const std::wstring & path)
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
//  CassoExplorerWindow::FileBytes::Close
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::FileBytes::Close()
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
//  CassoExplorerWindow::FileBytes::LooksLikeText
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::FileBytes::LooksLikeText() const
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
//  CassoExplorerWindow::FileBytes::ReadBytes
//
//  The window starts a little before the bytes asked for, so scrolling back
//  a few rows does not move it again straight away.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::FileBytes::ReadBytes (uint64_t offset, std::span<uint8_t> out) const
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
//  CassoExplorerWindow::FileBytes::ReadAt
//
//  Bytes the file no longer has, because it shrank while shown, read as zero.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::FileBytes::ReadAt (uint64_t offset, std::span<uint8_t> out) const
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
//  CassoExplorerWindow::FillStatus
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::FillStatus()
{
    const CassoExplorerBrowser::Status &  status = m_browser.GetStatus();



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

    m_status->SetText (kStatusCount,    status.selection);
    m_status->SetText (kStatusSelected, status.selected);
    m_status->SetText (kStatusFree,     status.freeSpace);
    m_status->SetText (kStatusDetail,   detail);
    m_status->SetText (kStatusZoom,     std::format (L"{}%", m_prefs.previewZoom));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::LayoutStatusFields
//
//  The item count stretches, and free space follows it. The preview's two
//  fields together are as wide as the preview pane, so they begin at its
//  left edge; with the preview hidden they keep a fixed width.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::LayoutStatusFields()
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
    m_status->SetFields ({ { L"", 0, false, -1, true },
                           { L"", 0, false, -1, true },
                           { L"", 0, true },
                           { L"", kStatusFreeDip, false },
                           { L"", 0, false, detail },
                           { L"", kStatusZoomDip, false } });
    m_status->Layout (band, m_scaler);

    FillStatus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::SetFocusPane
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::SetFocusPane (Pane pane)
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
    m_findBox->SetFocused         (pane == Pane::LocationSearch);
    m_toolbar->SetFocusIndex      (pane == Pane::Toolbar ? m_toolbarFocus : -1);
    m_commandBar->SetFocusIndex   (pane == Pane::CommandBar ? m_commandBarFocus : -1);
    m_previewToolbar->SetFocusIndex (pane == Pane::PreviewToolbar ? m_previewBarFocus : -1);
    m_searchBox.SetFocused          (pane == Pane::Search);
    m_goToBox.SetFocused            (pane == Pane::GoTo);

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::GetFocusStop
//
////////////////////////////////////////////////////////////////////////////////

FocusStop CassoExplorerWindow::GetFocusStop() const
{
    switch (m_focus)
    {
        case Pane::Toolbar: return FocusStop { FocusStop::Kind::ToolbarEntry, m_toolbarFocus };
        case Pane::CommandBar: return FocusStop { FocusStop::Kind::CommandBarEntry, m_commandBarFocus };
        case Pane::PreviewToolbar: return FocusStop { FocusStop::Kind::PreviewToolbarEntry, m_previewBarFocus };
        case Pane::Search:         return FocusStop { FocusStop::Kind::PreviewToolbarEntry, GetPreviewStopIndex (CassoExplorerCommands::kFind) };
        case Pane::GoTo:           return FocusStop { FocusStop::Kind::PreviewToolbarEntry, GetPreviewStopIndex (CassoExplorerCommands::kGoToOffset) };
        case Pane::Address: return FocusStop { FocusStop::Kind::Address };
        case Pane::Tabs:    return FocusStop { FocusStop::Kind::Tabs };
        case Pane::List:    return FocusStop { FocusStop::Kind::List };
        case Pane::Preview: return FocusStop { FocusStop::Kind::Preview };
        default:            return FocusStop { FocusStop::Kind::Tree };
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::SetFocusStop
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::SetFocusStop (const FocusStop & stop)
{
    switch (stop.kind)
    {
        case FocusStop::Kind::ToolbarEntry:
            m_toolbarFocus = stop.entry;
            SetFocusPane (Pane::Toolbar);
            break;

        case FocusStop::Kind::CommandBarEntry:
            m_commandBarFocus = stop.entry;
            SetFocusPane (Pane::CommandBar);
            break;

        case FocusStop::Kind::PreviewToolbarEntry:
            m_previewBarFocus = stop.entry;
            SetFocusPane ((stop.entry == GetPreviewStopIndex (CassoExplorerCommands::kFind))       ? Pane::Search
                        : (stop.entry == GetPreviewStopIndex (CassoExplorerCommands::kGoToOffset)) ? Pane::GoTo
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
//  CassoExplorerWindow::IsToolbarEntryAvailable
//
//  An entry on the strip and enabled; See more's button is always enabled.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::IsToolbarEntryAvailable (int index) const
{
    int  id = m_toolbar->GetEntryCommandId (index);



    if (!m_toolbar->IsEntryShown (index))
    {
        return false;
    }

    return (id == DxuiToolbar::kSeeMoreId) || IsEnabled (id);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::IsCommandBarEntryAvailable
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::IsCommandBarEntryAvailable (int index) const
{
    int  id = m_commandBar->GetEntryCommandId (index);



    if (!m_commandBar->IsEntryShown (index))
    {
        return false;
    }

    return (id == DxuiToolbar::kSeeMoreId) || IsEnabled (id);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::RouteCommandBarKey
//
//  As on the navigation toolbar: an open drop-down takes every key, Enter,
//  Space and Down press the button, and Left and Right step along the bar to
//  the next button that can be used, wrapping at the ends as a toolbar does.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::RouteCommandBarKey (const DxuiKeyEvent & ev)
{
    int  count = m_commandBar->GetEntryCount();
    int  step  = (ev.vk == VK_RIGHT) ? 1 : -1;
    int  at    = m_commandBarFocus;
    int  tries = 0;



    if (m_commandBar->OwnsKeyboard())
    {
        return m_commandBar->HandleKey (ev.vk);
    }

    switch (ev.vk)
    {
        case VK_RETURN:
        case VK_SPACE:
        case VK_DOWN:
            m_commandBar->ActivateFocused();
            return true;

        case VK_LEFT:
        case VK_RIGHT:
            for (tries = 0; tries < count; tries++)
            {
                at = (at + step + count) % count;

                if (IsCommandBarEntryAvailable (at))
                {
                    SetFocusStop (FocusStop { FocusStop::Kind::CommandBarEntry, at });
                    break;
                }
            }

            return true;

        default:
            return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::BuildFocusStops
//
//  A disabled toolbar button, such as Back with no history, is not a stop,
//  as disabled controls are not tab stops in Windows.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<FocusStop> CassoExplorerWindow::BuildFocusStops() const
{
    std::vector<bool>  enabled;
    std::vector<bool>  previewEnabled;
    std::vector<bool>  commandEnabled;



    for (int i = 0; i < m_toolbar->GetEntryCount(); i++)
    {
        enabled.push_back (IsToolbarEntryAvailable (i));
    }

    for (int id : (m_previewBarMode != 0) ? CassoExplorerCommands::GetPreviewToolbarCommandIds (m_previewBarMode == 2) : std::vector<int>())
    {
        previewEnabled.push_back (IsEnabled (id));
    }

    for (int i = 0; i < m_commandBar->GetEntryCount(); i++)
    {
        commandEnabled.push_back (IsCommandBarEntryAvailable (i));
    }

    return FocusRing::BuildStops (enabled, m_prefs.previewVisible, previewEnabled, commandEnabled);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::RouteToolbarKey
//
//  Enter, Space or Down presses the focused button; Left and Right walk the
//  strip's usable buttons and stop at its ends. A picker or a flyout opened
//  from the keyboard owns every key until it closes.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::RouteToolbarKey (bool preview, const DxuiKeyEvent & ev)
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
//  CassoExplorerWindow::StepToolbarFocus
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::StepToolbarFocus (bool preview, bool forward)
{
    std::vector<int>  ids;
    int               step = forward ? 1 : -1;
    int               at   = (preview ? m_previewBarFocus : m_toolbarFocus) + step;



    if (preview)
    {
        ids = CassoExplorerCommands::GetPreviewToolbarCommandIds (m_previewBarMode == 2);
    }
    else
    {
        for (int i = 0; i < m_toolbar->GetEntryCount(); i++)
        {
            ids.push_back (m_toolbar->GetEntryCommandId (i));
        }
    }

    while (at >= 0 && at < (int) ids.size())
    {
        if (preview ? IsEnabled (ids[(size_t) at]) : IsToolbarEntryAvailable (at))
        {
            SetFocusStop (FocusStop { preview ? FocusStop::Kind::PreviewToolbarEntry : FocusStop::Kind::ToolbarEntry, at });
            return;
        }

        at += step;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::Contains
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::Contains (const RECT & rect, POINT point)
{
    return point.x >= rect.left && point.x < rect.right && point.y >= rect.top && point.y < rect.bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ToLocal
//
//  List views take their events relative to their own origin.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMouseEvent CassoExplorerWindow::ToLocal (const DxuiMouseEvent & ev, const RECT & bounds)
{
    DxuiMouseEvent  local = ev;



    local.positionDip.x -= bounds.left;
    local.positionDip.y -= bounds.top;

    return local;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::OnMouse
//
//  The menu bar and the splitters take first refusal, since both reach over
//  the panes. A press in a pane moves focus to it; a list that is mid-drag
//  keeps every event until the button comes up.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::OnMouse (const DxuiMouseEvent & ev)
{
    POINT  point = ev.positionDip;
    bool   press = ev.kind == DxuiMouseEventKind::Down;



    ArmTick();

    //  A tree name cut off by the splitter shows whole as the pointer reaches
    //  it, and goes when the pointer leaves it or does anything else.
    if (ev.kind == DxuiMouseEventKind::Move)
    {
        UpdateLabelTip (point);
    }
    else
    {
        m_labelTip.Hide();
    }

    //  A pointer leaving the strip by any edge, or the window, takes the
    //  tab's hover with it.
    if (m_tabs->GetHoverIndex() >= 0 &&
        ((ev.kind == DxuiMouseEventKind::Move && !Contains (m_tabs->GetBounds(), point)) || ev.kind == DxuiMouseEventKind::Leave))
    {
        DxuiMouseEvent  away = ev;

        away.kind        = DxuiMouseEventKind::Move;
        away.positionDip = { -1, -1 };

        m_tabs->OnMouse (away);
        Invalidate();
    }

    //  The address's suggestions hold no capture, so a press anywhere else in
    //  the window is what closes them; the address bar's own presses edit
    //  the path or, on the caret, close them there.
    if (ev.kind == DxuiMouseEventKind::Down && m_suggest.IsOpen() && !Contains (m_address->GetBounds(), point))
    {
        m_suggest.Close();
    }

    //  A preview's message selects with the mouse, as text does, and has the
    //  text's own menu.
    if (m_previewMessage->IsVisible() && (m_previewMessage->IsInteracting() || Contains (m_previewMessage->GetBounds(), point)))
    {
        if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Right)
        {
            std::vector<DxuiPopupMenuItem>  items;

            SetFocusPane (Pane::Preview);
            m_menuCommands.clear();
            AddMenuCommand (items, L"&Copy",       [this]() { m_previewMessage->Copy(); });
            AddMenuCommand (items, L"Select &all", [this]() { m_previewMessage->SelectAll(); Invalidate(); });
            DxuiContextMenu::Show (*GetPopupHost(), point.x, point.y, std::move (items));
            return true;
        }

        if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left)
        {
            SetFocusPane (Pane::Preview);
        }

        if (m_previewMessage->OnMouse (ev))
        {
            Invalidate();
            return true;
        }
    }

    //  A command bar menu leaves the mouse with the window, so a press
    //  anywhere but the bar that opened it closes it here, as a click outside
    //  a menu does.
    if (ev.kind == DxuiMouseEventKind::Down)
    {
        for (DxuiToolbar * bar : { m_commandBar, m_toolbar, m_previewToolbar })
        {
            if (bar->IsMenuOpen() && !Contains (bar->GetBounds(), point))
            {
                bar->CloseMenu();
                Invalidate();
            }
        }
    }

    //  A five-button mouse's back and forward buttons move through the tab's
    //  history wherever the pointer is, as in Explorer.
    if (ev.kind == DxuiMouseEventKind::Up && (ev.button == DxuiMouseButton::X1 || ev.button == DxuiMouseButton::X2))
    {
        Dispatch (ev.button == DxuiMouseButton::X1 ? CassoExplorerCommands::kBack : CassoExplorerCommands::kForward);
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
        && Contains (m_status->GetFieldRect (kStatusZoom), point))
    {
        Dispatch (CassoExplorerCommands::kZoomReset);
        return true;
    }

    //  Ctrl with the wheel over the preview zooms it, as in a browser.
    if (ev.kind == DxuiMouseEventKind::Wheel && ev.ctrl && !ev.wheelHorizontal
        && m_prefs.previewVisible && Contains (m_previewRect, point))
    {
        Dispatch ((ev.wheelDelta > 0.0f) ? CassoExplorerCommands::kZoomIn : CassoExplorerCommands::kZoomOut);
        return true;
    }

    if (m_menuBar->OnMouse (ev))
    {
        return true;
    }

    //  A right-click on the address, not while it is being edited, gives
    //  Explorer's address menu.
    if (press && ev.button == DxuiMouseButton::Right && !m_address->IsEditing() && Contains (m_address->GetBounds(), point))
    {
        ShowAddressContextMenu (point.x, point.y);
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
        if (Contains (m_findBox->GetBounds(), point))
        {
            if (press)
            {
                SetFocusPane (Pane::LocationSearch);
            }

            m_findBox->OnMouse (ev);
            Invalidate();
            return true;
        }

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

    //  The strip under the pointer takes the event and drives the tooltip; the
    //  other still sees a move, so its hover clears when the pointer crosses.
    {
        DxuiToolbar &  under = GetToolbarUnder (m_menuBand.GetBounds(), point, *m_toolbar, *m_commandBar);
        DxuiToolbar &  other = (&under == m_commandBar) ? *m_toolbar : *m_commandBar;

        if (ev.kind == DxuiMouseEventKind::Move)
        {
            other.OnToolbarMouseMove (ev.positionDip.x, ev.positionDip.y);
        }
        else if (ev.kind == DxuiMouseEventKind::Leave)
        {
            other.OnToolbarMouseLeave();
        }

        if (RouteToolbarMouse (under, ev) || (ev.kind == DxuiMouseEventKind::Up && RouteToolbarMouse (other, ev)))
        {
            Invalidate();
            return true;
        }
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

    //  A right press waits for its release to open the menu, or drags the
    //  selection when it moves far enough first.
    if (m_rightPress.active)
    {
        if (ev.kind == DxuiMouseEventKind::Up && ev.button == DxuiMouseButton::Right)
        {
            m_rightPress.active = false;
            ShowListContextMenu (point.x, point.y, m_rightPress.group);
            return true;
        }

        if (ev.kind == DxuiMouseEventKind::Move && m_rightPress.canDrag
            && (abs (point.x - m_rightPress.start.x) > GetSystemMetrics (SM_CXDRAG)
             || abs (point.y - m_rightPress.start.y) > GetSystemMetrics (SM_CYDRAG)))
        {
            m_rightPress.active = false;
            BeginDragOut();
            Invalidate();
            return true;
        }
    }

    //  A press on a list row that moves past the drag distance drags the
    //  selection out. This comes ahead of the list's own handling, which would
    //  otherwise take the move as extending the selection. The list never sees
    //  the release, which the drag loop takes, so its drag ends here.
    if (m_dragArmed)
    {
        if (ev.kind == DxuiMouseEventKind::Up || ev.kind == DxuiMouseEventKind::Leave)
        {
            m_dragArmed = false;
        }
        else if (ev.kind == DxuiMouseEventKind::Move
              && (abs (point.x - m_dragStart.x) > GetSystemMetrics (SM_CXDRAG)
               || abs (point.y - m_dragStart.y) > GetSystemMetrics (SM_CYDRAG)))
        {
            m_dragArmed = false;
            m_list->EndDragSelect();
            BeginDragOut();
            Invalidate();
            return true;
        }
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

    //  Explorer's tree drops its hover as the pointer leaves it.
    if (m_tree->GetHoverRow() >= 0 && (ev.kind == DxuiMouseEventKind::Leave || !Contains (m_tree->GetBounds(), point)))
    {
        m_tree->SetHoverRow (-1);
        Invalidate();
    }

    UpdateTreeTip   (ev, point);
    UpdateStatusTip (ev, point);
    UpdateListTip   (ev, point);

    if (Contains (m_tree->GetBounds(), point))
    {
        if (press)
        {
            SetFocusPane (Pane::Tree);
        }

        //  Below the last node: the pane's own options, as Explorer's has.
        if (press && ev.button == DxuiMouseButton::Right && m_tree->HitTestRow (point.x, point.y) < 0)
        {
            ShowTreeEmptyMenu (point.x, point.y);
            return true;
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

    //  An empty image or folder shows a message in the list's place; a right-
    //  click there still opens the menu the list's empty space has.
    if (!m_list->IsVisible() && m_listMessage->IsVisible() && press && ev.button == DxuiMouseButton::Right &&
        Contains (m_list->GetBounds(), point))
    {
        SetFocusPane (Pane::List);
        m_list->ClearSelection();
        ShowListContextMenu (point.x, point.y);
        return true;
    }

    if (m_list->IsVisible() && Contains (m_list->GetBounds(), point))
    {
        DxuiMouseEvent  local = ToLocal (ev, m_list->GetBounds());

        //  A click gives the list focus without the row a Tab in would pick:
        //  on empty space it selects nothing, and on a row the press picks.
        if (press)
        {
            m_list->SetSeedRowOnFocus (false);
            SetFocusPane (Pane::List);
            m_list->SetSeedRowOnFocus (true);
        }

        //  The header has a menu of its own, for its columns.
        if (press && ev.button == DxuiMouseButton::Right && local.positionDip.y < m_list->GetHeaderHeightPx())
        {
            ShowListHeaderMenu (point.x, point.y, m_list->HitTestHeaderColumn (local.positionDip.x, local.positionDip.y));
            return true;
        }

        if (press && ev.button == DxuiMouseButton::Right)
        {
            int  row   = m_list->HitTestRow         (local.positionDip.x, local.positionDip.y);
            int  group = m_list->HitTestGroupHeader (local.positionDip.x, local.positionDip.y);

            //  A group's header selects its rows and adds the group's own
            //  commands to the menu for them, as Explorer's does.
            if (group >= 0)
            {
                m_list->SelectGroup (group);
            }
            else if (row >= 0 && !m_list->IsRowSelected (row))
            {
                m_list->ClickRow (row, false, false);
            }
            else if (row >= 0)
            {
                //  A row already selected keeps the selection and takes the
                //  focus, so Rename acts on it, as Explorer's does.
                m_list->SetFocusedRow (row);
            }
            else if (row < 0)
            {
                //  The empty space's menu is the folder's, so nothing stays
                //  selected for it to act on.
                m_list->ClearSelection();
            }

            //  The menu opens as the button comes up, as Explorer's does; a
            //  press on a row that moves first drags the selection instead.
            m_rightPress = RightPress { true, group < 0 && row >= 0, group, point };
            Invalidate();
            return true;
        }

        //  A press on a row may start a drag of the selection; one on the
        //  space between rows starts a rubber band instead, as in Explorer.
        if (press && ev.button == DxuiMouseButton::Left)
        {
            m_dragArmed = m_list->HitTestRow (local.positionDip.x, local.positionDip.y) >= 0;
            m_dragStart = point;
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
//  CassoExplorerWindow::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR CassoExplorerWindow::GetCursorForPoint (POINT clientPx) const
{
    LPCWSTR  cursor = m_treeSplitter->GetCursorForPoint (clientPx);
    RECT     bounds = m_list->GetBounds();



    //  The preview's text -- a listing, a catalog, a hex dump -- selects as
    //  text does, so it takes the I-beam, except over its scrollbars.
    for (IDxuiControl * view : { (IDxuiControl *) m_textView, (IDxuiControl *) m_hexView })
    {
        if (cursor == nullptr && view->IsVisible() && Contains (view->GetBounds(), clientPx) && !view->IsOverScrollbar (clientPx))
        {
            cursor = IDC_IBEAM;
        }
    }

    //  The list tests points in its own terms.
    if (cursor == nullptr && m_previewList->IsVisible() && Contains (m_previewList->GetBounds(), clientPx))
    {
        RECT    list  = m_previewList->GetBounds();
        POINT   local = { clientPx.x - list.left, clientPx.y - list.top };

        cursor = m_previewList->GetCursorForPoint (local);
        cursor = (cursor != nullptr || m_previewList->IsOverScrollbar (local)) ? cursor : IDC_IBEAM;
    }

    if (cursor == nullptr && m_previewMessage->IsVisible())
    {
        cursor = m_previewMessage->GetCursorForPoint (clientPx);
    }

    //  The address being edited is text, and takes the I-beam over it; its
    //  clear and history buttons keep the arrow.
    if (cursor == nullptr && m_address->IsEditing() && m_address->HitTest (clientPx.x, clientPx.y).part == DxuiAddressBar::Part::Blank)
    {
        cursor = IDC_IBEAM;
    }

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
//  CassoExplorerWindow::OnKey
//
//  Command keys first, then the menu bar's mnemonics and its open dropdown,
//  then Tab between panes, and the rest to the focused pane.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::OnKey (const DxuiKeyEvent & ev)
{
    int   command = 0;
    bool  handled = false;



    ArmTick();

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
    //  Tab and the keys it has no use for continue as usual.
    if (m_focus == Pane::LocationSearch)
    {
        handled = OnFindBoxKey (ev);

        if (handled || ev.kind != DxuiKeyEventKind::Down)
        {
            Invalidate();
            return handled;
        }
    }

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

    //  A preview that is only a message selects and copies as text does.
    if (ev.kind == DxuiKeyEventKind::Down && ev.ctrl && !ev.alt && (ev.vk == 'C' || ev.vk == 'A')
        && m_focus == Pane::Preview && m_previewMessage->IsVisible())
    {
        m_previewMessage->InvokeCommand ((ev.vk == 'C') ? DxuiStandardCommand::Copy : DxuiStandardCommand::SelectAll);
        Invalidate();
        return true;
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

    //  Ctrl+Z on the files undoes the last file operation; in a text box it is
    //  the box's own, which has the keys before this.
    if (ev.ctrl && !ev.alt && !ev.shift && ev.vk == 'Z' && (m_focus == Pane::List || m_focus == Pane::Tree) && m_renameRow < 0)
    {
        UndoLast();
        return true;
    }

    command = CassoExplorerCommands::TranslateKey (ev.vk, ev.ctrl, ev.alt, ev.shift);

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

    //  Copy in an image writes the files a paste would, as the context menu's
    //  Copy row does; cut and paste are the host's alone.
    if (m_focus == Pane::List && ev.ctrl && !ev.alt && ev.vk == 'C'
        && m_browser.IsImageLocation() && !m_browser.GetSelectedRows().empty())
    {
        CopyEntriesToClipboard (GetNamingStyle());
        return true;
    }

    if (m_focus == Pane::List && ev.ctrl && !ev.alt && !m_browser.IsImageLocation()
        && m_browser.GetLocation().kind == Location::Kind::HostFolder
        && (ev.vk == 'X' || ev.vk == 'C' || ev.vk == 'V'))
    {
        if (ev.vk == 'V')
        {
            RunVerb (CassoExplorerActions::Verb::Paste);
        }
        else if (!m_browser.GetSelectedRows().empty())
        {
            RunVerb (ev.vk == 'X' ? CassoExplorerActions::Verb::Cut : CassoExplorerActions::Verb::Copy);
        }

        return true;
    }

    if (ev.vk == VK_F2 && m_focus == Pane::List && IsListVerbOffered (CassoExplorerActions::Verb::Rename))
    {
        RunVerb (CassoExplorerActions::Verb::Rename);
        return true;
    }

    if (ev.vk == VK_DELETE && m_focus == Pane::List && !m_browser.GetSelectedRows().empty())
    {
        RunVerb (CassoExplorerActions::Verb::Delete);
        return true;
    }

    //  The hex view's two columns are stops of their own inside the preview
    //  pane: Tab moves between them while it has one left, and only then does
    //  the walk move on to the next pane.
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
        case Pane::CommandBar: handled = RouteCommandBarKey (ev);  break;
        case Pane::PreviewToolbar: handled = RouteToolbarKey (true, ev); break;
        case Pane::Search:                                               break;
        case Pane::GoTo:                                                 break;
        case Pane::Address: handled = m_address->OnKey (ev);     break;
        case Pane::LocationSearch: handled = OnFindBoxKey (ev);  break;
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
        Dispatch (CassoExplorerCommands::kUp);
        handled = true;
    }

    Invalidate();

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::IsEnabled
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::IsEnabled (int id) const
{
    const BrowserModel &  model    = m_browser.GetBrowserModel();
    DxuiStandardCommand   standard = CassoExplorerCommands::GetStandardCommand (id);
    bool                  enabled  = false;



    //  A standard command row is enabled when the focused control reports it
    //  available, and grayed when no control handles it.
    if (standard != DxuiStandardCommand::None)
    {
        return DxuiCommandRouter::Query (GetFocusedControl(), standard, enabled) && enabled;
    }

    switch (id)
    {
        case CassoExplorerCommands::kBack:              return model.HasTabs() && model.CanGoBack();
        case CassoExplorerCommands::kForward:           return model.HasTabs() && model.CanGoForward();
        case CassoExplorerCommands::kUp:                return m_browser.CanGoUp();
        case CassoExplorerCommands::kToggleDisassembly: return model.HasTabs();
        case CassoExplorerCommands::kLineAddresses:     return m_previewBarMode == 1;
        case CassoExplorerCommands::kFindNext:          return IsHexPreviewShowing() && !m_findBytes.empty();
        case CassoExplorerCommands::kFind:
        case CassoExplorerCommands::kNoData:
        case CassoExplorerCommands::kFormatHex:
        case CassoExplorerCommands::kFormatSigned:
        case CassoExplorerCommands::kFormatUnsigned:
        case CassoExplorerCommands::kColumns:
        case CassoExplorerCommands::kColumnsAuto:
        case CassoExplorerCommands::kColumns1:
        case CassoExplorerCommands::kColumns2:
        case CassoExplorerCommands::kColumns4:
        case CassoExplorerCommands::kColumns8:
        case CassoExplorerCommands::kColumns16:
        case CassoExplorerCommands::kGoToOffset:
        case CassoExplorerCommands::kGroup1:
        case CassoExplorerCommands::kGroup2:
        case CassoExplorerCommands::kGroup4:
        case CassoExplorerCommands::kGroup8:            return IsHexPreviewShowing();
        case CassoExplorerCommands::kCloseTab:
        case CassoExplorerCommands::kNextTab:
        case CassoExplorerCommands::kPreviousTab:       return model.GetTabCount() > 1;
        case CassoExplorerCommands::kUndo:              return !m_undo.empty();
        case CassoExplorerCommands::kCutItems:          return IsListVerbOffered (CassoExplorerActions::Verb::Cut);
        case CassoExplorerCommands::kCopyItems:         return IsListVerbOffered (CassoExplorerActions::Verb::Copy) || IsListVerbOffered (CassoExplorerActions::Verb::Get);
        case CassoExplorerCommands::kPasteItems:        return !m_browser.IsImageLocation() && m_browser.GetLocation().kind == Location::Kind::HostFolder
                                                         && m_shellVerbs.ClipboardHasFiles();
        case CassoExplorerCommands::kRenameItem:        return IsListVerbOffered (CassoExplorerActions::Verb::Rename);
        case CassoExplorerCommands::kDeleteItems:       return IsListVerbOffered (CassoExplorerActions::Verb::Delete);
        case CassoExplorerCommands::kEmptyRecycleBin:
        case CassoExplorerCommands::kRestoreItems:      return m_browser.GetLocation().kind == Location::Kind::RecycleBin && !m_browser.GetRows().empty();
        case CassoExplorerCommands::kNew:               return !m_browser.IsImageLocation() ? m_browser.GetLocation().kind == Location::Kind::HostFolder
                                                                                      : (m_browser.GetVolumeKind() == VolumeKind::ProDos && !m_browser.IsWriteProtected());
        case CassoExplorerCommands::kNewFolder:         return !m_browser.IsImageLocation() ? true
                                                                                      : (m_browser.GetVolumeKind() == VolumeKind::ProDos && !m_browser.IsWriteProtected());
        case CassoExplorerCommands::kNewDisk:           return !m_browser.IsImageLocation() && m_browser.GetLocation().kind == Location::Kind::HostFolder;
        default:                                  return true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::IsChecked
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::IsChecked (int id) const
{
    const BrowserModel &  model = m_browser.GetBrowserModel();



    if (id >= CassoExplorerCommands::kGroupByField && id <= CassoExplorerCommands::kGroupByField + (int) RowGrouping::Field::Size)
    {
        return model.HasTabs() && (int) model.GetActiveTab().groupBy == id - CassoExplorerCommands::kGroupByField;
    }

    if (id >= CassoExplorerCommands::kSortByColumn && id < CassoExplorerCommands::kSortByColumn + (int) CassoExplorerBrowser::GetColumns().size())
    {
        return model.HasTabs() && (int) model.GetActiveTab().sortColumn == id - CassoExplorerCommands::kSortByColumn;
    }

    if (id >= CassoExplorerCommands::kViewFirst && id < CassoExplorerCommands::kViewFirst + CassoExplorerPrefs::kViewCount)
    {
        return (int) m_listView == id - CassoExplorerCommands::kViewFirst;
    }

    switch (id)
    {
        case CassoExplorerCommands::kTogglePreview:     return m_prefs.previewVisible;
        case CassoExplorerCommands::kSortAscending:     return model.HasTabs() && !model.GetActiveTab().sortDescending;
        case CassoExplorerCommands::kSortDescending:    return model.HasTabs() && model.GetActiveTab().sortDescending;
        case CassoExplorerCommands::kGroupAscending:    return model.HasTabs() && !model.GetActiveTab().groupDescending;
        case CassoExplorerCommands::kGroupDescending:   return model.HasTabs() && model.GetActiveTab().groupDescending;
        case CassoExplorerCommands::kLineAddresses:     return m_prefs.lineAddresses;
        case CassoExplorerCommands::kToggleDisassembly: return model.HasTabs() && model.GetActiveTab().disassemble;
        case CassoExplorerCommands::kThemeLight:        return m_prefs.theme == CassoExplorerPrefs::kThemeLight;
        case CassoExplorerCommands::kThemeDark:         return m_prefs.theme == CassoExplorerPrefs::kThemeDark;
        case CassoExplorerCommands::kThemeSystem:       return m_prefs.theme != CassoExplorerPrefs::kThemeLight && m_prefs.theme != CassoExplorerPrefs::kThemeDark
                                                             && !IsCassoThemeName (m_prefs.theme);
        case CassoExplorerCommands::kThemeSkeuomorphic: return m_prefs.theme == CassoExplorerPrefs::kThemeSkeuomorphic;
        case CassoExplorerCommands::kThemeDarkModern:   return m_prefs.theme == CassoExplorerPrefs::kThemeDarkModern;
        case CassoExplorerCommands::kThemeRetroTerminal: return m_prefs.theme == CassoExplorerPrefs::kThemeRetroTerminal;
        case CassoExplorerCommands::kNoData:            return !m_hexView->IsShowingValues();
        case CassoExplorerCommands::kGroup1:            return m_hexView->IsShowingValues() && m_prefs.hexGrouping == 1;
        case CassoExplorerCommands::kGroup2:            return m_hexView->IsShowingValues() && m_prefs.hexGrouping == 2;
        case CassoExplorerCommands::kGroup4:            return m_hexView->IsShowingValues() && m_prefs.hexGrouping == 4;
        case CassoExplorerCommands::kGroup8:            return m_hexView->IsShowingValues() && m_prefs.hexGrouping == 8;
        case CassoExplorerCommands::kFormatHex:         return m_prefs.hexFormat == CassoExplorerPrefs::kHexFormatHex;
        case CassoExplorerCommands::kFormatSigned:      return m_prefs.hexFormat == CassoExplorerPrefs::kHexFormatSigned;
        case CassoExplorerCommands::kFormatUnsigned:    return m_prefs.hexFormat == CassoExplorerPrefs::kHexFormatUnsigned;
        case CassoExplorerCommands::kColumnsAuto:       return m_prefs.hexColumns == 0;
        case CassoExplorerCommands::kColumns1:          return m_prefs.hexColumns == 1;
        case CassoExplorerCommands::kColumns2:          return m_prefs.hexColumns == 2;
        case CassoExplorerCommands::kColumns4:          return m_prefs.hexColumns == 4;
        case CassoExplorerCommands::kColumns8:          return m_prefs.hexColumns == 8;
        case CassoExplorerCommands::kColumns16:         return m_prefs.hexColumns == 16;
        default:                                  return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::GetFocusedControl
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * CassoExplorerWindow::GetFocusedControl() const
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
//  CassoExplorerWindow::IsHexPreviewShowing
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::IsHexPreviewShowing() const
{
    return m_prefs.previewVisible && m_hexView->IsVisible();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::IsTextPreviewShowing
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::IsTextPreviewShowing() const
{
    return m_prefs.previewVisible && m_textView->IsVisible();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::BuildTextRows
//
//  A listing that is not a valid program ends with a blank row and the
//  warning that says why.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiTextView::Row> CassoExplorerWindow::BuildTextRows (const PreviewContent & preview, bool lineAddresses)
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
//  CassoExplorerWindow::GetLineAddresses
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

std::vector<Word> CassoExplorerWindow::GetLineAddresses (const std::vector<Byte> & program, bool integerBasic)
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
//  CassoExplorerWindow::SplitLineNumber
//
//  A listing line's number and the statement after the spaces that follow it,
//  or the whole line as one cell when it does not start with a number.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> CassoExplorerWindow::SplitLineNumber (const std::wstring & line)
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
//  CassoExplorerWindow::GetMessageRect
//
//  A pane's message wraps at a reading width, centered in the pane, rather
//  than running the whole width of a wide window.
//
////////////////////////////////////////////////////////////////////////////////

RECT CassoExplorerWindow::GetMessageRect (const RECT & pane) const
{
    int   width = (std::min) ((int) (pane.right - pane.left), m_scaler.ToPx (kMessageWidthDip));
    RECT  rect  = pane;



    rect.left  = pane.left + ((pane.right - pane.left) - width) / 2;
    rect.right = rect.left + width;

    return rect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::FormatPreviewError
//
//  "No preview", and three lines below it, why. An image refused for its size
//  holds a whole sentence about the file for the command line; the preview
//  says only the reason.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerWindow::FormatPreviewError (const std::wstring & message)
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
//  CassoExplorerWindow::SetPreviewZoom
//
//  The text and hex previews share one zoom, kept within the range the
//  preferences allow.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::SetPreviewZoom (int percent)
{
    int  clamped = (std::max) (CassoExplorerPrefs::kMinPreviewZoom, (std::min) (percent, CassoExplorerPrefs::kMaxPreviewZoom));



    m_prefs.previewZoom = clamped;

    m_textView->SetZoom ((float) clamped / 100.0f);
    m_hexView->SetZoom  ((float) clamped / 100.0f);
    m_picture->SetZoom  ((float) clamped / 100.0f);

    FillStatus();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::SetHexColumns
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::SetHexColumns (int columns)
{
    m_hexView->SetColumns (columns);
    m_prefs.hexColumns = columns;

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::SetHexFormat
//
//  Choosing a format shows the values it applies to.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::SetHexFormat (const char * format)
{
    m_prefs.hexFormat     = format;
    m_prefs.hexShowValues = true;

    m_hexView->SetShowValues  (true);
    m_hexView->SetValueFormat (ParseHexFormat (m_prefs.hexFormat));

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::SetHexShowValues
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::SetHexShowValues (bool show)
{
    m_prefs.hexShowValues = show;
    m_hexView->SetShowValues (show);

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ParseHexFormat
//
////////////////////////////////////////////////////////////////////////////////

DxuiHexView::ValueFormat CassoExplorerWindow::ParseHexFormat (const std::string & name)
{
    if (name == CassoExplorerPrefs::kHexFormatSigned)
    {
        return DxuiHexView::ValueFormat::Signed;
    }

    if (name == CassoExplorerPrefs::kHexFormatUnsigned)
    {
        return DxuiHexView::ValueFormat::Unsigned;
    }

    return DxuiHexView::ValueFormat::Hex;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::GetPreviewStopIndex
//
//  A command's place among the hex view toolbar's stops, or -1 while that
//  toolbar is not showing.
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerWindow::GetPreviewStopIndex (int commandId) const
{
    std::vector<int>  ids = CassoExplorerCommands::GetPreviewToolbarCommandIds (true);



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
//  CassoExplorerWindow::SetHexGrouping
//
//  A grouping the row cannot divide is refused by the view, and a refused
//  one is not written to the preferences either.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::SetHexGrouping (int grouping)
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
//  CassoExplorerWindow::GoToTyped
//
//  An address matches the offset column, which counts from the file's load
//  address when it has one; an offset moves from the caret. A target that
//  does not parse or is outside the file marks the box in error, and one
//  that is inside it moves the caret there and returns focus to the bytes.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::GoToTyped (const std::wstring & text)
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

    if (!CassoExplorerActions::TryParseGoTo (text, caret, target, last) || target < origin || last >= origin + count)
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
//  CassoExplorerWindow::OnSearchChanged
//
//  Each edit searches again from the start of the current match, so typing
//  more of a term keeps the match it has while it still fits. Hex digits find
//  bytes; other text finds characters with or without the high bit, since
//  Apple II text is stored both ways.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::OnSearchChanged (const std::wstring & text)
{
    if (!CassoExplorerActions::TryParseSearch (text, m_findBytes, m_findIsText))
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
//  CassoExplorerWindow::FindNext
//
//  Searches forward from just past the selection, wrapping at the end. An
//  incremental search starts at the selection itself and says nothing when
//  the term is not found, since the user is still typing it.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::FindNext (bool incremental)
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
    found = CassoExplorerActions::FindInSource ([source] (uint64_t offset, std::span<uint8_t> out) { source->ReadBytes (offset, out); },
                                          source->GetByteCount(), m_findBytes, m_findIsText, start);

    if (found == CassoExplorerActions::kNotFoundOffset)
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
//  CassoExplorerWindow::ShowTextContextMenu
//
//  Copy and Select all, which reach the text view as the focused control.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ShowTextContextMenu (int x, int y)
{
    std::vector<DxuiPopupMenuItem>  items;



    for (int id : { (int) CassoExplorerCommands::kCopy, (int) CassoExplorerCommands::kSelectAll })
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
//  CassoExplorerWindow::Dispatch
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::Dispatch (int id)
{
    HRESULT  hr      = S_OK;
    bool     refill  = false;
    bool     routed  = DxuiCommandRouter::Invoke (GetFocusedControl(),
                                                  CassoExplorerCommands::GetStandardCommand (id));



    //  A standard command belongs to whatever has focus, so it is routed
    //  rather than decided here.
    if (CassoExplorerCommands::GetStandardCommand (id) != DxuiStandardCommand::None)
    {
        IGNORE_RETURN_VALUE (routed, true);
        Invalidate();
        return;
    }

    if (id >= CassoExplorerCommands::kViewFirst && id < CassoExplorerCommands::kViewFirst + CassoExplorerPrefs::kViewCount)
    {
        //  Chosen for this folder alone, as Explorer remembers it.
        m_listView    = (DxuiListView::View) (id - CassoExplorerCommands::kViewFirst);
        m_listViewKey = GetFolderViewKey (nullptr);
        m_prefs.folderViews.Remember (m_listViewKey, m_listView);
        m_list->SetView (m_listView);
        SizeListIcons();
        FillList();
        Invalidate();
        return;
    }

    //  A field groups dates newest first, as Explorer does, and the others
    //  from the start of the alphabet or the smallest.
    if (id >= CassoExplorerCommands::kGroupByField && id <= CassoExplorerCommands::kGroupByField + (int) RowGrouping::Field::Size)
    {
        RowGrouping::Field  field = (RowGrouping::Field) (id - CassoExplorerCommands::kGroupByField);

        if (!IsChecked (id))
        {
            m_browser.SetGroupBy (field, field == RowGrouping::Field::DateModified);
            RememberFolderSort();
            FillList();
        }

        return;
    }

    if (id >= CassoExplorerCommands::kSortByColumn && id < CassoExplorerCommands::kSortByColumn + (int) CassoExplorerBrowser::GetColumns().size())
    {
        //  A new column sorts ascending; the column already in use keeps its
        //  direction.
        if (!IsChecked (id))
        {
            m_browser.SortByColumn (id - CassoExplorerCommands::kSortByColumn);

            if (m_browser.GetBrowserModel().GetActiveTab().sortDescending)
            {
                m_browser.SortByColumn (id - CassoExplorerCommands::kSortByColumn);
            }

            RememberFolderSort();
            FillList();
        }

        return;
    }

    switch (id)
    {
        case CassoExplorerCommands::kExit:
            OnWindowClose();
            break;

        case CassoExplorerCommands::kUndo:         UndoLast();                                   break;
        case CassoExplorerCommands::kCutItems:     RunVerb (CassoExplorerActions::Verb::Cut);    break;
        //  Ctrl+V and the command bar paste into the folder shown, whatever is
        //  selected, as Explorer's do; a folder's own menu pastes into it.
        case CassoExplorerCommands::kPasteItems:
        {
            PasteHere (m_browser.GetLocation().path);
            break;
        }

        case CassoExplorerCommands::kRenameItem:   RunVerb (CassoExplorerActions::Verb::Rename); break;
        case CassoExplorerCommands::kDeleteItems:  RunVerb (CassoExplorerActions::Verb::Delete); break;
        case CassoExplorerCommands::kEmptyRecycleBin: RunRecycleBinVerb (CassoExplorerActions::Verb::EmptyRecycleBin); break;
        case CassoExplorerCommands::kRestoreItems:    RunRecycleBinVerb (CassoExplorerActions::Verb::Restore); break;
        case CassoExplorerCommands::kNewFolder:    RunVerb (CassoExplorerActions::Verb::NewFolder); break;
        case CassoExplorerCommands::kNewDisk:      RunVerb (CassoExplorerActions::Verb::NewDisk);   break;

        //  Either way the selection goes on the clipboard: a real file through
        //  the shell, an entry in an image as the file a paste would write,
        //  in the style the Options dialog sets.
        case CassoExplorerCommands::kCopyItems:
            if (m_browser.IsImageLocation())
            {
                CopyEntriesToClipboard (GetNamingStyle());
            }
            else
            {
                RunVerb (CassoExplorerActions::Verb::Copy);
            }

            break;

        case CassoExplorerCommands::kSortAscending:
        case CassoExplorerCommands::kSortDescending:
            if (!IsChecked (id) && m_browser.GetBrowserModel().HasTabs())
            {
                m_browser.SortByColumn ((int) m_browser.GetBrowserModel().GetActiveTab().sortColumn);
                RememberFolderSort();
                FillList();
            }

            break;

        case CassoExplorerCommands::kGroupAscending:
        case CassoExplorerCommands::kGroupDescending:
            if (!IsChecked (id) && m_browser.GetBrowserModel().HasTabs())
            {
                m_browser.SetGroupBy (m_browser.GetBrowserModel().GetActiveTab().groupBy, id == CassoExplorerCommands::kGroupDescending);
                RememberFolderSort();
                FillList();
            }

            break;


        case CassoExplorerCommands::kRefresh:
            ReadFolderOptions();
            m_browser.GetBrowserModel().InvalidateAllCatalogs();
            hr     = m_browser.Refresh();
            refill = true;
            break;

        case CassoExplorerCommands::kTogglePreview:
            m_prefs.previewVisible = !m_prefs.previewVisible;
            FillPreview();
            RecomputeLayout();
            break;

        case CassoExplorerCommands::kToggleDisassembly:
            if (m_browser.GetBrowserModel().HasTabs())
            {
                m_browser.SetDisassemble (!m_browser.GetBrowserModel().GetActiveTab().disassemble);
                FillPreview();
            }

            break;

        case CassoExplorerCommands::kThemeLight:         SelectTheme (CassoExplorerPrefs::kThemeLight);         break;
        case CassoExplorerCommands::kThemeDark:          SelectTheme (CassoExplorerPrefs::kThemeDark);          break;
        case CassoExplorerCommands::kThemeSystem:        SelectTheme (CassoExplorerPrefs::kThemeFollowSystem);  break;
        case CassoExplorerCommands::kThemeSkeuomorphic:  SelectTheme (CassoExplorerPrefs::kThemeSkeuomorphic);  break;
        case CassoExplorerCommands::kThemeDarkModern:    SelectTheme (CassoExplorerPrefs::kThemeDarkModern);    break;
        case CassoExplorerCommands::kThemeRetroTerminal: SelectTheme (CassoExplorerPrefs::kThemeRetroTerminal); break;

        case CassoExplorerCommands::kGoToOffset:
            if (IsHexPreviewShowing())
            {
                m_previewBarFocus = GetPreviewStopIndex (CassoExplorerCommands::kGoToOffset);
                SetFocusPane (Pane::GoTo);
            }

            break;

        case CassoExplorerCommands::kLineAddresses:
            m_prefs.lineAddresses = !m_prefs.lineAddresses;
            m_textView->SetRows (BuildTextRows (m_browser.GetPreview(), m_prefs.lineAddresses));
            Invalidate();
            break;

        case CassoExplorerCommands::kFind:
            if (IsHexPreviewShowing())
            {
                m_previewBarFocus = GetPreviewStopIndex (CassoExplorerCommands::kFind);
                SetFocusPane (Pane::Search);
            }

            break;

        case CassoExplorerCommands::kZoomIn:         SetPreviewZoom (m_prefs.previewZoom + kPreviewZoomStep); break;
        case CassoExplorerCommands::kZoomOut:        SetPreviewZoom (m_prefs.previewZoom - kPreviewZoomStep); break;
        case CassoExplorerCommands::kZoomReset:      SetPreviewZoom (CassoExplorerPrefs::kDefaultPreviewZoom);      break;
        case CassoExplorerCommands::kFindNext:       FindNext();                                      break;
        case CassoExplorerCommands::kNoData:         SetHexShowValues (!m_hexView->IsShowingValues()); break;
        case CassoExplorerCommands::kFormatHex:      SetHexFormat (CassoExplorerPrefs::kHexFormatHex);      break;
        case CassoExplorerCommands::kFormatSigned:   SetHexFormat (CassoExplorerPrefs::kHexFormatSigned);   break;
        case CassoExplorerCommands::kFormatUnsigned: SetHexFormat (CassoExplorerPrefs::kHexFormatUnsigned); break;
        case CassoExplorerCommands::kColumns:                                                         break;
        case CassoExplorerCommands::kColumnsAuto:    SetHexColumns (0);                               break;
        case CassoExplorerCommands::kColumns1:       SetHexColumns (1);                               break;
        case CassoExplorerCommands::kColumns2:       SetHexColumns (2);                               break;
        case CassoExplorerCommands::kColumns4:       SetHexColumns (4);                               break;
        case CassoExplorerCommands::kColumns8:       SetHexColumns (8);                               break;
        case CassoExplorerCommands::kColumns16:      SetHexColumns (16);                              break;

        case CassoExplorerCommands::kGroup1: SetHexGrouping (1); break;
        case CassoExplorerCommands::kGroup2: SetHexGrouping (2); break;
        case CassoExplorerCommands::kGroup4: SetHexGrouping (4); break;
        case CassoExplorerCommands::kGroup8: SetHexGrouping (8); break;

        case CassoExplorerCommands::kOptions:           ShowOptions(); break;

        case CassoExplorerCommands::kBack:    refill = m_browser.GoBack();    break;
        case CassoExplorerCommands::kForward: refill = m_browser.GoForward(); break;
        case CassoExplorerCommands::kUp:      refill = m_browser.GoUp();      break;

        case CassoExplorerCommands::kEditAddress:
            SetFocusPane (Pane::Address);
            m_address->BeginEdit();
            break;

        case CassoExplorerCommands::kAddressHistory:
            SetFocusPane (Pane::Address);
            m_address->BeginEdit();
            ShowAddressHistoryMenu (m_address->GetBounds());
            break;

        case CassoExplorerCommands::kAbout:
            ShowAbout();
            break;

        case CassoExplorerCommands::kNewTab:
            m_browser.NewTab();
            refill = true;
            break;

        case CassoExplorerCommands::kCloseTab:
            refill = m_browser.CloseTab (m_browser.GetBrowserModel().GetActiveIndex());
            break;

        case CassoExplorerCommands::kNextTab:
        case CassoExplorerCommands::kPreviousTab:
            if (m_browser.GetBrowserModel().GetTabCount() > 1)
            {
                size_t  count = m_browser.GetBrowserModel().GetTabCount();
                size_t  next  = (m_browser.GetBrowserModel().GetActiveIndex() + (id == CassoExplorerCommands::kNextTab ? 1 : count - 1)) % count;

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
//  CassoExplorerWindow::ShowAbout
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ShowAbout()
{
    CassoExplorerAbout::Show (GetHwnd(), m_theme, GetModuleHandleW (nullptr));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::OnWindowClose
//
//  The preferences are saved by the shell once the loop ends; closing only
//  records where the window was and ends the loop.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::OnWindowClose()
{
    m_browser.GetBrowserModel().GetLocations (m_prefs.tabs);
    m_prefs.typedPaths = m_browser.GetTypedPaths().GetEntries();
    StorePlacement();
    Hide();
    PostQuitMessage (0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::SaveSession
//
//  Saved as each change is made, so a window that never closes cleanly -- a
//  crash, a process ended from outside, Windows signing out -- still opens
//  where it was left.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::SaveSession()
{
    HRESULT  hr = S_OK;



    if (m_context.fs == nullptr)
    {
        return;
    }

    m_browser.GetBrowserModel().GetLocations (m_prefs.tabs);
    m_prefs.typedPaths = m_browser.GetTypedPaths().GetEntries();
    StorePlacement();

    hr = m_prefs.Save (m_context.baseDir, *m_context.fs);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::OnEnterSizeMove
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::OnEnterSizeMove()
{
    m_inSizeMove = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::OnExitSizeMove
//
//  The end of a move or resize the user dragged. A drag that snaps the window
//  resizes it after this, and OnSize saves that.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::OnExitSizeMove()
{
    m_inSizeMove = false;
    SaveSession();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::OnSize
//
//  A resize outside a drag: maximizing, restoring, or a snap, whether from
//  the keyboard or at the end of a drag. One during a drag waits for its end,
//  and one while the window opens is where it was saved already.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult CassoExplorerWindow::OnSize (UINT widthPx, UINT heightPx)
{
    UNREFERENCED_PARAMETER (widthPx);
    UNREFERENCED_PARAMETER (heightPx);

    //  Minimizing is no place to reopen at.
    if (m_opened && !m_inSizeMove && !IsIconic (GetHwnd()))
    {
        SaveSession();
    }

    return DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::GetVerbLabel
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * CassoExplorerWindow::GetVerbLabel (CassoExplorerActions::Verb verb)
{
    switch (verb)
    {
        case CassoExplorerActions::Verb::Open:           return L"&Open";
        case CassoExplorerActions::Verb::Get:            return L"&Copy to folder...";
        case CassoExplorerActions::Verb::Put:            return L"&Put file...";
        case CassoExplorerActions::Verb::Delete:         return L"&Delete";
        case CassoExplorerActions::Verb::Rename:         return L"Re&name...";
        case CassoExplorerActions::Verb::Boot:           return L"Set as &startup program";
        case CassoExplorerActions::Verb::InsertDrive1:   return L"Insert into drive &1";
        case CassoExplorerActions::Verb::InsertDrive2:   return L"Insert into drive &2";
        case CassoExplorerActions::Verb::OpenInNewCasso: return L"Open in &new Casso";
        case CassoExplorerActions::Verb::NewDisk:        return L"&Disk image...";
        case CassoExplorerActions::Verb::NewFolder:      return L"&Folder";
        case CassoExplorerActions::Verb::Format:         return L"&Format disk image...";
        case CassoExplorerActions::Verb::ReadSectors:    return L"Read &sectors to file...";
        case CassoExplorerActions::Verb::WriteSectors:   return L"&Write sectors from file...";
        case CassoExplorerActions::Verb::ReadBlocks:     return L"Read &blocks to file...";
        case CassoExplorerActions::Verb::WriteBlocks:    return L"Write b&locks from file...";
        case CassoExplorerActions::Verb::Refresh:        return L"&Refresh";
        case CassoExplorerActions::Verb::OpenWith:       return L"Open wit&h";
        case CassoExplorerActions::Verb::MoreOptions:    return L"Show more &options";
        case CassoExplorerActions::Verb::Cut:            return L"Cu&t";
        case CassoExplorerActions::Verb::Copy:           return L"&Copy";
        case CassoExplorerActions::Verb::Paste:          return L"&Paste";
        case CassoExplorerActions::Verb::Share:          return L"&Share";
        case CassoExplorerActions::Verb::Restore:        return L"&Restore";
        case CassoExplorerActions::Verb::EmptyRecycleBin: return L"Empty Recycle &Bin";
        default:                                   return L"";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::GetVerbMenuGlyph
//
//  The icon beside a verb's row in a context menu: Explorer's own for the
//  verbs it has, and for Casso's, the nearest in the same icon font.
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * CassoExplorerWindow::GetVerbMenuGlyph (CassoExplorerActions::Verb verb)
{
    switch (verb)
    {
        case CassoExplorerActions::Verb::Open:           return s_kpszMdl2OpenFile;
        case CassoExplorerActions::Verb::OpenWith:       return s_kpszMdl2OpenWith;
        case CassoExplorerActions::Verb::InsertDrive1:   return s_kpszMdl2Save;
        case CassoExplorerActions::Verb::InsertDrive2:   return s_kpszMdl2Save;
        case CassoExplorerActions::Verb::OpenInNewCasso: return s_kpszMdl2Play;
        case CassoExplorerActions::Verb::Format:         return s_kpszMdl2HardDrive;
        case CassoExplorerActions::Verb::Refresh:        return s_kpszMdl2Refresh;
        case CassoExplorerActions::Verb::MoreOptions:    return s_kpszMdl2OpenInNewWindow;
        default:                                         return nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ApplyMenuSvgs
//
//  The commands the menus share -- the views, the preview pane -- take File
//  Explorer's icons for the theme in use.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ApplyMenuSvgs()
{
    static constexpr std::pair<DxuiListView::View, const wchar_t *>  kViews[] =
    {
        { DxuiListView::View::ExtraLargeIcons, L"windows.iconsize.extralarge" },
        { DxuiListView::View::LargeIcons,      L"windows.iconsize.large"      },
        { DxuiListView::View::MediumIcons,     L"windows.iconsize.medium"     },
        { DxuiListView::View::SmallIcons,      L"windows.iconsize.smallicon"  },
        { DxuiListView::View::List,            L"windows.iconsize.list"       },
        { DxuiListView::View::Details,         L"windows.iconsize.details"    },
        { DxuiListView::View::Tiles,           L"windows.iconsize.tile"       },
        { DxuiListView::View::Content,         L"windows.iconsize.content"    },
    };



    for (const auto & [view, name] : kViews)
    {
        m_commands.SetMenuSvg (CassoExplorerCommands::kViewFirst + (int) view, GetMenuSvg (name));
    }

    m_commands.SetMenuSvg (CassoExplorerCommands::kTogglePreview, GetMenuSvg (L"windows.previewpane"));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::GetVerbMenuSvgName
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * CassoExplorerWindow::GetVerbMenuSvgName (CassoExplorerActions::Verb verb)
{
    switch (verb)
    {
        case CassoExplorerActions::Verb::Open:         return L"windows.openfolder";
        case CassoExplorerActions::Verb::OpenWith:     return L"windows.openwith";
        case CassoExplorerActions::Verb::InsertDrive1: return L"casso.floppy525";
        case CassoExplorerActions::Verb::InsertDrive2: return L"casso.floppy525";
        case CassoExplorerActions::Verb::Format:       return L"windows.diskformat";
        case CassoExplorerActions::Verb::Refresh:      return L"windows.refresh";
        case CassoExplorerActions::Verb::MoreOptions:  return L"expandtoclassic";
        case CassoExplorerActions::Verb::Cut:          return L"windows.cut";
        case CassoExplorerActions::Verb::Copy:         return L"windows.copy";
        case CassoExplorerActions::Verb::Paste:        return L"windows.paste";
        case CassoExplorerActions::Verb::Rename:       return L"windows.rename";
        case CassoExplorerActions::Verb::Delete:       return L"windows.ribbondelete";
        case CassoExplorerActions::Verb::Share:        return L"Windows.ModernShare";
        case CassoExplorerActions::Verb::Restore:      return L"windows.recyclebin.restoreitems";
        case CassoExplorerActions::Verb::EmptyRecycleBin: return L"windows.recyclebin.empty";
        default:                                       return nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::GetOpenMenuImage
//
//  Explorer's Open row carries the icon of the program that will open the
//  item -- its own yellow folder for a folder -- so a folder or disk image,
//  which opens here, carries Casso Explorer's, and any other file the icon
//  of the program Windows opens it in. Drawn at the menu's icon size.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> CassoExplorerWindow::GetOpenMenuImage (bool browsable, const std::wstring & path)
{
    int                                   sizePx  = m_scaler.ToPx (kMenuIconDip);
    std::wstring                          program;
    std::wstring                          key;
    HICON                                 icon    = nullptr;
    HRESULT                               hr      = S_OK;
    auto                                  image   = std::make_shared<DxuiIconImage>();
    std::shared_ptr<const DxuiIconImage>  found;



    if (!browsable)
    {
        std::wstring  extension     = std::filesystem::path (path).extension().wstring();
        wchar_t       exe[MAX_PATH] = {};
        DWORD         length        = MAX_PATH;
        HRESULT       hrAssoc       = extension.empty() ? E_FAIL
                                                        : AssocQueryStringW (ASSOCF_NOTRUNCATE | ASSOCF_INIT_IGNOREUNKNOWN, ASSOCSTR_EXECUTABLE, extension.c_str(), L"open", exe, &length);

        if (FAILED (hrAssoc))
        {
            return nullptr;
        }

        program = exe;
    }

    key = std::to_wstring (sizePx) + L":" + program;

    if (m_openMenuImages.count (key) != 0)
    {
        return m_openMenuImages[key];
    }

    if (browsable)
    {
        icon = (HICON) LoadImageW (GetModuleHandleW (nullptr), MAKEINTRESOURCEW (IDI_CASSO_EXPLORER), IMAGE_ICON, sizePx, sizePx, LR_DEFAULTCOLOR);
    }
    else
    {
        hr = SHDefExtractIconW (program.c_str(), 0, 0, &icon, nullptr, (UINT) sizePx);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (icon != nullptr)
    {
        hr = DxuiIconImage::FromHicon (icon, sizePx, *image);
        DestroyIcon (icon);

        if (SUCCEEDED (hr))
        {
            found = image;
        }
    }

    m_openMenuImages[key] = found;

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::GetIconButtonTip
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerWindow::GetIconButtonTip (const DxuiCommand & command)
{
    std::wstring     tip   = command.GetLabelText();
    const wchar_t *  keys  = nullptr;



    tip.erase (std::remove (tip.begin(), tip.end(), L'&'), tip.end());

    switch ((CassoExplorerActions::Verb) command.id)
    {
        case CassoExplorerActions::Verb::Cut:    keys = L"Ctrl+X"; break;
        case CassoExplorerActions::Verb::Copy:   keys = L"Ctrl+C"; break;
        case CassoExplorerActions::Verb::Paste:  keys = L"Ctrl+V"; break;
        case CassoExplorerActions::Verb::Rename: keys = L"F2";     break;
        case CassoExplorerActions::Verb::Delete: keys = L"Delete"; break;
        default:                                                   break;
    }

    return (keys != nullptr) ? tip + L" (" + keys + L")" : tip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::GetMenuSvg
//
//  THE ICONS ARE READ, NOT SHIPPED: they are Microsoft's, in File Explorer's
//  own package, so Casso Explorer draws them from the copy on this machine and
//  a missing one falls back to the icon font. Each theme has its set -- the
//  light one fills its outlines, the dark one does not -- so a file is read
//  once per theme. Casso's own art, the 5.25-inch floppy, is drawn in the same
//  hand and the same three colors.
//
////////////////////////////////////////////////////////////////////////////////

const std::string * CassoExplorerWindow::GetMenuSvg (const wchar_t * name)
{
    bool          dark   = DxuiColor::ComputeRelativeLuminance (m_theme->Background()) < 0.5f;
    std::wstring  key    = std::wstring (dark ? L"dark:" : L"light:") + ((name != nullptr) ? name : L"");
    auto          found  = m_menuSvgs.find (key);
    std::string   svg;



    if (name == nullptr)
    {
        return nullptr;
    }

    if (found != m_menuSvgs.end())
    {
        return found->second ? found->second.get() : nullptr;
    }

    if (wcsncmp (name, L"casso.", 6) == 0)
    {
        svg = kFloppy525Svg;

        for (auto [from, to] : { std::pair<const char *, const char *> { "{INK}",  dark ? "#E0DFDF" : "#555" },
                                 std::pair<const char *, const char *> { "{ACC}",  dark ? "#4CC2FF" : "#0078D4" },
                                 std::pair<const char *, const char *> { "{BODY}", dark ? "none"    : "#FAFAFA" } })
        {
            for (size_t at = svg.find (from); at != std::string::npos; at = svg.find (from, at))
            {
                svg.replace (at, strlen (from), to);
            }
        }
    }
    else
    {
        wchar_t          windows[MAX_PATH] = {};
        UINT             length            = GetWindowsDirectoryW (windows, MAX_PATH);
        std::wstring     path              = std::wstring (windows, length) + kExplorerIconFolder + (dark ? L"theme-dark\\" : L"theme-light\\") + name + L".svg";
        std::ifstream    file              (path, std::ios::binary);

        //  A few are kept at the top of the folder rather than in a theme's.
        if (!file)
        {
            file.open (std::wstring (windows, length) + kExplorerIconFolder + name + L".svg", std::ios::binary);
        }

        if (file)
        {
            svg.assign (std::istreambuf_iterator<char> (file), std::istreambuf_iterator<char>());
        }
    }

    m_menuSvgs[key] = svg.empty() ? nullptr : std::make_unique<std::string> (std::move (svg));

    return m_menuSvgs[key] ? m_menuSvgs[key].get() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::GetVerbGlyph
//
//  The verbs Explorer shows as buttons, with its glyphs; null for the rest.
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * CassoExplorerWindow::GetVerbGlyph (CassoExplorerActions::Verb verb)
{
    switch (verb)
    {
        case CassoExplorerActions::Verb::Cut:    return s_kpszMdl2Cut;
        case CassoExplorerActions::Verb::Copy:   return s_kpszMdl2Copy;
        case CassoExplorerActions::Verb::Paste:  return s_kpszMdl2Paste;
        case CassoExplorerActions::Verb::Rename: return s_kpszMdl2Rename;
        case CassoExplorerActions::Verb::Delete: return s_kpszMdl2Delete;
        case CassoExplorerActions::Verb::Share:  return s_kpszMdl2Share;
        default:                           return nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::GetIconOrder
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerWindow::GetIconOrder (CassoExplorerActions::Verb verb)
{
    switch (verb)
    {
        case CassoExplorerActions::Verb::Cut:    return 0;
        case CassoExplorerActions::Verb::Copy:   return 1;
        case CassoExplorerActions::Verb::Paste:  return 2;
        case CassoExplorerActions::Verb::Rename: return 3;
        case CassoExplorerActions::Verb::Share:  return 4;
        default:                           return 5;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ShowListHeaderMenu
//
//  Explorer's header menu: fit the column under the pointer, fit them all,
//  then every column with a check by those showing. Name always shows.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ShowListHeaderMenu (int x, int y, int column)
{
    std::vector<DxuiPopupMenuItem>     items;
    std::vector<DxuiListView::Column>  columns = CassoExplorerBrowser::GetColumns();



    m_menuCommands.clear();

    if (column >= 0)
    {
        AddMenuCommand (items, L"Size column to &fit", [this, column]()
        {
            m_list->FitColumnToContent ((size_t) column);
            Invalidate();
        });
    }

    AddMenuCommand (items, L"Size &all columns to fit", [this]()
    {
        m_list->FitAllColumnsToContent();
        Invalidate();
    });

    items.push_back (DxuiPopupMenuItem::ForSeparator());

    for (size_t c = 0; c < columns.size() && c < m_listColumnChosen.size(); c++)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();

        command->label     = columns[c].title;
        command->isChecked = [this, c]() { return m_listColumnChosen[c]; };
        command->isEnabled = [c]() { return c != (size_t) CatalogModel::Column::Name; };
        command->dispatch  = [this, c]()
        {
            m_listColumnChosen[c] = !m_listColumnChosen[c];

            //  This folder's choice, and the one others without their own follow.
            m_prefs.hiddenColumns.clear();

            for (size_t each = 0; each < m_listColumnChosen.size(); each++)
            {
                if (!m_listColumnChosen[each])
                {
                    m_prefs.hiddenColumns.push_back ((int) each);
                }
            }

            RememberFolderColumns();
            FillList();
        };

        items.push_back (DxuiPopupMenuItem::ForCommand (command));
        m_menuCommands.push_back (std::move (command));
    }

    DxuiContextMenu::Show (*GetPopupHost(), x, y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::IsListColumnShown
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::IsListColumnShown (size_t column, bool chosen, Location::Kind kind, bool searching)
{
    bool  catalog     = column == (size_t) CatalogModel::Column::Address || column == (size_t) CatalogModel::Column::Locked;
    bool  recycled    = column == (size_t) CatalogModel::Column::OriginalLocation || column == (size_t) CatalogModel::Column::DateDeleted;
    bool  insideImage = kind == Location::Kind::DiskImage || kind == Location::Kind::DiskDirectory;
    bool  inBin       = kind == Location::Kind::RecycleBin;



    if (column == (size_t) CatalogModel::Column::Name)
    {
        return true;
    }

    if (recycled)
    {
        return inBin;
    }

    //  Search results show the folder each is in, as Explorer's do.
    if (column == (size_t) CatalogModel::Column::Folder)
    {
        return searching;
    }

    return chosen && (insideImage || !catalog);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ApplyColumnOrder
//
//  The order the user dragged the headers into, with any column it predates
//  at the end; in the Recycle Bin, Explorer's order for it.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ApplyColumnOrder()
{
    using Column = CatalogModel::Column;

    std::vector<size_t>  order;
    size_t               count = CassoExplorerBrowser::GetColumns().size();



    if (m_browser.GetLocation().kind == Location::Kind::RecycleBin)
    {
        for (Column column : { Column::Name, Column::OriginalLocation, Column::DateDeleted, Column::Size, Column::Type, Column::Modified, Column::Address, Column::Locked, Column::Folder })
        {
            order.push_back ((size_t) column);
        }
    }
    else
    {
        for (int column : m_folderColumnOrder)
        {
            if (column >= 0 && (size_t) column < count && std::find (order.begin(), order.end(), (size_t) column) == order.end())
            {
                order.push_back ((size_t) column);
            }
        }

        for (size_t column = 0; column < count; column++)
        {
            if (std::find (order.begin(), order.end(), column) == order.end())
            {
                order.push_back (column);
            }
        }
    }

    m_list->SetColumnOrder (order);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ShowListContextMenu
//
//  The rows come from the actions' verb list; each command runs its verb.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ShowListContextMenu (int x, int y, int group)
{
    std::vector<DxuiPopupMenuItem>                   items;
    Location                                         location;
    std::wstring                                     folderForCasso;
    bool                                             known          = false;
    bool                                             moreOptions    = false;
    bool                                             listFocused    = false;
    bool                                             browsable      = false;
    std::vector<std::shared_ptr<const DxuiCommand>>  iconCommands;
    std::vector<DxuiPopupMenuItem>                   newChoices;



    m_menuCommands.clear();
    AskCassoToDescribe();

    //  Explorer's Undo, naming what it undoes, while there is one.
    if (!m_undo.empty())
    {
        AddMenuCommand (items, GetUndoLabel (m_undo.back().kind), [this]() { UndoLast(); }, L"Ctrl+Z");
        m_menuCommands.back()->menuSvg = GetMenuSvg (L"windows.undo");
        items.push_back (DxuiPopupMenuItem::ForSeparator());
    }

    //  From a group's header: open or close it, and all of them, with the
    //  keys that do the same on a focused header.
    if (group >= 0 && m_listView != DxuiListView::View::List)
    {
        bool  collapsed = m_list->IsGroupCollapsed (group);

        AddMenuCommand (items, collapsed ? L"E&xpand group" : L"C&ollapse group", [this, group, collapsed]()
        {
            m_list->SetGroupCollapsed (group, !collapsed);
            Invalidate();
        }, collapsed ? L"Right" : L"Left");
        AddMenuCommand (items, L"&Expand all groups",   [this]() { m_list->SetAllGroupsCollapsed (false); Invalidate(); });
        AddMenuCommand (items, L"Co&llapse all groups", [this]() { m_list->SetAllGroupsCollapsed (true);  Invalidate(); });
        items.push_back (DxuiPopupMenuItem::ForSeparator());
    }

    //  A folder or a disk image opens in a new tab, as Explorer's items do.
    if (m_browser.GetSelectedRows().size() == 1 && m_browser.TryGetRowLocation (m_browser.GetSelectedRows()[0], location))
    {
        browsable = true;

        AddMenuCommand (items, L"Open in new &tab", [this, location]()
        {
            m_browser.OpenInNewTab (location);
            FillList();
        });
        m_menuCommands.back()->menuGlyph = s_kpszMdl2OpenInNewWindow;
        m_menuCommands.back()->menuSvg   = GetMenuSvg (L"windows.opennewtab");

        if (location.kind == Location::Kind::HostFolder)
        {
            AddPinMenuCommand (items, location.path);
        }

        items.push_back (DxuiPopupMenuItem::ForSeparator());
    }

    for (CassoExplorerActions::Verb verb : m_actions.GetListVerbs())
    {
        std::shared_ptr<DxuiCommand>  command;

        if (verb == CassoExplorerActions::Verb::MoreOptions)
        {
            moreOptions = true;
            continue;
        }

        //  The clipboard, rename and delete are Explorer's buttons along the
        //  menu's edge rather than rows, for a file and for an image's entry.
        //  Paste shows only with files to paste, so a menu with nothing to cut
        //  or copy and nothing to paste has no button row at all.
        if (verb == CassoExplorerActions::Verb::Paste && !m_shellVerbs.ClipboardHasFiles())
        {
            continue;
        }

        if (GetVerbGlyph (verb) != nullptr)
        {
            command           = std::make_shared<DxuiCommand>();
            command->id       = (int) verb;
            command->label    = GetVerbLabel (verb);
            command->glyph    = GetVerbGlyph (verb);
            command->menuSvg  = GetMenuSvg (GetVerbMenuSvgName (verb));
            command->dispatch = [this, verb]() { RunVerb (verb); };

            iconCommands.push_back (command);
            m_menuCommands.push_back (std::move (command));
            continue;
        }

        if (verb == CassoExplorerActions::Verb::OpenWith)
        {
            AddOpenWithMenu (items);
            continue;
        }

        //  New's choices go in its own submenu, where the Refresh row starts.
        if (verb == CassoExplorerActions::Verb::NewFolder || verb == CassoExplorerActions::Verb::NewDisk)
        {
            command           = std::make_shared<DxuiCommand>();
            command->id       = (int) verb;
            command->label    = GetVerbLabel (verb);
            command->dispatch = [this, verb]() { RunVerb (verb); };

            if (verb == CassoExplorerActions::Verb::NewFolder)
            {
                command->isEnabled = [this]() { return !m_browser.IsImageLocation() || m_browser.GetVolumeKind() == VolumeKind::ProDos; };
            }

            newChoices.push_back (DxuiPopupMenuItem::ForCommand (command));
            m_menuCommands.push_back (std::move (command));
            continue;
        }

        if (verb == CassoExplorerActions::Verb::Refresh && !newChoices.empty())
        {
            std::shared_ptr<DxuiCommand>  parent = std::make_shared<DxuiCommand>();
            bool                          dos33  = m_browser.IsImageLocation() && m_browser.GetVolumeKind() != VolumeKind::ProDos;

            parent->label     = L"Ne&w";
            parent->menuGlyph = s_kpszMdl2Add;
            parent->menuSvg   = GetMenuSvg (L"windows.newitem");
            parent->isEnabled = [dos33]() { return !dos33; };

            if (!items.empty())
            {
                items.push_back (DxuiPopupMenuItem::ForSeparator());
            }

            items.push_back (DxuiPopupMenuItem::ForSubmenu (parent, std::move (newChoices)));
            m_menuCommands.push_back (std::move (parent));
            newChoices.clear();
        }

        //  Refresh only places New; Explorer's menus leave it to F5.
        if (verb == CassoExplorerActions::Verb::Refresh)
        {
            continue;
        }

        command            = std::make_shared<DxuiCommand>();
        command->id        = (int) verb;
        command->label     = GetVerbLabel (verb);
        command->menuGlyph = GetVerbMenuGlyph (verb);
        command->menuSvg   = GetMenuSvg (GetVerbMenuSvgName (verb));
        command->dispatch  = [this, verb]() { RunVerb (verb); };

        if (verb == CassoExplorerActions::Verb::Open)
        {
            command->menuImage = GetOpenMenuImage (browsable, GetSelectedImagePath());
        }

        //  A machine known to have one drive cannot take drive 2; one not
        //  described yet is given the benefit of the doubt.
        if (verb == CassoExplorerActions::Verb::InsertDrive2)
        {
            command->isEnabled = [this]() { return m_cassoDriveCount != 1; };
        }

        if (verb == CassoExplorerActions::Verb::Paste)
        {
            command->isEnabled = [this]() { return m_shellVerbs.ClipboardHasFiles(); };
        }

        //  Copying out of an image goes by the clipboard or a drag, as in
        //  Explorer, which has no Copy to folder: the Copy verb is the button
        //  row's, and this one stands for the forms Copy as offers.
        if (verb == CassoExplorerActions::Verb::Get)
        {
            AddCopyAsMenu (items);
            continue;
        }

        items.push_back (DxuiPopupMenuItem::ForCommand (command));
        m_menuCommands.push_back (std::move (command));

        if (verb == CassoExplorerActions::Verb::Format)
        {
            std::vector<DxuiPopupMenuItem>  advanced;
            std::shared_ptr<DxuiCommand>    parent = std::make_shared<DxuiCommand>();

            for (CassoExplorerActions::Verb raw : { CassoExplorerActions::Verb::ReadSectors, CassoExplorerActions::Verb::WriteSectors,
                                              CassoExplorerActions::Verb::ReadBlocks,  CassoExplorerActions::Verb::WriteBlocks })
            {
                std::shared_ptr<DxuiCommand>  child = std::make_shared<DxuiCommand>();

                child->id       = (int) raw;
                child->label    = GetVerbLabel (raw);
                child->dispatch = [this, raw]() { RunRawVerb (raw); };

                advanced.push_back (DxuiPopupMenuItem::ForCommand (child));
                m_menuCommands.push_back (std::move (child));
            }

            parent->label = L"&Advanced";
            parent->menuGlyph = s_kpszMdl2Setting;
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

    //  A deleted item has no path to copy, and the bin itself describes it.
    if (!m_browser.GetSelectedRows().empty() && m_browser.GetLocation().kind == Location::Kind::RecycleBin)
    {
        items.push_back (DxuiPopupMenuItem::ForSeparator());
        AddMenuCommand (items, L"P&roperties", [this]() { RunRecycleBinVerb (CassoExplorerActions::Verb::Open); });
        m_menuCommands.back()->menuGlyph = s_kpszMdl2Repair;
        m_menuCommands.back()->menuSvg   = GetMenuSvg (L"windows.properties");
    }
    else if (!m_browser.GetSelectedRows().empty())
    {
        items.push_back (DxuiPopupMenuItem::ForSeparator());
        AddMenuCommand (items, L"Copy as &path", [this]() { CopySelectedPaths(); }, L"", s_kpszMdl2CopyPath);
        m_menuCommands.back()->menuSvg = GetMenuSvg (L"windows.copyaspath");

        if (m_browser.GetSelectedRows().size() == 1)
        {
            AddMenuCommand (items, L"P&roperties", [this]()
            {
                if (!m_browser.GetSelectedRows().empty())
                {
                    ShowRowProperties (m_browser.GetSelectedRows()[0]);
                }
            });
            m_menuCommands.back()->menuGlyph = s_kpszMdl2Repair;
            m_menuCommands.back()->menuSvg   = GetMenuSvg (L"windows.properties");
        }
    }

    //  In Explorer's order, whatever order the verbs came in.
    std::stable_sort (iconCommands.begin(), iconCommands.end(), [] (const auto & a, const auto & b)
    {
        return GetIconOrder ((CassoExplorerActions::Verb) a->id) < GetIconOrder ((CassoExplorerActions::Verb) b->id);
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
        AddMenuCommand (items, GetVerbLabel (CassoExplorerActions::Verb::MoreOptions), [this, screen]()
        {
            std::vector<std::wstring>  paths;
            HRESULT                    hr = S_OK;

            if (m_browser.GetLocation().kind == Location::Kind::RecycleBin)
            {
                m_browser.GetSelectedRecycledIds (paths);
                hr = m_shellVerbs.ShowRecycledMenu (GetHwnd(), paths, screen);
                RefreshAfterHostChange();
            }
            else
            {
                m_browser.GetSelectedHostPaths (paths);
                hr = m_shellVerbs.ShowShellMenu (GetHwnd(), paths, screen);
            }

            IGNORE_RETURN_VALUE (hr, S_OK);
        });
        m_menuCommands.back()->menuGlyph = s_kpszMdl2OpenInNewWindow;
    }

    //  The menu takes the focus look from the list while it is open, as
    //  Explorer's does: the item keeps its selection, drawn unfocused, and
    //  loses its focus border until the menu goes.
    listFocused = m_list->IsListFocused();

    if (listFocused)
    {
        m_list->SetListFocused (false);
        Invalidate();
    }

    DxuiContextMenu::Show (*GetPopupHost(), x, y, std::move (items), [this, listFocused] (bool)
    {
        if (listFocused && m_focus == Pane::List)
        {
            m_list->SetListFocused (true);
            Invalidate();
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::AddOpenWithMenu
//
//  The programs Windows recommends for the selected file, then its own
//  dialog for any other.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::AddOpenWithMenu (std::vector<DxuiPopupMenuItem> & items)
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

    parent->label = GetVerbLabel (CassoExplorerActions::Verb::OpenWith);
    parent->menuGlyph = s_kpszMdl2OpenWith;
    parent->menuSvg   = GetMenuSvg (L"windows.openwith");
    items.push_back (DxuiPopupMenuItem::ForSubmenu (parent, std::move (children)));
    m_menuCommands.push_back (std::move (parent));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::BeginRename
//
//  Over the name, with the name selected up to its extension, as Explorer
//  selects it; an entry inside an image has no extension to leave out.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::BeginRename()
{
    RECT          cell  = {};
    RECT          list  = m_list->GetBounds();
    int           row   = -1;
    std::wstring  name;
    size_t        dot   = std::wstring::npos;
    bool          host  = !m_browser.IsImageLocation();



    //  The focused item, as Explorer renames, even with others selected
    //  around it.
    row = m_list->GetSelectedRow();

    if (row < 0 || std::find (m_browser.GetSelectedRows().begin(), m_browser.GetSelectedRows().end(), row) == m_browser.GetSelectedRows().end())
    {
        row = m_browser.GetSelectedRows().empty() ? -1 : m_browser.GetSelectedRows()[0];
    }

    if (row < 0 || row >= (int) m_browser.GetRows().size())
    {
        return;
    }

    m_list->EnsureVisible (row);

    if (!m_list->GetCellTextRectPx (row, 0, cell))
    {
        return;
    }

    name = m_browser.GetRows()[(size_t) row].name;

    OffsetRect (&cell, list.left, list.top);

    //  Inside the row, as the selection box is, not over the whole of it.
    if (m_listView == DxuiListView::View::Details)
    {
        InflateRect (&cell, 0, -m_scaler.ToPx (DxuiListView::s_kRowBoxInsetYDip));
    }

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
//  CassoExplorerWindow::EndRename
//
//  A name left as it was, or emptied, changes nothing.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::EndRename (bool commit)
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

            if (SUCCEEDED (hr))
            {
                PushUndo (UndoStep { UndoStep::Kind::HostRename, Location(), { (std::filesystem::path (path).parent_path() / newName).wstring() }, oldName, newName });
            }
        }

        RefreshAfterHostChange();
        return;
    }

    {
        CassoExplorerActions::Outcome  renamed;

        //  The row renamed is the one the box was over, whatever else is
        //  selected.
        m_browser.SetSelectedRows ({ row });
        renamed = m_actions.RenameSelected (newName);

        if (renamed.Succeeded())
        {
            PushUndo (UndoStep { UndoStep::Kind::ImageRename, m_browser.GetLocation(), {}, oldName, newName });
        }

        ReportOutcome (renamed, L"Rename");
    }

    FillList();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::CreateDiskFromSelection
//
//  The selected host files and folders go onto the new disk. Whether they fit
//  is decided before the image exists, so a refusal leaves nothing behind.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::CreateDiskFromSelection (const CassoExplorerNewDiskDialog::Outcome & newDisk)
{
    std::vector<std::wstring>  paths;
    std::wstring               folder = m_browser.GetLocation().path;
    std::wstring               image  = CassoExplorerBrowser::JoinPath (folder, newDisk.fileName);
    VolumeKind                 kind   = (newDisk.request.formatName == "prodos") ? VolumeKind::ProDos : VolumeKind::Dos33;
    std::wstring                   refusal;
    CassoExplorerActions::Outcome  outcome;



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
//  CassoExplorerWindow::TryGetDropLocation
//
//  On the list: the image folder under the pointer, or the image the list
//  shows. On the tree: the image or directory node under the pointer. A host
//  folder takes no drop here; Explorer is for that.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::TryGetDropLocation (int tag, POINT screen, Location & outLocation)
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

        //  A root holds drives and known folders, not files, so it takes no drop.
        if (node == nullptr || !m_browser.TryGetNodeLocation (node->id, outLocation) || outLocation.kind == Location::Kind::Root)
        {
            return false;
        }
    }
    else
    {
        row = m_list->HitTestRow (client.x - list.left, client.y - list.top);

        //  A disk image row in a folder, or a directory row in an image, takes
        //  the drop itself; anywhere else in the list, the location shown does.
        if (row < 0 || !m_browser.TryGetRowLocation (row, outLocation) ||
            (outLocation.kind != Location::Kind::DiskDirectory && outLocation.kind != Location::Kind::DiskImage))
        {
            outLocation = m_browser.GetLocation();
        }
    }

    return outLocation.kind == Location::Kind::DiskImage || outLocation.kind == Location::Kind::DiskDirectory;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::TryGetHostDropFolder
//
//  A folder row or tree node on the host, or, on the list's empty space, the
//  host folder the list shows.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::TryGetHostDropFolder (int tag, POINT screen, std::wstring & outFolder)
{
    POINT                 client = screen;
    int                   row    = -1;
    RECT                  list   = m_list->GetBounds();
    const DxuiTreeNode  * node   = nullptr;
    Location              location;



    ScreenToClient (GetHwnd(), &client);

    if (tag == kDropTagTree)
    {
        row  = m_tree->HitTestRow (client.x, client.y);
        node = (row >= 0) ? m_tree->GetNodeAt (row) : nullptr;

        if (node == nullptr || !m_browser.TryGetNodeLocation (node->id, location))
        {
            return false;
        }
    }
    else
    {
        row = m_list->IsVisible() ? m_list->HitTestRow (client.x - list.left, client.y - list.top) : -1;

        if (row < 0 || !m_browser.TryGetRowLocation (row, location) || location.kind != Location::Kind::HostFolder)
        {
            location = m_browser.GetLocation();
        }
    }

    //  The Recycle Bin's own target recycles what is dropped on it, as a drop
    //  on Explorer's Recycle Bin does.
    if (location.kind == Location::Kind::RecycleBin)
    {
        outFolder = L"shell:RecycleBinFolder";
        return true;
    }

    if (location.kind != Location::Kind::HostFolder || location.path.empty())
    {
        return false;
    }

    outFolder = location.path;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ForwardHostDrag
//
//  Enters the folder's target when the drag first reaches it, leaving the
//  one before; over it after that.
//
////////////////////////////////////////////////////////////////////////////////

DWORD CassoExplorerWindow::ForwardHostDrag (IDataObject * data, const std::wstring & folder, POINT screen)
{
    HRESULT             hr     = S_OK;
    ComPtr<IShellItem>  item;
    DWORD               effect = m_dropTarget.GetAllowedEffects();
    POINTL              at     = { screen.x, screen.y };



    if (m_hostDrop != nullptr && IsSameFolder (folder, m_hostDropFolder))
    {
        hr = m_hostDrop->DragOver (m_dropTarget.GetDragKeyState(), at, &effect);

        return SUCCEEDED (hr) ? effect : DROPEFFECT_NONE;
    }

    LeaveHostDrop();

    hr = SHCreateItemFromParsingName (folder.c_str(), nullptr, IID_PPV_ARGS (&item));

    if (SUCCEEDED (hr))
    {
        hr = item->BindToHandler (nullptr, BHID_SFUIObject, IID_PPV_ARGS (&m_hostDrop));
    }

    if (FAILED (hr) || m_hostDrop == nullptr)
    {
        m_hostDrop.Reset();
        return DROPEFFECT_NONE;
    }

    m_hostDropFolder = folder;

    hr = m_hostDrop->DragEnter (data, m_dropTarget.GetDragKeyState(), at, &effect);

    return SUCCEEDED (hr) ? effect : DROPEFFECT_NONE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::LeaveHostDrop
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::LeaveHostDrop()
{
    if (m_hostDrop != nullptr)
    {
        HRESULT  hr = m_hostDrop->DragLeave();

        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    m_hostDrop.Reset();
    m_hostDropFolder.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::GetDropEffect
//
//  What a drop at this point would do, so the pointer and the words under the
//  drag image say so before the button is let go. A tab takes no drop, but
//  one the drag rests on opens, so the drop can go into its list.
//
////////////////////////////////////////////////////////////////////////////////

DWORD CassoExplorerWindow::GetDropEffect (IDataObject * data, int tag, POINT screen)
{
    Location      location;
    DropSource    source;
    std::wstring  hostFolder;
    bool          readOnly = false;
    HRESULT       hr       = S_OK;
    DWORD         effect   = DROPEFFECT_NONE;



    if (tag == kDropTagTabs)
    {
        HoverDropTab (screen);
        ShowDropTarget (tag, screen, false);
        DescribeDrop (data, DROPEFFECT_NONE, location);
        return DROPEFFECT_NONE;
    }

    if (data != nullptr && TryGetHostDropFolder (tag, screen, hostFolder))
    {
        effect = ForwardHostDrag (data, hostFolder, screen);
        ShowDropTarget (tag, screen, effect != DROPEFFECT_NONE);

        return effect;
    }

    LeaveHostDrop();

    if (data != nullptr && TryGetDropLocation (tag, screen, location) && ReadDropSource (data, source))
    {
        if (m_context.fs != nullptr)
        {
            hr = m_context.fs->GetReadOnlyAttribute (location.path, readOnly);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        effect = readOnly ? DROPEFFECT_NONE : ChooseDropEffect (source, location);
    }

    DescribeDrop   (data, effect, location);
    ShowDropTarget (tag, screen, effect != DROPEFFECT_NONE);

    return effect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ReadDropSource
//
//  Another image's entries when the drag holds them, host files otherwise.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::ReadDropSource (IDataObject * data, DropSource & outSource)
{
    FORMATETC  entries = { (CLIPFORMAT) RegisterClipboardFormatA (DragPayload::kPrivateFormatName), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM  medium  = {};
    HRESULT    hr      = S_OK;



    outSource = DropSource();

    if (data == nullptr)
    {
        return false;
    }

    hr = data->GetData (&entries, &medium);

    if (SUCCEEDED (hr))
    {
        const char  * bytes = (const char *) GlobalLock (medium.hGlobal);
        size_t        size  = GlobalSize (medium.hGlobal);

        if (bytes != nullptr)
        {
            outSource.fromImage = DragPayload::DecodeCatalogEntries (std::string (bytes, strnlen (bytes, size)),
                                                                     outSource.image, outSource.kind, outSource.catalogPaths);
            GlobalUnlock (medium.hGlobal);
        }

        ReleaseStgMedium (&medium);
    }

    if (outSource.fromImage)
    {
        return true;
    }

    hr = DxuiDragDropTarget::ExtractHDropPaths (data, outSource.hostPaths);

    return SUCCEEDED (hr) && !outSource.hostPaths.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ChooseDropEffect
//
//  A disk image is a volume of its own, so entries moved within one image
//  move, and anything coming from elsewhere is copied, unless Ctrl or Shift
//  says otherwise. A drop that would go nowhere is none: an image onto
//  itself, or entries moved into the directory they are already in or into
//  one of themselves.
//
////////////////////////////////////////////////////////////////////////////////

DWORD CassoExplorerWindow::ChooseDropEffect (const DropSource & source, const Location & target) const
{
    bool         same   = source.fromImage && IsSameFolder (TextEncoding::NarrowToWide (source.image), target.path);
    std::string  into   = (target.kind == Location::Kind::DiskDirectory) ? target.innerPath : std::string();
    DWORD        effect = DragPayload::ChooseDropEffect (same, m_dropTarget.GetDragKeyState(), m_dropTarget.GetAllowedEffects());



    for (const std::wstring & path : source.hostPaths)
    {
        if (IsSameFolder (path, target.path))
        {
            return DROPEFFECT_NONE;
        }
    }

    for (const std::string & path : (same ? source.catalogPaths : std::vector<std::string>()))
    {
        size_t       slash  = path.find_last_of ('/');
        std::string  parent = (slash == std::string::npos) ? std::string() : path.substr (0, slash);

        if (into == path || into.rfind (path + "/", 0) == 0)
        {
            return DROPEFFECT_NONE;
        }

        if (effect == DROPEFFECT_MOVE && parent == into)
        {
            return DROPEFFECT_NONE;
        }
    }

    return effect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::DescribeDrop
//
//  The words under the drag image, as Explorer's: "Move to Disks", "Copy to
//  GAMES". None where the drop would do nothing.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::DescribeDrop (IDataObject * data, DWORD effect, const Location & target)
{
    HRESULT       hr    = S_OK;
    std::wstring  place;
    size_t        slash = 0;



    if (data == nullptr)
    {
        return;
    }

    if (effect == DROPEFFECT_NONE)
    {
        hr = DxuiDragDropTarget::SetDropDescription (data, DROPIMAGE_INVALID, L"", L"");
        IGNORE_RETURN_VALUE (hr, S_OK);
        return;
    }

    if (target.kind == Location::Kind::DiskDirectory && !target.innerPath.empty())
    {
        slash = target.innerPath.find_last_of ('/');
        place = TextEncoding::NarrowToWide ((slash == std::string::npos) ? target.innerPath : target.innerPath.substr (slash + 1));
    }
    else
    {
        slash = target.path.find_last_of (L"\\/");
        place = (slash == std::wstring::npos) ? target.path : target.path.substr (slash + 1);
    }

    hr = DxuiDragDropTarget::SetDropDescription (data,
                                                 (effect == DROPEFFECT_MOVE) ? DROPIMAGE_MOVE : DROPIMAGE_COPY,
                                                 (effect == DROPEFFECT_MOVE) ? L"Move to %1" : L"Copy to %1",
                                                 place.c_str());
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::HoverDropTab
//
//  The tab under the drag opens at once, as Explorer's does.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::HoverDropTab (POINT screen)
{
    POINT  client = screen;
    int    index  = -1;



    ScreenToClient (GetHwnd(), &client);
    index = m_tabs->HitTest (client.x, client.y);

    if (index >= 0 && (size_t) index != m_browser.GetBrowserModel().GetActiveIndex())
    {
        SwitchToTab ((size_t) index);
        Invalidate();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ShowDropTarget
//
//  The row or tree node a drop would land on is lit as the pointer moves, as
//  Explorer lights its drop target. A drop on the list's empty space goes to
//  the location shown, so nothing is lit there. Nothing is redrawn while the
//  target stays the same, since OLE asks many times a second.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ShowDropTarget (int tag, POINT screen, bool accepted)
{
    POINT     client  = screen;
    RECT      list    = m_list->GetBounds();
    Location  location;
    int       listRow = -1;
    int       treeRow = -1;
    int       row     = -1;



    ScreenToClient (GetHwnd(), &client);

    if (accepted && tag == kDropTagTree)
    {
        treeRow = m_tree->HitTestRow (client.x, client.y);
    }
    else if (accepted && tag == kDropTagList)
    {
        row = m_list->HitTestRow (client.x - list.left, client.y - list.top);

        if (row >= 0 && m_browser.TryGetRowLocation (row, location) &&
            (location.kind == Location::Kind::DiskDirectory || location.kind == Location::Kind::DiskImage))
        {
            listRow = row;
        }
    }

    if (listRow == m_list->GetDropRow() && treeRow == m_treeDropRow)
    {
        return;
    }

    m_list->SetDropRow (listRow);
    m_treeDropRow = treeRow;
    m_tree->SetHoverRow (treeRow);

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ClearDropTarget
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ClearDropTarget()
{
    LeaveHostDrop();

    if (m_list->GetDropRow() < 0 && m_treeDropRow < 0)
    {
        return;
    }

    m_list->SetDropRow  (-1);
    m_tree->SetHoverRow (-1);
    m_treeDropRow = -1;

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::OnDrop
//
//  Another image's entries are copied byte for byte when the drag holds
//  them; otherwise the host files go in by Put's rules. A move removes what
//  it copied once everything has landed: entries from their image, and host
//  files to the Recycle Bin. The whole move happens here, so the source is
//  told of a copy and removes nothing itself.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::OnDrop (IDataObject * data, int tag, POINT screen)
{
    Location                       location;
    DropSource                     source;
    VolumeKind                     targetKind = VolumeKind::Unknown;
    std::string                    inner;
    VolumeListing                  listing;
    CassoExplorerActions::Outcome  outcome;
    CassoExplorerActions::Outcome  removed;
    DWORD                          effect     = DROPEFFECT_NONE;
    BOOL                           posted     = FALSE;
    std::wstring                   hostFolder;
    POINTL                         at         = { screen.x, screen.y };



    //  Onto a host folder, the folder's own target does the drop.
    if (m_hostDrop != nullptr && TryGetHostDropFolder (tag, screen, hostFolder) && IsSameFolder (hostFolder, m_hostDropFolder))
    {
        effect = m_dropTarget.GetAllowedEffects();

        HRESULT  hr = m_hostDrop->Drop (data, m_dropTarget.GetDragKeyState(), at, &effect);

        IGNORE_RETURN_VALUE (hr, S_OK);
        m_dropTarget.SetDropResult (effect);

        m_hostDrop.Reset();
        m_hostDropFolder.clear();
        return;
    }

    if (!TryGetDropLocation (tag, screen, location) || !ReadDropSource (data, source))
    {
        return;
    }

    effect = ChooseDropEffect (source, location);

    if (effect == DROPEFFECT_NONE)
    {
        return;
    }

    m_dropTarget.SetDropResult (DROPEFFECT_COPY);

    inner = (location.kind == Location::Kind::DiskDirectory) ? location.innerPath : std::string();

    //  What the target is, by listing where the drop goes.
    if (!m_browser.GetOperations().List (TextEncoding::WideToNarrow (location.path), inner, listing, targetKind).Succeeded())
    {
        ShowMessage (L"The disk image could not be read.", MB_ICONWARNING);
        return;
    }

    //  A right-drag asks what the drop means, once this call has returned and
    //  the drag itself is over.
    if (m_dropTarget.IsRightDrag())
    {
        m_pendingDrop              = PendingDrop();
        m_pendingDrop.valid        = true;
        m_pendingDrop.location     = location;
        m_pendingDrop.targetKind   = targetKind;
        m_pendingDrop.inner        = inner;
        m_pendingDrop.screen       = screen;
        m_pendingDrop.hostPaths    = source.hostPaths;
        m_pendingDrop.fromImage    = source.fromImage;
        m_pendingDrop.sourceImage  = source.image;
        m_pendingDrop.sourceKind   = source.kind;
        m_pendingDrop.catalogPaths = source.catalogPaths;

        posted = PostMessageW (GetHwnd(), kDropMenuMessage, 0, 0);
        IGNORE_RETURN_VALUE (posted, TRUE);
        return;
    }

    if (source.fromImage)
    {
        outcome = m_actions.CopyEntriesInto (source.image, source.kind, source.catalogPaths, location.path, targetKind, inner);
    }
    else
    {
        outcome = m_actions.PutInto (location.path, targetKind, inner, source.hostPaths, MakeAddressPrompt());
    }

    //  Only a copy that landed whole is taken from where it came.
    if (effect == DROPEFFECT_MOVE && outcome.Succeeded())
    {
        if (source.fromImage)
        {
            removed = m_actions.DeleteEntries (source.image, source.catalogPaths);

            if (!removed.Succeeded())
            {
                outcome = removed;
            }
        }
        else
        {
            RecycleHostFiles (source.hostPaths);
        }
    }

    if (effect != DROPEFFECT_MOVE)
    {
        PushPutUndo (outcome, location.path, inner);
    }

    ReportOutcome (outcome, (effect == DROPEFFECT_MOVE) ? L"Move" : L"Put");
    RefreshAfterHostChange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::RecycleHostFiles
//
//  To the Recycle Bin rather than gone, so a move whose files a disk image
//  holds in another form can still be undone.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::RecycleHostFiles (const std::vector<std::wstring> & paths)
{
    std::wstring     list;
    SHFILEOPSTRUCTW  operation = {};
    int              result    = 0;



    for (const std::wstring & path : paths)
    {
        list += path;
        list += L'\0';
    }

    list += L'\0';

    operation.wFunc  = FO_DELETE;
    operation.pFrom  = list.c_str();
    operation.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;

    result = SHFileOperationW (&operation);
    IGNORE_RETURN_VALUE (result, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::IsListVerbOffered
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::IsListVerbOffered (CassoExplorerActions::Verb verb) const
{
    std::vector<CassoExplorerActions::Verb>  verbs = m_actions.GetListVerbs();



    return std::find (verbs.begin(), verbs.end(), verb) != verbs.end();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::SetCommandBarDropDowns
//
//  The rows of New, Sort, View and Theme. They are commands, so each reads
//  whether it is enabled and checked when the menu draws.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::SetCommandBarDropDowns()
{
    std::vector<DxuiPopupMenuItem>  newItems;
    std::vector<DxuiPopupMenuItem>  sortItems;
    std::vector<DxuiPopupMenuItem>  viewItems;
    std::vector<DxuiPopupMenuItem>  themeItems;
    std::vector<DxuiPopupMenuItem>  groupItems;
    size_t                          column = 0;



    newItems.push_back (DxuiPopupMenuItem::ForCommand (m_commands.Find (CassoExplorerCommands::kNewFolder)));
    newItems.push_back (DxuiPopupMenuItem::ForCommand (m_commands.Find (CassoExplorerCommands::kNewDisk)));

    //  The columns the list shows here, as Explorer's menu lists them.
    for (column = 0; column < CassoExplorerBrowser::GetColumns().size(); column++)
    {
        bool  chosen = column >= m_listColumnChosen.size() || m_listColumnChosen[column];

        if (!IsListColumnShown (column, chosen, m_browser.GetLocation().kind, SearchQuery::IsId (m_browser.GetLocation().path)))
        {
            continue;
        }

        sortItems.push_back (DxuiPopupMenuItem::ForCommand (m_commands.Find (CassoExplorerCommands::kSortByColumn + (int) column)));
    }

    sortItems.push_back (DxuiPopupMenuItem::ForSeparator());
    sortItems.push_back (DxuiPopupMenuItem::ForCommand (m_commands.Find (CassoExplorerCommands::kSortAscending)));
    sortItems.push_back (DxuiPopupMenuItem::ForCommand (m_commands.Find (CassoExplorerCommands::kSortDescending)));

    //  Explorer keeps Group by at the foot of its Sort menu.
    for (int field : { 1, 2, 3, 4, 0 })
    {
        groupItems.push_back (DxuiPopupMenuItem::ForCommand (m_commands.Find (CassoExplorerCommands::kGroupByField + field)));
    }

    groupItems.push_back (DxuiPopupMenuItem::ForSeparator());
    groupItems.push_back (DxuiPopupMenuItem::ForCommand (m_commands.Find (CassoExplorerCommands::kGroupAscending)));
    groupItems.push_back (DxuiPopupMenuItem::ForCommand (m_commands.Find (CassoExplorerCommands::kGroupDescending)));

    sortItems.push_back (DxuiPopupMenuItem::ForSeparator());
    sortItems.push_back (DxuiPopupMenuItem::ForSubmenu (m_commands.Find (CassoExplorerCommands::kGroupBy), std::move (groupItems)));

    //  In Explorer's order.
    for (int view : { 1, 2, 3, 4, 5, 0, 6, 7 })
    {
        viewItems.push_back (DxuiPopupMenuItem::ForCommand (m_commands.Find (CassoExplorerCommands::kViewFirst + view)));
    }

    viewItems.push_back (DxuiPopupMenuItem::ForSeparator());
    viewItems.push_back (DxuiPopupMenuItem::ForCommand (m_commands.Find (CassoExplorerCommands::kOptions)));

    for (int id : { (int) CassoExplorerCommands::kThemeLight, (int) CassoExplorerCommands::kThemeDark, (int) CassoExplorerCommands::kThemeSystem,
                    (int) CassoExplorerCommands::kThemeSkeuomorphic, (int) CassoExplorerCommands::kThemeDarkModern, (int) CassoExplorerCommands::kThemeRetroTerminal })
    {
        themeItems.push_back (DxuiPopupMenuItem::ForCommand (m_commands.Find (id)));
    }

    m_commandBar->SetDropDownItems (CassoExplorerCommands::kNew,   std::move (newItems));
    m_commandBar->SetDropDownItems (CassoExplorerCommands::kSort,  std::move (sortItems));
    m_commandBar->SetDropDownItems (CassoExplorerCommands::kView,  std::move (viewItems));
    m_commandBar->SetDropDownItems (CassoExplorerCommands::kTheme, std::move (themeItems));

    //  Each theme is shown as the pointer passes over its row; leaving the
    //  menu without a choice puts back the one it opened on.
    m_commandBar->SetDropDownSinks (CassoExplorerCommands::kTheme, [this] (int index) { PreviewTheme (index); }, nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::SizeListIcons
//
//  The list's icons are drawn at its view's size, so they are fetched at that
//  size rather than scaled up from the tree's.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::SizeListIcons()
{
    UINT  dpi = GetDpiForWindow (GetHwnd());



    m_listIcons.SetDpi    (dpi);
    m_listIcons.SetSizePx ((m_listView == DxuiListView::View::Details) ? MulDiv (DxuiTreeView::s_kIconDip, (int) dpi, (int) DxuiDpiScaler::kBaseDpi)
                                                                        : DxuiListView::GetItemIconPx (m_listView, dpi));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ShowOptions
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ShowOptions()
{
    static constexpr const char          * kNamings[] = { CassoExplorerPrefs::kNamingDescriptive, CassoExplorerPrefs::kNamingCiderPress, CassoExplorerPrefs::kNamingAppleSingle };
    CassoExplorerOptionsDialog::Choices    choices;



    Win32UserClasses  classes;
    bool              wasRegistered    = FileAssociations::IsRegistered (classes);
    wchar_t           module[MAX_PATH] = {};
    std::wstring      folder;
    HRESULT           hr               = S_OK;



    choices.hostNaming = (int) GetNamingStyle();
    choices.registered = wasRegistered;

    if (!CassoExplorerOptionsDialog::Ask (GetHwnd(), m_theme, choices))
    {
        return;
    }

    if (choices.hostNaming >= 0 && choices.hostNaming < (int) std::size (kNamings))
    {
        m_prefs.hostNaming = kNamings[choices.hostNaming];
    }

    //  The user's file types change only when the box was changed.
    if (choices.registered != wasRegistered)
    {
        GetModuleFileNameW (nullptr, module, MAX_PATH);
        folder = module;
        folder.resize (folder.find_last_of (L"\\/"));

        hr = choices.registered ? FileAssociations::Register (classes, LaunchCommand::GetSiblingPath (folder, LaunchCommand::kCassoExe), module)
                                : FileAssociations::Unregister (classes);

        if (FAILED (hr))
        {
            ShowMessage (L"Windows did not take the change to the disk image types.", MB_ICONERROR);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::GetNamingStyle
//
//  The host file name style the Options dialog set; its choices are in the
//  style's own order.
//
////////////////////////////////////////////////////////////////////////////////

HostFileNaming::Style CassoExplorerWindow::GetNamingStyle() const
{
    if (m_prefs.hostNaming == CassoExplorerPrefs::kNamingCiderPress)
    {
        return HostFileNaming::Style::CiderPress;
    }

    if (m_prefs.hostNaming == CassoExplorerPrefs::kNamingAppleSingle)
    {
        return HostFileNaming::Style::AppleSingle;
    }

    return HostFileNaming::Style::Descriptive;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::SelectRowNamed
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::SelectRowNamed (const std::wstring & name)
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
//  CassoExplorerWindow::GetPasteFolder
//
//  A single folder row takes the paste; otherwise the folder being shown.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerWindow::GetPasteFolder() const
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
//  CassoExplorerWindow::GetFolderViewKey
//
//  A host folder by its path, a place inside a disk image by the image and
//  the directory, and a root by its id.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerWindow::GetFolderViewKey (FolderViews::FolderType * outType) const
{
    Location                 location = m_browser.GetLocation();
    FolderViews::FolderType  type     = FolderViews::FolderType::Generic;



    switch (location.kind)
    {
        case Location::Kind::None:
            return std::wstring();

        case Location::Kind::Root:
            if (outType != nullptr)
            {
                *outType = (location.path == TreeModel::kThisPcRootId) ? FolderViews::FolderType::Drives : type;
            }

            return location.path;

        case Location::Kind::HostFolder:
            if (outType != nullptr)
            {
                *outType = FolderViews::ReadFolderType (location.path);
            }

            return location.path;

        default:
            return location.path + L"|" + TextEncoding::NarrowToWide (location.innerPath);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ApplyFolderView
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ApplyFolderView()
{
    FolderViews::FolderType  type  = FolderViews::FolderType::Generic;
    std::wstring             key   = GetFolderViewKey (nullptr);
    FolderViewEntry          entry;



    if (key == m_listViewKey)
    {
        return;
    }

    GetFolderViewKey (&type);

    m_listViewKey = key;
    m_listView    = m_prefs.folderViews.GetView (key, type);
    m_list->SetView (m_listView);
    SizeListIcons();

    //  Each folder keeps its own sort and grouping, as Explorer's do; one
    //  never given either opens by name and ungrouped.
    entry = m_prefs.folderViews.GetEntry (key, type);

    if (entry.sortColumn > (int) CatalogModel::Column::DateDeleted)
    {
        entry.sortColumn = 0;
    }

    if (entry.groupBy > (int) RowGrouping::Field::Size)
    {
        entry.groupBy = 0;
    }

    m_browser.SetSortAndGroup ((CatalogModel::Column) entry.sortColumn, entry.sortDescending,
                               (RowGrouping::Field) entry.groupBy, entry.groupDescending);

    ApplyColumnWidths (entry.columnWidthsDip.empty() ? m_prefs.columnWidthsDip : entry.columnWidthsDip);

    //  Its own column order and choice of columns, or the latest ones.
    m_folderColumnOrder = entry.columnOrder.empty() ? m_prefs.columnOrder : entry.columnOrder;
    m_listColumnChosen.assign (CassoExplorerBrowser::GetColumns().size(), true);

    for (int hidden : (entry.columnsChosen ? entry.hiddenColumns : m_prefs.hiddenColumns))
    {
        if (hidden > (int) CatalogModel::Column::Name && (size_t) hidden < m_listColumnChosen.size())
        {
            m_listColumnChosen[(size_t) hidden] = false;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::RememberFolderSort
//
//  Kept only when the user sorts or groups, so a folder merely visited keeps
//  following the defaults.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::RememberFolderSort()
{
    FolderViewEntry  entry;



    if (m_listViewKey.empty() || !m_browser.GetBrowserModel().HasTabs())
    {
        return;
    }

    const BrowserModel::Tab &  tab = m_browser.GetBrowserModel().GetActiveTab();

    entry                 = m_prefs.folderViews.GetEntry (m_listViewKey, FolderViews::FolderType::Generic);
    entry.view            = m_listView;
    entry.sortColumn      = (int) tab.sortColumn;
    entry.sortDescending  = tab.sortDescending;
    entry.groupBy         = (int) tab.groupBy;
    entry.groupDescending = tab.groupDescending;

    m_prefs.folderViews.Remember (entry);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ReadFolderOptions
//
//  Explorer's options for hidden items, read again. When one has changed,
//  the tree is listed again under them, and true is returned so the caller
//  relists the folder.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::ReadFolderOptions()
{
    FolderOptions  options = FolderOptions::ReadFromShell();



    if (options == m_explorerOptions)
    {
        return false;
    }

    //  The pane's options Casso Explorer still follows change with them.
    m_explorerOptions = options;
    m_browser.SetFolderOptions (options);
    ApplyNavPaneOptions();
    m_browser.GetTreeModel().InvalidateAll();
    RefreshTree();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::RefreshAfterHostChange
//
//  The folder watcher sees the change too, a moment later; rereading now
//  shows it at once.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::RefreshAfterHostChange()
{
    HRESULT  hr = m_browser.Reload (true);



    IGNORE_RETURN_VALUE (hr, S_OK);
    FillList();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::GetSelectedImagePath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerWindow::GetSelectedImagePath() const
{
    std::vector<std::wstring>  paths;



    m_browser.GetSelectedHostPaths (paths);

    return (paths.size() == 1) ? paths[0] : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::RunRecycleBinVerb
//
//  A verb on the Recycle Bin's items, carried out by the bin itself. Whether
//  the verb was the bin's to carry out.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::RunRecycleBinVerb (CassoExplorerActions::Verb verb)
{
    using Verb = CassoExplorerActions::Verb;

    HRESULT                    hr = S_OK;
    std::vector<std::wstring>  ids;



    m_browser.GetSelectedRecycledIds (ids);

    switch (verb)
    {
        case Verb::Open:
            BAIL_OUT_IF (ids.empty(), S_OK);
            hr = m_shellVerbs.RunRecycledVerb (GetHwnd(), ids, IShellItemVerbs::RecycledVerb::Properties);
            break;

        case Verb::Restore:
        case Verb::Delete:
            //  Restore with nothing selected restores everything, as the
            //  command bar's Restore all items does.
            if (ids.empty() && verb == Verb::Restore)
            {
                m_browser.GetAllRecycledIds (ids);
            }

            BAIL_OUT_IF (ids.empty(), S_OK);
            hr = m_shellVerbs.RunRecycledVerb (GetHwnd(), ids, (verb == Verb::Restore) ? IShellItemVerbs::RecycledVerb::Restore
                                                                                       : IShellItemVerbs::RecycledVerb::Delete);
            RefreshAfterHostChange();
            break;

        case Verb::EmptyRecycleBin:
            hr = m_shellVerbs.EmptyRecycleBin (GetHwnd());
            RefreshAfterHostChange();
            break;

        default:
            return false;
    }

Error:
    IGNORE_RETURN_VALUE (hr, S_OK);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::GetCommandLabel
//
//  The label of a command whose label follows the selection, or empty for
//  the command's own: Restore restores the selected items, or all of them.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerWindow::GetCommandLabel (int id) const
{
    if (id == CassoExplorerCommands::kRestoreItems && !m_browser.GetSelectedRows().empty())
    {
        return L"Restore the selected items";
    }

    return std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::RunVerb
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::RunVerb (CassoExplorerActions::Verb verb)
{
    HRESULT                              hr        = S_OK;
    std::filesystem::path                picked;
    bool                                 chosen    = false;
    FileDialogSpec                       spec;
    int                                  answer    = 0;
    CassoExplorerActions::Outcome        outcome;
    std::vector<FileEntry>               entries;
    std::wstring                         newName;
    std::vector<std::wstring>            hostPaths;
    CassoExplorerNewDiskDialog::Outcome  newDisk;
    HostFileNaming::Style                style     = GetNamingStyle();



    //  The Recycle Bin's items are the shell's to restore, delete and
    //  describe; opening one shows its properties, as Explorer's does.
    if (m_browser.GetLocation().kind == Location::Kind::RecycleBin && RunRecycleBinVerb (verb))
    {
        return;
    }

    switch (verb)
    {
        case CassoExplorerActions::Verb::Open:
            if (m_browser.IsImageLocation() && !(m_browser.GetSelectedRows().size() == 1 && m_browser.OpenRow (m_browser.GetSelectedRows()[0])))
            {
                OpenSelectedEntries();
            }
            else if (m_browser.IsImageLocation())
            {
                FillList();
            }
            else if (m_browser.GetSelectedRows().size() > 1)
            {
                OpenEachSelected();
            }
            else if (!m_browser.GetSelectedRows().empty() && m_browser.OpenRow (m_browser.GetSelectedRows()[0]))
            {
                FillList();
            }
            else if (!GetSelectedImagePath().empty())
            {
                //  Not something to browse: a real file, in its own program.
                hr = m_shellVerbs.Open (GetHwnd(), GetSelectedImagePath());
            }

            break;

        case CassoExplorerActions::Verb::Get:
            hr = m_dialogs.PickFolder (GetHwnd(), picked, chosen);

            if (SUCCEEDED (hr) && chosen)
            {
                ReportOutcome (m_actions.GetSelected (picked.wstring(), style), L"Copy");
            }

            break;

        case CassoExplorerActions::Verb::Put:
            hr = m_dialogs.PickFileToOpen (GetHwnd(), spec, picked, chosen);

            if (SUCCEEDED (hr) && chosen)
            {
                outcome = m_actions.PutFiles ({ picked.wstring() }, MakeAddressPrompt());
                PushPutUndo   (outcome, m_browser.GetLocation().path, m_browser.GetLocation().innerPath);
                ReportOutcome (outcome, L"Put");
                FillList();
            }

            break;

        case CassoExplorerActions::Verb::Cut:
        case CassoExplorerActions::Verb::Copy:
            if (m_browser.IsImageLocation())
            {
                CopyEntriesToClipboard (GetNamingStyle());
                break;
            }

            m_browser.GetSelectedHostPaths (hostPaths);
            hr = m_shellVerbs.PlaceOnClipboard (GetHwnd(), hostPaths, verb == CassoExplorerActions::Verb::Cut);
            m_cutPaths = (SUCCEEDED (hr) && verb == CassoExplorerActions::Verb::Cut) ? hostPaths : std::vector<std::wstring>();
            FillList();
            break;

        case CassoExplorerActions::Verb::Share:
            m_browser.GetSelectedHostPaths (hostPaths);
            hr = m_shellVerbs.Share (GetHwnd(), hostPaths);
            break;

        case CassoExplorerActions::Verb::Paste:
            PasteHere (GetPasteFolder());
            break;

        case CassoExplorerActions::Verb::Delete:
            if (!m_browser.IsImageLocation())
            {
                //  Windows asks, and the Recycle Bin keeps it.
                m_browser.GetSelectedHostPaths (hostPaths);
                hr = m_shellVerbs.Recycle (GetHwnd(), hostPaths);

                if (SUCCEEDED (hr) && !hostPaths.empty())
                {
                    PushUndo (UndoStep { UndoStep::Kind::Recycle, Location(), hostPaths });
                }

                RefreshAfterHostChange();
                break;
            }

        {
            std::vector<std::wstring>  plan    = m_actions.DescribeDeletePlan();
            std::wstring               message = std::format (L"Delete {} selected item(s) from this disk image?",
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
                UndoStep  step { UndoStep::Kind::ImageDelete, m_browser.GetLocation() };
                wchar_t   temp[MAX_PATH] = {};

                //  An AppleSingle copy keeps each file's bytes, type and
                //  address, so a put brings it back as it was. The copy holds
                //  files only, so a deletion that takes a folder is not kept.
                m_browser.GetSelectedEntries (entries);

                if (std::none_of (entries.begin(), entries.end(), [] (const FileEntry & entry) { return entry.isDirectory; }) &&
                    GetTempPathW (MAX_PATH, temp) != 0)
                {
                    step.savedDir = std::format (L"{}CassoExplorerUndo\\{}", temp, GetTickCount64());
                    std::filesystem::create_directories (step.savedDir);
                }

                if (!step.savedDir.empty() && !m_actions.GetSelected (step.savedDir, HostFileNaming::Style::AppleSingle).Succeeded())
                {
                    step.savedDir.clear();
                }

                outcome = m_actions.DeleteSelected();

                if (outcome.Succeeded() && !step.savedDir.empty())
                {
                    PushUndo (std::move (step));
                }

                ReportOutcome (outcome, L"Delete");
                FillList();
            }

            break;
        }

        case CassoExplorerActions::Verb::Boot:
            ReportOutcome (m_actions.BootSelected(), L"Set startup program");
            FillList();
            break;

        case CassoExplorerActions::Verb::Rename:
            BeginRename();
            break;

        case CassoExplorerActions::Verb::InsertDrive1:
        case CassoExplorerActions::Verb::InsertDrive2:
            InsertIntoDrive (GetSelectedImagePath(), verb == CassoExplorerActions::Verb::InsertDrive1 ? 1 : 2);
            break;

        case CassoExplorerActions::Verb::OpenInNewCasso:
            OpenInNewCasso (GetSelectedImagePath());
            break;

        case CassoExplorerActions::Verb::NewDisk:
            newDisk = CassoExplorerNewDiskDialog::Ask (GetHwnd(), m_theme, false, [this] (const std::wstring & fileName)
            {
                return m_actions.IsNameTaken (m_browser.GetLocation().path, fileName);
            });

            if (newDisk.confirmed)
            {
                CreateDiskFromSelection (newDisk);
            }

            break;

        case CassoExplorerActions::Verb::NewFolder:
            //  An unused default name, open for renaming at once.
            newName = m_actions.GetNewFolderName();

            if (!m_browser.IsImageLocation())
            {
                hr = m_shellVerbs.CreateFolder (GetHwnd(), m_browser.GetLocation().path, newName);

                if (SUCCEEDED (hr))
                {
                    PushUndo (UndoStep { UndoStep::Kind::HostNewFolder, Location(), { (std::filesystem::path (m_browser.GetLocation().path) / newName).wstring() } });
                }

                RefreshAfterHostChange();
            }
            else
            {
                outcome = m_actions.CreateFolder (newName);

                if (outcome.Succeeded())
                {
                    PushUndo (UndoStep { UndoStep::Kind::ImageNewFolder, m_browser.GetLocation(), {}, std::wstring(), newName });
                }

                ReportOutcome (outcome, L"New folder");
                FillList();
            }

            SelectRowNamed (newName);
            BeginRename();
            break;

        case CassoExplorerActions::Verb::Format:
            newDisk = CassoExplorerNewDiskDialog::Ask (GetHwnd(), m_theme, true, {}, ChooseFormatDefault (m_actions.GetFormatTarget()),
                                                       std::filesystem::path (m_actions.GetFormatTarget()).filename().wstring());

            if (newDisk.confirmed)
            {
                answer = DxuiMessageBox (GetHwnd(), m_theme,
                                         (L"Format " + CassoExplorerActions::GetLeafName (m_actions.GetFormatTarget())
                                          + L"? Everything on it will be erased.").c_str(),
                                         L"Format", MB_YESNO | MB_ICONWARNING);

                if (answer == IDYES)
                {
                    ReportOutcome (m_actions.FormatImage (newDisk.request), L"Format");
                    FillList();
                }
            }

            break;

        case CassoExplorerActions::Verb::Refresh:
            Dispatch (CassoExplorerCommands::kRefresh);
            break;

        default:
            break;
    }

    IGNORE_RETURN_VALUE (hr, S_OK);
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ReportOutcome
//
//  A failure shows what the runner said; a success is quiet, since the list
//  already shows the result.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ReportOutcome (const CassoExplorerActions::Outcome & outcome, const wchar_t * verbName)
{
    if (outcome.Succeeded())
    {
        m_status->SetText (kStatusDetail, std::format (L"{}: {} file(s)", verbName, outcome.written));
        return;
    }

    ShowMessage (outcome.message.empty() ? std::wstring (verbName) + L" did not complete." : outcome.message, MB_ICONERROR);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ShowMessage
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ShowMessage (const std::wstring & text, UINT icon)
{
    int  result = DxuiMessageBox (GetHwnd(), m_theme, text.c_str(), CassoExplorerShell::kAppName, MB_OK | icon);



    IGNORE_RETURN_VALUE (result, IDOK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::InsertIntoDrive
//
//  The Casso that launched this browser when it is still there, otherwise
//  any running Casso, otherwise a new one with the disk in drive 1. The
//  answer arrives later as a reply message.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::InsertIntoDrive (const std::wstring & imagePath, int drive)
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
//  CassoExplorerWindow::OpenInNewCasso
//
//  The folder is recorded here, since the new Casso learns of the disk from
//  its command line rather than from a hand-off it would record itself.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::OpenInNewCasso (const std::wstring & imagePath)
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
//  CassoExplorerWindow::OpenPathInNewTab
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::OpenPathInNewTab (const std::wstring & path)
{
    std::wstring  full       (MAX_PATH, L'\0');
    DWORD         length     = GetFullPathNameW (path.c_str(), (DWORD) full.size(), full.data(), nullptr);
    DWORD         attributes = INVALID_FILE_ATTRIBUTES;



    //  The Recycle Bin, by its name or the shell's, in the tab that already
    //  shows it if there is one.
    if (_wcsicmp (path.c_str(), TreeModel::GetRootLabel (Location::kRecycleBinId).c_str()) == 0 || _wcsicmp (path.c_str(), L"shell:RecycleBinFolder") == 0)
    {
        for (size_t tab = 0; tab < m_browser.GetBrowserModel().GetTabCount(); tab++)
        {
            if (m_browser.GetBrowserModel().GetTab (tab).location.kind == Location::Kind::RecycleBin)
            {
                SwitchToTab (tab);
                return;
            }
        }

        m_browser.OpenInNewTab (Location::MakeRecycleBin());
        return;
    }

    //  Any other folder by the shell's name for it, such as shell:Libraries.
    if (Location::IsShellName (path))
    {
        Location  shellFolder = Location::MakeShellFolder (path, std::wstring());
        HRESULT   hr          = m_shellVerbs.GetShellItemName (path, shellFolder.label);

        IGNORE_RETURN_VALUE (hr, S_OK);
        m_browser.OpenInNewTab (shellFolder);
        return;
    }

    if (length >= full.size())
    {
        full.resize (length);
        length = GetFullPathNameW (path.c_str(), (DWORD) full.size(), full.data(), nullptr);
    }

    if (length == 0 || length >= full.size())
    {
        return;
    }

    full.resize (length);
    attributes = GetFileAttributesW (full.c_str());

    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        return;
    }

    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {
        m_browser.OpenInNewTab (Location::MakeHostFolder (full));
    }
    else if (TreeModel::IsSupportedImage (full))
    {
        m_browser.OpenInNewTab (Location::MakeDiskImage (full));
    }
    else
    {
        m_browser.OpenInNewTab (Location::MakeHostFolder (CassoExplorerBrowser::GetParentFolder (full)));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::OnCopyData
//
//  A reply is queued and shown after the send returns: Casso is blocked in
//  its send until this handler does, and a dialog opened here would hold it
//  there until the send timed out.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult CassoExplorerWindow::OnCopyData (WPARAM sender, LPARAM data)
{
    const COPYDATASTRUCT *     copy  = (const COPYDATASTRUCT *) data;
    Win32IntentChannel::Reply  reply;
    bool                       ours  = copy != nullptr && copy->dwData == Win32IntentChannel::GetReplyMessageId();



    UNREFERENCED_PARAMETER (sender);

    if (copy != nullptr && copy->dwData == kOpenPathCopyId && copy->lpData != nullptr)
    {
        OpenPathInNewTab (std::wstring ((const wchar_t *) copy->lpData, copy->cbData / sizeof (wchar_t)));
        FillList();

        return DxuiMessageResult::Handled;
    }

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
//  CassoExplorerWindow::OnAppMessage
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult CassoExplorerWindow::OnAppMessage (UINT msg, WPARAM wParam, LPARAM lParam)
{
    std::vector<Win32IntentChannel::Reply>  replies;



    UNREFERENCED_PARAMETER (lParam);

    //  A change arrived. Restarting the timer rather than re-reading now is
    //  what collapses a burst: a copy of a hundred files re-reads once, when
    //  the copying stops. Only for so long, though: a folder that never stops
    //  changing (a log being written, say) would hold off every re-read.
    if (msg == kFolderChangedMessage)
    {
        ULONGLONG  now = GetTickCount64();

        if (m_folderFirstChangeMs == 0)
        {
            m_folderFirstChangeMs = now;
        }

        if (now - m_folderFirstChangeMs < kFolderMaxSettleMs)
        {
            SetTimer (GetHwnd(), kFolderTimerId, kFolderSettleMs, nullptr);
        }

        return DxuiMessageResult::Handled;
    }

    //  The bin's changes come in bursts, as a folder's do, and matter only
    //  while it is shown.
    if (msg == kRecycleBinMessage)
    {
        if (m_browser.GetLocation().kind == Location::Kind::RecycleBin)
        {
            SetTimer (GetHwnd(), kRecycleBinTimerId, kFolderSettleMs, nullptr);
        }

        return DxuiMessageResult::Handled;
    }

    //  A shell folder's listing is in: the list shows it if it is still the
    //  folder shown, and the tree fills the node that asked for it.
    if (msg == kShellListedMessage)
    {
        for (const std::wstring & key : m_browser.GetShellListings().TakeArrivedKeys())
        {
            OnShellListed (key);
        }

        return DxuiMessageResult::Handled;
    }

    if (msg == kDropMenuMessage)
    {
        ShowDropMenu();

        return DxuiMessageResult::Handled;
    }

    if (msg == kInfoTipMessage)
    {
        RefreshListTip();

        return DxuiMessageResult::Handled;
    }

    if (msg == kIconsLoadedMessage)
    {
        if (m_listIcons.TakeLoaded())
        {
            RefreshListIcons();
        }

        return DxuiMessageResult::Handled;
    }

    if (msg == kRunCommandMessage)
    {
        if (IsEnabled ((int) wParam))
        {
            Dispatch ((int) wParam);
        }

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
                m_status->SetText (kStatusDetail, L"Inserted into Casso");
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
//  CassoExplorerWindow::ShowTreeContextMenu
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ShowTreeContextMenu (int x, int y, const std::wstring & id)
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
        m_menuCommands.back()->menuGlyph = s_kpszMdl2OpenInNewWindow;

        if (location.kind == Location::Kind::HostFolder)
        {
            AddPinMenuCommand (items, location.path);
        }
    }

    if (hasFolder && m_browser.CanRemoveFromCasso (id))
    {
        AddMenuCommand (items, L"&Remove from Casso", [this, folder]() { ChangeKnownFolder (folder, false); });
    }
    else if (hasFolder && m_browser.CanAddToCasso (id))
    {
        AddMenuCommand (items, L"&Add to Casso", [this, folder]() { ChangeKnownFolder (folder, true); });
    }

    //  The Recycle Bin's node empties it, as Explorer's does.
    if (hasLocation && location.kind == Location::Kind::RecycleBin)
    {
        AddMenuCommand (items, GetVerbLabel (CassoExplorerActions::Verb::EmptyRecycleBin), [this]()
        {
            HRESULT  hr = m_shellVerbs.EmptyRecycleBin (GetHwnd());

            IGNORE_RETURN_VALUE (hr, S_OK);
            RefreshAfterHostChange();
        });
        m_menuCommands.back()->menuSvg = GetMenuSvg (GetVerbMenuSvgName (CassoExplorerActions::Verb::EmptyRecycleBin));
    }
    else if (hasLocation && location.kind != Location::Kind::Root)
    {
        items.push_back (DxuiPopupMenuItem::ForSeparator());
        AddMenuCommand (items, L"Copy as &path", [this, location]()
        {
            DxuiClipboard::SetText (GetHwnd(), L"\"" + BrowserModel::FormatAddress (location) + L"\"");
        });
        AddMenuCommand (items, L"P&roperties", [this, location]() { ShowLocationProperties (location); });
        m_menuCommands.back()->menuGlyph = s_kpszMdl2Repair;
        m_menuCommands.back()->menuSvg   = GetMenuSvg (L"windows.properties");
    }

    if (!items.empty())
    {
        DxuiContextMenu::Show (*GetPopupHost(), x, y, std::move (items));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ShowTreeEmptyMenu
//
//  The pane's own options, as Explorer's menu below its last node has them,
//  each kept across runs.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ShowTreeEmptyMenu (int x, int y)
{
    std::vector<DxuiPopupMenuItem>  items;



    m_menuCommands.clear();

    AddNavPaneToggle (items, L"Show This &PC",          &CassoExplorerPrefs::navShowThisPc);
    AddNavPaneToggle (items, L"Show &Network",          &CassoExplorerPrefs::navShowNetwork);
    AddNavPaneToggle (items, L"Show &libraries",        &CassoExplorerPrefs::navShowLibraries);
    AddNavPaneToggle (items, L"Show &all folders",      &CassoExplorerPrefs::navShowAllFolders);
    AddNavPaneToggle (items, L"&Expand to open folder", &CassoExplorerPrefs::navExpandToCurrent);

    DxuiContextMenu::Show (*GetPopupHost(), x, y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::AddNavPaneToggle
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::AddNavPaneToggle (std::vector<DxuiPopupMenuItem> & items, const wchar_t * label, CassoExplorerPrefs::NavOption option)
{
    //  The first change parts this option from File Explorer's for good.
    AddMenuCommand (items, label, [this, option]()
    {
        m_prefs.*option = !IsNavOptionOn (option);
        ApplyNavPaneOptions();
        RefreshTree();

        if (IsNavOptionOn (&CassoExplorerPrefs::navExpandToCurrent))
        {
            RevealLocationInTree();
        }
    });
    m_menuCommands.back()->isChecked = [this, option]() { return IsNavOptionOn (option); };
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::IsNavOptionOn
//
//  Casso Explorer's own value where the user set one; otherwise File
//  Explorer's, read from the shell for the roots and from its settings for
//  the other two.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::IsNavOptionOn (CassoExplorerPrefs::NavOption option) const
{
    const TreeModel &  tree = m_browser.GetTreeModel();



    if (option == &CassoExplorerPrefs::navShowThisPc)
    {
        return tree.IsNavRootShown (TreeModel::NavRoot::ThisPc);
    }

    if (option == &CassoExplorerPrefs::navShowNetwork)
    {
        return tree.IsNavRootShown (TreeModel::NavRoot::Network);
    }

    if (option == &CassoExplorerPrefs::navShowLibraries)
    {
        return tree.IsNavRootShown (TreeModel::NavRoot::Libraries);
    }

    if (option == &CassoExplorerPrefs::navShowAllFolders)
    {
        return m_prefs.navShowAllFolders.value_or (m_explorerOptions.paneShowsAllFolders);
    }

    return m_prefs.navExpandToCurrent.value_or (m_explorerOptions.paneExpandsToCurrent);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ApplyNavPaneOptions
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ApplyNavPaneOptions()
{
    TreeModel::NavPaneOptions  options;



    options.showThisPc     = m_prefs.navShowThisPc;
    options.showNetwork    = m_prefs.navShowNetwork;
    options.showLibraries  = m_prefs.navShowLibraries;
    options.showAllFolders = IsNavOptionOn (&CassoExplorerPrefs::navShowAllFolders);

    m_browser.GetTreeModel().SetNavPaneOptions (options);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::AddPinMenuCommand
//
//  The shell carries the change out, so File Explorer's Quick access changes
//  with Casso Explorer's, and the pane reads the list again after it.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::AddPinMenuCommand (std::vector<DxuiPopupMenuItem> & items, const std::wstring & folder)
{
    bool  pinned = m_browser.IsPinnedToQuickAccess (folder);



    AddMenuCommand (items, pinned ? L"Unpin from &Quick access" : L"Pin to &Quick access", [this, folder, pinned]()
    {
        HRESULT  hr = m_shellVerbs.SetPinnedToQuickAccess (GetHwnd(), folder, !pinned);

        IGNORE_RETURN_VALUE (hr, S_OK);
        m_browser.RefreshShellRoots();
        RefreshTree();
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::OnShellListed
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::OnShellListed (const std::wstring & key)
{
    std::wstring  listKey = CassoExplorerBrowser::s_kListKey;
    std::wstring  treeKey = CassoExplorerBrowser::s_kTreeKey;
    std::wstring  nodeId;
    HRESULT       hr      = S_OK;



    if (key.starts_with (listKey))
    {
        if (m_browser.GetLocation().kind == Location::Kind::ShellFolder && m_browser.GetLocation().path == key.substr (listKey.size()))
        {
            hr = m_browser.Refresh();
            IGNORE_RETURN_VALUE (hr, S_OK);
            FillList();
        }
    }
    else if (key.starts_with (treeKey))
    {
        nodeId = std::wstring (TreeModel::kShellRootTag) + key.substr (treeKey.size());

        if (m_tree->FindRowById (nodeId) >= 0)
        {
            m_browser.GetTreeModel().Invalidate (nodeId);
            m_tree->ReplaceChildren (nodeId, m_browser.GetTreeChildren (nodeId));
        }
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ShowTabContextMenu
//
//  Explorer's tab menu. The last tab never closes, and the last tab has no
//  tabs to its right, so those rows are disabled rather than doing nothing.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ShowTabContextMenu (int x, int y, int index)
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
//  CassoExplorerWindow::AddMenuCommand
//
//  One row of a context menu, its command kept alive until the next menu.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::AddMenuCommand (std::vector<DxuiPopupMenuItem> & items, const wchar_t * label, std::function<void()> dispatch, const wchar_t * accelerator,
                                          const wchar_t * menuGlyph)
{
    std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();



    command->label       = label;
    command->accelerator = accelerator;
    command->menuGlyph   = menuGlyph;
    command->dispatch    = std::move (dispatch);

    items.push_back (DxuiPopupMenuItem::ForCommand (command));
    m_menuCommands.push_back (std::move (command));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::AddCopyAsMenu
//
//  The button row's Copy puts the selected files on the clipboard as real
//  files, in the style the Options dialog sets, and Copy as offers each style
//  for this copy alone. A paste into Explorer then writes them.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::AddCopyAsMenu (std::vector<DxuiPopupMenuItem> & items)
{
    static constexpr struct { const wchar_t * label; HostFileNaming::Style style; }  kStyles[] =
    {
        { L"&Descriptive",  HostFileNaming::Style::Descriptive },
        { L"&CiderPress",   HostFileNaming::Style::CiderPress  },
        { L"&AppleSingle",  HostFileNaming::Style::AppleSingle },
    };
    std::vector<DxuiPopupMenuItem>  choices;
    std::shared_ptr<DxuiCommand>    parent = std::make_shared<DxuiCommand>();



    for (const auto & row : kStyles)
    {
        std::shared_ptr<DxuiCommand>  child = std::make_shared<DxuiCommand>();
        HostFileNaming::Style         style = row.style;

        child->label    = row.label;
        child->dispatch = [this, style]() { CopyEntriesToClipboard (style); };

        choices.push_back (DxuiPopupMenuItem::ForCommand (child));
        m_menuCommands.push_back (std::move (child));
    }

    parent->label = L"Cop&y as";
    parent->menuGlyph = s_kpszMdl2Copy;
    parent->menuSvg   = GetMenuSvg (L"windows.copy");
    items.push_back (DxuiPopupMenuItem::ForSubmenu (parent, std::move (choices)));
    m_menuCommands.push_back (std::move (parent));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::CopyEntriesToClipboard
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::CopyEntriesToClipboard (HostFileNaming::Style style)
{
    HRESULT  hr = CassoExplorerDragOut::CopyToClipboard (m_browser, style);



    if (FAILED (hr))
    {
        ShowMessage (L"The files could not be copied.", MB_ICONWARNING);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::CopySelectedPaths
//
//  Each path in quotes on its own line, as Explorer's Copy as path gives them.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::CopySelectedPaths()
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
//  CassoExplorerWindow::OpenSelectedEntries
//
//  Files inside an image open in the programs Windows has for them, from
//  copies in a folder of their own, as Explorer opens a file inside a zip.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::OpenSelectedEntries()
{
    std::vector<std::wstring>  paths;
    HRESULT                    hr    = CassoExplorerDragOut::WriteToTempFolder (m_browser, GetNamingStyle(), paths);



    if (FAILED (hr))
    {
        ShowMessage (L"The files could not be read out of the image to open.", MB_ICONWARNING);
        return;
    }

    for (const std::wstring & path : paths)
    {
        hr = m_shellVerbs.Open (GetHwnd(), path);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::OpenEachSelected
//
//  Several items open each on its own, as Explorer's do: a folder or an image
//  in a tab of its own, a file in its program. All are read before the first
//  tab opens, since a new tab lists something else.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::OpenEachSelected()
{
    HRESULT                    hr = S_OK;
    std::vector<Location>      places;
    std::vector<std::wstring>  files;
    Location                   place;
    std::wstring               path;



    for (int row : m_browser.GetSelectedRows())
    {
        if (m_browser.TryGetRowLocation (row, place))
        {
            places.push_back (place);
        }
        else if (!m_browser.IsImageLocation() && m_browser.TryGetRowPath (row, path))
        {
            files.push_back (path);
        }
    }

    for (const std::wstring & file : files)
    {
        hr = m_shellVerbs.Open (GetHwnd(), file);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    for (const Location & each : places)
    {
        m_browser.OpenInNewTab (each);
    }

    if (!places.empty())
    {
        FillList();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ShowRowProperties
//
//  A host item, a drive among them, has Windows' own Properties sheet. An
//  entry inside an image has none, so its catalog details are shown instead.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ShowRowProperties (int row)
{
    Location                           location = m_browser.GetLocation();
    std::vector<DxuiListView::Column>  columns  = CassoExplorerBrowser::GetColumns();
    std::vector<DxuiListView::Cell>    cells;
    std::wstring                       path;
    std::wstring                       text;
    size_t                             i        = 0;
    FileEntry                          entry;



    if (!m_browser.TryGetRowPath (row, path))
    {
        return;
    }

    if (location.kind == Location::Kind::HostFolder || location.kind == Location::Kind::Root)
    {
        ShowHostProperties (path);
        return;
    }

    //  An entry inside an image: Explorer's General tab, from its catalog.
    if (m_browser.TryGetRowEntry (row, entry))
    {
        CassoExplorerPropertiesDialog::Show (GetHwnd(), m_theme, m_browser.GetRows()[(size_t) row].name,
                                            CassoExplorerProperties::DescribeEntry (entry, m_browser.GetVolumeKind(), BrowserModel::FormatAddress (location)));
        return;
    }

    cells = CassoExplorerBrowser::ToCells (m_browser.GetRows()[(size_t) row], location);
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
//  CassoExplorerWindow::ShowLocationProperties
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ShowLocationProperties (const Location & location)
{
    //  Home and the roots have no host path to show properties for.
    if (location.kind == Location::Kind::None || location.kind == Location::Kind::Root)
    {
        return;
    }

    if (location.kind == Location::Kind::DiskDirectory)
    {
        ShowMessage (BrowserModel::FormatAddress (location), MB_ICONINFORMATION);
        return;
    }

    ShowHostProperties (location.path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ShowHostProperties
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ShowHostProperties (const std::wstring & path)
{
    BOOL  shown = SHObjectProperties (GetHwnd(), SHOP_FILEPATH, path.c_str(), nullptr);



    IGNORE_RETURN_VALUE (shown, TRUE);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ChangeKnownFolder
//
//  The shared file is changed under its lock, then read back whole, so a
//  folder Casso recorded in the meantime shows too.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ChangeKnownFolder (const std::wstring & folder, bool add)
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
    m_browser.GetTreeModel().Invalidate (TreeModel::kCassoRootId);
    m_tree->ReplaceChildren (TreeModel::kCassoRootId, m_browser.GetTreeChildren (TreeModel::kCassoRootId));
    UpdateWatchedFolders();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::RefreshTree
//
//  Reads the whole tree again in place, as Explorer's F5 does: the roots, then
//  each open node in row order, which puts every parent ahead of its children.
//  What is open, highlighted and on screen stays, by the tree's own merge.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::RefreshTree()
{
    std::vector<std::wstring>  open;
    std::vector<DxuiTreeNode>  roots;
    int                        row = 0;



    for (row = 0; row < m_tree->GetVisibleCount(); row++)
    {
        const DxuiTreeNode *  node = m_tree->GetNodeAt (row);

        if (node != nullptr && node->expanded)
        {
            open.push_back (node->id);
        }
    }

    m_browser.GetTreeModel().InvalidateAll();
    m_browser.GetTreeRoots (roots);
    m_tree->ReplaceRoots (std::move (roots));

    for (const std::wstring & id : open)
    {
        if (m_tree->FindRowById (id) >= 0)
        {
            m_tree->ReplaceChildren (id, m_browser.GetTreeChildren (id));
        }
    }

    UpdateWatchedFolders();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::OnActivateApp
//
//  Coming back to the window reads again the settings that change without a
//  message to say so: the Windows colors, when the theme follows them, and
//  Explorer's folder options. Each is applied only when it changed.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult CassoExplorerWindow::OnActivateApp (bool active)
{
    HRESULT  hr = S_OK;



    //  The address's suggestions go when the app does; they hold no capture
    //  to see the click that took it elsewhere.
    if (!active)
    {
        m_suggest.Close();

        for (DxuiToolbar * bar : { m_commandBar, m_toolbar, m_previewToolbar })
        {
            bar->CloseMenu();
        }
    }

    //  Applying a theme repaints the whole window and its title bar, so it is
    //  done only when the colors Windows reports are different.
    if (active && m_menuBar != nullptr && !IsChecked (CassoExplorerCommands::kThemeLight) && !IsChecked (CassoExplorerCommands::kThemeDark))
    {
        DxuiWindowsThemeColors &                    colors     = DxuiWindowsThemeColors::Instance();
        bool                                        wasDark    = colors.IsDarkMode();
        DxuiWindowsThemeColors::SystemColors        wasSystem  = colors.GetSystemColors();

        colors.Refresh();

        if (colors.IsDarkMode() != wasDark || colors.GetSystemColors() != wasSystem)
        {
            ApplyTheme();
        }
    }

    //  Explorer's folder options send no message either, so they too are read
    //  again, and the folders relisted only when one changed.
    if (active && m_list != nullptr && ReadFolderOptions())
    {
        hr = m_browser.Reload (true);
        IGNORE_RETURN_VALUE (hr, S_OK);
        FillList();
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
//  CassoExplorerWindow::FillAddress
//
//  The segments and the path of where the active tab is.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::FillAddress()
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
//  CassoExplorerWindow::SubmitAddress
//
//  A path that goes nowhere says so and leaves the text as typed.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::SubmitAddress (const std::wstring & text)
{
    if (m_browser.NavigateToAddress (text))
    {
        SetFocusPane (Pane::List);
        FillList();

        //  The typed path joins the history now, not only at a clean exit, and
        //  a folder joins Explorer's list too. A path into a disk image stays
        //  in Casso Explorer's own: Explorer would open the image file.
        SaveSession();

        if (m_browser.GetLocation().kind == Location::Kind::HostFolder)
        {
            WriteExplorerTypedPath (CassoExplorerBrowser::RootOnSystemDrive (text));
        }
    }
    else
    {
        ShowMessage (L"Casso Explorer can't find \"" + text + L"\". Check the path and try again.", MB_ICONWARNING);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ShowAddressMenu
//
//  The folders and images inside a segment's location, hung from the
//  separator after it, as Explorer's address bar lists a folder's children.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ShowAddressMenu (int index, const RECT & anchor)
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
//  CassoExplorerWindow::UpdateWatchedFolders
//
//  The folder the list is showing -- for an image, the folder holding it --
//  and every host folder the tree has open, since those are the folders whose
//  contents are on screen.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::UpdateWatchedFolders()
{
    std::vector<std::wstring>  folders;
    Location                   showing = m_browser.GetLocation();
    int                        row     = 0;



    if (m_folderWatch == nullptr)
    {
        return;
    }

    //  The shown folder's parent too: deleting the folder is a change there.
    if (showing.kind == Location::Kind::HostFolder && !showing.path.empty())
    {
        folders.push_back (showing.path);

        if (showing.path.size() > 3)
        {
            folders.push_back (CassoExplorerBrowser::GetParentFolder (showing.path));
        }
    }
    else if (!showing.path.empty())
    {
        folders.push_back (CassoExplorerBrowser::GetParentFolder (showing.path));
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
//  CassoExplorerWindow::IsSameFolder
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::IsSameFolder (const std::wstring & a, const std::wstring & b)
{
    return CompareStringOrdinal (a.c_str(), (int) a.size(), b.c_str(), (int) b.size(), TRUE) == CSTR_EQUAL;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::IsShownFolderIn
//
//  Whether the list shows one of the folders: the folder itself, or, inside a
//  disk image, the folder that holds the image.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::IsShownFolderIn (const std::vector<std::wstring> & folders) const
{
    Location      showing = m_browser.GetLocation();
    std::wstring  shown;



    if (showing.path.empty())
    {
        return false;
    }

    shown = (showing.kind == Location::Kind::HostFolder) ? showing.path : CassoExplorerBrowser::GetParentFolder (showing.path);

    return std::any_of (folders.begin(), folders.end(), [&] (const std::wstring & folder) { return IsSameFolder (folder, shown); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::RefreshOpenTreeFolders
//
//  Reads again, in place, what the tree shows under each open node the changed
//  folders hold: a folder's subfolders, or a disk image's. Returns whether any
//  node was read again.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::RefreshOpenTreeFolders (const std::vector<std::wstring> & folders)
{
    std::vector<std::wstring>  stale;
    int                        row = 0;



    for (row = 0; row < m_tree->GetVisibleCount(); row++)
    {
        const DxuiTreeNode *  node = m_tree->GetNodeAt (row);
        Location              at;

        //  A closed folder is read again too, so its first subfolder gives it
        //  a chevron.
        if (node != nullptr
            && m_browser.TryGetNodeLocation (node->id, at) && !at.path.empty()
            && (node->expanded || at.kind == Location::Kind::HostFolder))
        {
            std::wstring  holder = (at.kind == Location::Kind::HostFolder) ? at.path : CassoExplorerBrowser::GetParentFolder (at.path);

            if (std::any_of (folders.begin(), folders.end(), [&] (const std::wstring & folder) { return IsSameFolder (folder, holder); }))
            {
                stale.push_back (node->id);
            }
        }
    }

    //  In row order, so a parent goes first; a node its parent's change took
    //  away has no row by the time its turn comes, and is passed over.
    for (const std::wstring & id : stale)
    {
        if (m_tree->FindRowById (id) >= 0)
        {
            m_browser.GetTreeModel().Invalidate (id);
            m_tree->ReplaceChildren (id, m_browser.GetTreeChildren (id));
        }
    }

    return !stale.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::RefreshChangedFolders
//
//  Runs once a burst has settled. The same files stay selected, and the view
//  stays on the same files, by the rules RefreshAnchor sets out: a file added
//  or removed elsewhere in the folder is no reason to lose the selection or to
//  move what is on screen.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::RefreshChangedFolders()
{
    HRESULT                    hr = S_OK;
    std::vector<std::wstring>  changed;
    std::vector<std::wstring>  keys;
    RefreshAnchor::Before      before;
    RefreshAnchor::After       after;



    if (m_folderWatch == nullptr)
    {
        return;
    }

    //  Not while a rename is open or a button is down: the rows under either
    //  would change. The changes wait, and the timer tries again.
    if (m_renameRow >= 0 || m_dragArmed || m_list->IsInteracting() || m_tree->IsInteracting())
    {
        SetTimer (GetHwnd(), kFolderTimerId, kFolderSettleMs, nullptr);
        return;
    }

    if (!m_folderWatch->TakeChanged (changed))
    {
        return;
    }


    //  A folder written to often -- a disk image Casso has mounted, say --
    //  must not cost the tree or the list anything when neither shows it.
    if (RefreshOpenTreeFolders (changed))
    {
        UpdateWatchedFolders();
    }

    //  The folder shown was deleted: the change is its parent's, so the list
    //  moves up, as Explorer's does, rather than staying on what is gone.
    if (m_browser.GetBrowserModel().HasTabs() && m_browser.LeaveMissingLocation())
    {
        FillList();
        return;
    }

    if (!m_browser.GetBrowserModel().HasTabs() || !IsShownFolderIn (changed))
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
//  CassoExplorerWindow::ApplyStoredColumnWidths
//
//  The widths from the last run, where there are any. A stored zero means the
//  column was never given a width of its own and still fits itself.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ApplyStoredColumnWidths()
{
    ApplyColumnWidths (m_prefs.columnWidthsDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ApplyColumnWidths
//
//  A column with no width here goes back to fitting itself, so one folder's
//  widths do not stay on in the next.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ApplyColumnWidths (const std::vector<int> & widthsDip)
{
    size_t  c = 0;



    for (c = 0; c < m_list->GetColumnCount(); c++)
    {
        int  dip = (c < widthsDip.size()) ? widthsDip[c] : 0;

        //  At the DPI the list holds now, which it rescales from when its
        //  DPI changes; the window's may not have caught up yet.
        m_list->SetColumnOverrideWidthPx (c, (dip > 0) ? MulDiv (dip, (int) m_list->GetDpi(), (int) DxuiDpiScaler::kBaseDpi) : -1);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::RememberFolderColumns
//
//  The order and the choice of columns the folder shows now, kept for it.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::RememberFolderColumns()
{
    FolderViewEntry  entry;



    if (m_listViewKey.empty())
    {
        return;
    }

    entry               = m_prefs.folderViews.GetEntry (m_listViewKey, FolderViews::FolderType::Generic);
    entry.view          = m_listView;
    entry.columnOrder   = m_folderColumnOrder;
    entry.columnsChosen = true;
    entry.hiddenColumns.clear();

    for (size_t column = 0; column < m_listColumnChosen.size(); column++)
    {
        if (!m_listColumnChosen[column])
        {
            entry.hiddenColumns.push_back ((int) column);
        }
    }

    m_prefs.folderViews.Remember (entry);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::RememberFolderColumnWidths
//
//  The folder starts from the widths it showed, so the columns the user did
//  not touch keep what they had.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::RememberFolderColumnWidths (int column, int widthDip)
{
    FolderViewEntry  entry;



    if (m_listViewKey.empty() || column < 0)
    {
        return;
    }

    entry      = m_prefs.folderViews.GetEntry (m_listViewKey, FolderViews::FolderType::Generic);
    entry.view = m_listView;

    if (entry.columnWidthsDip.empty())
    {
        entry.columnWidthsDip = m_prefs.columnWidthsDip;
    }

    if ((size_t) column >= entry.columnWidthsDip.size())
    {
        entry.columnWidthsDip.resize ((size_t) column + 1, 0);
    }

    entry.columnWidthsDip[(size_t) column] = widthDip;

    m_prefs.folderViews.Remember (entry);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::GetEmptyLocationMessage
//
//  What an empty list says, in the words the holding file system uses. ProDOS
//  calls them directories, which is what the command line's mkdir and rmdir
//  make, and Windows calls them folders. An empty disk image is worth telling
//  apart from either: it holds nothing at all, where an empty folder says
//  nothing about the rest of the disk.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerWindow::GetEmptyLocationMessage (Location::Kind kind)
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
//  CassoExplorerWindow::ShowAddressHistoryMenu
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

void CassoExplorerWindow::ShowAddressHistoryMenu (const RECT & anchor)
{
    UNREFERENCED_PARAMETER (anchor);

    //  The caret opens the history and, pressed again, closes it.
    if (m_suggest.IsOpen())
    {
        m_suggest.Close();
        return;
    }

    ShowAddressSuggestions (std::wstring());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ChooseFormatDefault
//
//  The file system the image holds now, when it holds one; otherwise what
//  its size and kind allow. DOS 3.3 fits only a 140K floppy, so anything
//  larger -- an 800K disk, a hard disk image -- starts at ProDOS, and so does
//  a ProDOS-order image.
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerWindow::ChooseFormatDefault (const std::wstring & image)
{
    VolumeListing              listing;
    VolumeKind                 kind      = VolumeKind::Unknown;
    WIN32_FILE_ATTRIBUTE_DATA  data      = {};
    std::wstring               extension = std::filesystem::path (image).extension().wstring();
    uint64_t                   size      = 0;



    if (m_browser.GetOperations().List (TextEncoding::WideToNarrow (image), listing, kind).Succeeded())
    {
        if (kind == VolumeKind::ProDos)
        {
            return CassoExplorerNewDiskChoices::kFormatProDos;
        }

        if (kind == VolumeKind::Dos33)
        {
            return CassoExplorerNewDiskChoices::kFormatDos33;
        }
    }

    if (GetFileAttributesExW (image.c_str(), GetFileExInfoStandard, &data))
    {
        size = ((uint64_t) data.nFileSizeHigh << 32) | data.nFileSizeLow;
    }

    for (const wchar_t * prodos : { L".po", L".2mg", L".2img", L".hdv" })
    {
        if (_wcsicmp (extension.c_str(), prodos) == 0)
        {
            return CassoExplorerNewDiskChoices::kFormatProDos;
        }
    }

    //  A nibble or WOZ image is larger than the sectors it holds; its size
    //  says nothing about the disk.
    if (size > kDos33ImageBytes && _wcsicmp (extension.c_str(), L".nib") != 0 && _wcsicmp (extension.c_str(), L".woz") != 0)
    {
        return CassoExplorerNewDiskChoices::kFormatProDos;
    }

    return CassoExplorerNewDiskChoices::kFormatDos33;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ShowAddressSuggestions
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ShowAddressSuggestions (const std::wstring & typed)
{
    m_suggest.Show (m_address->GetBounds(), typed.empty() ? GetAddressHistory() : GetCompletions (typed));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::OnAddressKey
//
//  While the list shows, Up and Down walk it, the box showing the row walked
//  to, or what was typed once the walk leaves the list; Escape closes it and
//  leaves the edit open. Enter goes to whatever the box shows.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::OnAddressKey (WPARAM vk)
{
    std::wstring  shown;



    if (!m_suggest.IsOpen())
    {
        return false;
    }

    if (vk == VK_DOWN || vk == VK_UP)
    {
        shown = m_suggest.MoveHighlight ((vk == VK_DOWN) ? 1 : -1);
        m_address->SetEditText (shown.empty() ? m_addressTyped : shown);
        Invalidate();
        return true;
    }

    if (vk == VK_ESCAPE)
    {
        m_suggest.Close();
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::GetAddressHistory
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> CassoExplorerWindow::GetAddressHistory() const
{
    return TypedPathHistory::Merge (ReadExplorerTypedPaths(), m_browser.GetTypedPaths().GetEntries());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::GetCompletions
//
//  What is in the folder the typed path has reached, whose names start with
//  what follows its last backslash, as Explorer lists them: folders and
//  files, hidden ones left out, sorted by name.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> CassoExplorerWindow::GetCompletions (const std::wstring & typed)
{
    std::vector<std::wstring>  out;
    std::wstring               rooted = CassoExplorerBrowser::RootOnSystemDrive (typed);
    size_t                     slash  = rooted.find_last_of (L'\\');
    std::wstring               folder;
    std::wstring               prefix;
    std::wstring               shownFolder;
    WIN32_FIND_DATAW           found  = {};
    HANDLE                     search = INVALID_HANDLE_VALUE;



    if (slash == std::wstring::npos)
    {
        return out;
    }

    folder      = rooted.substr (0, slash + 1);
    prefix      = rooted.substr (slash + 1);
    shownFolder = typed.substr (0, typed.find_last_of (L'\\') + 1);

    search = FindFirstFileExW ((folder + L"*").c_str(), FindExInfoBasic, &found, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);

    if (search == INVALID_HANDLE_VALUE)
    {
        return out;
    }

    do
    {
        std::wstring  name = found.cFileName;

        if (name == L"." || name == L".." || (found.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) != 0)
        {
            continue;
        }

        if (_wcsnicmp (name.c_str(), prefix.c_str(), prefix.size()) == 0)
        {
            out.push_back (shownFolder + name);
        }
    }
    while (out.size() < kMaxCompletions && FindNextFileW (search, &found));

    FindClose (search);

    std::sort (out.begin(), out.end(), [] (const std::wstring & a, const std::wstring & b) { return _wcsicmp (a.c_str(), b.c_str()) < 0; });

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::PushUndo
//
//  Kept to a few dozen steps, as an image deletion's copy is a folder on disk.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::PushUndo (UndoStep step)
{
    static constexpr size_t  s_kMaxSteps = 50;



    m_undo.push_back (std::move (step));

    if (m_undo.size() > s_kMaxSteps)
    {
        std::error_code  ignored;

        if (!m_undo.front().savedDir.empty())
        {
            std::filesystem::remove_all (m_undo.front().savedDir, ignored);
        }

        m_undo.erase (m_undo.begin());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::GetUndoLabel
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * CassoExplorerWindow::GetUndoLabel (UndoStep::Kind kind)
{
    switch (kind)
    {
        case UndoStep::Kind::Recycle:
        case UndoStep::Kind::ImageDelete:    return L"&Undo delete";
        case UndoStep::Kind::HostRename:
        case UndoStep::Kind::ImageRename:    return L"&Undo rename";
        case UndoStep::Kind::HostNewFolder:
        case UndoStep::Kind::ImageNewFolder: return L"&Undo new folder";
        case UndoStep::Kind::HostCopy:
        case UndoStep::Kind::ImagePut:       return L"&Undo copy";
        case UndoStep::Kind::HostMove:       return L"&Undo move";
        default:                             return L"&Undo";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::UndoLast
//
//  An image step goes back to the image it was made in first, since what it
//  puts back is selected by name in the list.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::UndoLast()
{
    HRESULT                    hr = S_OK;
    UndoStep                   step;
    std::vector<std::wstring>  saved;
    std::error_code            ignored;



    if (m_undo.empty())
    {
        MessageBeep (MB_OK);
        return;
    }

    step = std::move (m_undo.back());
    m_undo.pop_back();

    switch (step.kind)
    {
        case UndoStep::Kind::Recycle:
            hr = m_shellVerbs.RestoreRecycled (GetHwnd(), step.paths);
            m_restoreTicks = kRestoreRetries;
            SetTimer (GetHwnd(), kRestoreTimerId, 1000, nullptr);
            break;

        case UndoStep::Kind::HostRename:
            hr = m_shellVerbs.RenameItem (GetHwnd(), step.paths[0], step.oldName);
            break;

        case UndoStep::Kind::HostNewFolder:
        case UndoStep::Kind::HostCopy:
            hr = m_shellVerbs.Recycle (GetHwnd(), step.paths);
            break;

        case UndoStep::Kind::HostMove:
            hr = m_shellVerbs.MoveItemsTo (GetHwnd(), step.paths, step.sources);
            break;

        default:
            if (m_browser.GetLocation() != step.location)
            {
                m_browser.NavigateToLocation (step.location);
                FillList();
            }

            if (step.kind == UndoStep::Kind::ImageRename)
            {
                SelectRowNamed (step.newName);
                ReportOutcome (m_actions.RenameSelected (step.oldName), L"Undo rename");
            }
            else if (step.kind == UndoStep::Kind::ImageNewFolder)
            {
                SelectRowNamed (step.newName);
                ReportOutcome (m_actions.DeleteSelected(), L"Undo new folder");
            }
            else if (step.kind == UndoStep::Kind::ImagePut)
            {
                ReportOutcome (m_actions.DeleteEntries (TextEncoding::WideToNarrow (step.location.path), step.entries), L"Undo copy");
            }
            else
            {
                for (const std::filesystem::directory_entry & entry : std::filesystem::directory_iterator (step.savedDir, ignored))
                {
                    saved.push_back (entry.path().wstring());
                }

                ReportOutcome (m_actions.PutFiles (saved, MakeAddressPrompt()), L"Undo delete");
                std::filesystem::remove_all (step.savedDir, ignored);
            }

            FillList();
            return;
    }

    if (FAILED (hr))
    {
        ShowMessage (L"That could not be undone.", MB_ICONWARNING);
    }

    RefreshAfterHostChange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::PasteHere
//
//  The clipboard's files into the folder, with an undo that takes back what
//  the paste made: a copy goes to the Recycle Bin, a move goes back.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::PasteHere (const std::wstring & folder)
{
    IShellItemVerbs::PasteResult  pasted;
    HRESULT                       hr     = m_shellVerbs.PasteInto (GetHwnd(), folder, pasted);



    IGNORE_RETURN_VALUE (hr, S_OK);
    m_cutPaths.clear();

    if (!pasted.created.empty())
    {
        UndoStep  step;

        step.kind    = pasted.moved ? UndoStep::Kind::HostMove : UndoStep::Kind::HostCopy;
        step.paths   = pasted.created;
        step.sources = pasted.sources;

        PushUndo (std::move (step));
    }

    RefreshAfterHostChange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::PushPutUndo
//
//  A put or a copy into an image is undone by deleting what it made, in the
//  directory it went into.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::PushPutUndo (const CassoExplorerActions::Outcome & outcome, const std::wstring & image, const std::string & inner)
{
    UndoStep  step;



    if (outcome.created.empty())
    {
        return;
    }

    step.kind     = UndoStep::Kind::ImagePut;
    step.location = inner.empty() ? Location::MakeDiskImage (image) : Location::MakeDiskDirectory (image, inner);
    step.entries  = outcome.created;

    PushUndo (std::move (step));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::UpdateLabelTip
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::UpdateLabelTip (POINT point)
{
    int   row = -1;
    RECT  box = {};



    if (m_tree->IsVisible() && Contains (m_tree->GetBounds(), point))
    {
        row = m_tree->HitTestRow (point.x, point.y);
    }

    if (row < 0 || m_theme == nullptr || !m_tree->GetClippedLabelRect (row, box))
    {
        m_labelTip.Hide();
        return;
    }

    m_labelTip.Show (box, m_tree->GetLabelFill (row, *m_theme), [this, row] (IDxuiPainter & painter, IDxuiTextRenderer & text)
    {
        m_tree->PaintLabel (row, painter, text, *m_theme);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ShowAddressContextMenu
//
//  Explorer's: the location as text (both copies), editing it, and clearing
//  the typed history -- Explorer's own list too, since the two are shown as
//  one.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ShowAddressContextMenu (int x, int y)
{
    std::vector<DxuiPopupMenuItem>  items;
    Location                        location = m_browser.GetLocation();
    std::wstring                    address  = BrowserModel::FormatAddress (location);



    m_menuCommands.clear();

    //  Explorer's two both put the path on as text; neither puts the folder.
    AddMenuCommand (items, L"&Copy address", [this, address]() { DxuiClipboard::SetText (GetHwnd(), address); });

    AddMenuCommand (items, L"Copy address as &text", [this, address]() { DxuiClipboard::SetText (GetHwnd(), address); });

    AddMenuCommand (items, L"&Edit address", [this]()
    {
        SetFocusPane (Pane::Address);
        m_address->BeginEdit();
        Invalidate();
    });

    AddMenuCommand (items, L"&Delete history", [this]()
    {
        HKEY  key = nullptr;

        m_browser.GetTypedPaths().Clear();
        m_prefs.typedPaths.clear();

        if (RegOpenKeyExW (HKEY_CURRENT_USER, kExplorerTypedPathsKey, 0, KEY_ALL_ACCESS, &key) == ERROR_SUCCESS)
        {
            RegDeleteTreeW (key, nullptr);
            RegCloseKey (key);
        }
    });

    DxuiContextMenu::Show (*GetPopupHost(), x, y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ReadExplorerTypedPaths
//
//  Explorer's typed paths, url1 the newest.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> CassoExplorerWindow::ReadExplorerTypedPaths()
{
    std::vector<std::wstring>  out;
    HKEY                       key    = nullptr;
    LSTATUS                    status = RegOpenKeyExW (HKEY_CURRENT_USER, kExplorerTypedPathsKey, 0, KEY_READ, &key);
    size_t                     i      = 0;



    if (status != ERROR_SUCCESS)
    {
        return out;
    }

    for (i = 1; i <= TypedPathHistory::kMaxEntries; i++)
    {
        wchar_t  value[MAX_PATH * 2] = {};
        DWORD    bytes               = sizeof (value) - sizeof (wchar_t);
        DWORD    type                = 0;

        status = RegQueryValueExW (key, std::format (L"url{}", i).c_str(), nullptr, &type, (BYTE *) value, &bytes);

        if (status == ERROR_SUCCESS && type == REG_SZ && value[0] != L'\0')
        {
            out.push_back (value);
        }
    }

    RegCloseKey (key);

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::WriteExplorerTypedPath
//
//  Puts a folder at the top of Explorer's typed paths, as Explorer does when
//  one is typed there: a repeat moves up, and the oldest past the limit goes.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::WriteExplorerTypedPath (const std::wstring & path)
{
    std::vector<std::wstring>  paths  = ReadExplorerTypedPaths();
    HKEY                       key    = nullptr;
    LSTATUS                    status = ERROR_SUCCESS;
    size_t                     i      = 0;



    paths.erase (std::remove_if (paths.begin(), paths.end(),
                                 [&path] (const std::wstring & held) { return _wcsicmp (held.c_str(), path.c_str()) == 0; }),
                 paths.end());
    paths.insert (paths.begin(), path);

    if (paths.size() > TypedPathHistory::kMaxEntries)
    {
        paths.resize (TypedPathHistory::kMaxEntries);
    }

    status = RegCreateKeyExW (HKEY_CURRENT_USER, kExplorerTypedPathsKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);

    if (status != ERROR_SUCCESS)
    {
        return;
    }

    for (i = 0; i < paths.size(); i++)
    {
        status = RegSetValueExW (key, std::format (L"url{}", i + 1).c_str(), 0, REG_SZ,
                                 (const BYTE *) paths[i].c_str(), (DWORD) ((paths[i].size() + 1) * sizeof (wchar_t)));
        IGNORE_RETURN_VALUE (status, ERROR_SUCCESS);
    }

    RegCloseKey (key);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ShowAddressOverflowMenu
//
//  The segments collapsed behind the address bar's overflow button, nearest
//  first, as Explorer lists them.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ShowAddressOverflowMenu (const RECT & anchor)
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
//  CassoExplorerWindow::OnFindBoxKey
//
//  Enter searches, Escape empties the box and leaves a search's results, as
//  Explorer's box does; every other key edits the query.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::OnFindBoxKey (const DxuiKeyEvent & ev)
{
    bool  down = ev.kind == DxuiKeyEventKind::Down;



    if (down && ev.vk == VK_RETURN)
    {
        SearchLocation (m_findBox->GetText());
        return true;
    }

    if (down && ev.vk == VK_ESCAPE)
    {
        m_findBox->SetText (L"");

        if (SearchQuery::IsId (m_browser.GetLocation().path) && m_browser.GoBack())
        {
            FillList();
        }

        SetFocusPane (Pane::List);
        return true;
    }

    return m_findBox->OnKey (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::SearchLocation
//
//  The folder shown and everything below it; a search from a search's
//  results searches the same folder again.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::SearchLocation (const std::wstring & query)
{
    Location      at    = m_browser.GetLocation();
    std::wstring  scope;
    std::wstring  previous;



    if (query.empty())
    {
        return;
    }

    if (at.kind == Location::Kind::HostFolder)
    {
        scope = at.path;
    }
    else if (SearchQuery::IsId (at.path))
    {
        SearchQuery::TryParseId (at.path, scope, previous);
    }
    else if (at.kind == Location::Kind::ShellFolder && !at.path.empty() && !Location::IsShellName (at.path))
    {
        scope = at.path;
    }

    if (scope.empty())
    {
        ShowMessage (L"Search covers folders on a disk; this location has none.", MB_ICONINFORMATION);
        return;
    }

    m_browser.NavigateToLocation (Location::MakeShellFolder (SearchQuery::MakeId (scope, query), SearchQuery::GetLabel (scope)));
    FillList();
    SetFocusPane (Pane::List);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ShowAddressRootsMenu
//
//  The chevron after the location's icon: the desktop's roots, then the
//  folders on the user's desktop, each with its icon, as Explorer's has.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ShowAddressRootsMenu (const RECT & anchor)
{
    std::vector<DxuiPopupMenuItem>                 items;
    std::vector<IShellItemVerbs::ShellFolderItem>  folders;
    HRESULT                                        hr = m_shellVerbs.ListDesktopFolders (folders);



    IGNORE_RETURN_VALUE (hr, S_OK);
    m_menuCommands.clear();

    for (const IShellItemVerbs::ShellFolderItem & folder : folders)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();
        std::wstring                  target  = folder.path.empty() ? folder.id : folder.path;

        //  This PC and the Recycle Bin open as Casso Explorer's own, with
        //  their drives and their Restore.
        Location  own = folder.isRecycleBin ? Location::MakeRecycleBin()
                      : folder.isThisPc     ? Location::MakeRoot (TreeModel::kThisPcRootId)
                                            : Location();

        command->label     = EscapeMnemonics (folder.name);
        command->menuImage = m_shellIcons.GetForPath (target, true);
        command->dispatch  = [this, target, own]()
        {
            if (own.kind != Location::Kind::None)
            {
                m_browser.NavigateToLocation (own);
            }
            else
            {
                m_browser.NavigateToAddress (target);
            }

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
//  CassoExplorerWindow::ShowHistoryMenu
//
//  Where Back or Forward would go, nearest first; picking one takes as many
//  steps as it is away.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ShowHistoryMenu (bool forward, const RECT & anchor)
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

        command->label    = EscapeMnemonics (CassoExplorerBrowser::GetLocationLabel (stack[stack.size() - 1 - i]));
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
//  CassoExplorerWindow::EscapeMnemonics
//
//  A menu label reads an ampersand as a mnemonic marker, so a name's own is
//  doubled.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerWindow::EscapeMnemonics (const std::wstring & text)
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
//  CassoExplorerWindow::GetProfileRoot
//
//  The user's profile folder and the name the shell gives it, which is the
//  user's own name; empty when the shell will not say.
//
////////////////////////////////////////////////////////////////////////////////

BrowserModel::AddressRoot CassoExplorerWindow::GetProfileRoot()
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
//  CassoExplorerWindow::FillTabs
//
//  Tabs from the left of the strip, labeled by where each tab is. They share
//  the strip's width less the + button, no wider than a full tab and no
//  narrower than the minimum; past that the strip scrolls.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::FillTabs()
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
            case Location::Kind::Root:          tab.icon = m_shellIcons.GetForKind ((location.path == TreeModel::kCassoRootId) ? IShellIcons::Kind::Casso : IShellIcons::Kind::ThisPc); break;
            case Location::Kind::RecycleBin:    tab.icon = m_shellIcons.GetForKind (IShellIcons::Kind::RecycleBin); break;
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
//  CassoExplorerWindow::SwitchToTab
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::SwitchToTab (size_t index)
{
    //  A tab left on a folder deleted since moves up as it is shown.
    if (m_browser.SwitchTab (index))
    {
        m_browser.LeaveMissingLocation();
        FillList();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::BeginDragOut
//
//  The drag runs its own loop until the drop or the cancel, so the list is
//  told the button came up afterwards; it saw the press and never the
//  release.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::BeginDragOut()
{
    HRESULT                                  hr      = S_OK;
    DWORD                                    effect  = DROPEFFECT_NONE;
    DxuiMouseEvent                           release;
    HostFileNaming::Style                    style   = GetNamingStyle();
    std::vector<DxuiDragDropSource::Format>  formats = CassoExplorerDragOut::BuildFormats (m_browser, style);
    POINT                                    start   = {};
    std::vector<FileEntry>                   entries;
    std::vector<std::string>                 paths;
    bool                                     inImage = m_browser.IsImageLocation();
    std::string                              image   = TextEncoding::WideToNarrow (m_browser.GetLocation().path);
    CassoExplorerActions::Outcome            outcome;
    BOOL                                     got     = FALSE;



    release.kind   = DxuiMouseEventKind::Up;
    release.button = DxuiMouseButton::Left;
    m_list->OnMouse (release);

    if (formats.empty())
    {
        return;
    }

    //  What a move would take away, read now: the drag may change the list.
    if (inImage)
    {
        m_browser.GetSelectedEntries (entries);

        for (const FileEntry & entry : entries)
        {
            paths.push_back (m_browser.GetEntryPath (entry));
        }
    }

    got = GetCursorPos (&start);
    IGNORE_RETURN_VALUE (got, TRUE);

    hr = DxuiDragDropSource::Begin (std::move (formats), DROPEFFECT_COPY | DROPEFFECT_MOVE, start, effect);
    IGNORE_RETURN_VALUE (hr, S_OK);

    //  A target that moved the entries by copying them leaves their removal
    //  to the source, as Explorer does with files it cannot move itself. A
    //  host file is Explorer's to move, and Casso Explorer's own drops report
    //  a copy once they have finished a move themselves.
    if (effect == DROPEFFECT_MOVE && inImage && !paths.empty())
    {
        outcome = m_actions.DeleteEntries (image, paths);
        ReportOutcome (outcome, L"Move");
        RefreshAfterHostChange();
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ShowDropMenu
//
//  What a right-drag's drop does, asked at the point it landed. A file whose
//  name or container records what it is leaves nothing to choose, so the
//  menu names that and offers it alone; a file that records nothing offers
//  the conversions a put can make.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ShowDropMenu()
{
    std::vector<DxuiPopupMenuItem>  items;
    CassoExplorerActions::DropKind  kind;
    std::wstring                    label;



    if (!m_pendingDrop.valid)
    {
        return;
    }

    m_menuCommands.clear();

    if (m_pendingDrop.fromImage)
    {
        //  Entries from another image go across as they are, types mapped.
        AddMenuCommand (items, L"&Copy here", [this]() { RunDrop (CassoExplorerActions::Conversion::ByContent); });
    }
    else
    {
        kind = m_actions.DescribeDrop (m_pendingDrop.hostPaths, m_pendingDrop.targetKind);

        if (kind.determined)
        {
            label = L"&Copy here as " + kind.label;
            AddMenuCommand (items, label.c_str(), [this]() { RunDrop (CassoExplorerActions::Conversion::ByContent); });
        }
        else
        {
            AddMenuCommand (items, L"&Copy here",                   [this]() { RunDrop (CassoExplorerActions::Conversion::ByContent); });
            AddMenuCommand (items, L"Copy here as &text",           [this]() { RunDrop (CassoExplorerActions::Conversion::Text); });
            AddMenuCommand (items, L"Copy here as Applesoft &BASIC", [this]() { RunDrop (CassoExplorerActions::Conversion::Applesoft); });
            AddMenuCommand (items, L"Copy here as &binary...",      [this]() { RunDrop (CassoExplorerActions::Conversion::Binary); });
        }
    }

    items.push_back (DxuiPopupMenuItem::ForSeparator());
    AddMenuCommand (items, L"Ca&ncel", [this]() { m_pendingDrop = PendingDrop(); });

    DxuiContextMenu::Show (*GetPopupHost(), m_pendingDrop.screen.x, m_pendingDrop.screen.y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::RunDrop
//
//  The drop the menu settled, with the conversion it chose.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::RunDrop (CassoExplorerActions::Conversion conversion)
{
    PendingDrop                    drop    = m_pendingDrop;
    CassoExplorerActions::Outcome  outcome;



    m_pendingDrop = PendingDrop();

    if (!drop.valid)
    {
        return;
    }

    if (drop.fromImage)
    {
        outcome = m_actions.CopyEntriesInto (drop.sourceImage, drop.sourceKind, drop.catalogPaths,
                                             drop.location.path, drop.targetKind, drop.inner);
    }
    else
    {
        outcome = m_actions.PutInto (drop.location.path, drop.targetKind, drop.inner, drop.hostPaths,
                                     MakeAddressPrompt(), conversion);
    }

    PushPutUndo    (outcome, drop.location.path, drop.inner);
    ReportOutcome  (outcome, L"Put");
    RefreshAfterHostChange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::OnDropFile
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::OnDropFile (const std::wstring & path)
{
    CassoExplorerActions::Outcome  outcome = m_actions.PutFiles ({ path }, MakeAddressPrompt());



    PushPutUndo   (outcome, m_browser.GetLocation().path, m_browser.GetLocation().innerPath);
    ReportOutcome (outcome, L"Put");
    FillList();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::PreviewTheme
//
//  The theme on a row of the Theme menu, in the menu's order, applied while
//  the pointer is over it. A row past the end, or none, changes nothing.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::PreviewTheme (int index)
{
    if (index < 0 || index >= (int) std::size (s_kpszThemeRows) || m_prefs.theme == s_kpszThemeRows[index])
    {
        return;
    }

    SelectTheme (s_kpszThemeRows[index]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::SelectTheme
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::SelectTheme (const char * name)
{
    m_prefs.theme = name;
    ApplyTheme();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::IsCassoThemeName
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::IsCassoThemeName (const std::string & name)
{
    return name == CassoExplorerPrefs::kThemeSkeuomorphic
        || name == CassoExplorerPrefs::kThemeDarkModern
        || name == CassoExplorerPrefs::kThemeRetroTerminal;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::MakeAddressPrompt
//
//  Asks for a binary's load address, prefilled with the content's
//  suggestion, and asks again after an address that does not parse.
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerActions::AddressFn CassoExplorerWindow::MakeAddressPrompt()
{
    return [this] (const std::wstring & hostName, Word suggested, Word & outAddress)
    {
        std::wstring  text = std::format (L"${:04X}", suggested);

        for (;;)
        {
            if (!CassoExplorerPromptDialog::Ask (GetHwnd(), m_theme, L"Load Address",
                                           hostName + L" is a binary. Load address:", text, 8, text))
            {
                return false;
            }

            if (CassoExplorerActions::TryParseAddress (text, outAddress))
            {
                return true;
            }

            ShowMessage (L"Type an address from $0000 to $FFFF.", MB_ICONWARNING);
        }
    };
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::FindCassoTarget
//
//  The Casso that launched this browser while it is still there, otherwise
//  any running Casso, otherwise none.
//
////////////////////////////////////////////////////////////////////////////////

HWND CassoExplorerWindow::FindCassoTarget() const
{
    std::vector<HWND>  running;
    HWND               window  = nullptr;
    CassoTarget        target;



    //  Top-level windows come back front to back, so the first Casso found
    //  is the one most recently active.
    while ((window = FindWindowExW (nullptr, window, Win32IntentChannel::kWindowClass, nullptr)) != nullptr)
    {
        running.push_back (window);
    }

    target = CassoTargeting::Choose (m_context.owner, m_context.owner != nullptr && IsWindow (m_context.owner) != FALSE,
                                     running, MachineConfig());

    return target.hwnd;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::GetDefaultMachineDriveCount
//
//  The drives of the machine a new Casso would start, chosen as Casso chooses
//  it: the machine last selected, else the Apple //e. Read once, from its
//  configuration alone; its ROMs are not needed to count its drives. Both
//  drives when the configuration cannot be read.
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerWindow::GetDefaultMachineDriveCount()
{
    constexpr std::wstring_view  s_kPreferredDefaultMachine = L"Apple2e";
    HRESULT                hr          = S_OK;
    GlobalUserPrefs        prefs;
    Win32FileSystem        files;
    std::wstring           machine;
    std::vector<fs::path>  searchPaths;
    fs::path               configPath;
    std::string            jsonText;
    std::string            error;
    MachineConfig          config;
    int                    drives      = 0;



    BAIL_OUT_IF (m_defaultDriveCount >= 0, S_OK);

    m_defaultDriveCount = Win32IntentChannel::kMaxDriveCount;

    hr = prefs.Load (AssetBootstrap::GetAssetBaseDirectory().wstring(), files);
    IGNORE_RETURN_VALUE (hr, S_OK);

    searchPaths = PathResolver::BuildSearchPaths (PathResolver::GetExecutableDirectory(), PathResolver::GetWorkingDirectory());
    machine.assign (prefs.lastSelectedMachine.begin(), prefs.lastSelectedMachine.end());
    machine     = MachineScanner::SelectCanonical (MachineScanner::Scan (searchPaths, &MachineScanner::ListDirectory, &MachineScanner::ReadFile),
                                                   machine, s_kPreferredDefaultMachine);
    configPath  = PathResolver::FindFile (searchPaths, fs::path ("Machines") / fs::path (machine) / (fs::path (machine).string() + ".json"));
    BAIL_OUT_IF (configPath.empty(), S_OK);

    hr = files.ReadAllText (configPath.wstring(), jsonText);
    CHR (hr);

    //  Every file found where it is named, so a missing ROM does not stop
    //  the slots from being read.
    hr = MachineConfigLoader::Load (jsonText, fs::path (machine).string(), searchPaths,
                                    [] (const std::vector<fs::path> &, const fs::path & relative) { return relative; },
                                    config, error);
    CHR (hr);

    drives = config.AttachedDiskIiDriveCount();

    if (drives > 0)
    {
        m_defaultDriveCount = drives;
    }

Error:
    return m_defaultDriveCount;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::AskCassoToDescribe
//
//  The answer arrives as a reply message and updates the drive count the
//  menus read. With no Casso running, a new one opens with the default
//  machine, so its configuration says how many drives there are.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::AskCassoToDescribe()
{
    HWND  target = FindCassoTarget();
    bool  sent   = false;



    if (target == nullptr)
    {
        m_cassoDriveCount = GetDefaultMachineDriveCount();
        return;
    }

    sent = Win32IntentChannel::SendTo (target, GetHwnd(), Win32IntentChannel::GetMessageId(), Win32IntentChannel::EncodeDescribe());
    IGNORE_RETURN_VALUE (sent, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::GetVerbTitle
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerWindow::GetVerbTitle (CassoExplorerActions::Verb verb)
{
    std::wstring  title = GetVerbLabel (verb);



    std::erase (title, L'&');

    while (!title.empty() && title.back() == L'.')
    {
        title.pop_back();
    }

    return title;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::RunRawVerb
//
//  A read asks where the bytes start and how many, then where to save them;
//  a write asks for the file, then where it goes, then confirms, since it
//  overwrites whatever is there with no file system to stop it.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::RunRawVerb (CassoExplorerActions::Verb verb)
{
    HRESULT                          hr       = S_OK;
    bool                             sectors  = verb == CassoExplorerActions::Verb::ReadSectors || verb == CassoExplorerActions::Verb::WriteSectors;
    bool                             reading  = verb == CassoExplorerActions::Verb::ReadSectors || verb == CassoExplorerActions::Verb::ReadBlocks;
    std::wstring                     title    = GetVerbTitle (verb);
    std::filesystem::path            picked;
    bool                             chosen   = false;
    FileDialogSpec                   spec;
    int                              answer   = 0;
    CassoExplorerActions::Outcome    outcome;
    CassoExplorerRawDialog::Outcome  where;



    if (m_actions.GetFormatTarget().empty())
    {
        return;
    }

    //  The raw files are bytes as they lie on the disk; the picker keeps its
    //  own folder and names typed, apart from Casso's other pickers.
    spec.filters    = { FileDialogFilter { sectors ? L"Sector dumps (*.bin)" : L"Block dumps (*.bin)", L"*.bin" },
                        FileDialogFilter { L"All files (*.*)", L"*.*" } };
    spec.clientGuid = s_kRawPickerGuid;

    if (!reading)
    {
        spec.title = title;
        hr         = m_dialogs.PickFileToOpen (GetHwnd(), spec, picked, chosen);

        if (FAILED (hr) || !chosen)
        {
            return;
        }
    }

    where = CassoExplorerRawDialog::Ask (GetHwnd(), m_theme, title, sectors, reading);

    if (!where.confirmed)
    {
        return;
    }

    if (reading)
    {
        spec.defaultFileName  = sectors ? L"sectors.bin" : L"blocks.bin";
        spec.defaultExtension = L"bin";
        spec.askToReplace     = false;
        spec.title            = L"Save as";

        //  The picker's own replace prompt is Win32's; this one follows the
        //  theme, and No goes back to the picker.
        do
        {
            hr = m_dialogs.PickFileToSave (GetHwnd(), spec, picked, chosen);

            if (FAILED (hr) || !chosen)
            {
                return;
            }

            spec.initialFolder   = picked.parent_path();
            spec.defaultFileName = picked.filename().wstring();
        }
        while (GetFileAttributesW (picked.c_str()) != INVALID_FILE_ATTRIBUTES &&
               DxuiMessageBox (GetHwnd(), m_theme,
                               (picked.filename().wstring() + L" already exists. Overwrite it?").c_str(),
                               L"Confirm save as", MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2, { L"Overwrite", L"Cancel" }) != IDOK);

        outcome = sectors ? m_actions.ReadSectors (where.start, where.sector, where.count, picked.wstring(), where.numbering)
                          : m_actions.ReadBlocks  (where.start, where.count, picked.wstring());

        ReportOutcome (outcome, reading ? L"Read" : L"Write");
        return;
    }

    answer = DxuiMessageBox (GetHwnd(), m_theme,
                             L"The file's bytes will be written straight onto the disk image, over what is there now.",
                             title.c_str(),
                             MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2, { L"Overwrite", L"Cancel" });

    if (answer != IDOK)
    {
        return;
    }

    outcome = sectors ? m_actions.WriteSectors (where.start, where.sector, picked.wstring(), where.numbering)
                      : m_actions.WriteBlocks  (where.start, picked.wstring());

    ReportOutcome (outcome, L"Write");
    FillList();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::GetNowMs
//
////////////////////////////////////////////////////////////////////////////////

int64_t CassoExplorerWindow::GetNowMs()
{
    return (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
               std::chrono::steady_clock::now().time_since_epoch()).count();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::FitPanes
//
//  What the panes are short of comes out of the preview, then the tree,
//  each no further than its minimum; the list keeps its own. A body too
//  narrow even for the three minimums divides among them in proportion.
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerWindow::PaneWidths CassoExplorerWindow::FitPanes (int bodyDip, int treeDip, int previewDip)
{
    PaneWidths  widths;
    int         sashes    = (previewDip > 0) ? 2 * DxuiSplitter::kSashDip : DxuiSplitter::kSashDip;
    int         available = (std::max) (0, bodyDip - sashes);
    int         listFloor = kMinListWidthDip;
    int         deficit   = 0;
    int         cut       = 0;
    int         total     = 0;



    widths.tree    = (std::max) (0, treeDip);
    widths.preview = (std::max) (0, previewDip);
    deficit        = widths.tree + widths.preview + listFloor - available;

    if (deficit > 0 && widths.preview > kMinPreviewWidthDip)
    {
        cut             = (std::min) (deficit, widths.preview - kMinPreviewWidthDip);
        widths.preview -= cut;
        deficit        -= cut;
    }

    if (deficit > 0 && widths.tree > kMinTreeWidthDip)
    {
        cut          = (std::min) (deficit, widths.tree - kMinTreeWidthDip);
        widths.tree -= cut;
        deficit     -= cut;
    }

    if (deficit > 0)
    {
        total          = widths.tree + widths.preview + listFloor;
        widths.tree    = MulDiv (widths.tree,    available, total);
        widths.preview = MulDiv (widths.preview, available, total);
    }

    widths.list = available - widths.tree - widths.preview;

    return widths;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::GetListRowHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerWindow::GetListRowHeightPx (UINT dpi)
{
    int  half    = (int) ((kListRowHalfDip * dpi + 95) / 96);
    int  partial = (dpi % 96 != 0) ? 1 : 0;



    return 2 * half + partial;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::GetAddressRect
//
////////////////////////////////////////////////////////////////////////////////

RECT CassoExplorerWindow::GetAddressRect (const RECT & free, const RECT & strip, const DxuiDpiScaler & scaler)
{
    int   height = scaler.ToPx (kAddressBoxDip);
    int   middle = (strip.top + strip.bottom - DxuiToolbar::GetEdgePx (scaler)) / 2;
    RECT  rect   = free;



    if (free.right <= free.left)
    {
        return free;
    }

    rect.top    = middle - height / 2;
    rect.bottom = rect.top + height;

    return rect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::GetToolbarUnder
//
////////////////////////////////////////////////////////////////////////////////

DxuiToolbar & CassoExplorerWindow::GetToolbarUnder (const RECT & commandBarBand, POINT point, DxuiToolbar & navToolbar, DxuiToolbar & commandBar)
{
    return Contains (commandBarBand, point) ? commandBar : navToolbar;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ShowHoverTip
//
//  One tooltip serves every part of the window. Each part hides only a tip it
//  showed itself, so a move that one part has no tip for does not take down
//  another's.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ShowHoverTip (TipOwner owner, const RECT & anchor, const std::wstring & text)
{
    m_tipOwner = owner;
    m_tooltip.RequestShow (anchor, text, GetNowMs());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::HideHoverTip
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::HideHoverTip (TipOwner owner)
{
    if (m_tipOwner == owner)
    {
        m_tooltip.RequestHide (GetNowMs());
        m_tipOwner = TipOwner::None;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::UpdateTreeTip
//
//  The Casso node says what the folders under it are, since nothing else on
//  screen does.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::UpdateTreeTip (const DxuiMouseEvent & ev, POINT point)
{
    int                   row  = -1;
    const DxuiTreeNode *  node = nullptr;
    RECT                  tree = m_tree->GetBounds();
    RECT                  anchor;



    if (ev.kind == DxuiMouseEventKind::Move && Contains (tree, point))
    {
        row  = m_tree->HitTestRow (point.x, point.y);
        node = m_tree->GetNodeAt (row);
    }

    if (node == nullptr || node->id != TreeModel::kCassoRootId)
    {
        HideHoverTip (TipOwner::Tree);
        return;
    }

    anchor.left   = tree.left;
    anchor.right  = tree.right;
    anchor.top    = tree.top + (row - m_tree->GetTopRow()) * m_tree->GetRowHeight();
    anchor.bottom = anchor.top + m_tree->GetRowHeight();

    ShowHoverTip (TipOwner::Tree, anchor, kCassoNodeTip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::UpdateListTip
//
//  An item view's name cut short with an ellipsis shows whole in a tip, as
//  Explorer's do.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::UpdateListTip (const DxuiMouseEvent & ev, POINT point)
{
    m_listTipPoint  = point;
    m_listTipActive = ev.kind == DxuiMouseEventKind::Move;

    RefreshListTip();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::RefreshListTip
//
//  Explorer's tip for the item under the pointer: what the shell says of a
//  real file or folder, read in the background, or the type, size and date of
//  an entry inside an image; with the whole name first when the view cut it
//  short. Over the whole row in Details, the icon alone in Content, and the
//  icon and its name in the other views.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::RefreshListTip()
{
    RECT                  list     = m_list->GetBounds();
    POINT                 point    = m_listTipPoint;
    RECT                  anchor   = {};
    RECT                  text     = {};
    int                   row      = -1;
    std::wstring          path;
    std::wstring          details;
    std::wstring          tip;
    bool                  overItem = false;



    //  None while a menu is open over the list: the pointer is on the menu.
    if (m_listTipActive && m_list->IsVisible() && Contains (list, point) && !GetPopupHost()->GetContextMenu().IsVisible())
    {
        row = m_list->HitTestRow (point.x - list.left, point.y - list.top);
    }

    if (row >= 0 && row < (int) m_browser.GetRows().size() && m_list->GetCellTextRectPx (row, 0, text))
    {
        OffsetRect (&text, list.left, list.top);

        //  Content shows its tip over the icon only, left of the text.
        overItem = m_listView != DxuiListView::View::Content || point.x < text.left;
    }

    if (!overItem)
    {
        HideHoverTip (TipOwner::List);
        return;
    }

    {
        const CatalogRow &  row0 = m_browser.GetRows()[(size_t) row];

        if (m_browser.GetLocation().kind == Location::Kind::HostFolder && (size_t) row < m_rowProblems.size() && !m_rowProblems[(size_t) row].empty())
        {
            //  A broken image's tip says why it is broken.
            details = m_rowProblems[(size_t) row];
        }
        else if (m_browser.GetLocation().kind == Location::Kind::HostFolder && m_browser.TryGetRowPath (row, path))
        {
            //  Not yet read: the tip comes when it is, by a posted message.
            if (!m_infoTips.TryGet (path, details) && !m_list->IsItemNameCut (row))
            {
                HideHoverTip (TipOwner::List);
                return;
            }
        }
        else if (!row0.isDirectory || m_browser.GetLocation().kind == Location::Kind::RecycleBin)
        {
            details = L"Type: " + row0.typeText;

            if (!row0.isDirectory)
            {
                details += L"\nSize: " + CassoExplorerBrowser::FormatSize (row0.sizeBytes);
            }

            if (row0.hasModified)
            {
                details += L"\nDate modified: " + CassoExplorerBrowser::FormatModified (row0.modifiedUnix, row0.modifiedIsWallClock);
            }
        }

        tip = m_list->IsItemNameCut (row) ? row0.name : std::wstring();
        tip += (!tip.empty() && !details.empty()) ? L"\n" + details : details;
    }

    if (tip.empty())
    {
        HideHoverTip (TipOwner::List);
        return;
    }

    //  Details: the whole row; other views: the item's cell.
    anchor = text;

    if (m_listView == DxuiListView::View::Details)
    {
        anchor.left  = list.left;
        anchor.right = list.right;
    }

    ShowHoverTip (TipOwner::List, anchor, tip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::UpdateStatusTip
//
//  The free space of a disk image says which disk it counts.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::UpdateStatusTip (const DxuiMouseEvent & ev, POINT point)
{
    const std::wstring &  tip    = m_browser.GetStatus().freeSpaceTip;
    RECT                  anchor = m_status->GetFieldRect (kStatusFree);



    if (ev.kind != DxuiMouseEventKind::Move || tip.empty() || !Contains (anchor, point))
    {
        HideHoverTip (TipOwner::Status);
        return;
    }

    ShowHoverTip (TipOwner::Status, anchor, tip.c_str());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::RouteToolbarMouse
//
//  The toolbar takes its pointer input through its own calls rather than
//  OnMouse. A move always reaches it, so its hover clears when the pointer
//  leaves; it is consumed only over the strip. The tooltip follows the hover.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::RouteToolbarMouse (DxuiToolbar & toolbar, const DxuiMouseEvent & ev)
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

            //  A button clicked stays quiet until the pointer has left it, as
            //  Explorer's command bar does, rather than tipping again over the
            //  menu it opened.
            if (m_tipMuted && Contains (m_tipMuteRect, ev.positionDip))
            {
                tip = nullptr;
            }
            else
            {
                m_tipMuted = false;
            }

            if (tip != nullptr && *tip != L'\0')
            {
                ShowHoverTip (TipOwner::Toolbar, anchor, tip);
            }
            else
            {
                HideHoverTip (TipOwner::Toolbar);
            }

            break;

        case DxuiMouseEventKind::Down:
            took = ev.button == DxuiMouseButton::Left && toolbar.OnToolbarLButtonDown (x, y);

            //  A right-click on Back or Forward lists where each would go, as
            //  Explorer's does.
            if (ev.button == DxuiMouseButton::Right && toolbar.TryGetEntryRect (CassoExplorerCommands::kBack, anchor) && Contains (anchor, ev.positionDip))
            {
                ShowHistoryMenu (false, anchor);
                took = true;
            }
            else if (ev.button == DxuiMouseButton::Right && toolbar.TryGetEntryRect (CassoExplorerCommands::kForward, anchor) && Contains (anchor, ev.positionDip))
            {
                ShowHistoryMenu (true, anchor);
                took = true;
            }

            if (took)
            {
                m_tooltip.HideImmediate();
                m_tipMuted = toolbar.GetTooltipAt (x, y, m_tipMuteRect) != nullptr;
            }

            break;

        case DxuiMouseEventKind::Up:
            took = ev.button == DxuiMouseButton::Left && toolbar.OnToolbarLButtonUp (x, y);
            break;

        case DxuiMouseEventKind::Leave:
            toolbar.OnToolbarMouseLeave();
            HideHoverTip (TipOwner::Toolbar);
            break;

        default:
            break;
    }

    return took;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::OnTimer
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult CassoExplorerWindow::OnTimer (UINT_PTR timerId)
{
    bool  barsMoved = false;



    //  The Recycle Bin restores on its own time, so the folder is read again
    //  each second for a while after an undo.
    if (timerId == kRestoreTimerId)
    {
        if (--m_restoreTicks <= 0)
        {
            KillTimer (GetHwnd(), kRestoreTimerId);
        }

        RefreshAfterHostChange();

        return DxuiMessageResult::Handled;
    }

    if (timerId == kRecycleBinTimerId)
    {
        KillTimer (GetHwnd(), kRecycleBinTimerId);

        if (m_browser.GetLocation().kind == Location::Kind::RecycleBin)
        {
            RefreshAfterHostChange();
        }

        return DxuiMessageResult::Handled;
    }

    if (timerId == kFolderTimerId)
    {
        KillTimer (GetHwnd(), kFolderTimerId);
        m_folderFirstChangeMs = 0;
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
    if (m_menuBar->WantsTick() || m_toolbar->WantsTick() || m_commandBar->WantsTick() || m_previewToolbar->WantsTick() || GetPopupHost()->GetContextMenu().WantsTick())
    {
        m_menuBar->TickMenus (GetNowMs());
        m_toolbar->TickMenus (GetNowMs());
        m_commandBar->TickMenus (GetNowMs());
        m_previewToolbar->TickMenus (GetNowMs());
        GetPopupHost()->GetContextMenu().Tick (GetNowMs());
        Invalidate();
    }

    //  Scrollbars widen and narrow over a few frames as the pointer comes and
    //  goes.
    barsMoved = ((int) m_hexView->TickScrollbars (GetNowMs())
               | (int) m_textView->TickScrollbars (GetNowMs())
               | (int) m_list->TickScrollbars (GetNowMs())
               | (int) m_list->IsHeaderSliding ((int64_t) GetTickCount64())
               | (int) m_list->IsGroupSliding()
               | (int) m_previewList->TickScrollbars (GetNowMs())
               | (int) m_tree->TickScrollbars (GetNowMs())
               | (int) m_picture->TickScrollbars (GetNowMs())) != 0;

    if (barsMoved)
    {
        Invalidate();
    }

    //  The search box's caret blinks on the frames this asks for: one each
    //  time it turns on or off, not one every tick.
    if (m_focus == Pane::Search || m_focus == Pane::GoTo || m_focus == Pane::LocationSearch)
    {
        bool  caretOn = (m_focus == Pane::Search) ? m_searchBox.IsCaretOn() : (m_focus == Pane::GoTo) ? m_goToBox.IsCaretOn() : m_findBox->IsCaretOn();

        if (caretOn != m_caretOn)
        {
            m_caretOn = caretOn;
            Invalidate();
        }
    }

    //  Nothing left to animate: the tick stops until the next input.
    if (!barsMoved && !IsTickWanted())
    {
        KillTimer (GetHwnd(), kTooltipTimerId);
        m_tickArmed = false;
    }

    //  Not handled, so the host does not repaint after every tick: the work
    //  above repaints only when something moved.
    return DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ArmTick
//
//  Tooltips, menus, the address chevron, scrollbars and the search caret
//  animate on a display-rate tick. Each animation starts from input, so input
//  starts the tick, and the tick stops itself once nothing wants it.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ArmTick()
{
    if (!m_tickArmed && GetHwnd() != nullptr)
    {
        m_tickArmed = SetTimer (GetHwnd(), kTooltipTimerId, kTooltipTickMs, nullptr) != 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::IsTickWanted
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::IsTickWanted() const
{
    return m_tooltip.WantsTick() || m_tooltip.IsVisible() || m_address->WantsTick()
        || m_menuBar->IsOpen() || m_toolbar->IsMenuOpen() || m_commandBar->IsMenuOpen() || m_previewToolbar->IsMenuOpen()
        || m_menuBar->WantsTick() || m_toolbar->WantsTick() || m_commandBar->WantsTick() || m_previewToolbar->WantsTick()
        || GetPopupHost()->GetContextMenu().IsVisible() || GetPopupHost()->GetContextMenu().WantsTick()
        || m_focus == Pane::Search || m_focus == Pane::GoTo || m_focus == Pane::LocationSearch;
}
