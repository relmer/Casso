#pragma once

#include "Pch.h"

struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkListActions
//
//  The controls under the list's message for a CassoLink connection: the
//  Connect or Bootstrap button, the Windows Firewall remedies, and the virtual
//  drives' image links.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkListActions
{
public:
    explicit CassoLinkListActions (ExplorerContext & context);
    ~CassoLinkListActions();

    //  The list's Bootstrap button's size.
    static constexpr int  kListActionWidthDip  = 160;
    static constexpr int  kListActionWideDip   = 200;   // the firewall's actions, whose labels are longer
    static constexpr int  kListActionHeightDip = 32;
    static constexpr int  kDriveLinkWidthDip   = 360;   // a virtual drive's image name, as a link
    static constexpr int  kDriveLabelWidthDip  = 120;   // "Drive 1:", ending at the message's middle
    static constexpr int  kDriveLabelGapDip    = 6;
    static constexpr int  kDriveLinkHeightDip  = 26;

    void  CreateListActions     ();
    void  LayoutListActions     ();
    bool  IsFirewallMessage     () const { return m_listActionFirewall; }
    bool  IsMessageAboveActions () const { return m_listActionFirewall || m_driveLinksShown; }
    RECT  GetListActionRect     () const;
    RECT  GetListActionAltRect  () const;
    RECT  GetDriveLinkRect      (int drive) const;
    RECT  GetDriveLabelRect     (int drive) const;
    bool  IsUsingVirtualDrives  (const std::string & name, std::vector<std::wstring> & outDrives) const;
    void  FillListAction        (bool hasMessage);
    void  PollFirewall          ();
    void  FixFirewall           (bool markPrivate);

private:
    std::string  GetBootstrapConnection () const;
    bool         IsStoppedServer        (const std::string & name) const;

    ExplorerContext  & m_context;
    uint32_t           m_firewallVersion      = 0;   // FirewallMonitor::GetVersion the views were drawn from
    HANDLE             m_firewallHelper       = nullptr;   // the elevated helper, until it exits
    DxuiButton       * m_listAction           = nullptr;
    DxuiButton       * m_listActionAlt        = nullptr;
    bool               m_listActionFirewall   = false;
    std::string        m_listActionConnection;
    bool               m_listActionConnects   = false;

    //  The virtual drives' images under the message while the CassoLink
    //  ProDOS disk's driver uses them; each opens its image.
    std::array<DxuiButton *, 2>  m_driveLinks      = {};
    std::array<DxuiLabel *, 2>   m_driveLabels     = {};
    std::array<std::wstring, 2>  m_driveLinkPaths;
    bool                         m_driveLinksShown = false;
};
