#pragma once

#include "Pch.h"

class FirewallMonitor;
class ILinkAnnouncements;
struct ExplorerContext;
struct LinkTreeEntry;
struct Location;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager
//
//  The CassoLink connections in the window: adding, editing and removing
//  them, bootstrapping an Apple, the firewall's remedies, a linked Apple's
//  drives and their whole-disk reads and writes, the list's link buttons
//  and the status bar's activity log.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkManager
{
public:
    explicit CassoLinkManager (ExplorerContext & context);
    ~CassoLinkManager();

    //  A CassoLink operation finished or a link changed state: the tree, the
    //  list and the status bar are brought up to date.
    void  RefreshCassoLinkViews();

    //  The list's Bootstrap button's size.
    static constexpr int  kListActionWidthDip   = 160;
    static constexpr int  kListActionWideDip    = 200;   // the firewall's actions, whose labels are longer
    static constexpr int  kListActionHeightDip  = 32;
    static constexpr int  kActivityShownMinutes = 15;
    void  HandleConnectionChanged     ();

    //  The running Cassos read again, at start and when their folder changes.
    void  HandleRunningChanged  ();

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
    void  EditConnection            (const std::string & name);
    void  RemoveConnection          (const std::string & name);
    void  Disconnect      (const std::string & name);
    void  Connect         (const std::string & name);
    void  SearchComPorts          (const std::string & name);
    void  BootstrapConnection       (const std::string & name);
    void  PlayCassetteBootstrap (const std::string & name);
    void  SaveCassetteBootstrap (const std::string & name);
    bool  IsSerialConnection        (const std::string & name) const;
    bool  CanBootstrapConnection    (const std::string & name) const;
    std::wstring  GetLatestActivity () const;
    std::wstring  FormatActivity (size_t lines) const;
    void  ShowActivityLog ();
    void  StartAdtProServers ();
    bool  IsConnectionReplying        (const std::string & name) const;
    bool  IsConnectionRemoved         (const std::wstring & link) const;
    void  AddConnectionRows (std::vector<DxuiPopupMenuItem> & items, bool hasLocation, const Location & location);
    bool  IsWifiModemConnection        (const std::string & name) const;
    void  ReleaseIdleConnections    ();
    void  ApplyConnections          ();

    //  The list's button under its message: Connect for a link the user
    //  disconnected, Bootstrap the Apple for a serial link whose Apple is
    //  not replying, or Allow in Windows Firewall for a connection the
    //  firewall blocks. On a public network the second button beside it is
    //  Make this network private.
    //  The buttons under the list's message for a link, and whether the
    //  message is the firewall's.
    void         CreateListActions    ();
    void         LayoutListActions    ();
    bool         HandleListActionMouse    (const DxuiMouseEvent & ev);
    bool         IsFirewallMessage    () const { return m_listActionFirewall; }

    //  The status bar's detail has the pending operations, until they finish.
    void         MarkJobsShown        () { m_linkJobsShown = true; }
    RECT         GetListActionRect    () const;
    RECT         GetListActionAltRect () const;
    std::string  GetBootstrapConnection     () const;
    void         FillListAction       (bool hasMessage);

    //  The firewall read again on the drive poll, the views redrawn when it
    //  changed, and the elevated helper's result once it exits.
    void         PollFirewall         ();
    void         FixFirewall          (bool markPrivate);
    bool         TryGetConnectionEntry      (const std::wstring & name, LinkTreeEntry & outEntry) const;

    uint32_t  GetAnnouncedChanges () const { return m_announcedChanges; }

private:

    ExplorerContext  & m_context;

    uint32_t     m_announcedChanges   = 0;   // ILinkAnnouncements::GetChangeCount at the last look
    uint32_t     m_firewallVersion    = 0;   // FirewallMonitor::GetVersion the views were drawn from
    HANDLE       m_firewallHelper     = nullptr;   // the elevated helper, until it exits
    DxuiButton * m_listAction         = nullptr;
    DxuiButton * m_listActionAlt      = nullptr;
    bool         m_listActionFirewall = false;
    bool         m_linkJobsShown      = false;
    std::string  m_listActionConnection;
    bool         m_listActionConnects = false;

    //  The cassette bootstrap's audio, while PlaySound sends it from memory.
    std::vector<Byte>        m_cassetteWav;
};
