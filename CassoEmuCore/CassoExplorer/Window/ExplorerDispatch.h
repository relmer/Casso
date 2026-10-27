#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/CassoExplorerCommands.h"
#include "CassoExplorer/CassoExplorerNamedControl.h"
#include "CassoExplorer/CassoExplorerNewDiskDialog.h"
#include "CassoExplorer/ICassoExplorerHost.h"
#include "CassoExplorer/Model/FocusRing.h"
#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "Config/IFileSystem.h"
#include "Seams/Win32HostDialogs.h"
#include "Seams/Win32IntentChannel.h"
#include "Seams/Win32ProcessLauncher.h"
#include "CassoExplorer/CrtPictureRenderer.h"
#include "Seams/Win32ContentDetails.h"
#include "Ui/Debugger/SourceSyntax.h"
#include "Ui/ThemeLoader.h"
#include "Config/CrtResolver.h"
#include "Config/CrtPresets.h"
#include "Seams/Win32ShellIcons.h"
#include "Seams/Win32InfoTips.h"
#include "Seams/Win32ShellItemVerbs.h"
#include "Seams/Win32PackagedCommands.h"
#include "Ui/Chrome/CassoTheme.h"
#include "CassoExplorer/Model/FolderWatch.h"
#include "CassoExplorer/Model/RefreshAnchor.h"

class CassoLinkConnections;
class FirewallMonitor;
class RunningCassoRegistry;
class ILinkAnnouncements;
struct CassoExplorerSessionWindow;
class CassoExplorerWindow;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerDispatch
//
//  The commands: running one by id, whether it is enabled or checked,
//  and the verbs it runs, raw sector and block verbs included, with
//  their outcomes reported.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerDispatch
{
public:
    explicit ExplorerDispatch (CassoExplorerWindow & window);

    static constexpr uint64_t  kDos33ImageBytes = 35 * 16 * 256;

    //  Each zoom step, in percent of the theme's text size.
    static constexpr int    kPreviewZoomStep     = 10;

    //  How long the list stays empty on a refresh, as File Explorer's does,
    //  so the refresh can be seen.
    static constexpr UINT  kRefreshBlankMs        = 100;

    //  A verb's label as a window's title: no access key, no trailing dots.
    static std::wstring  GetVerbTitle (CassoExplorerActions::Verb verb);

    //  The raw read and write pickers' own history, apart from other pickers.
    static constexpr GUID  s_kRawPickerGuid = { 0xe347dd39, 0x4a18, 0x4a0d, { 0x93, 0x4a, 0xfd, 0x63, 0xb7, 0x9c, 0xba, 0x3e } };
    int   ChooseFormatDefault     (const std::wstring & image);
    void  ExtractAll();
    void  Dispatch     (int id);
    bool  IsEnabled    (int id) const;
    bool  IsChecked    (int id) const;
    void  CreateDisk (const CassoExplorerNewDiskDialog::Outcome & newDisk);
    void  ShowOptions            ();
    bool  IsToolbarEntryAvailable (int index) const;
    bool  IsCommandBarEntryAvailable (int index) const;
    HostFileNaming::Style  GetNamingStyle () const;
    void  RunVerb             (CassoExplorerActions::Verb verb);
    bool  RunRecycleBinVerb (CassoExplorerActions::Verb verb);
    std::wstring  GetCommandLabel (int id) const;
    void  RunRawVerb          (CassoExplorerActions::Verb verb);
    bool  IsRawTargetCurrent  (const std::wstring & target);
    void  ReportOutcome       (const CassoExplorerActions::Outcome & outcome, const wchar_t * verbName);
    void  ExtractImageFiles   (const std::wstring & imagePath);

private:
    friend class CassoExplorerWindow;

    CassoExplorerWindow  & m_window;
};
