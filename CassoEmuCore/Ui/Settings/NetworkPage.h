#pragma once

#include "Pch.h"

#include "Config/NetworkCardSettings.h"
#include "Net/FirewallMonitor.h"

class SettingsPanelState;





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkPage
//
//  Settings > Network: the Uthernet II Ethernet card's settings. The tab is
//  present only while the card is turned on in the Machine tab. Every edit
//  goes into the sheet's SettingsPanelState, so OK saves it with the machine
//  and applies it to the running card, and Cancel discards it.
//
//      * MAC address           (DxuiTextInput, applied once it parses)
//      * LAN access            (DxuiCheckbox)
//      * Port forwards         (DxuiListView, with protocol, ports, Add and Remove)
//
//  A warning banner under the controls has the last error in the fixed
//  two-line form, and clears on the next valid edit.
//
//  While LAN access is on, a second warning banner above it reports a port
//  forward or a guest's port that Windows Firewall blocks, with Allow, and
//  Make private on a public network, which run the elevated helper.
//
////////////////////////////////////////////////////////////////////////////////

class NetworkPage : public DxuiPropertyPage
{
public:

    using GuestPortsFn = std::function<std::vector<FirewallPort> ()>;

    explicit NetworkPage (std::wstring title = L"Network");
    ~NetworkPage () override;

    void  SetState     (SettingsPanelState * state);
    void  SetPopupHost (DxuiHwndSource * host);

    //  The firewall's verdicts, and the ports the running guest has open.
    //  Without them, no port is reported blocked.
    void  SetFirewall  (FirewallMonitor * firewall, GuestPortsFn guestPorts);

    //  The ports other computers send to: each port forward's host port, and
    //  each of the guest's, once; none while LAN access is off, since every
    //  one of them is then on the loopback address or drops what arrives
    //  from other computers.
    static std::vector<FirewallPort>  CollectInboundPorts (const NetworkCardSettings & network, const std::vector<FirewallPort> & guestPorts);

    const std::wstring & GetFirewallText () const { return m_firewallBanner.GetText(); }
    bool                 IsFirewallShown () const { return m_firewallBanner.IsVisible(); }

    void  Layout       (const RECT & rect, const DxuiDpiScaler & scaler) override;
    void  Rebuild      ();

    // Called each dialog tick: rebuilds when the staged card changed outside
    // this page (the Machine tab's tree, a machine switch, Cancel).
    void  Sync         ();

    // Accessors for tests and wiring.
    DxuiTextInput  & GetMacInput     () { return m_mac;     }
    DxuiCheckbox   & GetLanCheckbox  () { return m_lan;     }
    DxuiListView   & GetForwardList  () { return m_forwards; }
    DxuiButton     & GetAddButton    () { return m_add;     }
    DxuiButton     & GetRemoveButton () { return m_remove;  }
    DxuiTextInput  & GetHostPortInput  () { return m_hostPort;  }
    DxuiTextInput  & GetGuestPortInput () { return m_guestPort; }
    DxuiComboBox   & GetProtocolCombo  () { return m_protocol;  }
    const std::wstring & GetErrorText () const { return m_error.GetText(); }

private:

    struct CardView
    {
        int                  slot    = 0;
        NetworkCardSettings  network;

        bool operator== (const CardView & other) const = default;
    };

    static RECT          MakeRect   (int l, int t, int w, int h);
    static bool          TryParsePort (const std::wstring & text, uint16_t & outPort);

    int       LayoutCardRows      (int x, int y, int width, const DxuiDpiScaler & scaler);
    int       LayoutForwardRows   (int controlsX, int y, const DxuiDpiScaler & scaler);
    void      ApplyDpi            (UINT dpi);
    CardView  ReadCard            () const;
    void      SyncWidgets         ();
    void      SyncForwardList     ();
    void      ApplyEnabledState   ();
    void      ShowError           (const std::string & error);
    void      Relayout            ();
    void      WireCallbacks       ();

    void      OnMacChanged        (const std::wstring & text);
    void      OnLanChanged        (bool lanAccess);
    void      OnAddForward        ();
    void      OnRemoveForward     ();
    void      SyncFirewall        ();
    void      OnFirewallAction    (size_t index);

    SettingsPanelState  * m_state       = nullptr;
    DxuiHwndSource      * m_popupHost   = nullptr;
    CardView              m_view;
    RECT                  m_lastRect    = {};
    DxuiDpiScaler         m_lastScaler;
    bool                  m_hasLayout   = false;

    FirewallMonitor            * m_firewall        = nullptr;
    GuestPortsFn                 m_guestPorts;
    uint32_t                     m_firewallVersion = 0;
    std::vector<FirewallPort>    m_inbound;                      // the ports the banner was drawn for
    std::vector<FirewallPort>    m_blocked;
    HANDLE                       m_firewallHelper  = nullptr;    // the elevated helper, until it exits
    bool                         m_firewallDrawn   = false;

    DxuiInfoBanner    m_banner;
    DxuiLabel         m_macLabel;
    DxuiTextInput     m_mac;
    DxuiCheckbox      m_lan;
    DxuiLabel         m_forwardsLabel;
    DxuiListView      m_forwards;
    DxuiComboBox      m_protocol;
    DxuiLabel         m_hostPortLabel;
    DxuiTextInput     m_hostPort;
    DxuiLabel         m_guestPortLabel;
    DxuiTextInput     m_guestPort;
    DxuiButton        m_add;
    DxuiButton        m_remove;
    DxuiActionBanner  m_firewallBanner;
    DxuiInfoBanner    m_error;
};
