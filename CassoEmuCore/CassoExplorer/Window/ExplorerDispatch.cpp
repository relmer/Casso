#include "Pch.h"

#include "CassoExplorer/Window/ExplorerDispatch.h"
#include "CassoExplorer/CassoExplorerIcons.h"
#include "CassoExplorer/CassoExplorerRawDialog.h"
#include "CassoExplorer/CassoExplorerOptionsDialog.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/Model/LaunchCommand.h"
#include "Config/FileAssociations.h"
#include "Seams/Win32UserClasses.h"
#include "Core/MachineConfig.h"
#include "Core/TextEncoding.h"
#include "Core/AppTimerIds.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDispatch::ExplorerDispatch
//
////////////////////////////////////////////////////////////////////////////////

ExplorerDispatch::ExplorerDispatch (CassoExplorerWindow & window)
    : m_window (window)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDispatch::ExtractImageFiles
//
//  The image's files go into a folder beside it, which is then selected in
//  the list when the list is on that folder's parent.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerDispatch::ExtractImageFiles (const std::wstring & imagePath)
{
    std::wstring                   folder;
    CassoExplorerActions::Outcome  outcome;



    if (imagePath.empty())
    {
        return;
    }

    outcome = m_window.m_actions.ExtractImage (imagePath, GetNamingStyle(), folder);

    ReportOutcome (outcome, L"Extract all files");
    m_window.m_folderSync.RefreshAfterHostChange();

    if (!folder.empty() && m_window.m_browser.GetLocation().kind == Location::Kind::HostFolder
        && std::filesystem::path (m_window.m_browser.GetLocation().path) == std::filesystem::path (folder).parent_path())
    {
        m_window.m_listPane.SelectRowNamed (std::filesystem::path (folder).filename().wstring());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDispatch::ExtractAll
//
//  The archive's own Extract All: its wizard, for where the files go, then
//  the copy of the files there, as Explorer's does.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerDispatch::ExtractAll()
{
    HRESULT       hr     = S_OK;
    std::wstring  target = m_window.m_browser.GetExtractTarget();



    BAIL_OUT_IF (target.empty(), S_OK);

    hr = m_window.m_shellVerbs.RunItemVerb (m_window.GetHwnd(), { target }, "extract");
    CHR (hr);

Error:
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDispatch::IsToolbarEntryAvailable
//
//  An entry on the strip and enabled; See more's button is always enabled.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerDispatch::IsToolbarEntryAvailable (int index) const
{
    int  id = m_window.m_toolbars.GetNavToolbar().GetEntryCommandId (index);



    if (!m_window.m_toolbars.GetNavToolbar().IsEntryShown (index))
    {
        return false;
    }

    return (id == DxuiToolbar::kSeeMoreId) || IsEnabled (id);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDispatch::IsCommandBarEntryAvailable
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerDispatch::IsCommandBarEntryAvailable (int index) const
{
    int  id = m_window.m_toolbars.GetCommandBar().GetEntryCommandId (index);



    if (!m_window.m_toolbars.GetCommandBar().IsEntryShown (index))
    {
        return false;
    }

    return (id == DxuiToolbar::kSeeMoreId) || IsEnabled (id);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDispatch::IsEnabled
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerDispatch::IsEnabled (int id) const
{
    const BrowserModel &  model    = m_window.m_browser.GetBrowserModel();
    DxuiStandardCommand   standard = CassoExplorerCommands::GetStandardCommand (id);
    bool                  enabled  = false;
    Location              drive;



    //  A standard command row is enabled when the focused control reports it
    //  available, and grayed when no control handles it.
    if (standard != DxuiStandardCommand::None)
    {
        return DxuiCommandRouter::Query (m_window.m_input.GetFocusedControl(), standard, enabled) && enabled;
    }

    switch (id)
    {
        case CassoExplorerCommands::kBack:              return model.HasTabs() && model.CanGoBack();
        case CassoExplorerCommands::kForward:           return model.HasTabs() && model.CanGoForward();
        case CassoExplorerCommands::kUp:                return m_window.m_browser.CanGoUp();
        case CassoExplorerCommands::kToggleDisassembly: return model.HasTabs() && m_window.m_browser.GetPreview().kind == PreviewContent::Kind::Hex && m_window.m_browser.GetPreview().hostPath.empty();
        case CassoExplorerCommands::kLineAddresses:     return m_window.m_previewPane.GetBarMode() == CassoExplorerCommands::kPreviewBarListing;
        case CassoExplorerCommands::kPictureMonochrome:
        case CassoExplorerCommands::kPictureColor:      return m_window.m_browser.CanTogglePictureColor();
        case CassoExplorerCommands::kFindNext:          return m_window.m_previewPane.CanFindNext();
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
        case CassoExplorerCommands::kGroup8:            return m_window.m_previewPane.IsHexPreviewShowing();
        case CassoExplorerCommands::kNextTab:
        case CassoExplorerCommands::kPreviousTab:       return model.GetTabCount() > 1;
        case CassoExplorerCommands::kUndo:              return m_window.m_fileOps.CanUndo();
        case CassoExplorerCommands::kCutItems:          return m_window.m_listPane.GetMenus().IsListVerbOffered (CassoExplorerActions::Verb::Cut);
        case CassoExplorerCommands::kCopyItems:         return m_window.m_listPane.GetMenus().IsListVerbOffered (CassoExplorerActions::Verb::Copy) || m_window.m_listPane.GetMenus().IsListVerbOffered (CassoExplorerActions::Verb::Get) || m_window.m_browser.AreSelectedRowsSearchMatches();
        case CassoExplorerCommands::kPasteItems:        return (m_window.m_browser.IsVolumeLocation() || m_window.m_browser.GetLocation().kind == Location::Kind::HostFolder)
                                                         && m_window.m_shellVerbs.ClipboardHasFiles();
        case CassoExplorerCommands::kRenameItem:        return m_window.m_listPane.GetMenus().IsListVerbOffered (CassoExplorerActions::Verb::Rename);
        case CassoExplorerCommands::kShareItems:        return m_window.m_listPane.GetMenus().IsListVerbOffered (CassoExplorerActions::Verb::Share);
        case CassoExplorerCommands::kExtractAll:        return !m_window.m_browser.GetExtractTarget().empty();
        case CassoExplorerCommands::kReadDiskToImage:
        case CassoExplorerCommands::kWriteImageToDisk:  return m_window.m_browser.IsAppleDriveOpen() || m_window.m_links.TryGetSelectedLinkDrive (drive);
        case CassoExplorerCommands::kCancelLinkOperation: return (m_window.m_browser.IsAppleDriveOpen() || m_window.m_links.TryGetSelectedLinkDrive (drive)) && m_window.m_context.links != nullptr && !m_window.m_context.links->GetPendingJobs().empty();
        case CassoExplorerCommands::kDeleteItems:       return m_window.m_listPane.GetMenus().IsListVerbOffered (CassoExplorerActions::Verb::Delete);
        case CassoExplorerCommands::kEmptyRecycleBin:
        case CassoExplorerCommands::kRestoreItems:      return m_window.m_browser.GetLocation().kind == Location::Kind::RecycleBin && !m_window.m_browser.GetRows().empty();
        case CassoExplorerCommands::kNew:               return !m_window.m_browser.IsVolumeLocation() ? m_window.m_browser.GetLocation().kind == Location::Kind::HostFolder
                                                                                      : (m_window.m_browser.GetVolumeKind() == VolumeKind::ProDos && !m_window.m_browser.IsWriteProtected());
        case CassoExplorerCommands::kNewFolder:         return !m_window.m_browser.IsVolumeLocation() ? true
                                                                                      : (m_window.m_browser.GetVolumeKind() == VolumeKind::ProDos && !m_window.m_browser.IsWriteProtected());
        case CassoExplorerCommands::kNewDisk:           return !m_window.m_browser.IsImageLocation() && m_window.m_browser.GetLocation().kind == Location::Kind::HostFolder;
        default:                                  return true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDispatch::IsChecked
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerDispatch::IsChecked (int id) const
{
    const BrowserModel &  model = m_window.m_browser.GetBrowserModel();



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
        return (int) m_window.m_listPane.GetColumns().GetView() == id - CassoExplorerCommands::kViewFirst;
    }

    switch (id)
    {
        case CassoExplorerCommands::kTogglePreview:     return m_window.m_prefs.previewVisible;
        case CassoExplorerCommands::kSortAscending:     return model.HasTabs() && !model.GetActiveTab().sortDescending;
        case CassoExplorerCommands::kSortDescending:    return model.HasTabs() && model.GetActiveTab().sortDescending;
        case CassoExplorerCommands::kGroupAscending:    return model.HasTabs() && !model.GetActiveTab().groupDescending;
        case CassoExplorerCommands::kGroupDescending:   return model.HasTabs() && model.GetActiveTab().groupDescending;
        case CassoExplorerCommands::kLineAddresses:     return m_window.m_prefs.lineAddresses;
        case CassoExplorerCommands::kToggleDisassembly: return model.HasTabs() && model.GetActiveTab().disassemble;
        case CassoExplorerCommands::kThemeLight:        return m_window.m_prefs.theme == CassoExplorerPrefs::kThemeLight;
        case CassoExplorerCommands::kThemeDark:         return m_window.m_prefs.theme == CassoExplorerPrefs::kThemeDark;
        case CassoExplorerCommands::kThemeSystem:       return m_window.m_prefs.theme != CassoExplorerPrefs::kThemeLight && m_window.m_prefs.theme != CassoExplorerPrefs::kThemeDark
                                                             && !ExplorerThemes::IsCassoThemeName (m_window.m_prefs.theme);
        case CassoExplorerCommands::kThemeSkeuomorphic: return m_window.m_prefs.theme == CassoExplorerPrefs::kThemeSkeuomorphic;
        case CassoExplorerCommands::kThemeDarkModern:   return m_window.m_prefs.theme == CassoExplorerPrefs::kThemeDarkModern;
        case CassoExplorerCommands::kThemeRetroTerminal: return m_window.m_prefs.theme == CassoExplorerPrefs::kThemeRetroTerminal;
        case CassoExplorerCommands::kPictureMonochrome: return m_window.m_browser.CanTogglePictureColor() && m_window.m_browser.GetPreview().pictureMode == PicturePreview::Mode::DoubleHiResMono;
        case CassoExplorerCommands::kPictureColor:      return m_window.m_browser.CanTogglePictureColor() && m_window.m_browser.GetPreview().pictureMode == PicturePreview::Mode::DoubleHiRes;
        case CassoExplorerCommands::kNoData:            return !m_window.m_previewPane.IsShowingValues();
        case CassoExplorerCommands::kGroup1:            return m_window.m_previewPane.IsShowingValues() && m_window.m_prefs.hexGrouping == 1;
        case CassoExplorerCommands::kGroup2:            return m_window.m_previewPane.IsShowingValues() && m_window.m_prefs.hexGrouping == 2;
        case CassoExplorerCommands::kGroup4:            return m_window.m_previewPane.IsShowingValues() && m_window.m_prefs.hexGrouping == 4;
        case CassoExplorerCommands::kGroup8:            return m_window.m_previewPane.IsShowingValues() && m_window.m_prefs.hexGrouping == 8;
        case CassoExplorerCommands::kFormatHex:         return m_window.m_prefs.hexFormat == CassoExplorerPrefs::kHexFormatHex;
        case CassoExplorerCommands::kFormatSigned:      return m_window.m_prefs.hexFormat == CassoExplorerPrefs::kHexFormatSigned;
        case CassoExplorerCommands::kFormatUnsigned:    return m_window.m_prefs.hexFormat == CassoExplorerPrefs::kHexFormatUnsigned;
        case CassoExplorerCommands::kColumnsAuto:       return m_window.m_prefs.hexColumns == 0;
        case CassoExplorerCommands::kColumns1:          return m_window.m_prefs.hexColumns == 1;
        case CassoExplorerCommands::kColumns2:          return m_window.m_prefs.hexColumns == 2;
        case CassoExplorerCommands::kColumns4:          return m_window.m_prefs.hexColumns == 4;
        case CassoExplorerCommands::kColumns8:          return m_window.m_prefs.hexColumns == 8;
        case CassoExplorerCommands::kColumns16:         return m_window.m_prefs.hexColumns == 16;
        default:                                  return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDispatch::Dispatch
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerDispatch::Dispatch (int id)
{
    HRESULT  hr     = S_OK;
    bool     refill = false;
    bool     routed = DxuiCommandRouter::Invoke (m_window.m_input.GetFocusedControl(),
                                                  CassoExplorerCommands::GetStandardCommand (id));



    //  A standard command belongs to whatever has focus, so it is routed
    //  rather than decided here.
    if (CassoExplorerCommands::GetStandardCommand (id) != DxuiStandardCommand::None)
    {
        IGNORE_RETURN_VALUE (routed, true);
        m_window.Invalidate();
        return;
    }

    if (id >= CassoExplorerCommands::kViewFirst && id < CassoExplorerCommands::kViewFirst + CassoExplorerPrefs::kViewCount)
    {
        m_window.m_listPane.GetColumns().SetView ((DxuiListView::View) (id - CassoExplorerCommands::kViewFirst));
        m_window.m_toolbars.ShowViewOnButton();
        m_window.m_listPane.SizeListIcons();
        m_window.m_listPane.FillList();
        m_window.Invalidate();
        return;
    }

    //  A field groups dates newest first, as Explorer does, and the others
    //  from the start of the alphabet or the smallest.
    if (id >= CassoExplorerCommands::kGroupByField && id <= CassoExplorerCommands::kGroupByField + (int) RowGrouping::Field::Size)
    {
        RowGrouping::Field  field = (RowGrouping::Field) (id - CassoExplorerCommands::kGroupByField);

        if (!IsChecked (id))
        {
            m_window.m_browser.SetGroupBy (field, field == RowGrouping::Field::DateModified);
            m_window.m_listPane.GetColumns().RememberFolderSort();
            m_window.m_listPane.FillList();
        }

        return;
    }

    if (id >= CassoExplorerCommands::kSortByColumn && id < CassoExplorerCommands::kSortByColumn + (int) BrowserCells::GetColumns().size())
    {
        //  A new column sorts ascending; the column already in use keeps its
        //  direction.
        if (!IsChecked (id))
        {
            m_window.m_browser.SortByColumn (id - CassoExplorerCommands::kSortByColumn);

            if (m_window.m_browser.GetBrowserModel().GetActiveTab().sortDescending)
            {
                m_window.m_browser.SortByColumn (id - CassoExplorerCommands::kSortByColumn);
            }

            m_window.m_listPane.GetColumns().RememberFolderSort();
            m_window.m_listPane.FillList();
        }

        return;
    }

    switch (id)
    {
        //  Exit closes every window and writes them all for the next launch.
        case CassoExplorerCommands::kExit:
            if (m_window.m_context.host != nullptr)
            {
                m_window.m_context.host->ExitAll();
            }
            else
            {
                m_window.OnWindowClose();
            }

            break;

        case CassoExplorerCommands::kUndo:         m_window.m_fileOps.UndoLast();                                   break;
        case CassoExplorerCommands::kCutItems:     RunVerb (CassoExplorerActions::Verb::Cut);    break;
        //  Ctrl+V and the command bar paste into the folder shown, whatever is
        //  selected, as Explorer's do; a folder's own menu pastes into it.
        case CassoExplorerCommands::kPasteItems:
        {
            if (m_window.m_browser.IsVolumeLocation())
            {
                m_window.m_fileOps.PasteIntoVolume();
                break;
            }

            m_window.m_fileOps.PasteHere (m_window.m_browser.GetLocation().path);
            break;
        }

        case CassoExplorerCommands::kRenameItem:   RunVerb (CassoExplorerActions::Verb::Rename); break;
        case CassoExplorerCommands::kShareItems:   RunVerb (CassoExplorerActions::Verb::Share);  break;
        case CassoExplorerCommands::kExtractAll:   ExtractAll();                                  break;
        case CassoExplorerCommands::kReadDiskToImage:     m_window.m_links.ReadDiskToImage();                   break;
        case CassoExplorerCommands::kWriteImageToDisk:    m_window.m_links.WriteImageToDisk();                  break;
        case CassoExplorerCommands::kCancelLinkOperation: m_window.m_actions.CancelLinkOperation();     break;
        case CassoExplorerCommands::kDeleteItems:  RunVerb (CassoExplorerActions::Verb::Delete); break;
        case CassoExplorerCommands::kEmptyRecycleBin: RunRecycleBinVerb (CassoExplorerActions::Verb::EmptyRecycleBin); break;
        case CassoExplorerCommands::kRestoreItems:    RunRecycleBinVerb (CassoExplorerActions::Verb::Restore); break;
        case CassoExplorerCommands::kNewFolder:    RunVerb (CassoExplorerActions::Verb::NewFolder); break;
        case CassoExplorerCommands::kNewDisk:      RunVerb (CassoExplorerActions::Verb::NewDisk);   break;

        //  Either way the selection goes on the clipboard: a real file through
        //  the shell, an entry in an image as the file a paste would write,
        //  in the style the Options dialog sets.
        case CassoExplorerCommands::kCopyItems:
            if (m_window.m_browser.IsVolumeLocation() || m_window.m_browser.AreSelectedRowsSearchMatches())
            {
                m_window.m_fileOps.CopyEntriesToClipboard (GetNamingStyle());
            }
            else
            {
                RunVerb (CassoExplorerActions::Verb::Copy);
            }

            break;

        case CassoExplorerCommands::kSortAscending:
        case CassoExplorerCommands::kSortDescending:
            if (!IsChecked (id) && m_window.m_browser.GetBrowserModel().HasTabs())
            {
                m_window.m_browser.SortByColumn ((int) m_window.m_browser.GetBrowserModel().GetActiveTab().sortColumn);
                m_window.m_listPane.GetColumns().RememberFolderSort();
                m_window.m_listPane.FillList();
            }

            break;

        case CassoExplorerCommands::kGroupAscending:
        case CassoExplorerCommands::kGroupDescending:
            if (!IsChecked (id) && m_window.m_browser.GetBrowserModel().HasTabs())
            {
                m_window.m_browser.SetGroupBy (m_window.m_browser.GetBrowserModel().GetActiveTab().groupBy, id == CassoExplorerCommands::kGroupDescending);
                m_window.m_listPane.GetColumns().RememberFolderSort();
                m_window.m_listPane.FillList();
            }

            break;


        case CassoExplorerCommands::kRefresh:
            m_window.m_listPane.GetColumns().ReadFolderOptions();
            m_window.m_folderSync.ReloadKnownFolders();
            m_window.m_browser.GetBrowserModel().InvalidateAllCatalogs();
            m_window.m_browser.ForgetLinkVisit();
            hr = m_window.m_browser.Refresh();

            //  The list's rows are hidden for a moment and then refilled, as
            //  File Explorer's are, so the refresh can be seen. Its scroll
            //  position does not change.
            m_window.m_listPane.SetContentHidden (true);
            SetTimer (m_window.GetHwnd(), (UINT_PTR) AppTimerId::ExplorerRefresh, kRefreshBlankMs, nullptr);
            break;

        case CassoExplorerCommands::kTogglePreview:
            m_window.m_prefs.previewVisible = !m_window.m_prefs.previewVisible;
            m_window.m_previewPane.FillPreview();
            m_window.m_layout.RecomputeLayout();
            break;

        case CassoExplorerCommands::kToggleDisassembly:
            if (m_window.m_browser.GetBrowserModel().HasTabs())
            {
                m_window.m_browser.SetDisassemble (!m_window.m_browser.GetBrowserModel().GetActiveTab().disassemble);
                m_window.m_previewPane.FillPreview();
            }

            break;

        case CassoExplorerCommands::kThemeLight:         m_window.m_themes.Select (CassoExplorerPrefs::kThemeLight);         break;
        case CassoExplorerCommands::kThemeDark:          m_window.m_themes.Select (CassoExplorerPrefs::kThemeDark);          break;
        case CassoExplorerCommands::kThemeSystem:        m_window.m_themes.Select (CassoExplorerPrefs::kThemeFollowSystem);  break;
        case CassoExplorerCommands::kThemeSkeuomorphic:  m_window.m_themes.Select (CassoExplorerPrefs::kThemeSkeuomorphic);  break;
        case CassoExplorerCommands::kThemeDarkModern:    m_window.m_themes.Select (CassoExplorerPrefs::kThemeDarkModern);    break;
        case CassoExplorerCommands::kThemeRetroTerminal: m_window.m_themes.Select (CassoExplorerPrefs::kThemeRetroTerminal); break;
        case CassoExplorerCommands::kPictureMonochrome:  m_window.m_previewPane.ShowPictureForm (false);                               break;
        case CassoExplorerCommands::kPictureColor:       m_window.m_previewPane.ShowPictureForm (true);                                break;

        case CassoExplorerCommands::kGoToOffset:
            m_window.m_previewPane.FocusGoTo();
            break;

        case CassoExplorerCommands::kLineAddresses:
            m_window.m_previewPane.ToggleLineAddresses();
            break;

        //  Find searches the hex preview while it has focus; elsewhere it goes
        //  to the search box, as Explorer's Ctrl+E and Ctrl+F do.
        case CassoExplorerCommands::kFind:
            if (!m_window.m_previewPane.FocusFind())
            {
                m_window.m_input.SetFocusPane (ExplorerPane::LocationSearch);
            }

            break;

        case CassoExplorerCommands::kZoomIn:         m_window.m_previewPane.SetPreviewZoom (m_window.m_prefs.previewZoom + kPreviewZoomStep); break;
        case CassoExplorerCommands::kZoomOut:        m_window.m_previewPane.SetPreviewZoom (m_window.m_prefs.previewZoom - kPreviewZoomStep); break;
        case CassoExplorerCommands::kZoomReset:      m_window.m_previewPane.SetPreviewZoom (CassoExplorerPrefs::kDefaultPreviewZoom);      break;
        case CassoExplorerCommands::kFindNext:       m_window.m_previewPane.FindNext();                                      break;
        case CassoExplorerCommands::kNoData:         m_window.m_previewPane.SetHexShowValues (!m_window.m_previewPane.IsShowingValues()); break;
        case CassoExplorerCommands::kFormatHex:      m_window.m_previewPane.SetHexFormat (CassoExplorerPrefs::kHexFormatHex);      break;
        case CassoExplorerCommands::kFormatSigned:   m_window.m_previewPane.SetHexFormat (CassoExplorerPrefs::kHexFormatSigned);   break;
        case CassoExplorerCommands::kFormatUnsigned: m_window.m_previewPane.SetHexFormat (CassoExplorerPrefs::kHexFormatUnsigned); break;
        case CassoExplorerCommands::kColumns:                                                         break;
        case CassoExplorerCommands::kColumnsAuto:    m_window.m_previewPane.SetHexColumns (0);                               break;
        case CassoExplorerCommands::kColumns1:       m_window.m_previewPane.SetHexColumns (1);                               break;
        case CassoExplorerCommands::kColumns2:       m_window.m_previewPane.SetHexColumns (2);                               break;
        case CassoExplorerCommands::kColumns4:       m_window.m_previewPane.SetHexColumns (4);                               break;
        case CassoExplorerCommands::kColumns8:       m_window.m_previewPane.SetHexColumns (8);                               break;
        case CassoExplorerCommands::kColumns16:      m_window.m_previewPane.SetHexColumns (16);                              break;

        case CassoExplorerCommands::kGroup1: m_window.m_previewPane.SetHexGrouping (1); break;
        case CassoExplorerCommands::kGroup2: m_window.m_previewPane.SetHexGrouping (2); break;
        case CassoExplorerCommands::kGroup4: m_window.m_previewPane.SetHexGrouping (4); break;
        case CassoExplorerCommands::kGroup8: m_window.m_previewPane.SetHexGrouping (8); break;

        case CassoExplorerCommands::kOptions:           ShowOptions(); break;

        case CassoExplorerCommands::kBack:    refill = m_window.m_browser.GoBack();    break;
        case CassoExplorerCommands::kForward: refill = m_window.m_browser.GoForward(); break;
        case CassoExplorerCommands::kUp:      refill = m_window.m_browser.GoUp();      break;

        case CassoExplorerCommands::kEditAddress:
            m_window.m_input.SetFocusPane (ExplorerPane::Address);
            m_window.m_addressBar.BeginEdit();
            break;

        case CassoExplorerCommands::kAddressHistory:
            m_window.m_input.SetFocusPane (ExplorerPane::Address);
            m_window.m_addressBar.BeginEdit();
            m_window.m_addressBar.ShowHistoryDropDown();
            break;

        case CassoExplorerCommands::kAbout:
            m_window.ShowAbout();
            break;

        case CassoExplorerCommands::kAddLink:
            m_window.m_links.EditLink (std::string());
            break;

        case CassoExplorerCommands::kNewTab:
            m_window.m_browser.NewTab();
            refill = true;
            break;

        case CassoExplorerCommands::kCloseTab:
            m_window.m_tabStrip.CloseTabAt (m_window.m_browser.GetBrowserModel().GetActiveIndex());
            break;

        case CassoExplorerCommands::kNextTab:
        case CassoExplorerCommands::kPreviousTab:
            if (m_window.m_browser.GetBrowserModel().GetTabCount() > 1)
            {
                size_t  count = m_window.m_browser.GetBrowserModel().GetTabCount();
                size_t  next  = (m_window.m_browser.GetBrowserModel().GetActiveIndex() + (id == CassoExplorerCommands::kNextTab ? 1 : count - 1)) % count;

                m_window.m_tabStrip.SwitchToTab (next);
            }

            break;

        default:
            break;
    }

    IGNORE_RETURN_VALUE (hr, S_OK);

    if (refill)
    {
        m_window.m_listPane.FillList();
    }

    m_window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDispatch::CreateDisk
//
//  Create an empty disk in the listed folder, whatever is selected, as File
//  Explorer's New does.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerDispatch::CreateDisk (const CassoExplorerNewDiskDialog::Outcome & newDisk)
{
    CassoExplorerActions::Outcome  outcome = m_window.m_actions.CreateImage (m_window.m_browser.GetLocation().path, newDisk.fileName, newDisk.request);



    ReportOutcome (outcome, L"New disk");
    m_window.m_folderSync.RefreshAfterHostChange();
    m_window.m_listPane.SelectRowNamed (newDisk.fileName);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDispatch::ShowOptions
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerDispatch::ShowOptions()
{
    static constexpr const char              * kNamings[] = { CassoExplorerPrefs::kNamingDescriptive, CassoExplorerPrefs::kNamingCiderPress, CassoExplorerPrefs::kNamingAppleSingle };
    CassoExplorerOptionsDialog::Choices        choices;
    CassoExplorerOptionsDialog::DisplayHost    display;



    Win32UserClasses  classes;
    Win32UserClasses  fileExts         (Win32UserClasses::kFileExtsRoot);
    bool              wasRegistered    = FileAssociations::IsRegistered (classes);
    wchar_t           module[MAX_PATH] = {};
    DWORD             length           = 0;
    std::wstring      folder;
    HRESULT           hr               = S_OK;



    choices.hostNaming   = (int) GetNamingStyle();
    choices.registered   = wasRegistered;

    choices.monitor      = (int) m_window.m_previewPane.GetPreviewMonitor();
    choices.customArgb   = m_window.m_prefs.previewCustomArgb;
    choices.crtOverrides = m_window.m_prefs.crtOverrides;

    //  The Display page's changes are drawn in the preview as they are made,
    //  and Cancel draws the preview as it was.
    display.themeDefaults = m_window.m_previewPane.GetPreviewCrtDefaults();
    display.preview       = [this] (const CassoExplorerOptionsDialog::Choices & shown)
    {
        m_window.m_prefs.crtOverrides      = shown.crtOverrides;
        m_window.m_prefs.previewCustomArgb = shown.customArgb;
        std::erase_if (m_window.m_prefs.crtOverrides, [] (const auto & entry) { return entry.second.IsEmpty(); });
        m_window.m_previewPane.SelectMonitor ((PicturePreview::Monitor) shown.monitor);
        m_window.m_previewPane.UpdateCrtPicture (true);
    };

    if (!CassoExplorerOptionsDialog::Ask (m_window.GetHwnd(), m_window.m_theme, display, choices))
    {
        return;
    }

    if (choices.hostNaming >= 0 && choices.hostNaming < (int) std::size (kNamings))
    {
        m_window.m_prefs.hostNaming = kNamings[choices.hostNaming];
    }


    //  The user's file types change only when the box was changed.
    if (choices.registered != wasRegistered)
    {
        length = GetModuleFileNameW (nullptr, module, MAX_PATH);
        folder = std::filesystem::path (module).parent_path().wstring();

        //  A path cut short at MAX_PATH would register a command that runs nothing.
        if (choices.registered && (length == 0 || length >= MAX_PATH))
        {
            hr = HRESULT_FROM_WIN32 (ERROR_INSUFFICIENT_BUFFER);
        }
        else
        {
            hr = choices.registered ? FileAssociations::Register (classes, LaunchCommand::GetSiblingPath (folder, LaunchCommand::kCassoExe), module)
                                    : FileAssociations::Unregister (classes, fileExts);
        }

        if (FAILED (hr))
        {
            m_window.ShowMessage (L"Windows did not take the change to the disk image types.", MB_ICONERROR);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDispatch::GetNamingStyle
//
//  The host file name style the Options dialog set; its choices are in the
//  style's own order.
//
////////////////////////////////////////////////////////////////////////////////

HostFileNaming::Style ExplorerDispatch::GetNamingStyle() const
{
    if (m_window.m_prefs.hostNaming == CassoExplorerPrefs::kNamingCiderPress)
    {
        return HostFileNaming::Style::CiderPress;
    }

    if (m_window.m_prefs.hostNaming == CassoExplorerPrefs::kNamingAppleSingle)
    {
        return HostFileNaming::Style::AppleSingle;
    }

    return HostFileNaming::Style::Descriptive;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDispatch::RunRecycleBinVerb
//
//  A verb on the Recycle Bin's items, carried out by the bin itself. Whether
//  the verb was the bin's to carry out.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerDispatch::RunRecycleBinVerb (CassoExplorerActions::Verb verb)
{
    using Verb = CassoExplorerActions::Verb;

    HRESULT                    hr = S_OK;
    std::vector<std::wstring>  ids;



    m_window.m_browser.GetSelectedRecycledIds (ids);

    switch (verb)
    {
        case Verb::Open:
            BAIL_OUT_IF (ids.empty(), S_OK);
            hr = m_window.m_shellVerbs.RunRecycledVerb (m_window.GetHwnd(), ids, IShellItemVerbs::RecycledVerb::Properties);
            break;

        case Verb::Restore:
        case Verb::Delete:
            //  Restore with nothing selected restores everything, as the
            //  command bar's Restore all items does.
            if (ids.empty() && verb == Verb::Restore)
            {
                m_window.m_browser.GetAllRecycledIds (ids);
            }

            BAIL_OUT_IF (ids.empty(), S_OK);
            hr = m_window.m_shellVerbs.RunRecycledVerb (m_window.GetHwnd(), ids, (verb == Verb::Restore) ? IShellItemVerbs::RecycledVerb::Restore
                                                                                       : IShellItemVerbs::RecycledVerb::Delete);
            m_window.m_folderSync.RefreshAfterHostChange();
            break;

        case Verb::EmptyRecycleBin:
            hr = m_window.m_shellVerbs.EmptyRecycleBin (m_window.GetHwnd());
            m_window.m_folderSync.RefreshAfterHostChange();
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
//  ExplorerDispatch::GetCommandLabel
//
//  The label of a command whose label follows the selection, or empty for
//  the command's own: Restore restores the selected items, or all of them.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerDispatch::GetCommandLabel (int id) const
{
    Location      drive;
    std::wstring  title;



    if (id == CassoExplorerCommands::kRestoreItems && !m_window.m_browser.GetSelectedRows().empty())
    {
        return L"Restore the selected items";
    }

    //  A linked Apple's drive by its slot, drive and volume: the open drive, or
    //  the drive selected on its machine.
    if (m_window.m_browser.IsAppleDriveOpen())
    {
        title = m_window.m_actions.DescribeAppleDrive();
    }
    else if (m_window.m_links.TryGetSelectedLinkDrive (drive))
    {
        title = m_window.m_browser.GetTreeModel().GetLinkDriveTitle (drive.path);
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
//  ExplorerDispatch::RunVerb
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerDispatch::RunVerb (CassoExplorerActions::Verb verb)
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
    if (m_window.m_browser.GetLocation().kind == Location::Kind::RecycleBin && RunRecycleBinVerb (verb))
    {
        return;
    }

    switch (verb)
    {
        case CassoExplorerActions::Verb::Open:
            if (m_window.m_browser.IsImageLocation() && !(m_window.m_browser.GetSelectedRows().size() == 1 && m_window.m_browser.OpenRow (m_window.m_browser.GetSelectedRows()[0])))
            {
                m_window.m_fileOps.OpenSelectedEntries();
            }
            else if (m_window.m_browser.IsImageLocation())
            {
                m_window.m_listPane.FillList();
            }
            else if (m_window.m_browser.GetSelectedRows().size() > 1)
            {
                m_window.m_fileOps.OpenEachSelected();
            }
            else if (!m_window.m_browser.GetSelectedRows().empty() && m_window.m_browser.OpenRow (m_window.m_browser.GetSelectedRows()[0]))
            {
                m_window.m_listPane.FillList();
            }
            else if (!m_window.m_fileOps.GetSelectedImagePath().empty())
            {
                //  Not something to browse: a real file, in its own program.
                hr = m_window.m_shellVerbs.Open (m_window.GetHwnd(), m_window.m_fileOps.GetSelectedImagePath());
            }

            break;

        case CassoExplorerActions::Verb::Get:
            hr = m_window.m_dialogs.PickFolder (m_window.GetHwnd(), picked, chosen);

            if (SUCCEEDED (hr) && chosen)
            {
                ReportOutcome (m_window.m_actions.GetSelected (picked.wstring(), style, m_window.m_fileOps.MakeCollisionPrompt()), L"Copy");
            }

            break;

        case CassoExplorerActions::Verb::Put:
            hr = m_window.m_dialogs.PickFileToOpen (m_window.GetHwnd(), spec, picked, chosen);

            if (SUCCEEDED (hr) && chosen)
            {
                outcome = m_window.m_actions.PutFiles ({ picked.wstring() }, m_window.m_fileOps.MakeAddressPrompt());

                //  An Apple drive's writes finish later, over the link, with no undo.
                if (!m_window.m_browser.IsAppleDriveLocation())
                {
                    m_window.m_fileOps.PushPutUndo (outcome, m_window.m_browser.GetLocation().path, m_window.m_browser.GetLocation().innerPath);
                }

                ReportOutcome (outcome, L"Put");
                m_window.m_listPane.FillList();
            }

            break;

        case CassoExplorerActions::Verb::Cut:
        case CassoExplorerActions::Verb::Copy:
            if (m_window.m_browser.IsVolumeLocation())
            {
                m_window.m_fileOps.CopyEntriesToClipboard (GetNamingStyle());
                break;
            }

            if (m_window.m_fileOps.TryGetSelectedShellIds (hostPaths))
            {
                hr = m_window.m_shellVerbs.PlaceOnClipboard (m_window.GetHwnd(), hostPaths, verb == CassoExplorerActions::Verb::Cut);
            }

            m_window.m_listPane.SetCutPaths ((SUCCEEDED (hr) && verb == CassoExplorerActions::Verb::Cut) ? hostPaths : std::vector<std::wstring>());
            m_window.m_listPane.FillList();
            break;

        case CassoExplorerActions::Verb::Share:
            m_window.m_browser.GetSelectedHostPaths (hostPaths);
            hr = m_window.m_shellVerbs.Share (m_window.GetHwnd(), hostPaths);
            break;

        case CassoExplorerActions::Verb::ExtractAll:
            ExtractAll();
            break;

        case CassoExplorerActions::Verb::ExtractFiles:
            ExtractImageFiles (m_window.m_fileOps.GetSelectedImagePath());
            break;

        case CassoExplorerActions::Verb::Paste:
            if (m_window.m_browser.IsVolumeLocation())
            {
                m_window.m_fileOps.PasteIntoVolume();
                break;
            }

            m_window.m_fileOps.PasteHere (m_window.m_fileOps.GetPasteFolder());
            break;

        case CassoExplorerActions::Verb::Delete:
            if (!m_window.m_browser.IsVolumeLocation())
            {
                //  Windows asks, and the Recycle Bin keeps it. A file inside a
                //  zip has no file of its own to recycle, so it is deleted for
                //  good after Windows' confirmation, as in File Explorer.
                if (m_window.m_previewPane.IsArchiveLocation())
                {
                    m_window.m_fileOps.DeleteSelectionPermanently();
                    break;
                }

                //  An item a shell folder such as Home lists may be a recent or
                //  a cloud file with no path of its own; its own Delete
                //  command removes it, as File Explorer's does.
                if (m_window.m_fileOps.TryGetSelectedShellIds (hostPaths))
                {
                    hr = (m_window.m_browser.GetLocation().kind == Location::Kind::ShellFolder)
                       ? m_window.m_shellVerbs.RunItemVerb (m_window.GetHwnd(), hostPaths, "delete")
                       : m_window.m_shellVerbs.Recycle (m_window.GetHwnd(), hostPaths);
                }

                if (FAILED (hr) && hr != HRESULT_FROM_WIN32 (ERROR_CANCELLED))
                {
                    m_window.ShowMessage (L"The selected items could not be deleted.", MB_ICONWARNING);
                }

                if (SUCCEEDED (hr) && !hostPaths.empty() && m_window.m_browser.GetLocation().kind == Location::Kind::HostFolder)
                {
                    m_window.m_fileOps.PushUndo (ExplorerFileOps::UndoStep { ExplorerFileOps::UndoStep::Kind::Recycle, Location(), hostPaths });
                }

                m_window.m_folderSync.RefreshAfterHostChange();
                break;
            }

        {
            std::vector<std::wstring>  plan  = m_window.m_actions.DescribeDeletePlan();
            size_t                     count = m_window.m_browser.GetSelectedRows().size();
            std::wstring               link;
            int                        slot  = 0;
            int                        drive = 0;
            std::wstring               where = TreeModel::TryParseLinkId (m_window.m_browser.GetLocation().path, link, slot, drive)
                                                ? std::format (L"the disk in drive {}", drive)
                                                : std::filesystem::path (m_window.m_browser.GetLocation().path).filename().wstring();
            std::wstring               what     = (count == 1) ? m_window.m_browser.GetRows()[(size_t) m_window.m_browser.GetSelectedRows()[0]].name
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
            answer = DxuiMessageBox (m_window.GetHwnd(), m_window.m_theme, message.c_str(), L"Delete", MB_OKCANCEL | MB_ICONWARNING, { L"Delete", L"Cancel" });

            if (answer == IDOK)
            {
                ExplorerFileOps::UndoStep  step { ExplorerFileOps::UndoStep::Kind::ImageDelete, m_window.m_browser.GetLocation() };
                wchar_t   temp[MAX_PATH] = {};

                //  An AppleSingle copy keeps each file's bytes, type and
                //  address, so a put brings it back as it was. The copy holds
                //  files only, so a deletion that takes a folder is not kept.
                m_window.m_browser.GetSelectedEntries (entries);

                if (!m_window.m_browser.IsAppleDriveLocation() &&
                    std::none_of (entries.begin(), entries.end(), [] (const FileEntry & entry) { return entry.isDirectory; }) &&
                    GetTempPathW (MAX_PATH, temp) != 0)
                {
                    std::error_code  ignore;

                    step.savedDir = std::format (L"{}CassoExplorerUndo\\{}", temp, GetTickCount64());
                    std::filesystem::create_directories (step.savedDir, ignore);
                }

                if (!step.savedDir.empty() && !m_window.m_actions.GetSelected (step.savedDir, HostFileNaming::Style::AppleSingle).Succeeded())
                {
                    step.savedDir.clear();
                }

                outcome = m_window.m_actions.DeleteSelected();

                if (outcome.Succeeded() && !step.savedDir.empty())
                {
                    m_window.m_fileOps.PushUndo (std::move (step));
                }

                ReportOutcome (outcome, L"Delete");
                m_window.m_listPane.FillList();
            }

            break;
        }

        case CassoExplorerActions::Verb::Boot:
            ReportOutcome (m_window.m_actions.BootSelected(), L"Set startup program");
            m_window.m_listPane.FillList();
            break;

        case CassoExplorerActions::Verb::Rename:
            m_window.m_fileOps.BeginRename();
            break;

        case CassoExplorerActions::Verb::InsertDrive1:
        case CassoExplorerActions::Verb::InsertDrive2:
            m_window.m_fileOps.InsertIntoDrive (m_window.m_fileOps.GetSelectedImagePath(), verb == CassoExplorerActions::Verb::InsertDrive1 ? 1 : 2);
            break;

        case CassoExplorerActions::Verb::OpenInNewCasso:
            m_window.m_fileOps.OpenInNewCasso (m_window.m_fileOps.GetSelectedImagePath());
            break;

        case CassoExplorerActions::Verb::NewDisk:
            newDisk = CassoExplorerNewDiskDialog::Ask (m_window.GetHwnd(), m_window.m_theme, false, [this] (const std::wstring & fileName)
            {
                return m_window.m_actions.IsNameTaken (m_window.m_browser.GetLocation().path, fileName);
            });

            if (newDisk.confirmed)
            {
                CreateDisk (newDisk);
            }

            break;

        case CassoExplorerActions::Verb::NewFolder:
            //  An unused default name, open for renaming at once.
            newName = m_window.m_actions.GetNewFolderName();

            if (!m_window.m_browser.IsVolumeLocation())
            {
                hr = m_window.m_shellVerbs.CreateFolder (m_window.GetHwnd(), m_window.m_browser.GetLocation().path, newName);

                if (SUCCEEDED (hr))
                {
                    m_window.m_fileOps.PushUndo (ExplorerFileOps::UndoStep { ExplorerFileOps::UndoStep::Kind::HostNewFolder, Location(), { (std::filesystem::path (m_window.m_browser.GetLocation().path) / newName).wstring() } });
                }

                m_window.m_folderSync.RefreshAfterHostChange();
            }
            else
            {
                outcome = m_window.m_actions.CreateFolder (newName);

                if (outcome.Succeeded() && !m_window.m_browser.IsAppleDriveLocation())
                {
                    m_window.m_fileOps.PushUndo (ExplorerFileOps::UndoStep { ExplorerFileOps::UndoStep::Kind::ImageNewFolder, m_window.m_browser.GetLocation(), {}, std::wstring(), newName });
                }

                ReportOutcome (outcome, L"New folder");
                m_window.m_listPane.FillList();
            }

            if (m_window.m_listPane.SelectRowNamed (newName))
            {
                m_window.m_fileOps.BeginRename();
            }

            break;

        case CassoExplorerActions::Verb::Format:
            newDisk = CassoExplorerNewDiskDialog::Ask (m_window.GetHwnd(), m_window.m_theme, true, {}, ChooseFormatDefault (m_window.m_actions.GetFormatTarget()),
                                                       std::filesystem::path (m_window.m_actions.GetFormatTarget()).filename().wstring());

            if (newDisk.confirmed)
            {
                answer = DxuiMessageBox (m_window.GetHwnd(), m_window.m_theme,
                                         (L"Format " + CassoExplorerActions::GetLeafName (m_window.m_actions.GetFormatTarget())
                                          + L"? Everything on it will be erased.").c_str(),
                                         L"Format", MB_OKCANCEL | MB_ICONWARNING, { L"Format", L"Cancel" });

                if (answer == IDOK)
                {
                    ReportOutcome (m_window.m_actions.FormatImage (newDisk.request), L"Format");
                    m_window.m_listPane.FillList();
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
    m_window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDispatch::ReportOutcome
//
//  A failure shows what the runner said; a success is quiet, since the list
//  already shows the result.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerDispatch::ReportOutcome (const CassoExplorerActions::Outcome & outcome, const wchar_t * verbName)
{
    if (outcome.Succeeded())
    {
        m_window.m_statusBar.SetDetail (std::format (L"{}: {} file(s)", verbName, outcome.written));

        //  For a copy salvaged from a damaged disk, what could not be read is shown.
        if (!outcome.warning.empty())
        {
            m_window.ShowMessage (outcome.warning, MB_ICONWARNING);
        }

        return;
    }

    m_window.ShowMessage (outcome.message.empty() ? std::wstring (verbName) + L" did not complete." : outcome.message, MB_ICONERROR);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDispatch::ChooseFormatDefault
//
//  The file system the image holds now, when it holds one; otherwise what
//  its size and kind allow. DOS 3.3 fits only a 140K floppy, so anything
//  larger -- an 800K disk, a hard disk image -- starts at ProDOS, and so does
//  a ProDOS-order image.
//
////////////////////////////////////////////////////////////////////////////////

int ExplorerDispatch::ChooseFormatDefault (const std::wstring & image)
{
    VolumeListing              listing;
    VolumeKind                 kind      = VolumeKind::Unknown;
    WIN32_FILE_ATTRIBUTE_DATA  data      = {};
    std::wstring               extension = std::filesystem::path (image).extension().wstring();
    uint64_t                   size      = 0;



    if (m_window.m_browser.GetOperations().List (TextEncoding::WideToNarrow (image), listing, kind).Succeeded())
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
//  ExplorerDispatch::GetVerbTitle
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerDispatch::GetVerbTitle (CassoExplorerActions::Verb verb)
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
//  ExplorerDispatch::RunRawVerb
//
//  A read asks where the bytes start and how many, then where to save them;
//  a write asks for the file, then where it goes, then confirms, since it
//  overwrites whatever is there with no file system to stop it.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerDispatch::RunRawVerb (CassoExplorerActions::Verb verb)
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
    std::wstring                     target   = m_window.m_actions.GetFormatTarget();



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
        hr         = m_window.m_dialogs.PickFileToOpen (m_window.GetHwnd(), spec, picked, chosen);

        if (FAILED (hr) || !chosen)
        {
            return;
        }
    }

    where = CassoExplorerRawDialog::Ask (m_window.GetHwnd(), m_window.m_theme, title, sectors, reading);

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
            hr = m_window.m_dialogs.PickFileToSave (m_window.GetHwnd(), spec, picked, chosen);

            if (FAILED (hr) || !chosen)
            {
                return;
            }

            spec.initialFolder   = picked.parent_path();
            spec.defaultFileName = picked.filename().wstring();
        }
        while (GetFileAttributesW (picked.c_str()) != INVALID_FILE_ATTRIBUTES &&
               DxuiMessageBox (m_window.GetHwnd(), m_window.m_theme,
                               (picked.filename().wstring() + L" already exists. Overwrite it?").c_str(),
                               L"Confirm save as", MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2, { L"Overwrite", L"Cancel" }) != IDOK);

        if (!IsRawTargetCurrent (target))
        {
            return;
        }

        outcome = sectors ? m_window.m_actions.ReadSectors (where.start, where.sector, where.count, picked.wstring(), where.numbering)
                          : m_window.m_actions.ReadBlocks  (where.start, where.count, picked.wstring());

        ReportOutcome (outcome, reading ? L"Read" : L"Write");
        return;
    }

    answer = DxuiMessageBox (m_window.GetHwnd(), m_window.m_theme,
                             (L"The file's bytes will be written straight onto " + CassoExplorerActions::GetLeafName (target) + L", over what is there now.").c_str(),
                             title.c_str(),
                             MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2, { L"Overwrite", L"Cancel" });

    if (answer != IDOK || !IsRawTargetCurrent (target))
    {
        return;
    }

    outcome = sectors ? m_window.m_actions.WriteSectors (where.start, where.sector, picked.wstring(), where.numbering)
                      : m_window.m_actions.WriteBlocks  (where.start, picked.wstring());

    ReportOutcome (outcome, L"Write");
    m_window.m_listPane.FillList();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDispatch::IsRawTargetCurrent
//
//  The window's timers run while the dialogs are open, and a refresh can move
//  the list off the image the raw read or write was chosen for.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerDispatch::IsRawTargetCurrent (const std::wstring & target)
{
    if (m_window.m_actions.GetFormatTarget() == target)
    {
        return true;
    }

    m_window.ShowMessage (CassoExplorerActions::GetLeafName (target) + L" is no longer selected, so nothing was read or written.", MB_ICONWARNING);
    return false;
}
