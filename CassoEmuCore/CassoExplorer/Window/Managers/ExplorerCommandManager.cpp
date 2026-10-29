#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"
#include "CassoExplorer/CassoExplorerIcons.h"
#include "CassoExplorer/CassoExplorerRawDialog.h"
#include "CassoExplorer/CassoExplorerOptionsDialog.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/Model/LaunchCommand.h"
#include "Config/FileAssociations.h"
#include "Seams/Win32UserClasses.h"
#include "Update/Win32InstallEnvironment.h"
#include "Core/MachineConfig.h"
#include "Core/TextEncoding.h"
#include "Core/AppTimerIds.h"
#include "CassoExplorer/Window/Panes/ExplorerToolbars.h"
#include "CassoExplorer/CassoExplorerCommands.h"
#include "CassoExplorer/Window/Managers/ExplorerInput.h"
#include "CassoExplorer/Window/Managers/ExplorerPreviewManager.h"
#include "CassoExplorer/Window/Managers/ExplorerFileOps.h"
#include "CassoExplorer/Window/Managers/ExplorerFolderSync.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "Seams/Win32ShellItemVerbs.h"
#include "CassoExplorer/Window/Panes/ExplorerStatusBar.h"
#include "CassoExplorer/Window/Managers/ExplorerMenus.h"
#include "Seams/IHostDialogs.h"
#include "Seams/Win32HostDialogs.h"
#include "CassoExplorer/Window/Managers/CassoLinkManager.h"
#include "CassoExplorer/Window/Managers/ExplorerThemes.h"
#include "CassoExplorer/Window/Managers/ExplorerLayout.h"
#include "CassoExplorer/Window/Panes/ExplorerAddressBar.h"
#include "CassoExplorer/Window/Managers/ExplorerTabManager.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCommandManager::ExplorerCommandManager
//
////////////////////////////////////////////////////////////////////////////////

ExplorerCommandManager::ExplorerCommandManager (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCommandManager::ExtractImageFiles
//
//  The image's files go into a folder beside it, which is then selected in
//  the list when the list is on that folder's parent.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerCommandManager::ExtractImageFiles (const std::wstring & imagePath)
{
    std::wstring                   folder;
    CassoExplorerActions::Outcome  outcome;



    if (imagePath.empty())
    {
        return;
    }

    outcome = m_context.actions.ExtractImage (imagePath, GetNamingStyle(), folder);

    ReportOutcome (outcome, L"Extract all files");
    m_context.folderSync.RefreshAfterHostChange();

    if (!folder.empty() && m_context.browser.GetLocation().kind == Location::Kind::HostFolder
        && std::filesystem::path (m_context.browser.GetLocation().path) == std::filesystem::path (folder).parent_path())
    {
        m_context.listPane.SelectRowNamed (std::filesystem::path (folder).filename().wstring());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCommandManager::ExtractAll
//
//  The archive's own Extract All: its wizard, for where the files go, then
//  the copy of the files there, as Explorer's does.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerCommandManager::ExtractAll()
{
    HRESULT       hr     = S_OK;
    std::wstring  target = m_context.browser.GetExtractTarget();



    BAIL_OUT_IF (target.empty(), S_OK);

    hr = m_context.shellVerbs.RunItemVerb (m_context.window.GetHwnd(), { target }, "extract");
    CHR (hr);

Error:
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCommandManager::IsToolbarEntryAvailable
//
//  An entry on the strip and enabled; See more's button is always enabled.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerCommandManager::IsToolbarEntryAvailable (int index) const
{
    int  id = m_context.toolbars.GetNavToolbar().GetEntryCommandId (index);



    if (!m_context.toolbars.GetNavToolbar().IsEntryShown (index))
    {
        return false;
    }

    return (id == DxuiToolbar::kSeeMoreId) || IsEnabled (id);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCommandManager::IsCommandBarEntryAvailable
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerCommandManager::IsCommandBarEntryAvailable (int index) const
{
    int  id = m_context.toolbars.GetCommandBar().GetEntryCommandId (index);



    if (!m_context.toolbars.GetCommandBar().IsEntryShown (index))
    {
        return false;
    }

    return (id == DxuiToolbar::kSeeMoreId) || IsEnabled (id);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCommandManager::IsEnabled
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerCommandManager::IsEnabled (int id) const
{
    const BrowserModel &  model    = m_context.browser.GetBrowserModel();
    DxuiStandardCommand   standard = CassoExplorerCommands::GetStandardCommand (id);
    bool                  enabled  = false;
    Location              drive;



    //  A standard command row is enabled when the focused control reports it
    //  available, and grayed when no control handles it.
    if (standard != DxuiStandardCommand::None)
    {
        return DxuiCommandRouter::Query (m_context.input.GetFocusedControl(), standard, enabled) && enabled;
    }

    switch (id)
    {
        case CassoExplorerCommands::kBack:              return model.HasTabs() && model.CanGoBack();
        case CassoExplorerCommands::kForward:           return model.HasTabs() && model.CanGoForward();
        case CassoExplorerCommands::kUp:                return m_context.browser.CanGoUp();
        case CassoExplorerCommands::kToggleDisassembly: return model.HasTabs() && m_context.browser.GetPreview().kind == PreviewContent::Kind::Hex && m_context.browser.GetPreview().hostPath.empty();
        case CassoExplorerCommands::kLineAddresses:     return m_context.preview.GetBarMode() == CassoExplorerCommands::kPreviewBarListing;
        case CassoExplorerCommands::kPictureMonochrome:
        case CassoExplorerCommands::kPictureColor:      return m_context.browser.CanTogglePictureColor();
        case CassoExplorerCommands::kFindNext:          return m_context.preview.GetFind().CanFindNext();
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
        case CassoExplorerCommands::kGroup8:            return m_context.preview.GetHex().IsShowing();
        case CassoExplorerCommands::kNextTab:
        case CassoExplorerCommands::kPreviousTab:       return model.GetTabCount() > 1;
        case CassoExplorerCommands::kUndo:              return m_context.fileOps.CanUndo();
        case CassoExplorerCommands::kCutItems:          return m_context.listPane.GetMenus().IsListVerbOffered (CassoExplorerActions::Verb::Cut);
        case CassoExplorerCommands::kCopyItems:         return m_context.listPane.GetMenus().IsListVerbOffered (CassoExplorerActions::Verb::Copy) || m_context.listPane.GetMenus().IsListVerbOffered (CassoExplorerActions::Verb::Get) || m_context.browser.AreSelectedRowsSearchMatches();
        case CassoExplorerCommands::kPasteItems:        return (m_context.browser.IsVolumeLocation() || m_context.browser.GetLocation().kind == Location::Kind::HostFolder)
                                                         && m_context.shellVerbs.ClipboardHasFiles();
        case CassoExplorerCommands::kRenameItem:        return m_context.listPane.GetMenus().IsListVerbOffered (CassoExplorerActions::Verb::Rename);
        case CassoExplorerCommands::kShareItems:        return m_context.listPane.GetMenus().IsListVerbOffered (CassoExplorerActions::Verb::Share);
        case CassoExplorerCommands::kExtractAll:        return !m_context.browser.GetExtractTarget().empty();
        case CassoExplorerCommands::kReadDiskToImage:
        case CassoExplorerCommands::kWriteImageToDisk:  return m_context.browser.IsAppleDriveOpen() || m_context.cassoLinkManager.TryGetSelectedLinkDrive (drive);
        case CassoExplorerCommands::kCancelLinkOperation: return (m_context.browser.IsAppleDriveOpen() || m_context.cassoLinkManager.TryGetSelectedLinkDrive (drive)) && m_context.app.links != nullptr && !m_context.app.links->GetPendingJobs().empty();
        case CassoExplorerCommands::kDeleteItems:       return m_context.listPane.GetMenus().IsListVerbOffered (CassoExplorerActions::Verb::Delete);
        case CassoExplorerCommands::kEmptyRecycleBin:
        case CassoExplorerCommands::kRestoreItems:      return m_context.browser.GetLocation().kind == Location::Kind::RecycleBin && !m_context.browser.GetRows().empty();
        case CassoExplorerCommands::kNew:               return !m_context.browser.IsVolumeLocation() ? m_context.browser.GetLocation().kind == Location::Kind::HostFolder
                                                                                      : (m_context.browser.GetVolumeKind() == VolumeKind::ProDos && !m_context.browser.IsWriteProtected());
        case CassoExplorerCommands::kNewFolder:         return !m_context.browser.IsVolumeLocation() ? true
                                                                                      : (m_context.browser.GetVolumeKind() == VolumeKind::ProDos && !m_context.browser.IsWriteProtected());
        case CassoExplorerCommands::kNewDisk:           return !m_context.browser.IsImageLocation() && m_context.browser.GetLocation().kind == Location::Kind::HostFolder;
        default:                                  return true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCommandManager::IsChecked
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerCommandManager::IsChecked (int id) const
{
    const BrowserModel &  model = m_context.browser.GetBrowserModel();



    if (id >= CassoExplorerCommands::kGroupByField && id <= CassoExplorerCommands::kGroupByField + (int) RowGrouping::Field::Size)
    {
        return model.HasTabs() && (int) model.GetActiveTab().groupBy == id - CassoExplorerCommands::kGroupByField;
    }

    if (id >= CassoExplorerCommands::kSortByColumn && id < CassoExplorerCommands::kSortByColumn + (int) BrowserCells::GetColumns().size())
    {
        return model.HasTabs() && (int) model.GetActiveTab().sortColumn == id - CassoExplorerCommands::kSortByColumn;
    }

    if (id >= CassoExplorerCommands::kViewFirst && id < CassoExplorerCommands::kViewFirst + CassoExplorerPrefs::kViewCount)
    {
        return (int) m_context.listPane.GetColumns().GetView() == id - CassoExplorerCommands::kViewFirst;
    }

    switch (id)
    {
        case CassoExplorerCommands::kTogglePreview:     return m_context.prefs.previewVisible;
        case CassoExplorerCommands::kSortAscending:     return model.HasTabs() && !model.GetActiveTab().sortDescending;
        case CassoExplorerCommands::kSortDescending:    return model.HasTabs() && model.GetActiveTab().sortDescending;
        case CassoExplorerCommands::kGroupAscending:    return model.HasTabs() && !model.GetActiveTab().groupDescending;
        case CassoExplorerCommands::kGroupDescending:   return model.HasTabs() && model.GetActiveTab().groupDescending;
        case CassoExplorerCommands::kLineAddresses:     return m_context.prefs.lineAddresses;
        case CassoExplorerCommands::kToggleDisassembly: return model.HasTabs() && model.GetActiveTab().disassemble;
        case CassoExplorerCommands::kThemeLight:        return m_context.prefs.theme == CassoExplorerPrefs::kThemeLight;
        case CassoExplorerCommands::kThemeDark:         return m_context.prefs.theme == CassoExplorerPrefs::kThemeDark;
        case CassoExplorerCommands::kThemeSystem:       return m_context.prefs.theme != CassoExplorerPrefs::kThemeLight && m_context.prefs.theme != CassoExplorerPrefs::kThemeDark
                                                             && !ExplorerThemes::IsCassoThemeName (m_context.prefs.theme);
        case CassoExplorerCommands::kThemeSkeuomorphic: return m_context.prefs.theme == CassoExplorerPrefs::kThemeSkeuomorphic;
        case CassoExplorerCommands::kThemeDarkModern:   return m_context.prefs.theme == CassoExplorerPrefs::kThemeDarkModern;
        case CassoExplorerCommands::kThemeRetroTerminal: return m_context.prefs.theme == CassoExplorerPrefs::kThemeRetroTerminal;
        case CassoExplorerCommands::kPictureMonochrome: return m_context.browser.CanTogglePictureColor() && m_context.browser.GetPreview().pictureMode == PicturePreview::Mode::DoubleHiResMono;
        case CassoExplorerCommands::kPictureColor:      return m_context.browser.CanTogglePictureColor() && m_context.browser.GetPreview().pictureMode == PicturePreview::Mode::DoubleHiRes;
        case CassoExplorerCommands::kNoData:            return !m_context.preview.GetHex().IsShowingValues();
        case CassoExplorerCommands::kGroup1:            return m_context.preview.GetHex().IsShowingValues() && m_context.prefs.hexGrouping == 1;
        case CassoExplorerCommands::kGroup2:            return m_context.preview.GetHex().IsShowingValues() && m_context.prefs.hexGrouping == 2;
        case CassoExplorerCommands::kGroup4:            return m_context.preview.GetHex().IsShowingValues() && m_context.prefs.hexGrouping == 4;
        case CassoExplorerCommands::kGroup8:            return m_context.preview.GetHex().IsShowingValues() && m_context.prefs.hexGrouping == 8;
        case CassoExplorerCommands::kFormatHex:         return m_context.prefs.hexFormat == CassoExplorerPrefs::kHexFormatHex;
        case CassoExplorerCommands::kFormatSigned:      return m_context.prefs.hexFormat == CassoExplorerPrefs::kHexFormatSigned;
        case CassoExplorerCommands::kFormatUnsigned:    return m_context.prefs.hexFormat == CassoExplorerPrefs::kHexFormatUnsigned;
        case CassoExplorerCommands::kColumnsAuto:       return m_context.prefs.hexColumns == 0;
        case CassoExplorerCommands::kColumns1:          return m_context.prefs.hexColumns == 1;
        case CassoExplorerCommands::kColumns2:          return m_context.prefs.hexColumns == 2;
        case CassoExplorerCommands::kColumns4:          return m_context.prefs.hexColumns == 4;
        case CassoExplorerCommands::kColumns8:          return m_context.prefs.hexColumns == 8;
        case CassoExplorerCommands::kColumns16:         return m_context.prefs.hexColumns == 16;
        default:                                  return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCommandManager::Dispatch
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerCommandManager::Dispatch (int id)
{
    HRESULT  hr     = S_OK;
    bool     refill = false;
    bool     routed = DxuiCommandRouter::Invoke (m_context.input.GetFocusedControl(),
                                                  CassoExplorerCommands::GetStandardCommand (id));



    //  A standard command belongs to whatever has focus, so it is routed
    //  rather than decided here.
    if (CassoExplorerCommands::GetStandardCommand (id) != DxuiStandardCommand::None)
    {
        IGNORE_RETURN_VALUE (routed, true);
        m_context.window.Invalidate();
        return;
    }

    if (id >= CassoExplorerCommands::kViewFirst && id < CassoExplorerCommands::kViewFirst + CassoExplorerPrefs::kViewCount)
    {
        m_context.listPane.GetColumns().SetView ((DxuiListView::View) (id - CassoExplorerCommands::kViewFirst));
        m_context.toolbars.ShowViewOnButton();
        m_context.listPane.SizeListIcons();
        m_context.listPane.FillList();
        m_context.window.Invalidate();
        return;
    }

    //  A field groups dates newest first, as Explorer does, and the others
    //  from the start of the alphabet or the smallest.
    if (id >= CassoExplorerCommands::kGroupByField && id <= CassoExplorerCommands::kGroupByField + (int) RowGrouping::Field::Size)
    {
        RowGrouping::Field  field = (RowGrouping::Field) (id - CassoExplorerCommands::kGroupByField);

        if (!IsChecked (id))
        {
            m_context.browser.SetGroupBy (field, field == RowGrouping::Field::DateModified);
            m_context.listPane.GetColumns().RememberFolderSort();
            m_context.listPane.FillList();
        }

        return;
    }

    if (id >= CassoExplorerCommands::kSortByColumn && id < CassoExplorerCommands::kSortByColumn + (int) BrowserCells::GetColumns().size())
    {
        //  A new column sorts ascending; the column already in use keeps its
        //  direction.
        if (!IsChecked (id))
        {
            m_context.browser.SortByColumn (id - CassoExplorerCommands::kSortByColumn);

            if (m_context.browser.GetBrowserModel().GetActiveTab().sortDescending)
            {
                m_context.browser.SortByColumn (id - CassoExplorerCommands::kSortByColumn);
            }

            m_context.listPane.GetColumns().RememberFolderSort();
            m_context.listPane.FillList();
        }

        return;
    }

    switch (id)
    {
        //  Exit closes every window and writes them all for the next launch.
        case CassoExplorerCommands::kExit:
            if (m_context.app.host != nullptr)
            {
                m_context.app.host->ExitAll();
            }
            else
            {
                m_context.window.CloseWindow();
            }

            break;

        case CassoExplorerCommands::kUndo:         m_context.fileOps.UndoLast();                                   break;
        case CassoExplorerCommands::kCutItems:     RunVerb (CassoExplorerActions::Verb::Cut);    break;
        //  Ctrl+V and the command bar paste into the folder shown, whatever is
        //  selected, as Explorer's do; a folder's own menu pastes into it.
        case CassoExplorerCommands::kPasteItems:
        {
            if (m_context.browser.IsVolumeLocation())
            {
                m_context.fileOps.PasteIntoVolume();
                break;
            }

            m_context.fileOps.PasteHere (m_context.browser.GetLocation().path);
            break;
        }

        case CassoExplorerCommands::kRenameItem:   RunVerb (CassoExplorerActions::Verb::Rename); break;
        case CassoExplorerCommands::kShareItems:   RunVerb (CassoExplorerActions::Verb::Share);  break;
        case CassoExplorerCommands::kExtractAll:   ExtractAll();                                  break;
        case CassoExplorerCommands::kReadDiskToImage:     m_context.cassoLinkManager.ReadDiskToImage();                   break;
        case CassoExplorerCommands::kWriteImageToDisk:    m_context.cassoLinkManager.WriteImageToDisk();                  break;
        case CassoExplorerCommands::kCancelLinkOperation: m_context.actions.CancelLinkOperation();     break;
        case CassoExplorerCommands::kDeleteItems:  RunVerb (CassoExplorerActions::Verb::Delete); break;
        case CassoExplorerCommands::kEmptyRecycleBin: RunRecycleBinVerb (CassoExplorerActions::Verb::EmptyRecycleBin); break;
        case CassoExplorerCommands::kRestoreItems:    RunRecycleBinVerb (CassoExplorerActions::Verb::Restore); break;
        case CassoExplorerCommands::kNewFolder:    RunVerb (CassoExplorerActions::Verb::NewFolder); break;
        case CassoExplorerCommands::kNewDisk:      RunVerb (CassoExplorerActions::Verb::NewDisk);   break;

        //  Either way the selection goes on the clipboard: a real file through
        //  the shell, an entry in an image as the file a paste would write,
        //  in the style the Options dialog sets.
        case CassoExplorerCommands::kCopyItems:
            if (m_context.browser.IsVolumeLocation() || m_context.browser.AreSelectedRowsSearchMatches())
            {
                m_context.fileOps.CopyEntriesToClipboard (GetNamingStyle());
            }
            else
            {
                RunVerb (CassoExplorerActions::Verb::Copy);
            }

            break;

        case CassoExplorerCommands::kSortAscending:
        case CassoExplorerCommands::kSortDescending:
            if (!IsChecked (id) && m_context.browser.GetBrowserModel().HasTabs())
            {
                m_context.browser.SortByColumn ((int) m_context.browser.GetBrowserModel().GetActiveTab().sortColumn);
                m_context.listPane.GetColumns().RememberFolderSort();
                m_context.listPane.FillList();
            }

            break;

        case CassoExplorerCommands::kGroupAscending:
        case CassoExplorerCommands::kGroupDescending:
            if (!IsChecked (id) && m_context.browser.GetBrowserModel().HasTabs())
            {
                m_context.browser.SetGroupBy (m_context.browser.GetBrowserModel().GetActiveTab().groupBy, id == CassoExplorerCommands::kGroupDescending);
                m_context.listPane.GetColumns().RememberFolderSort();
                m_context.listPane.FillList();
            }

            break;


        case CassoExplorerCommands::kRefresh:
            m_context.listPane.GetColumns().ReadFolderOptions();
            m_context.folderSync.ReloadKnownFolders();
            m_context.browser.GetBrowserModel().InvalidateAllCatalogs();
            m_context.browser.ForgetLinkVisit();
            hr = m_context.browser.Refresh();

            //  The list's rows are hidden for a moment and then refilled, as
            //  File Explorer's are, so the refresh can be seen. Its scroll
            //  position does not change.
            m_context.listPane.SetContentHidden (true);
            SetTimer (m_context.window.GetHwnd(), (UINT_PTR) AppTimerId::ExplorerRefresh, kRefreshBlankMs, nullptr);
            break;

        case CassoExplorerCommands::kTogglePreview:
            m_context.prefs.previewVisible = !m_context.prefs.previewVisible;
            m_context.preview.FillPreview();
            m_context.layout.RecomputeLayout();
            break;

        case CassoExplorerCommands::kToggleDisassembly:
            if (m_context.browser.GetBrowserModel().HasTabs())
            {
                m_context.browser.SetDisassemble (!m_context.browser.GetBrowserModel().GetActiveTab().disassemble);
                m_context.preview.FillPreview();
            }

            break;

        case CassoExplorerCommands::kThemeLight:         m_context.themes.Select (CassoExplorerPrefs::kThemeLight);         break;
        case CassoExplorerCommands::kThemeDark:          m_context.themes.Select (CassoExplorerPrefs::kThemeDark);          break;
        case CassoExplorerCommands::kThemeSystem:        m_context.themes.Select (CassoExplorerPrefs::kThemeFollowSystem);  break;
        case CassoExplorerCommands::kThemeSkeuomorphic:  m_context.themes.Select (CassoExplorerPrefs::kThemeSkeuomorphic);  break;
        case CassoExplorerCommands::kThemeDarkModern:    m_context.themes.Select (CassoExplorerPrefs::kThemeDarkModern);    break;
        case CassoExplorerCommands::kThemeRetroTerminal: m_context.themes.Select (CassoExplorerPrefs::kThemeRetroTerminal); break;
        case CassoExplorerCommands::kPictureMonochrome:  m_context.preview.GetPicture().ShowForm (false);                               break;
        case CassoExplorerCommands::kPictureColor:       m_context.preview.GetPicture().ShowForm (true);                                break;

        case CassoExplorerCommands::kGoToOffset:
            m_context.preview.GetFind().FocusGoTo();
            break;

        case CassoExplorerCommands::kLineAddresses:
            m_context.preview.GetText().ToggleLineAddresses();
            break;

        //  Find searches the hex preview while it has focus; elsewhere it goes
        //  to the search box, as Explorer's Ctrl+E and Ctrl+F do.
        case CassoExplorerCommands::kFind:
            if (!m_context.preview.GetFind().FocusFind())
            {
                m_context.input.SetFocusPane (ExplorerPane::LocationSearch);
            }

            break;

        case CassoExplorerCommands::kZoomIn:         m_context.preview.SetPreviewZoom (m_context.prefs.previewZoom + kPreviewZoomStep); break;
        case CassoExplorerCommands::kZoomOut:        m_context.preview.SetPreviewZoom (m_context.prefs.previewZoom - kPreviewZoomStep); break;
        case CassoExplorerCommands::kZoomReset:      m_context.preview.SetPreviewZoom (CassoExplorerPrefs::kDefaultPreviewZoom);      break;
        case CassoExplorerCommands::kFindNext:       m_context.preview.GetFind().FindNext();                                      break;
        case CassoExplorerCommands::kNoData:         m_context.preview.GetHex().SetShowValues (!m_context.preview.GetHex().IsShowingValues()); break;
        case CassoExplorerCommands::kFormatHex:      m_context.preview.GetHex().SetFormat (CassoExplorerPrefs::kHexFormatHex);      break;
        case CassoExplorerCommands::kFormatSigned:   m_context.preview.GetHex().SetFormat (CassoExplorerPrefs::kHexFormatSigned);   break;
        case CassoExplorerCommands::kFormatUnsigned: m_context.preview.GetHex().SetFormat (CassoExplorerPrefs::kHexFormatUnsigned); break;
        case CassoExplorerCommands::kColumns:                                                         break;
        case CassoExplorerCommands::kColumnsAuto:    m_context.preview.GetHex().SetColumns (0);                               break;
        case CassoExplorerCommands::kColumns1:       m_context.preview.GetHex().SetColumns (1);                               break;
        case CassoExplorerCommands::kColumns2:       m_context.preview.GetHex().SetColumns (2);                               break;
        case CassoExplorerCommands::kColumns4:       m_context.preview.GetHex().SetColumns (4);                               break;
        case CassoExplorerCommands::kColumns8:       m_context.preview.GetHex().SetColumns (8);                               break;
        case CassoExplorerCommands::kColumns16:      m_context.preview.GetHex().SetColumns (16);                              break;

        case CassoExplorerCommands::kGroup1: m_context.preview.GetHex().SetGrouping (1); break;
        case CassoExplorerCommands::kGroup2: m_context.preview.GetHex().SetGrouping (2); break;
        case CassoExplorerCommands::kGroup4: m_context.preview.GetHex().SetGrouping (4); break;
        case CassoExplorerCommands::kGroup8: m_context.preview.GetHex().SetGrouping (8); break;

        case CassoExplorerCommands::kOptions:           ShowOptions(); break;

        case CassoExplorerCommands::kBack:    refill = m_context.browser.GoBack();    break;
        case CassoExplorerCommands::kForward: refill = m_context.browser.GoForward(); break;
        case CassoExplorerCommands::kUp:      refill = m_context.browser.GoUp();      break;

        case CassoExplorerCommands::kEditAddress:
            m_context.input.SetFocusPane (ExplorerPane::Address);
            m_context.addressBar.BeginEdit();
            break;

        case CassoExplorerCommands::kAddressHistory:
            m_context.input.SetFocusPane (ExplorerPane::Address);
            m_context.addressBar.BeginEdit();
            m_context.addressBar.ShowHistoryDropDown();
            break;

        case CassoExplorerCommands::kAbout:
            m_context.window.ShowAbout();
            break;

        case CassoExplorerCommands::kAddLink:
            m_context.cassoLinkManager.EditConnection (std::string());
            break;

        case CassoExplorerCommands::kNewTab:
            m_context.browser.NewTab();
            refill = true;
            break;

        case CassoExplorerCommands::kCloseTab:
            m_context.tabManager.CloseTabAt (m_context.browser.GetBrowserModel().GetActiveIndex());
            break;

        case CassoExplorerCommands::kNextTab:
        case CassoExplorerCommands::kPreviousTab:
            if (m_context.browser.GetBrowserModel().GetTabCount() > 1)
            {
                size_t  count = m_context.browser.GetBrowserModel().GetTabCount();
                size_t  next  = (m_context.browser.GetBrowserModel().GetActiveIndex() + (id == CassoExplorerCommands::kNextTab ? 1 : count - 1)) % count;

                m_context.tabManager.SwitchToTab (next);
            }

            break;

        default:
            break;
    }

    IGNORE_RETURN_VALUE (hr, S_OK);

    if (refill)
    {
        m_context.listPane.FillList();
    }

    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCommandManager::CreateDisk
//
//  Create an empty disk in the listed folder, whatever is selected, as File
//  Explorer's New does.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerCommandManager::CreateDisk (const CassoExplorerNewDiskDialog::Outcome & newDisk)
{
    CassoExplorerActions::Outcome  outcome = m_context.actions.CreateImage (m_context.browser.GetLocation().path, newDisk.fileName, newDisk.request);



    ReportOutcome (outcome, L"New disk");
    m_context.folderSync.RefreshAfterHostChange();
    m_context.listPane.SelectRowNamed (newDisk.fileName);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCommandManager::ShowOptions
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerCommandManager::ShowOptions()
{
    static constexpr const char              * kNamings[] = { CassoExplorerPrefs::kNamingDescriptive, CassoExplorerPrefs::kNamingCiderPress, CassoExplorerPrefs::kNamingAppleSingle };
    CassoExplorerOptionsDialog::Choices        choices;
    CassoExplorerOptionsDialog::DisplayHost    display;



    Win32UserClasses         classes;
    Win32UserClasses         fileExts         (Win32UserClasses::kFileExtsRoot);
    Win32InstallEnvironment  install;
    wchar_t                  module[MAX_PATH] = {};
    DWORD                    length           = GetModuleFileNameW (nullptr, module, MAX_PATH);
    std::wstring             folder           = std::filesystem::path (module).parent_path().wstring();
    std::wstring             cassoExe         = LaunchCommand::GetSiblingPath (folder, LaunchCommand::kCassoExe);
    bool                     packaged         = install.HasPackageIdentity();
    bool                     wasRegistered    = packaged || FileAssociations::IsRegistered (classes, cassoExe, module);
    HRESULT                  hr               = S_OK;



    //  An installed package declares the disk image types in its manifest,
    //  so the box is checked and cannot be changed there.
    choices.hostNaming   = (int) GetNamingStyle();
    choices.registered   = wasRegistered;
    choices.packaged     = packaged;

    choices.monitor      = (int) m_context.preview.GetPicture().GetMonitor();
    choices.customArgb   = m_context.prefs.previewCustomArgb;
    choices.crtOverrides = m_context.prefs.crtOverrides;

    //  The Display page's changes are drawn in the preview as they are made,
    //  and Cancel draws the preview as it was.
    display.themeDefaults = m_context.preview.GetPicture().GetCrtDefaults();
    display.preview       = [this] (const CassoExplorerOptionsDialog::Choices & shown)
    {
        m_context.prefs.crtOverrides      = shown.crtOverrides;
        m_context.prefs.previewCustomArgb = shown.customArgb;
        std::erase_if (m_context.prefs.crtOverrides, [] (const auto & entry) { return entry.second.IsEmpty(); });
        m_context.preview.GetPicture().SelectMonitor ((PicturePreview::Monitor) shown.monitor);
        m_context.preview.GetPicture().UpdateCrtPicture (true);
    };

    if (!CassoExplorerOptionsDialog::Ask (m_context.window.GetHwnd(), m_context.theme, display, choices))
    {
        return;
    }

    if (choices.hostNaming >= 0 && choices.hostNaming < (int) std::size (kNamings))
    {
        m_context.prefs.hostNaming = kNamings[choices.hostNaming];
    }


    //  The user's file types change only when the box was changed.
    if (!packaged && choices.registered != wasRegistered)
    {
        //  A path cut short at MAX_PATH would register a command that runs nothing.
        if (choices.registered && (length == 0 || length >= MAX_PATH))
        {
            hr = HRESULT_FROM_WIN32 (ERROR_INSUFFICIENT_BUFFER);
        }
        else
        {
            hr = choices.registered ? FileAssociations::Register (classes, cassoExe, module)
                                    : FileAssociations::Unregister (classes, fileExts);
        }

        if (FAILED (hr))
        {
            m_context.window.ShowMessage (L"Windows did not take the change to the disk image types.", MB_ICONERROR);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCommandManager::GetNamingStyle
//
//  The host file name style the Options dialog set; its choices are in the
//  style's own order.
//
////////////////////////////////////////////////////////////////////////////////

HostFileNaming::Style ExplorerCommandManager::GetNamingStyle() const
{
    if (m_context.prefs.hostNaming == CassoExplorerPrefs::kNamingCiderPress)
    {
        return HostFileNaming::Style::CiderPress;
    }

    if (m_context.prefs.hostNaming == CassoExplorerPrefs::kNamingAppleSingle)
    {
        return HostFileNaming::Style::AppleSingle;
    }

    return HostFileNaming::Style::Descriptive;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCommandManager::RunRecycleBinVerb
//
//  A verb on the Recycle Bin's items, carried out by the bin itself. Whether
//  the verb was the bin's to carry out.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerCommandManager::RunRecycleBinVerb (CassoExplorerActions::Verb verb)
{
    using Verb = CassoExplorerActions::Verb;

    HRESULT                    hr = S_OK;
    std::vector<std::wstring>  ids;



    m_context.browser.GetSelectedRecycledIds (ids);

    switch (verb)
    {
        case Verb::Open:
            BAIL_OUT_IF (ids.empty(), S_OK);
            hr = m_context.shellVerbs.RunRecycledVerb (m_context.window.GetHwnd(), ids, IShellItemVerbs::RecycledVerb::Properties);
            break;

        case Verb::Restore:
        case Verb::Delete:
            //  Restore with nothing selected restores everything, as the
            //  command bar's Restore all items does.
            if (ids.empty() && verb == Verb::Restore)
            {
                m_context.browser.GetAllRecycledIds (ids);
            }

            BAIL_OUT_IF (ids.empty(), S_OK);
            hr = m_context.shellVerbs.RunRecycledVerb (m_context.window.GetHwnd(), ids, (verb == Verb::Restore) ? IShellItemVerbs::RecycledVerb::Restore
                                                                                       : IShellItemVerbs::RecycledVerb::Delete);
            m_context.folderSync.RefreshAfterHostChange();
            break;

        case Verb::EmptyRecycleBin:
            hr = m_context.shellVerbs.EmptyRecycleBin (m_context.window.GetHwnd());
            m_context.folderSync.RefreshAfterHostChange();
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
//  ExplorerCommandManager::GetCommandLabel
//
//  The label of a command whose label follows the selection, or empty for
//  the command's own: Restore restores the selected items, or all of them.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerCommandManager::GetCommandLabel (int id) const
{
    Location      drive;
    std::wstring  title;



    if (id == CassoExplorerCommands::kRestoreItems && !m_context.browser.GetSelectedRows().empty())
    {
        return L"Restore the selected items";
    }

    //  A linked Apple's drive by its slot, drive and volume: the open drive, or
    //  the drive selected on its machine.
    if (m_context.browser.IsAppleDriveOpen())
    {
        title = m_context.actions.DescribeAppleDrive();
    }
    else if (m_context.cassoLinkManager.TryGetSelectedLinkDrive (drive))
    {
        title = m_context.browser.GetTreeModel().GetLinkDriveTitle (drive.path);
    }

    if (id == CassoExplorerCommands::kReadDiskToImage && !title.empty())
    {
        return std::format (L"Create image from {}...", title);
    }

    if (id == CassoExplorerCommands::kWriteImageToDisk && !title.empty())
    {
        return std::format (L"Write image to {}...", title);
    }

    return std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCommandManager::RunVerb
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerCommandManager::RunVerb (CassoExplorerActions::Verb verb)
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
    if (m_context.browser.GetLocation().kind == Location::Kind::RecycleBin && RunRecycleBinVerb (verb))
    {
        return;
    }

    switch (verb)
    {
        case CassoExplorerActions::Verb::Open:
            if (m_context.browser.IsImageLocation() && !(m_context.browser.GetSelectedRows().size() == 1 && m_context.browser.OpenRow (m_context.browser.GetSelectedRows()[0])))
            {
                m_context.fileOps.OpenSelectedEntries();
            }
            else if (m_context.browser.IsImageLocation())
            {
                m_context.listPane.FillList();
            }
            else if (m_context.browser.GetSelectedRows().size() > 1)
            {
                m_context.fileOps.OpenEachSelected();
            }
            else if (!m_context.browser.GetSelectedRows().empty() && m_context.browser.OpenRow (m_context.browser.GetSelectedRows()[0]))
            {
                m_context.listPane.FillList();
            }
            else if (!m_context.fileOps.GetSelectedImagePath().empty())
            {
                //  Not something to browse: a real file, in its own program.
                hr = m_context.shellVerbs.Open (m_context.window.GetHwnd(), m_context.fileOps.GetSelectedImagePath());
            }

            break;

        case CassoExplorerActions::Verb::Get:
            hr = m_context.dialogs.PickFolder (m_context.window.GetHwnd(), picked, chosen);

            if (SUCCEEDED (hr) && chosen)
            {
                ReportOutcome (m_context.actions.GetSelected (picked.wstring(), style, m_context.fileOps.MakeCollisionPrompt()), L"Copy");
            }

            break;

        case CassoExplorerActions::Verb::Put:
            hr = m_context.dialogs.PickFileToOpen (m_context.window.GetHwnd(), spec, picked, chosen);

            if (SUCCEEDED (hr) && chosen)
            {
                outcome = m_context.actions.PutFiles ({ picked.wstring() }, m_context.fileOps.MakeAddressPrompt());

                //  An Apple drive's writes finish later, over the link, with no undo.
                if (!m_context.browser.IsAppleDriveLocation())
                {
                    m_context.fileOps.PushPutUndo (outcome, m_context.browser.GetLocation().path, m_context.browser.GetLocation().innerPath);
                }

                ReportOutcome (outcome, L"Put");
                m_context.listPane.FillList();
            }

            break;

        case CassoExplorerActions::Verb::Cut:
        case CassoExplorerActions::Verb::Copy:
            if (m_context.browser.IsVolumeLocation())
            {
                m_context.fileOps.CopyEntriesToClipboard (GetNamingStyle());
                break;
            }

            if (m_context.fileOps.TryGetSelectedShellIds (hostPaths))
            {
                hr = m_context.shellVerbs.PlaceOnClipboard (m_context.window.GetHwnd(), hostPaths, verb == CassoExplorerActions::Verb::Cut);
            }

            m_context.listPane.SetCutPaths ((SUCCEEDED (hr) && verb == CassoExplorerActions::Verb::Cut) ? hostPaths : std::vector<std::wstring>());
            m_context.listPane.FillList();
            break;

        case CassoExplorerActions::Verb::Share:
            m_context.browser.GetSelectedHostPaths (hostPaths);
            hr = m_context.shellVerbs.Share (m_context.window.GetHwnd(), hostPaths);
            break;

        case CassoExplorerActions::Verb::ExtractAll:
            ExtractAll();
            break;

        case CassoExplorerActions::Verb::ExtractFiles:
            ExtractImageFiles (m_context.fileOps.GetSelectedImagePath());
            break;

        case CassoExplorerActions::Verb::Paste:
            if (m_context.browser.IsVolumeLocation())
            {
                m_context.fileOps.PasteIntoVolume();
                break;
            }

            m_context.fileOps.PasteHere (m_context.fileOps.GetPasteFolder());
            break;

        case CassoExplorerActions::Verb::Delete:
            if (!m_context.browser.IsVolumeLocation())
            {
                //  Windows asks, and the Recycle Bin keeps it. A file inside a
                //  zip has no file of its own to recycle, so it is deleted for
                //  good after Windows' confirmation, as in File Explorer.
                if (m_context.preview.IsArchiveLocation())
                {
                    m_context.fileOps.DeleteSelectionPermanently();
                    break;
                }

                //  An item a shell folder such as Home lists may be a recent or
                //  a cloud file with no path of its own; its own Delete
                //  command removes it, as File Explorer's does.
                if (m_context.fileOps.TryGetSelectedShellIds (hostPaths))
                {
                    hr = (m_context.browser.GetLocation().kind == Location::Kind::ShellFolder)
                       ? m_context.shellVerbs.RunItemVerb (m_context.window.GetHwnd(), hostPaths, "delete")
                       : m_context.shellVerbs.Recycle (m_context.window.GetHwnd(), hostPaths);
                }

                if (FAILED (hr) && hr != HRESULT_FROM_WIN32 (ERROR_CANCELLED))
                {
                    m_context.window.ShowMessage (L"The selected items could not be deleted.", MB_ICONWARNING);
                }

                if (SUCCEEDED (hr) && !hostPaths.empty() && m_context.browser.GetLocation().kind == Location::Kind::HostFolder)
                {
                    m_context.fileOps.PushUndo (ExplorerFileOps::UndoStep { ExplorerFileOps::UndoStep::Kind::Recycle, Location(), hostPaths });
                }

                m_context.folderSync.RefreshAfterHostChange();
                break;
            }

        {
            std::vector<std::wstring>  plan  = m_context.actions.DescribeDeletePlan();
            size_t                     count = m_context.browser.GetSelectedRows().size();
            std::wstring               link;
            int                        slot  = 0;
            int                        drive = 0;
            std::wstring               where = TreeModel::TryParseLinkId (m_context.browser.GetLocation().path, link, slot, drive)
                                                ? std::format (L"the disk in drive {}", drive)
                                                : std::filesystem::path (m_context.browser.GetLocation().path).filename().wstring();
            std::wstring               what     = (count == 1) ? m_context.browser.GetRows()[(size_t) m_context.browser.GetSelectedRows()[0]].name
                                                               : std::format (L"these {} items", count);
            std::wstring               message  = std::format (L"Delete {} from {}?", what, where);

            //  A folder goes with everything below it, which the list cannot
            //  show, so the plan's own rows and totals go into the question.
            for (const std::wstring & line : plan)
            {
                message += L"\n" + line;
            }

            //  The buttons are labeled with their actions, as File Explorer's Delete File
            //  dialog's are.
            answer = DxuiMessageBox (m_context.window.GetHwnd(), m_context.theme, message.c_str(), L"Delete", MB_OKCANCEL | MB_ICONWARNING, { L"Delete", L"Cancel" });

            if (answer == IDOK)
            {
                ExplorerFileOps::UndoStep  step           { ExplorerFileOps::UndoStep::Kind::ImageDelete, m_context.browser.GetLocation() };
                wchar_t                    temp[MAX_PATH] = {};

                //  An AppleSingle copy keeps each file's bytes, type and
                //  address, so a put brings it back as it was. The copy holds
                //  files only, so a deletion that takes a folder is not kept.
                m_context.browser.GetSelectedEntries (entries);

                if (!m_context.browser.IsAppleDriveLocation() &&
                    std::none_of (entries.begin(), entries.end(), [] (const FileEntry & entry) { return entry.isDirectory; }) &&
                    GetTempPathW (MAX_PATH, temp) != 0)
                {
                    std::error_code  ignore;

                    step.savedDir = std::format (L"{}CassoExplorerUndo\\{}", temp, GetTickCount64());
                    std::filesystem::create_directories (step.savedDir, ignore);
                }

                if (!step.savedDir.empty() && !m_context.actions.GetSelected (step.savedDir, HostFileNaming::Style::AppleSingle).Succeeded())
                {
                    step.savedDir.clear();
                }

                outcome = m_context.actions.DeleteSelected();

                if (outcome.Succeeded() && !step.savedDir.empty())
                {
                    m_context.fileOps.PushUndo (std::move (step));
                }

                ReportOutcome (outcome, L"Delete");
                m_context.listPane.FillList();
            }

            break;
        }

        case CassoExplorerActions::Verb::Boot:
            ReportOutcome (m_context.actions.BootSelected(), L"Set startup program");
            m_context.listPane.FillList();
            break;

        case CassoExplorerActions::Verb::Rename:
            m_context.fileOps.BeginRename();
            break;

        case CassoExplorerActions::Verb::InsertDrive1:
        case CassoExplorerActions::Verb::InsertDrive2:
            m_context.fileOps.InsertIntoDrive (m_context.fileOps.GetSelectedImagePath(), verb == CassoExplorerActions::Verb::InsertDrive1 ? 1 : 2);
            break;

        case CassoExplorerActions::Verb::OpenInNewCasso:
            m_context.fileOps.OpenInNewCasso (m_context.fileOps.GetSelectedImagePath());
            break;

        case CassoExplorerActions::Verb::NewDisk:
            newDisk = CassoExplorerNewDiskDialog::Ask (m_context.window.GetHwnd(), m_context.theme, false, [this] (const std::wstring & fileName)
            {
                return m_context.actions.IsNameTaken (m_context.browser.GetLocation().path, fileName);
            });

            if (newDisk.confirmed)
            {
                CreateDisk (newDisk);
            }

            break;

        case CassoExplorerActions::Verb::NewFolder:
            //  An unused default name, open for renaming at once.
            newName = m_context.actions.GetNewFolderName();

            if (!m_context.browser.IsVolumeLocation())
            {
                hr = m_context.shellVerbs.CreateFolder (m_context.window.GetHwnd(), m_context.browser.GetLocation().path, newName);

                if (SUCCEEDED (hr))
                {
                    m_context.fileOps.PushUndo (ExplorerFileOps::UndoStep { ExplorerFileOps::UndoStep::Kind::HostNewFolder, Location(), { (std::filesystem::path (m_context.browser.GetLocation().path) / newName).wstring() } });
                }

                m_context.folderSync.RefreshAfterHostChange();
            }
            else
            {
                outcome = m_context.actions.CreateFolder (newName);

                if (outcome.Succeeded() && !m_context.browser.IsAppleDriveLocation())
                {
                    m_context.fileOps.PushUndo (ExplorerFileOps::UndoStep { ExplorerFileOps::UndoStep::Kind::ImageNewFolder, m_context.browser.GetLocation(), {}, std::wstring(), newName });
                }

                ReportOutcome (outcome, L"New folder");
                m_context.listPane.FillList();
            }

            if (m_context.listPane.SelectRowNamed (newName))
            {
                m_context.fileOps.BeginRename();
            }

            break;

        case CassoExplorerActions::Verb::Format:
            newDisk = CassoExplorerNewDiskDialog::Ask (m_context.window.GetHwnd(), m_context.theme, true, {}, ChooseFormatDefault (m_context.actions.GetFormatTarget()),
                                                       std::filesystem::path (m_context.actions.GetFormatTarget()).filename().wstring());

            if (newDisk.confirmed)
            {
                answer = DxuiMessageBox (m_context.window.GetHwnd(), m_context.theme,
                                         (L"Format " + CassoExplorerActions::GetLeafName (m_context.actions.GetFormatTarget())
                                          + L"? Everything on it will be erased.").c_str(),
                                         L"Format", MB_OKCANCEL | MB_ICONWARNING, { L"Format", L"Cancel" });

                if (answer == IDOK)
                {
                    ReportOutcome (m_context.actions.FormatImage (newDisk.request), L"Format");
                    m_context.listPane.FillList();
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
    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCommandManager::ReportOutcome
//
//  A failure shows what the runner said; a success is quiet, since the list
//  already shows the result.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerCommandManager::ReportOutcome (const CassoExplorerActions::Outcome & outcome, const wchar_t * verbName)
{
    if (outcome.Succeeded())
    {
        m_context.statusBar.SetDetail (std::format (L"{}: {} file(s)", verbName, outcome.written));

        //  For a copy salvaged from a damaged disk, what could not be read is shown.
        if (!outcome.warning.empty())
        {
            m_context.window.ShowMessage (outcome.warning, MB_ICONWARNING);
        }

        return;
    }

    m_context.window.ShowMessage (outcome.message.empty() ? std::wstring (verbName) + L" did not complete." : outcome.message, MB_ICONERROR);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCommandManager::ChooseFormatDefault
//
//  The file system the image holds now, when it holds one; otherwise what
//  its size and kind allow. DOS 3.3 fits only a 140K floppy, so anything
//  larger -- an 800K disk, a hard disk image -- starts at ProDOS, and so does
//  a ProDOS-order image.
//
////////////////////////////////////////////////////////////////////////////////

int ExplorerCommandManager::ChooseFormatDefault (const std::wstring & image)
{
    VolumeListing              listing;
    VolumeKind                 kind      = VolumeKind::Unknown;
    WIN32_FILE_ATTRIBUTE_DATA  data      = {};
    std::wstring               extension = std::filesystem::path (image).extension().wstring();
    uint64_t                   size      = 0;



    if (m_context.browser.GetOperations().List (TextEncoding::WideToNarrow (image), listing, kind).Succeeded())
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
//  ExplorerCommandManager::GetVerbTitle
//
//  A verb's label as a window's title: no access key, no trailing dots.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerCommandManager::GetVerbTitle (CassoExplorerActions::Verb verb)
{
    std::wstring  title = ExplorerMenus::GetVerbLabel (verb);



    std::erase (title, L'&');

    while (!title.empty() && title.back() == L'.')
    {
        title.pop_back();
    }

    return title;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCommandManager::RunRawVerb
//
//  A read asks where the bytes start and how many, then where to save them;
//  a write asks for the file, then where it goes, then confirms, since it
//  overwrites whatever is there with no file system to stop it.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerCommandManager::RunRawVerb (CassoExplorerActions::Verb verb)
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
    std::wstring                     target   = m_context.actions.GetFormatTarget();



    if (target.empty())
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
        hr         = m_context.dialogs.PickFileToOpen (m_context.window.GetHwnd(), spec, picked, chosen);

        if (FAILED (hr) || !chosen)
        {
            return;
        }
    }

    where = CassoExplorerRawDialog::Ask (m_context.window.GetHwnd(), m_context.theme, title, sectors, reading);

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
            hr = m_context.dialogs.PickFileToSave (m_context.window.GetHwnd(), spec, picked, chosen);

            if (FAILED (hr) || !chosen)
            {
                return;
            }

            spec.initialFolder   = picked.parent_path();
            spec.defaultFileName = picked.filename().wstring();
        }
        while (GetFileAttributesW (picked.c_str()) != INVALID_FILE_ATTRIBUTES &&
               DxuiMessageBox (m_context.window.GetHwnd(), m_context.theme,
                               (picked.filename().wstring() + L" already exists. Overwrite it?").c_str(),
                               L"Confirm save as", MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2, { L"Overwrite", L"Cancel" }) != IDOK);

        if (!IsRawTargetCurrent (target))
        {
            return;
        }

        outcome = sectors ? m_context.actions.ReadSectors (where.start, where.sector, where.count, picked.wstring(), where.numbering)
                          : m_context.actions.ReadBlocks  (where.start, where.count, picked.wstring());

        ReportOutcome (outcome, reading ? L"Read" : L"Write");
        return;
    }

    answer = DxuiMessageBox (m_context.window.GetHwnd(), m_context.theme,
                             (L"The file's bytes will be written straight onto " + CassoExplorerActions::GetLeafName (target) + L", over what is there now.").c_str(),
                             title.c_str(),
                             MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2, { L"Overwrite", L"Cancel" });

    if (answer != IDOK || !IsRawTargetCurrent (target))
    {
        return;
    }

    outcome = sectors ? m_context.actions.WriteSectors (where.start, where.sector, picked.wstring(), where.numbering)
                      : m_context.actions.WriteBlocks  (where.start, picked.wstring());

    ReportOutcome (outcome, L"Write");
    m_context.listPane.FillList();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCommandManager::IsRawTargetCurrent
//
//  The window's timers run while the dialogs are open, and a refresh can move
//  the list off the image the raw read or write was chosen for.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerCommandManager::IsRawTargetCurrent (const std::wstring & target)
{
    if (m_context.actions.GetFormatTarget() == target)
    {
        return true;
    }

    m_context.window.ShowMessage (CassoExplorerActions::GetLeafName (target) + L" is no longer selected, so nothing was read or written.", MB_ICONWARNING);
    return false;
}
