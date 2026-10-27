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
//  ExplorerLinks
//
//  The CassoLink connections in the window: adding, editing and removing
//  them, bootstrapping an Apple, the firewall's remedies, a linked Apple's
//  drives and their whole-disk reads and writes, the list's link buttons
//  and the status bar's activity log.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerLinks
{
public:
    explicit ExplorerLinks (CassoExplorerWindow & window);
    ~ExplorerLinks();

    //  A CassoLink operation finished or a link changed state: the tree, the
    //  list and the status bar are brought up to date.
    void  RefreshLinkViews();

    //  The list's Bootstrap button: its size, and how far below the middle of
    //  the list's message it starts, past two lines of text.
    static constexpr int  kListActionWidthDip   = 160;
    static constexpr int  kListActionWideDip    = 200;   // the firewall's actions, whose labels are longer
    static constexpr int  kListActionGapDip     = 12;   // between the two firewall actions
    static constexpr int  kListActionHeightDip  = 32;
    static constexpr int  kListActionOffsetDip  = 36;
    static constexpr int  kActivityShownMinutes = 15;
    void  OnLinkChanged     ();

    //  The running Cassos read again, at start and when their folder changes.
    void  OnRunningChanged  ();

    //  The whole-disk read and write pickers' own history.
    static constexpr GUID  s_kLinkDiskPickerGuid = { 0x71d2ac7c, 0x3b03, 0x4c2c, { 0xac, 0xae, 0x2d, 0xa5, 0x0c, 0x9d, 0x7f, 0x8b } };
    void  ReadDiskToImage   (bool blankUnreadable = false, const std::wstring & imagePath = std::wstring());

    //  An image onto a linked Apple's drive: the drive is checked for the
    //  link disk, the user confirms, and the write is queued.
    void  StartImageWrite   (const Location & drive, const std::wstring & imagePath);

    //  The drive of the one drive row selected while the list is on a linked
    //  machine. The image commands run on it, and dragging it drags its disk,
    //  as for the drive's node in the tree.
    bool  TryGetSelectedLinkDrive (Location & outDrive) const;
    bool  IsOnLinkedMachine       () const;
    void  OpenSelectedLinkDrive   ();
    void  WriteImageToDisk  ();
    void  SaveLinkToDisk    ();
    void  FormatAppleDisk   (const Location & location);
    void  SaveLinkDiskImage (const std::string & name);

    //  The CassoLink connections: added or edited through the link dialog, removed,
    //  or disconnected, which closes the link's port. Each change is saved
    //  at once.
    void  EditLink            (const std::string & name);
    void  RemoveLink          (const std::string & name);
    void  DisconnectLink      (const std::string & name);
    void  ConnectLink         (const std::string & name);
    void  SearchLink          (const std::string & name);
    void  BootstrapLink       (const std::string & name);
    void  PlayCassetteBootstrap (const std::string & name);
    void  SaveCassetteBootstrap (const std::string & name);
    bool  IsSerialLink        (const std::string & name) const;
    bool  CanBootstrapLink    (const std::string & name) const;
    std::wstring  GetLatestActivity () const;
    std::wstring  FormatActivity (size_t lines) const;
    void  ShowActivityLog ();
    void  StartAdtProServers ();
    bool  IsLinkReplying        (const std::string & name) const;
    bool  IsLinkRemoved         (const std::wstring & link) const;
    void  AddLinkRows (std::vector<DxuiPopupMenuItem> & items, bool hasLocation, const Location & location);
    bool  IsWifiModemLink        (const std::string & name) const;
    void  ReleaseIdleLinks    ();
    void  ApplyLinks          ();

    //  The list's button under its message: Connect for a link the user
    //  disconnected, Bootstrap the Apple for a serial link whose Apple is
    //  not replying, or Allow in Windows Firewall for a connection the
    //  firewall blocks. On a public network the second button beside it is
    //  Make this network private.
    RECT         GetListActionRect    () const;
    RECT         GetListActionAltRect () const;
    std::string  GetBootstrapLink     () const;
    void         FillListAction       (bool hasMessage);

    //  The firewall read again on the drive poll, the views redrawn when it
    //  changed, and the elevated helper's result once it exits.
    void         PollFirewall         ();
    void         FixFirewall          (bool markPrivate);
    bool         TryGetLinkEntry      (const std::wstring & name, LinkTreeEntry & outEntry) const;

private:
    friend class CassoExplorerWindow;

    CassoExplorerWindow  & m_window;

    uint32_t     m_announcedChanges   = 0;   // ILinkAnnouncements::GetChangeCount at the last look
    uint32_t     m_firewallVersion    = 0;   // FirewallMonitor::GetVersion the views were drawn from
    HANDLE       m_firewallHelper     = nullptr;   // the elevated helper, until it exits
    std::string  m_listActionLink;
    bool         m_listActionConnects = false;

    //  The cassette bootstrap's audio, while PlaySound sends it from memory.
    std::vector<Byte>        m_cassetteWav;
};
