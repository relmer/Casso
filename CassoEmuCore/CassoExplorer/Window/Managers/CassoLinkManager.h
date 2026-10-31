#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/Location.h"

class FirewallMonitor;
class ILinkAnnouncements;
struct ExplorerContext;
struct LinkTreeEntry;





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

    //  The list's Bootstrap button's size.
    static constexpr int  kListActionWidthDip   = 160;
    static constexpr int  kListActionWideDip    = 200;   // the firewall's actions, whose labels are longer
    static constexpr int  kListActionHeightDip  = 32;
    static constexpr int  kActivityShownMinutes = 15;

    //  The whole-disk read and write pickers' own history.
    static constexpr GUID  s_kLinkDiskPickerGuid = { 0x71d2ac7c, 0x3b03, 0x4c2c, { 0xac, 0xae, 0x2d, 0xa5, 0x0c, 0x9d, 0x7f, 0x8b } };

    void          RefreshCassoLinkViews   ();
    void          ReadDiskToImage         (const Location & drive = Location());
    void          StartImageWrite         (const Location & drive, const std::wstring & imagePath);
    void          StartFullSend           (const Location & drive = Location(), const std::wstring & extension = std::wstring());
    void          StartFullSendOn         (const std::string & link, const std::wstring & extension);
    void          AddFullSendMenu         (std::vector<DxuiPopupMenuItem> & items, const std::function<void (const std::wstring & extension)> & start);
    bool          TryGetSelectedLinkDrive (Location & outDrive) const;
    bool          TryGetTargetDrive       (const Location & drive, Location & outDrive, std::wstring & outLink, int & outNumber) const;
    bool          IsOnLinkedMachine       () const;
    void          OpenSelectedLinkDrive   ();
    void          WriteImageToDisk        (const Location & drive = Location());
    void          SaveLinkToDisk          (const Location & drive = Location());
    void          FormatAppleDisk         (const Location & location);
    void          SaveLinkDiskImage       (const std::string & name);
    void          EditConnection          (const std::string & name);
    void          RemoveConnection        (const std::string & name);
    void          Disconnect              (const std::string & name);
    void          Connect                 (const std::string & name);
    void          SearchComPorts          (const std::string & name);
    void          BootstrapConnection     (const std::string & name);
    void          PlayCassetteBootstrap   (const std::string & name);
    void          SaveCassetteBootstrap   (const std::string & name);
    bool          IsSerialConnection      (const std::string & name) const;
    bool          CanBootstrapConnection  (const std::string & name) const;
    std::wstring  GetLatestActivity       () const;
    std::wstring  FormatActivity          (size_t lines) const;
    void          ShowActivityLog         ();
    void          StartAdtProServers      ();
    bool          IsConnectionReplying    (const std::string & name) const;
    bool          IsConnectionRemoved     (const std::wstring & link) const;
    void          AddConnectionRows       (std::vector<DxuiPopupMenuItem> & items, bool hasLocation, const Location & location);
    bool          IsWifiModemConnection   (const std::string & name) const;
    void          ReleaseIdleConnections  ();
    void          ApplyConnections        ();
    void          CreateListActions       ();
    void          LayoutListActions       ();
    bool          IsFirewallMessage       () const { return m_listActionFirewall; }

    //  The status bar's detail has the pending operations, until they finish.
    void         MarkJobsShown          () { m_linkJobsShown = true; }
    RECT         GetListActionRect      () const;
    RECT         GetListActionAltRect   () const;
    std::string  GetBootstrapConnection () const;
    void         FillListAction         (bool hasMessage);
    void         PollFirewall           ();
    void         FixFirewall            (bool markPrivate);
    bool         TryGetConnectionEntry  (const std::wstring & name, LinkTreeEntry & outEntry) const;
    uint32_t     GetAnnouncedChanges    () const { return m_announcedChanges; }

    void  HandleConnectionChanged ();
    void  HandleRunningChanged    ();
    bool  HandleListActionMouse   (const DxuiMouseEvent & ev);

private:
    ExplorerContext  & m_context;
    uint32_t           m_announcedChanges     = 0;   // ILinkAnnouncements::GetChangeCount at the last look
    uint32_t           m_firewallVersion      = 0;   // FirewallMonitor::GetVersion the views were drawn from
    HANDLE             m_firewallHelper       = nullptr;   // the elevated helper, until it exits
    DxuiButton       * m_listAction           = nullptr;
    DxuiButton       * m_listActionAlt        = nullptr;
    bool               m_listActionFirewall   = false;
    bool               m_linkJobsShown        = false;
    std::string        m_listActionConnection;
    bool               m_listActionConnects   = false;

    //  The cassette bootstrap's audio, while PlaySound sends it from memory.
    std::vector<Byte>        m_cassetteWav;
};
