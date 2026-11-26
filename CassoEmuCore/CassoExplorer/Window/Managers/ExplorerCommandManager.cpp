#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"
#include "CassoExplorer/Window/Managers/ExplorerDragDrop.h"
#include "CassoExplorer/Window/Managers/ExplorerImageTools.h"
#include "CassoExplorer/Window/Managers/ExplorerVerbs.h"
#include "CassoExplorer/CassoExplorerCopyOptionsDialog.h"
#include "CassoExplorer/Picker/PickerRequest.h"
#include "CassoExplorer/Window/Managers/ExplorerCollectionTools.h"
#include "CassoExplorer/Window/Managers/ExplorerPreviewOutput.h"
#include "CassoExplorer/CassoExplorerIcons.h"
#include "AssetBootstrap.h"
#include "CassoExplorer/CassoExplorerRawDialog.h"
#include "CassoExplorer/CassoExplorerOptionsDialog.h"
#include "CassoExplorer/CassoExplorerPromptDialog.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/Model/LaunchCommand.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "CassoExplorer/Window/Panes/ExplorerPickerSearch.h"
#include "Config/FileAssociations.h"
#include "Seams/Win32UserClasses.h"
#include "Update/Win32InstallEnvironment.h"
#include "Core/TextEncoding.h"
#include "Core/AppTimerIds.h"
#include "CassoExplorer/Window/Panes/ExplorerToolbars.h"
#include "CassoExplorer/CassoExplorerCommands.h"
#include "CassoExplorer/Window/Managers/ExplorerInput.h"
#include "CassoExplorer/Window/Managers/ExplorerPreviewManager.h"
#include "CassoExplorer/Window/Managers/ExplorerClipboard.h"
#include "CassoExplorer/Window/Managers/ExplorerTransferPrompts.h"
#include "CassoExplorer/Window/Managers/ExplorerDeletion.h"
#include "CassoExplorer/Window/Managers/ExplorerUndo.h"
#include "CassoExplorer/Window/Managers/ExplorerRename.h"
#include "CassoExplorer/Window/Managers/ExplorerManifestSync.h"
#include "CassoExplorer/Window/Managers/ExplorerOpen.h"
#include "CassoExplorer/Window/Managers/ExplorerCassoTarget.h"
#include "CassoExplorer/Window/Managers/ExplorerProperties.h"
#include "CassoExplorer/Window/Managers/ExplorerFolderSync.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "Seams/Win32ShellItemVerbs.h"
#include "CassoExplorer/Window/Panes/ExplorerStatusBar.h"
#include "CassoExplorer/Window/Managers/ExplorerMenus.h"
#include "Seams/IHostDialogs.h"
#include "Seams/Win32HostDialogs.h"
#include "CassoExplorer/Window/Managers/CassoLinkManager.h"
#include "CassoExplorer/Window/Managers/CassoLinkDriveManager.h"
#include "CassoExplorer/Window/Managers/FullSendWindowManager.h"
#include "CassoExplorer/Window/Managers/ExplorerThemes.h"
#include "CassoExplorer/Window/Managers/ExplorerLayout.h"
#include "CassoExplorer/Window/Panes/ExplorerAddressBar.h"
#include "CassoExplorer/Window/Managers/ExplorerTabManager.h"
#include "CassoExplorer/WozPropertiesDialog.h"





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
    Location              pasteDrive;



    //  A picker has only the commands on its list.
    if (IsBlockedInPicker (id))
    {
        return false;
    }

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
        case CassoExplorerCommands::kUp:                return m_context.browser.GetNavigator().CanGoUp();
        case CassoExplorerCommands::kLineAddresses:
        case CassoExplorerCommands::kCrossReference:    return m_context.preview.GetBarMode() == CassoExplorerCommands::kPreviewBarListing;
        case CassoExplorerCommands::kPictureMonochrome:
        case CassoExplorerCommands::kPictureColor:      return m_context.browser.GetPreviewer().GetMonitorChoice().CanTogglePictureColor();
        case CassoExplorerCommands::kGetFonts:          return m_context.browser.GetPreviewer().GetPreview().fontsMissing && !AssetBootstrap::AreIIgsFontsCached();
        case CassoExplorerCommands::kPictureCrt:
        case CassoExplorerCommands::kSavePicture:       return m_context.browser.GetPreviewer().GetPreview().kind == PreviewContent::Kind::Picture && !m_context.browser.GetPreviewer().GetPreview().bgra.empty();
        case CassoExplorerCommands::kExpandPreview:     return m_context.preview.IsShown();
        case CassoExplorerCommands::kViewAs:            return !m_context.browser.GetPreviewer().GetPreview().viewChoices.empty();
        case CassoExplorerCommands::kCopyPreview:       return ExplorerPreviewOutput::CanCopy (m_context);
        case CassoExplorerCommands::kSavePreviewAs:     return ExplorerPreviewOutput::CanSaveAs (m_context);
        case CassoExplorerCommands::kSaveResultsCsv:    return ExplorerCollectionTools::CanSaveCsv (m_context);
        case CassoExplorerCommands::kDiskList:
        case CassoExplorerCommands::kFindDuplicates:    return !ExplorerCollectionTools::GetFolder (m_context).empty();
        case CassoExplorerCommands::kPreviousFile:      return m_context.preview.CanStep (false);
        case CassoExplorerCommands::kNextFile:          return m_context.preview.CanStep (true);
        case CassoExplorerCommands::kEditWozMetadata:   return !m_context.browser.GetPreviewer().GetPreview().wozPath.empty();
        case CassoExplorerCommands::kCharsetAppleText:
        case CassoExplorerCommands::kCharsetMacOsRoman:
        case CassoExplorerCommands::kCharsetIso8859:    return m_context.browser.GetPreviewer().GetPreview().charset != Charset::Default;
        case CassoExplorerCommands::kRecordLength:      return m_context.browser.GetPreviewer().GetPreview().recordLength > 0;
        case CassoExplorerCommands::kSheetValues:
        case CassoExplorerCommands::kSheetFormulas:     return m_context.browser.GetPreviewer().GetPreview().sheetForm != SheetForm::None;
        case CassoExplorerCommands::kFindNext:          return m_context.preview.GetFind().CanFindNext();

        //  The search box finds in the hex view and filters a VAR file's variables.
        case CassoExplorerCommands::kFind:              return m_context.preview.GetHex().IsShowing() || m_context.preview.GetListing().IsVariables();
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
        case CassoExplorerCommands::kUndo:              return m_context.undo.CanUndo();
        case CassoExplorerCommands::kCutItems:          return m_context.listPane.GetMenus().IsListVerbOffered (CassoExplorerActions::Verb::Cut);
        case CassoExplorerCommands::kCopyItems:         return m_context.listPane.GetMenus().IsListVerbOffered (CassoExplorerActions::Verb::Copy) || m_context.listPane.GetMenus().IsListVerbOffered (CassoExplorerActions::Verb::Get) || m_context.browser.GetSearchMatches().AreSelectedRowsSearchMatches();
        case CassoExplorerCommands::kPasteItems:        return (m_context.browser.IsVolumeLocation() || m_context.dragDrop.TryGetPasteDrive (pasteDrive) || m_context.browser.GetLocation().kind == Location::Kind::HostFolder)
                                                         && m_context.shellVerbs.ClipboardHasFiles();
        case CassoExplorerCommands::kRenameItem:        return m_context.listPane.GetMenus().IsListVerbOffered (CassoExplorerActions::Verb::Rename);
        case CassoExplorerCommands::kShareItems:        return m_context.listPane.GetMenus().IsListVerbOffered (CassoExplorerActions::Verb::Share);
        case CassoExplorerCommands::kExtractAll:        return !m_context.browser.GetArchives().GetExtractTarget().empty();
        case CassoExplorerCommands::kInspectDiskImage:  return m_context.actions.CanInspect();
        case CassoExplorerCommands::kCompareDiskImages: return m_context.actions.CanCompare();
        case CassoExplorerCommands::kInitializeCard:    return m_context.app.cards != nullptr;
        case CassoExplorerCommands::kReadDiskToImage:
        case CassoExplorerCommands::kWriteImageToDisk:  return m_context.browser.IsAppleDriveOpen() || m_context.linkDrives.TryGetSelectedLinkDrive (drive);
        case CassoExplorerCommands::kCancelLinkOperation: return (m_context.browser.IsAppleDriveOpen() || m_context.linkDrives.TryGetSelectedLinkDrive (drive)) && m_context.app.links != nullptr && !m_context.app.links->GetPendingJobs().empty();
        case CassoExplorerCommands::kDeleteItems:       return m_context.listPane.GetMenus().IsListVerbOffered (CassoExplorerActions::Verb::Delete);
        case CassoExplorerCommands::kCloseSearch:
        case CassoExplorerCommands::kSearchOptions:     return true;
        case CassoExplorerCommands::kSortByRelevance:   return SearchQuery::IsId (m_context.browser.GetLocation().path);
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



    if (id >= CassoExplorerCommands::kGroupByField && id <= CassoExplorerCommands::kGroupByLast)
    {
        return model.HasTabs() && (int) model.GetActiveTab().groupBy == id - CassoExplorerCommands::kGroupByField;
    }

    if (id >= CassoExplorerCommands::kSortByColumn && id < CassoExplorerCommands::kSortByColumn + GetSortColumnCount())
    {
        return model.HasTabs() && (int) model.GetActiveTab().sortColumn == id - CassoExplorerCommands::kSortByColumn;
    }

    if (id == CassoExplorerCommands::kSortByRelevance)
    {
        return model.HasTabs() && model.GetActiveTab().sortColumn == CatalogModel::kRelevance;
    }

    if (id >= CassoExplorerCommands::kViewFirst && id < CassoExplorerCommands::kViewFirst + CassoExplorerPrefs::kViewCount)
    {
        return (int) m_context.listPane.GetColumns().GetView() == id - CassoExplorerCommands::kViewFirst;
    }

    switch (id)
    {
        case CassoExplorerCommands::kTogglePreview:     return m_context.prefs.previewVisible;
        case CassoExplorerCommands::kExpandPreview:     return m_context.preview.IsExpanded();
        case CassoExplorerCommands::kViewAsDefault:
        case CassoExplorerCommands::kViewAsText:
        case CassoExplorerCommands::kViewAsHex:
        case CassoExplorerCommands::kViewAsDisassembly:
        case CassoExplorerCommands::kViewAsHiRes:
        case CassoExplorerCommands::kViewAsDoubleHiRes:
        case CassoExplorerCommands::kViewAsLoRes:
        case CassoExplorerCommands::kViewAsResourceFork: return (int) m_context.browser.GetPreviewer().GetPreview().viewAs == id - CassoExplorerCommands::kViewAsDefault;
        case CassoExplorerCommands::kSortAscending:     return model.HasTabs() && !model.GetActiveTab().sortDescending;
        case CassoExplorerCommands::kSortDescending:    return model.HasTabs() && model.GetActiveTab().sortDescending;
        case CassoExplorerCommands::kGroupAscending:    return model.HasTabs() && !model.GetActiveTab().groupDescending;
        case CassoExplorerCommands::kGroupDescending:   return model.HasTabs() && model.GetActiveTab().groupDescending;
        case CassoExplorerCommands::kLineAddresses:     return m_context.prefs.lineAddresses;
        case CassoExplorerCommands::kPictureCrt:        return m_context.prefs.previewCrt;
        case CassoExplorerCommands::kCrossReference:    return m_context.preview.GetText().IsCrossReference();
        case CassoExplorerCommands::kPictureMonochrome: return m_context.browser.GetPreviewer().GetMonitorChoice().CanTogglePictureColor() && m_context.browser.GetPreviewer().GetPreview().pictureMode == PicturePreview::Mode::DoubleHiResMono;
        case CassoExplorerCommands::kPictureColor:      return m_context.browser.GetPreviewer().GetMonitorChoice().CanTogglePictureColor() && m_context.browser.GetPreviewer().GetPreview().pictureMode != PicturePreview::Mode::DoubleHiResMono;
        case CassoExplorerCommands::kCharsetAppleText:  return m_context.browser.GetPreviewer().GetPreview().charset == Charset::AppleText;
        case CassoExplorerCommands::kCharsetMacOsRoman: return m_context.browser.GetPreviewer().GetPreview().charset == Charset::MacOsRoman;
        case CassoExplorerCommands::kCharsetIso8859:    return m_context.browser.GetPreviewer().GetPreview().charset == Charset::Iso8859_1;
        case CassoExplorerCommands::kSheetValues:       return m_context.browser.GetPreviewer().GetPreview().sheetForm == SheetForm::Values;
        case CassoExplorerCommands::kSheetFormulas:     return m_context.browser.GetPreviewer().GetPreview().sheetForm == SheetForm::Formulas;
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
        default:                                  return m_context.themes.IsCommandChecked (id);
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
    bool     routed = false;



    //  A picker has only the commands on its list, whatever sent this one.
    if (IsBlockedInPicker (id))
    {
        return;
    }

    routed = DxuiCommandRouter::Invoke (m_context.input.GetFocusedControl(), CassoExplorerCommands::GetStandardCommand (id));

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
    if (id >= CassoExplorerCommands::kGroupByField && id <= CassoExplorerCommands::kGroupByLast)
    {
        RowGrouping::Field  field = (RowGrouping::Field) (id - CassoExplorerCommands::kGroupByField);

        if (!IsChecked (id))
        {
            m_context.browser.GetRowOrder().SetGroupBy (field, field == RowGrouping::Field::DateModified || field == RowGrouping::Field::DateDeleted || field == RowGrouping::Field::Relevance);
            m_context.listPane.GetColumns().RememberFolderSort();
            m_context.listPane.FillList();
            m_context.toolbars.SetCommandBarDropDowns();
        }

        return;
    }

    if ((id >= CassoExplorerCommands::kSortByColumn && id < CassoExplorerCommands::kSortByColumn + GetSortColumnCount()) || id == CassoExplorerCommands::kSortByRelevance)
    {
        int  column = (id == CassoExplorerCommands::kSortByRelevance) ? (int) CatalogModel::kRelevance : id - CassoExplorerCommands::kSortByColumn;

        //  A new column sorts ascending, and Relevance descending, as File
        //  Explorer sorts them; the column already in use keeps its direction.
        if (!IsChecked (id))
        {
            m_context.browser.GetRowOrder().SortByColumn (column);

            if (m_context.browser.GetBrowserModel().GetActiveTab().sortDescending != (id == CassoExplorerCommands::kSortByRelevance))
            {
                m_context.browser.GetRowOrder().SortByColumn (column);
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

        case CassoExplorerCommands::kUndo:         m_context.undo.UndoLast();                                   break;
        case CassoExplorerCommands::kCutItems:     m_context.verbs.RunVerb (CassoExplorerActions::Verb::Cut);    break;
        //  Ctrl+V and the command bar paste into the folder shown, whatever is
        //  selected, as Explorer's do; a folder's own menu pastes into it.
        case CassoExplorerCommands::kPasteItems:
        {
            Location  drive;

            if (m_context.dragDrop.TryGetPasteDrive (drive))
            {
                m_context.dragDrop.PasteOnAppleDrive (drive);
                break;
            }

            if (m_context.browser.IsVolumeLocation())
            {
                m_context.clipboard.PasteIntoVolume();
                break;
            }

            m_context.clipboard.PasteHere (m_context.browser.GetLocation().path);
            break;
        }

        case CassoExplorerCommands::kRenameItem:   m_context.verbs.RunVerb (CassoExplorerActions::Verb::Rename); break;
        case CassoExplorerCommands::kShareItems:   m_context.verbs.RunVerb (CassoExplorerActions::Verb::Share);  break;
        case CassoExplorerCommands::kExtractAll:   m_context.imageTools.ExtractAll();                                  break;
        case CassoExplorerCommands::kInspectDiskImage:  m_context.verbs.RunVerb (CassoExplorerActions::Verb::InspectDiskImage);  break;
        case CassoExplorerCommands::kCompareDiskImages: m_context.verbs.RunVerb (CassoExplorerActions::Verb::CompareDiskImages); break;
        case CassoExplorerCommands::kInitializeCard:    m_context.opener.InitializeShownCard();                   break;
        case CassoExplorerCommands::kReadDiskToImage:     m_context.linkDrives.ReadDiskToImage();                         break;
        case CassoExplorerCommands::kWriteImageToDisk:    m_context.linkDrives.WriteImageToDisk();                        break;
        case CassoExplorerCommands::kCancelLinkOperation: m_context.actions.GetAppleDrive().CancelLinkOperation();     break;
        case CassoExplorerCommands::kDeleteItems:  m_context.verbs.RunVerb (CassoExplorerActions::Verb::Delete); break;
        case CassoExplorerCommands::kCloseSearch:     m_context.addressBar.CloseSearch(); break;
        case CassoExplorerCommands::kEmptyRecycleBin: m_context.verbs.RunRecycleBinVerb (CassoExplorerActions::Verb::EmptyRecycleBin); break;
        case CassoExplorerCommands::kRestoreItems:    m_context.verbs.RunRecycleBinVerb (CassoExplorerActions::Verb::Restore); break;
        case CassoExplorerCommands::kNewFolder:    m_context.verbs.RunVerb (CassoExplorerActions::Verb::NewFolder); break;
        case CassoExplorerCommands::kNewDisk:      m_context.verbs.RunVerb (CassoExplorerActions::Verb::NewDisk);   break;

        //  Either way the selection goes on the clipboard: a real file through
        //  the shell, an entry in an image as the file a paste would write,
        //  in the style the Options dialog sets.
        case CassoExplorerCommands::kCopyItems:
            if (m_context.browser.IsVolumeLocation() || m_context.browser.GetSearchMatches().AreSelectedRowsSearchMatches())
            {
                m_context.clipboard.CopyEntriesToClipboard (m_context.verbs.GetNamingStyle());
            }
            else
            {
                m_context.verbs.RunVerb (CassoExplorerActions::Verb::Copy);
            }

            break;

        case CassoExplorerCommands::kSortAscending:
        case CassoExplorerCommands::kSortDescending:
            if (!IsChecked (id) && m_context.browser.GetBrowserModel().HasTabs())
            {
                m_context.browser.GetRowOrder().SortByColumn ((int) m_context.browser.GetBrowserModel().GetActiveTab().sortColumn);
                m_context.listPane.GetColumns().RememberFolderSort();
                m_context.listPane.FillList();
            }

            break;

        case CassoExplorerCommands::kGroupAscending:
        case CassoExplorerCommands::kGroupDescending:
            if (!IsChecked (id) && m_context.browser.GetBrowserModel().HasTabs())
            {
                m_context.browser.GetRowOrder().SetGroupBy (m_context.browser.GetBrowserModel().GetActiveTab().groupBy, id == CassoExplorerCommands::kGroupDescending);
                m_context.listPane.GetColumns().RememberFolderSort();
                m_context.listPane.FillList();
            }

            break;


        case CassoExplorerCommands::kRefresh:
            if (m_context.browser.IsSearchRunning())
            {
                m_context.browser.StopSearch();
                break;
            }

            m_context.listPane.GetColumns().ReadFolderOptions();
            m_context.folderSync.ReloadKnownFolders();

            //  A storage card put in or taken out since the last look is
            //  added to the tree or removed from it.
            m_context.folderSync.CheckDrives();

            if (m_context.app.firewall != nullptr)
            {
                m_context.app.firewall->Invalidate();
            }

            m_context.browser.GetBrowserModel().InvalidateAllCatalogs();
            m_context.browser.GetLoader().ForgetLinkVisit();
            hr = m_context.browser.Refresh();

            //  The list's rows are hidden for a moment and then refilled, as
            //  File Explorer's are, so the refresh can be seen. Its scroll
            //  position does not change.
            m_context.listPane.SetContentHidden (true);
            SetTimer (m_context.window.GetHwnd(), (UINT_PTR) AppTimerId::ExplorerRefresh, kRefreshBlankMs, nullptr);
            break;

        case CassoExplorerCommands::kTogglePreview:
            m_context.prefs.previewVisible = !m_context.prefs.previewVisible;
            m_context.preview.SetPinned (m_context.prefs.previewVisible);
            m_context.preview.FillPreview();
            m_context.layout.RecomputeLayout();
            break;

        case CassoExplorerCommands::kExpandPreview:      m_context.preview.SetExpanded (!m_context.preview.IsExpanded()); break;
        case CassoExplorerCommands::kCopyPreview:        ExplorerPreviewOutput::Copy (m_context);                           break;
        case CassoExplorerCommands::kSavePreviewAs:      ExplorerPreviewOutput::SaveAs (m_context);                         break;
        case CassoExplorerCommands::kSaveResultsCsv:     ExplorerCollectionTools::SaveCsv (m_context);                      break;
        case CassoExplorerCommands::kDiskList:           ExplorerCollectionTools::Open (m_context, CollectionQuery::Tool::DiskList);   break;
        case CassoExplorerCommands::kFindDuplicates:     ExplorerCollectionTools::Open (m_context, CollectionQuery::Tool::Duplicates); break;
        case CassoExplorerCommands::kPreviousFile:
        case CassoExplorerCommands::kNextFile:           m_context.preview.Step (id == CassoExplorerCommands::kNextFile);  break;
        case CassoExplorerCommands::kViewAsDefault:
        case CassoExplorerCommands::kViewAsText:
        case CassoExplorerCommands::kViewAsHex:
        case CassoExplorerCommands::kViewAsDisassembly:
        case CassoExplorerCommands::kViewAsHiRes:
        case CassoExplorerCommands::kViewAsDoubleHiRes:
        case CassoExplorerCommands::kViewAsLoRes:
        case CassoExplorerCommands::kViewAsResourceFork:
            m_context.browser.GetPreviewer().GetViewState().SetViewAs ((ViewChoice) (id - CassoExplorerCommands::kViewAsDefault));

            //  Hex and Disassembly are the two forms of the hex view, so each
            //  selects its form.
            if (id == CassoExplorerCommands::kViewAsHex || id == CassoExplorerCommands::kViewAsDisassembly)
            {
                m_context.browser.GetPreviewer().GetViewState().SetHexForm ((id == CassoExplorerCommands::kViewAsHex) ? PreviewDecoder::HexForm::Bytes : PreviewDecoder::HexForm::Disassembly);
            }

            m_context.preview.FillPreview();
            break;

        case CassoExplorerCommands::kPictureMonochrome:  m_context.preview.GetPicture().ShowForm (false);                               break;
        case CassoExplorerCommands::kPictureColor:       m_context.preview.GetPicture().ShowForm (true);                                break;
        case CassoExplorerCommands::kSavePicture:        m_context.preview.GetPicture().SavePicture();                                  break;
        case CassoExplorerCommands::kPictureCrt:         m_context.preview.GetPicture().ToggleCrt();                                    break;
        case CassoExplorerCommands::kGetFonts:           GetFonts();                                                                    break;
        case CassoExplorerCommands::kEditWozMetadata:    m_context.properties.ShowWozProperties (m_context.browser.GetPreviewer().GetPreview().wozPath, WozPropertiesDialog::kMetaTab); break;

        case CassoExplorerCommands::kCharsetAppleText:   ChooseCharset (Charset::AppleText);  break;
        case CassoExplorerCommands::kCharsetMacOsRoman:  ChooseCharset (Charset::MacOsRoman); break;
        case CassoExplorerCommands::kCharsetIso8859:     ChooseCharset (Charset::Iso8859_1);  break;
        case CassoExplorerCommands::kRecordLength:       m_context.preview.ChooseRecordLength(); break;
        case CassoExplorerCommands::kSheetValues:        ChooseSheetForm (false);             break;
        case CassoExplorerCommands::kSheetFormulas:      ChooseSheetForm (true);              break;

        case CassoExplorerCommands::kGoToOffset:
            m_context.preview.GetFind().FocusGoTo();
            break;

        case CassoExplorerCommands::kLineAddresses:
            m_context.preview.GetText().ToggleLineAddresses();
            break;

        case CassoExplorerCommands::kCrossReference:
            m_context.preview.GetText().ToggleCrossReference();
            break;

        //  Find searches the hex preview while it has focus; elsewhere it goes
        //  to the search box, as Explorer's Ctrl+E and Ctrl+F do.
        case CassoExplorerCommands::kFind:
            if (m_context.pickerSearch.HasBox())
            {
                m_context.pickerSearch.Focus();
            }
            else if (!m_context.preview.GetFind().FocusFind())
            {
                m_context.input.SetFocusPane (ExplorerPane::LocationSearch);
            }

            break;

        case CassoExplorerCommands::kZoomIn:         m_context.preview.SetPreviewZoom (m_context.preview.GetShownZoom() + kPreviewZoomStep); break;
        case CassoExplorerCommands::kZoomOut:        m_context.preview.SetPreviewZoom (m_context.preview.GetShownZoom() - kPreviewZoomStep); break;
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
        case CassoExplorerCommands::kFullSendStatus:    m_context.fullSendWindows.ShowFullSendWindow (true); break;

        case CassoExplorerCommands::kBack:    refill = m_context.browser.GetNavigator().GoBack();    break;
        case CassoExplorerCommands::kForward: refill = m_context.browser.GetNavigator().GoForward(); break;
        case CassoExplorerCommands::kUp:      refill = m_context.browser.GetNavigator().GoUp();      break;

        case CassoExplorerCommands::kEditAddress:
            m_context.input.SetFocusPane (ExplorerPane::Address);
            m_context.addressBar.BeginEdit();
            break;

        case CassoExplorerCommands::kAddressHistory:
            m_context.input.SetFocusPane (ExplorerPane::Address);
            m_context.addressBar.BeginEdit();
            m_context.addressBar.ShowHistoryDropDown();
            break;

        case CassoExplorerCommands::kCheckForUpdates:
            if (m_context.app.host != nullptr)
            {
                m_context.app.host->CheckForUpdates (m_context.window.GetHwnd());
            }

            break;

        case CassoExplorerCommands::kAbout:
            m_context.window.ShowAbout();
            break;

        case CassoExplorerCommands::kAddLink:
            m_context.cassoLinkManager.EditConnection (std::string());
            break;

        case CassoExplorerCommands::kNewTab:
            m_context.browser.GetNavigator().NewTab();
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

        //  A Theme menu row, or no command.
        default:
            m_context.themes.SelectForCommand (id);
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
//  ExplorerCommandManager::ChooseSheetForm
//
//  Lists the previewed spreadsheet again as formulas or as values.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerCommandManager::ChooseSheetForm (bool formulas)
{
    m_context.browser.GetPreviewer().GetViewState().SetFormulas (formulas);
    m_context.preview.FillPreview();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCommandManager::ChooseCharset
//
//  Reads the previewed text again in a character set.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerCommandManager::ChooseCharset (Charset charset)
{
    m_context.browser.GetPreviewer().GetViewState().SetCharset (charset);
    m_context.preview.FillPreview();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCommandManager::GetFonts
//
//  The fonts disks downloaded, then the picture made again with its text.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerCommandManager::GetFonts()
{
    std::string  error;
    HRESULT      hr    = AssetBootstrap::EnsureIIgsFonts (error);



    if (FAILED (hr))
    {
        m_context.window.ShowMessage (std::format (L"The Apple IIgs fonts could not be downloaded. {}", TextEncoding::NarrowToWide (error)), MB_ICONERROR);
        return;
    }

    if (m_context.preview.TryLoadFonts())
    {
        m_context.preview.FillPreview();
    }
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
    Win32UserClasses         settings         (Win32UserClasses::kCassoRoot);
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
    choices.hostNaming   = (int) m_context.verbs.GetNamingStyle();
    choices.registered   = wasRegistered;
    choices.rawCopyMenu  = m_context.prefs.rawCopyMenu;
    choices.packaged     = packaged;
    choices.autoPreview  = m_context.prefs.autoPreview;
    choices.enterExpands = m_context.prefs.enterExpandsPreview;
    choices.autoThumbnails = m_context.prefs.autoThumbnails;
    choices.fullSendFolder    = m_context.prefs.fullSendFolder.empty() ? FullSendWindowManager::GetDefaultFullSendFolder() : m_context.prefs.fullSendFolder;
    choices.fullSendExtension = m_context.prefs.fullSendExtension;
    choices.bootSoundPlace    = m_context.prefs.bootSoundPlace;
    choices.verifyReads       = m_context.prefs.verifyReads;
    choices.verifyWrites      = m_context.prefs.verifyWrites;
    choices.autoVerify        = m_context.prefs.autoVerify;
    choices.fullSendProtection = m_context.prefs.fullSendProtection;

    choices.monitor      = (int) m_context.preview.GetPicture().GetMonitor();
    choices.customArgb   = m_context.prefs.previewCustomArgb;
    choices.crtOverrides = m_context.prefs.crtOverrides;

    //  The Display page's changes are drawn in the preview as they are made,
    //  and Cancel draws the preview as it was.
    display.pickFolder    = [this] (HWND owner, std::wstring & ioFolder)
    {
        std::filesystem::path  picked;
        bool                   chosen = false;
        HRESULT                result = m_context.dialogs.PickFolder (owner, picked, chosen);

        if (SUCCEEDED (result) && chosen)
        {
            ioFolder = picked.wstring();
        }
    };
    if (m_context.app.pictures != nullptr)
    {
    }

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

    m_context.prefs.autoPreview         = choices.autoPreview;
    m_context.prefs.enterExpandsPreview = choices.enterExpands;
    m_context.prefs.autoThumbnails      = choices.autoThumbnails;
    m_context.prefs.rawCopyMenu         = choices.rawCopyMenu;
    m_context.prefs.fullSendFolder    = choices.fullSendFolder == FullSendWindowManager::GetDefaultFullSendFolder() ? std::wstring() : choices.fullSendFolder;
    m_context.prefs.fullSendExtension = choices.fullSendExtension;
    m_context.prefs.bootSoundPlace    = choices.bootSoundPlace;
    m_context.prefs.verifyReads       = choices.verifyReads;
    m_context.prefs.verifyWrites      = choices.verifyWrites;
    m_context.prefs.autoVerify        = choices.autoVerify;
    m_context.prefs.fullSendProtection = choices.fullSendProtection;
    m_context.fullSendWindows.ApplyFullSendPrefs();
    m_context.cassoLinkManager.ApplyVerifyPrefs();


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

        //  Turned off here, the types stay off when a copy next starts.
        if (SUCCEEDED (hr))
        {
            hr = FileAssociations::SetDeclined (settings, !choices.registered);
        }

        if (FAILED (hr))
        {
            m_context.window.ShowMessage (L"Windows did not take the change to the disk image types.", MB_ICONERROR);
        }
    }
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

    if (id == CassoExplorerCommands::kViewAsDefault && !m_context.browser.GetPreviewer().GetPreview().defaultView.empty())
    {
        return m_context.browser.GetPreviewer().GetPreview().defaultView + L" (default)";
    }

    //  A linked Apple's drive by its slot, drive and volume: the open drive, or
    //  the drive selected on its machine.
    if (m_context.browser.IsAppleDriveOpen())
    {
        title = m_context.actions.GetAppleDrive().DescribeAppleDrive();
    }
    else if (m_context.linkDrives.TryGetSelectedLinkDrive (drive))
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
//  ExplorerCommandManager::GetSortColumnCount
//
////////////////////////////////////////////////////////////////////////////////

int ExplorerCommandManager::GetSortColumnCount() const
{
    return (std::min) ((int) m_context.listPane.GetColumns().GetListColumns().size(), CassoExplorerCommands::kViewFirst - CassoExplorerCommands::kSortByColumn);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCommandManager::IsBlockedInPicker
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerCommandManager::IsBlockedInPicker (int id) const
{
    const PickerRequest *  picker = m_context.app.picker;



    return picker != nullptr && !CassoExplorerCommands::IsAllowedInPicker (id, picker->IsSave());
}
