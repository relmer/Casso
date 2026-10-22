#pragma once

#include "Pch.h"

#include "CassoLink/CassoLinkSettings.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkChoices
//
//  What the link dialog offers, the settings a set of choices becomes, and
//  the rule for each field's value, apart from the dialog so they are
//  testable without a window.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerLinkChoices
{
public:
    struct TransportChoice
    {
        const wchar_t  * label;
        LinkTransport    transport;
    };

    static constexpr TransportChoice  kTransports[] =
    {
        { L"Serial port",      LinkTransport::Serial    },
        { L"Casso (emulated)", LinkTransport::Emulated  },
        { L"TCP",              LinkTransport::Tcp       },
        { L"Wi-Fi modem",      LinkTransport::WifiModem },
    };

    static constexpr DWORD  kSpeeds[] = { 115200, 19200, 9600, 4800, 2400, 1200 };

    //  The Apple's serial port: a //c's port 2, which is at slot 2's
    //  addresses, or a Super Serial Card's slot.
    static constexpr int    kSlots[]  = { 2, 1, 3, 4, 5, 6, 7 };

    static std::vector<std::wstring>  GetTransportLabels ();
    static std::vector<std::wstring>  GetSpeedLabels     ();
    static std::vector<std::wstring>  GetSlotLabels      ();

    //  The index of a transport, a speed or a slot in the lists; 0 for one
    //  not there.
    static int   GetTransportIndex (LinkTransport transport);
    static int   GetSpeedIndex     (DWORD baud);
    static int   GetSlotIndex      (int slot);

    //  The rows each transport has (FR-083c). A serial link has a COM port,
    //  speed, bootstrap speed and handshaking; TCP and an emulated machine,
    //  an address; a Wi-Fi modem, the TCP port it connects to, speed and
    //  handshaking.
    static bool  UsesPort      (LinkTransport transport) { return transport == LinkTransport::Serial; }
    static bool  UsesAddress   (LinkTransport transport) { return transport != LinkTransport::Serial; }
    static bool  UsesSpeed     (LinkTransport transport) { return transport == LinkTransport::Serial || transport == LinkTransport::WifiModem; }
    static bool  UsesBootstrap (LinkTransport transport) { return transport == LinkTransport::Serial; }
    static bool  UsesHandshake (LinkTransport transport) { return transport == LinkTransport::Serial || transport == LinkTransport::WifiModem; }

    //  Every transport to an Apple has the Apple's port; plain TCP is ADTPro's
    //  serial over IP, whose far end is a program rather than an Apple.
    static bool  UsesSlot      (LinkTransport transport) { return transport != LinkTransport::Tcp; }

    //  The address row's label and its value when nothing is entered. A
    //  Wi-Fi modem's row is the TCP port alone, as it listens on every
    //  address; an emulated machine's is Casso's own serial port on this PC.
    static const wchar_t *  GetAddressLabel   (LinkTransport transport);
    static std::wstring     GetDefaultAddress (LinkTransport transport);

    //  The address row's text for a link's settings, and the reason it
    //  cannot be used for a transport, or empty.
    static std::wstring     GetAddressText    (const CassoLinkSettings & settings);
    static std::wstring     ValidateAddressFor (LinkTransport transport, const std::wstring & text);
    //  Why a field's value cannot be used, or empty when it can. A name must
    //  be new among `otherNames`; a port is COM and a number, or empty to
    //  search; an address is a host, a colon and a port from 1 to 65535.
    static std::wstring  ValidateName    (const std::wstring & name, const std::vector<std::wstring> & otherNames);
    static std::wstring  ValidatePort    (const std::wstring & port);
    static std::wstring  ValidateAddress (const std::wstring & address);
    static std::wstring  ValidateTcpPort (const std::wstring & port);

    //  The settings for the chosen values. An index out of range takes the
    //  list's first choice.
    static CassoLinkSettings  MakeSettings (const std::wstring & name, int transportIndex, const std::wstring & port, const std::wstring & address,
                                            int speedIndex, int bootstrapIndex, bool hardwareHandshake, int slotIndex = 0);

    //  The Name box's text for a link: the part in parentheses after the kind
    //  of connection, empty for none, or the whole label of one saved before
    //  labels were the kind of connection.
    static std::wstring       GetNameText  (const CassoLinkSettings & settings);

    static constexpr size_t  kMaxNameLength = 60;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkPanel
//
//  The link form's rows, one under another, each a label beside its field;
//  a field's error pushes the rows below it down.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerLinkPanel : public DxuiPanel
{
public:
    struct Children
    {
        DxuiLabel       * nameLabel      = nullptr;
        DxuiTextInput   * name           = nullptr;
        DxuiFieldError  * nameError      = nullptr;
        DxuiLabel       * transportLabel = nullptr;
        DxuiComboBox    * transport      = nullptr;
        DxuiLabel       * portLabel      = nullptr;
        DxuiTextInput   * port           = nullptr;
        DxuiFieldError  * portError      = nullptr;
        DxuiLabel       * addressLabel   = nullptr;
        DxuiTextInput   * address        = nullptr;
        DxuiFieldError  * addressError   = nullptr;
        DxuiLabel       * slotLabel      = nullptr;
        DxuiComboBox    * slot           = nullptr;
        DxuiLabel       * speedLabel     = nullptr;
        DxuiComboBox    * speed          = nullptr;
        DxuiLabel       * bootstrapLabel = nullptr;
        DxuiComboBox    * bootstrap      = nullptr;
        DxuiCheckbox    * handshake      = nullptr;
    };

    void  Init (const Children & children);
    void  SetOnChildPressed (std::function<void (IDxuiControl *)> fn) { m_onPressed = std::move (fn); }

    //  What the panel measures its errors with.
    void  SetMeasure (IDxuiTextRenderer * text, const IDxuiTheme * theme) { m_text = text; m_theme = theme; }

    //  The height of the rows at the panel's width, errors included.
    int   GetRequiredHeightPx () const { return ArrangeRows (m_boundsDip, m_scaler, false); }

    void  Layout  (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;

    //  The rows placed again, after a row's visibility changes.
    void  Relayout () { Layout (GetBounds(), m_scaler); }
    bool  OnMouse (const DxuiMouseEvent & ev) override;

    static constexpr int  kRowHeightDip     = 30;
    static constexpr int  kRowGapDip        = 8;
    static constexpr int  kLabelWidthDip    = 130;
    static constexpr int  kFallbackErrorDip = 20;
    static constexpr int  kErrorTopDip      = 4;
    static constexpr int  kRowCount         = 8;

private:
    int   ArrangeRows      (const RECT & bounds, const DxuiDpiScaler & scaler, bool place) const;
    int   PlaceRow         (const RECT & bounds, int y, DxuiLabel * label, IDxuiControl * field, DxuiFieldError * error,
                            const DxuiDpiScaler & scaler, bool place) const;
    int   GetErrorHeightPx (const DxuiFieldError & error, int widthPx, const DxuiDpiScaler & scaler) const;

    Children                               m_kids;
    std::function<void (IDxuiControl *)>   m_onPressed;
    IDxuiTextRenderer                    * m_text  = nullptr;
    const IDxuiTheme                     * m_theme = nullptr;
    DxuiDpiScaler                          m_scaler;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkDialog
//
//  Adds a link or edits one: its name, how it connects, and its speeds.
//  Each field is checked as it changes, and OK is disabled until every
//  field passes.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerLinkDialog : public DxuiDialogWindow
{
public:
    struct Outcome
    {
        bool               confirmed = false;
        CassoLinkSettings  settings;
    };

    //  Runs the dialog modally over `owner`, starting from `initial`.
    //  otherNames are the other links', which this one's must differ from.
    static Outcome  Ask (HWND owner, const IDxuiTheme * theme, const CassoLinkSettings & initial,
                         const std::vector<std::wstring> & otherNames, bool adding);

protected:
    void  OnCreate     () override;
    void  OnDialogTick () override;

private:
    void  Revalidate   ();
    void  ShowTransportRows ();
    void  FitToContent ();

    const IDxuiTheme           * m_theme          = nullptr;
    CassoLinkSettings            m_initial;
    std::vector<std::wstring>    m_otherNames;
    bool                         m_adding         = true;
    bool                         m_fitted         = false;
    LinkTransport                m_shownTransport = LinkTransport::Serial;
    DxuiButton                 * m_ok             = nullptr;
    DxuiFieldValidator           m_validator;
    Outcome                      m_outcome;
    DxuiLabel                    m_nameLabel;
    DxuiTextInput                m_name;
    DxuiFieldError               m_nameError;
    DxuiLabel                    m_transportLabel;
    DxuiComboBox                 m_transport;
    DxuiLabel                    m_portLabel;
    DxuiTextInput                m_port;
    DxuiFieldError               m_portError;
    DxuiLabel                    m_addressLabel;
    DxuiTextInput                m_address;
    DxuiFieldError               m_addressError;
    DxuiLabel                    m_slotLabel;
    DxuiComboBox                 m_slot;
    DxuiLabel                    m_speedLabel;
    DxuiComboBox                 m_speed;
    DxuiLabel                    m_bootstrapLabel;
    DxuiComboBox                 m_bootstrap;
    DxuiCheckbox                 m_handshake { L"Hardware handshake (RTS and CTS)" };
    CassoExplorerLinkPanel     * m_body    = nullptr;
};
