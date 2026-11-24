#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/Location.h"

class FirewallMonitor;
struct CassoLinkSettings;
class ILinkAnnouncements;
struct ExplorerContext;
struct LinkTreeEntry;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager
//
//  The CassoLink connections in the window: adding, editing and removing
//  them, bootstrapping an Apple and the status bar's activity log.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkManager
{
public:
    explicit CassoLinkManager (ExplorerContext & context);
    ~CassoLinkManager();

    static constexpr int  kActivityShownMinutes = 15;
    static constexpr int  kFaultTextMax         = 400;

    void          RefreshCassoLinkViews   ();
    void          EditConnection          (const std::string & name);
    void          RemoveConnection        (const std::string & name);
    void          Disconnect              (const std::string & name);
    void          Connect                 (const std::string & name);
    void          SearchComPorts          (const std::string & name);
    void          BootstrapConnection     (const std::string & name);
    void          PlayCassetteBootstrap   (const std::string & name);
    void          SaveCassetteBootstrap   (const std::string & name);
    void          ArmLinkFaults           ();
    bool          IsSerialConnection      (const std::string & name) const;
    bool          HasVirtualDrives        (const std::string & name) const;
    bool          IsCassetteConnection    (const std::string & name) const;
    bool          CanBootstrapConnection  (const std::string & name) const;
    bool          CanMakeSosDisk          (const std::string & name) const;
    std::wstring  GetLatestActivity       () const;
    std::wstring  FormatActivity          (size_t lines) const;

    //  The status bar's text for the pending operations: each Full Send's
    //  summary while any is in progress, the first operation's otherwise.
    std::wstring  DescribeJobs            ();
    void          ShowActivityLog         ();
    void          StartConnections        ();
    void          SaveConnectionState     ();
    bool          IsConnectionReplying    (const std::string & name) const;
    bool          IsLaunchConnection      (const CassoLinkSettings & settings) const;
    bool          IsConnectionRemoved     (const std::wstring & link) const;
    void          AddConnectionRows       (std::vector<DxuiPopupMenuItem> & items, bool hasLocation, const Location & location);
    bool          IsWifiModemConnection   (const std::string & name) const;
    void          ReleaseIdleConnections  ();
    void          ApplyConnections        ();
    void          ApplyVerifyPrefs        ();
    void          HandleTransferTimer     ();
    void          HandleCassetteTimer     ();
    void          NoteListFilled          ();

    //  The status bar's detail has the pending operations, until they finish.
    void      MarkJobsShown         () { m_linkJobsShown = true; }
    bool      TryGetConnectionEntry (const std::wstring & name, LinkTreeEntry & outEntry) const;
    uint32_t  GetAnnouncedChanges   () const { return m_announcedChanges; }

    void  HandleConnectionChanged ();
    void  HandleRunningChanged    ();

private:
    void  UpdateUnseenFiles   ();
    void  UpdateTransferTimer ();

    static constexpr UINT  kTransferFrameMs = 33;
    static constexpr UINT  kPulseFrameMs    = 150;
    static constexpr int   kPulseFrames     = 8;

    //  The cassette audio is sent again while no Ready from the shim has
    //  arrived a few seconds after it ends, up to kCassettePlays in all.
    static constexpr int   kCassettePlays   = 5;
    static constexpr UINT  kCassetteGraceMs = 4000;

    bool  PlayCassette         ();
    bool  HasCassetteStarted   () const;

    ExplorerContext  & m_context;
    bool               m_transferTimer    = false;
    int                m_pulseFrame       = 0;
    uint32_t           m_announcedChanges = 0;   // ILinkAnnouncements::GetChangeCount at the last look
    bool               m_linkJobsShown    = false;

    //  The cassette bootstrap's audio, while PlaySound sends it from memory,
    //  the link it starts, and how many times it was sent.
    std::vector<Byte>        m_cassetteWav;
    std::string              m_cassetteLink;
    int                      m_cassettePlays = 0;
};
