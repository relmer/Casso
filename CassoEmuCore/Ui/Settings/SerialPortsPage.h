#pragma once

#include "Pch.h"

#include "Ui/Settings/SerialAttachmentView.h"

class DxuiHwndSource;
class IHostSerialPorts;
class SettingsPanelState;





////////////////////////////////////////////////////////////////////////////////
//
//  SerialPortsPage
//
//  Settings > Serial ports, on a //c: what is plugged into each of its two
//  built-in serial ports, port 1 the printer port and port 2 the modem port.
//  Present only while the staged machine has the ports. Edits are staged in
//  SettingsPanelState; an attachment is opened when the machine is built, so
//  a change rebuilds it on OK.
//
////////////////////////////////////////////////////////////////////////////////

class SerialPortsPage : public DxuiPropertyPage
{
public:
    explicit SerialPortsPage (std::wstring title = L"Serial ports");

    void  SetState       (SettingsPanelState * state);
    void  SetPopupHost   (DxuiHwndSource * host);
    void  SetSerialPorts (IHostSerialPorts * ports);
    void  SetOnBrowse    (SerialAttachmentView::BrowseFn fn);

    void  Layout  (const RECT & rect, const DxuiDpiScaler & scaler) override;
    void  Rebuild ();

    SerialAttachmentView &  GetPort (SerialPortId port) { return (port == SerialPortId::Port1) ? m_port1 : m_port2; }

private:
    void  OnChanged (SerialPortId port, const SerialAttachmentChoice & choice);
    void  Relayout  ();

    SettingsPanelState    * m_state     = nullptr;
    SerialAttachmentView    m_port1     { SerialAttachmentView::Kind::BuiltInPort, L"Port 1 (printer):" };
    SerialAttachmentView    m_port2     { SerialAttachmentView::Kind::BuiltInPort, L"Port 2 (modem):" };
    bool                    m_hasLayout = false;
    RECT                    m_lastRect  = {};
    DxuiDpiScaler           m_lastScaler;
};
