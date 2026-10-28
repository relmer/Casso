#pragma once

#include "Pch.h"

#include "Ui/Settings/SerialAttachmentChoice.h"

class DxuiHwndSource;
class IHostSerialPorts;





////////////////////////////////////////////////////////////////////////////////
//
//  SerialAttachmentView
//
//  The controls for one serial connector: the attachment drop-down with a
//  note on what the attachment does, then the chosen attachment's options.
//  A file attachment has its file and Browse; a COM port, the PC's ports,
//  read again each time the list opens; TCP, connect or listen, the host and
//  the port, and Telnet; the modem, its listen port and Telnet.
//
//  The view adopts its controls into the page that has it, and Layout
//  places them from a given point and returns the bottom. Each edit is
//  reported through the change callback, after which the page stages it and
//  lays out again. A port that does not parse is not reported, and an error
//  under the controls states the rule.
//
//  A Super Serial Card's connector can have nothing on it; a //c port lists
//  the loopback plug first, which an empty device means there.
//
////////////////////////////////////////////////////////////////////////////////

class SerialAttachmentView
{
public:
    enum class Kind
    {
        Card,
        BuiltInPort,
    };

    struct Metrics
    {
        int  rowHeight  = 0;
        int  rowGap     = 0;
        int  sectionGap = 0;
        int  noteHeight = 0;
    };

    using BrowseFn = std::function<bool (bool forOutput, std::string & inOutPath)>;
    using ChangeFn = std::function<void (const SerialAttachmentChoice & choice)>;

    SerialAttachmentView (Kind kind, std::wstring label);

    void  AdoptInto      (DxuiPanel & panel);
    void  SetPopupHost   (DxuiHwndSource * host);
    void  SetSerialPorts (IHostSerialPorts * ports) { m_ports = ports; }
    void  SetOnBrowse    (BrowseFn fn)              { m_onBrowse = std::move (fn); }
    void  SetOnChange    (ChangeFn fn)              { m_onChange = std::move (fn); }
    void  SetChoice      (const SerialAttachmentChoice & choice);
    int   Layout         (int x, int y, int labelWidth, int controlsWidth, const Metrics & metrics, const DxuiDpiScaler & scaler);

    const SerialAttachmentChoice &  GetChoice () const { return m_choice; }

    // The device strings in drop-down order, and the note for one.
    const std::vector<std::string> &  GetDevices () const { return m_devices; }
    std::wstring                      GetNote    (const std::string & device) const;

    // The attachment drop-down's width for its longest item, so a page can
    // line its drop-downs up.
    float  GetAttachFitWidthPx (IDxuiTextRenderer & text) const { return m_attach.GetFitWidthPx (text); }

    // Test accessors.
    DxuiComboBox        & GetAttachDropdown   () { return m_attach; }
    DxuiComboBox        & GetComPortDropdown  () { return m_comPort; }
    DxuiComboBox        & GetConnectDropdown  () { return m_connect; }
    DxuiTextInput       & GetHostInput        () { return m_host; }
    DxuiTextInput       & GetPortInput        () { return m_port; }
    DxuiTextInput       & GetListenPortInput  () { return m_listenPort; }
    DxuiCheckbox        & GetTelnetCheckbox   () { return m_telnet; }
    DxuiButton          & GetBrowseButton     () { return m_browse; }
    const std::wstring  & GetNoteText         () const { return m_noteText; }
    const std::wstring  & GetErrorText        () const { return m_error.GetText(); }
    bool                  IsErrorShown        () const { return m_error.IsVisible(); }

    // A port number from 1 to 65535, or, when isZeroAllowed, an empty text
    // for 0.
    static bool  TryParsePort (const std::wstring & text, bool isZeroAllowed, int & outPort);

private:
    static constexpr int  kConnectChoice = 0;
    static constexpr int  kListenChoice  = 1;

    bool  IsDevice        (const char * device) const { return m_choice.device == device; }
    bool  IsLoopback      () const;
    void  SyncControls    ();
    void  RefreshComPorts ();
    void  Report          ();
    void  ShowError       (bool isShown);

    void  OnAttachPicked  (int choice);
    void  OnComPortPicked (int choice);
    void  OnConnectPicked (int choice);
    void  OnHostChanged   (const std::wstring & text);
    void  OnPortChanged   (const std::wstring & text);
    void  OnListenChanged (const std::wstring & text);
    void  OnTelnetChanged (bool isOn);
    void  OnBrowse        ();

    Kind                       m_kind;
    std::wstring               m_label;
    std::vector<std::string>   m_devices;
    SerialAttachmentChoice     m_choice;
    std::vector<std::string>   m_comPortNames;
    IHostSerialPorts         * m_ports    = nullptr;
    BrowseFn                   m_onBrowse;
    ChangeFn                   m_onChange;
    std::wstring               m_noteText;

    DxuiLabel                  m_attachLabel;
    DxuiComboBox               m_attach;
    DxuiLabel                  m_note;
    DxuiLabel                  m_pathLabel;
    DxuiLabel                  m_pathValue;
    DxuiButton                 m_browse;
    DxuiLabel                  m_comPortLabel;
    DxuiComboBox               m_comPort;
    DxuiLabel                  m_connectLabel;
    DxuiComboBox               m_connect;
    DxuiLabel                  m_hostLabel;
    DxuiTextInput              m_host;
    DxuiLabel                  m_portLabel;
    DxuiTextInput              m_port;
    DxuiLabel                  m_listenPortLabel;
    DxuiTextInput              m_listenPort;
    DxuiCheckbox               m_telnet { L"Telnet" };
    DxuiInfoBanner             m_error;
};
