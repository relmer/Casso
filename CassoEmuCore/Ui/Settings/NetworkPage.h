#pragma once

#include "Pch.h"

#include "Config/NetworkCardSettings.h"

class SettingsPanelState;





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkPage
//
//  Settings > Network: the Uthernet II Ethernet card. Every edit goes into
//  the sheet's SettingsPanelState, so OK saves it with the machine and Cancel
//  restores it; the card picks the settings up when the machine restarts.
//
//      * Uthernet II           (DxuiToggle: card in or out)
//      * Slot                  (DxuiComboBox: slots 1-7, taken ones disabled)
//      * MAC address           (DxuiTextInput, applied once it parses)
//      * LAN access            (DxuiCheckbox)
//      * Port forwards         (DxuiListView, with protocol, ports, Add and Remove)
//
//  A warning banner under the controls has the last error in the fixed
//  two-line form, and clears on the next valid edit.
//
////////////////////////////////////////////////////////////////////////////////

class NetworkPage : public DxuiPropertyPage
{
public:

    explicit NetworkPage (std::wstring title = L"Network");

    void  SetState     (SettingsPanelState * state);
    void  SetPopupHost (DxuiHwndSource * host);

    void  Layout       (const RECT & rect, const DxuiDpiScaler & scaler) override;
    void  Rebuild      ();

    // Called each dialog tick: rebuilds when the staged card changed outside
    // this page (the Machine tab's tree, a machine switch, Cancel).
    void  Sync         ();

    // Accessors for tests and wiring.
    DxuiToggle     & GetEnableToggle () { return m_enable;  }
    DxuiComboBox   & GetSlotCombo    () { return m_slot;    }
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
        bool                 offered = false;
        bool                 present = false;
        bool                 enabled = false;
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
    void      SyncSlotItems       ();
    void      ApplyEnabledState   ();
    void      ShowError           (const std::string & error);
    void      Relayout            ();
    void      WireCallbacks       ();

    void      OnEnableChanged     (bool enabled);
    void      OnSlotSelected      (int index);
    void      OnMacChanged        (const std::wstring & text);
    void      OnLanChanged        (bool lanAccess);
    void      OnAddForward        ();
    void      OnRemoveForward     ();

    SettingsPanelState  * m_state       = nullptr;
    CardView              m_view;
    RECT                  m_lastRect    = {};
    DxuiDpiScaler         m_lastScaler;
    bool                  m_hasLayout   = false;

    DxuiInfoBanner  m_banner;
    DxuiLabel       m_enableLabel;
    DxuiToggle      m_enable;
    DxuiLabel       m_slotLabel;
    DxuiComboBox    m_slot;
    DxuiLabel       m_macLabel;
    DxuiTextInput   m_mac;
    DxuiCheckbox    m_lan;
    DxuiLabel       m_forwardsLabel;
    DxuiListView    m_forwards;
    DxuiComboBox    m_protocol;
    DxuiLabel       m_hostPortLabel;
    DxuiTextInput   m_hostPort;
    DxuiLabel       m_guestPortLabel;
    DxuiTextInput   m_guestPort;
    DxuiButton      m_add;
    DxuiButton      m_remove;
    DxuiInfoBanner  m_error;
};
