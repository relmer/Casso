#pragma once

#include "Pch.h"

#include "CassoLink/CassoLinkSettings.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkChoices
//
//  What the connection dialog offers, the settings a set of choices becomes,
//  and the rule for each field's value, apart from the dialog so they are
//  testable without a window (FR-083c).
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
        { L"Serial port",                  LinkTransport::Serial       },
        { L"TCP",                          LinkTransport::Tcp          },
        { L"Wi-Fi modem",                  LinkTransport::WifiModem    },
        { L"Uthernet II",                  LinkTransport::Uthernet     },
        { L"ADTPro-compatible server",     LinkTransport::AdtProServer },
    };

    static constexpr DWORD  kSpeeds[] = { 115200, 19200, 9600, 4800, 2400, 1200 };

    static std::vector<std::wstring>  GetTransportLabels ();
    static std::vector<std::wstring>  GetSpeedLabels     ();

    //  The index of a transport or a speed in the lists; 0 for one not there.
    static int   GetTransportIndex (LinkTransport transport);
    static int   GetSpeedIndex     (DWORD baud);

    //  The transport at an index; the first for one out of range.
    static LinkTransport  GetTransport (int transportIndex);

    //  The port drop-down: for a serial connection, Any, which searches the
    //  ports, then the ports found; for an ADTPro server, TCP address, then
    //  the ports found. The index of a link's port in it, 0 for none.
    static std::vector<std::wstring>  GetPortLabels (LinkTransport transport, const std::vector<std::wstring> & comPorts);
    static int                        GetPortIndex  (const CassoLinkSettings & settings, const std::vector<std::wstring> & comPorts);

    //  The rows each transport has. The port's second choice and on are COM
    //  ports; an ADTPro server on its first has an address, and on a COM port
    //  a speed and the handshake.
    static bool  UsesPort      (LinkTransport transport)                 { return transport == LinkTransport::Serial || transport == LinkTransport::AdtProServer; }
    static bool  UsesAddress   (LinkTransport transport, int portIndex);
    static bool  UsesSpeed     (LinkTransport transport, int portIndex);
    static bool  UsesBootstrap (LinkTransport transport)                 { return transport == LinkTransport::Serial; }
    static bool  UsesHandshake (LinkTransport transport, int portIndex) { return UsesSpeed (transport, portIndex); }
    static bool  UsesFolder    (LinkTransport transport)                 { return transport == LinkTransport::AdtProServer; }

    //  What the transport connects, for the line under the drop-down.
    static const wchar_t *  GetNote              (LinkTransport transport);
    static const wchar_t *  GetAddressPlaceholder (LinkTransport transport);

    //  The info tips beside the bootstrap speed and the handshake.
    static constexpr const wchar_t *  kBootstrapTip = L"The speed of the loader typed into the Apple when it has no CassoLink disk, "
                                                      L"after IN# and Ctrl+A 14B on the Apple. A boot disk starts at the speed below.";
    static constexpr const wchar_t *  kHandshakeTip = L"Start with it off. Many null-modem cables do not connect RTS and CTS, or "
                                                      L"connect each end's to each other, and a //c's serial chip has CTS tied on. A "
                                                      L"cable without the lines stops every transfer while handshake is on.";

    //  Why an address cannot be used, or empty when it can; an empty one has
    //  no error displayed, though the connection cannot be added without
    //  one. An address is a host, a colon and a port from 1 to 65535, and an
    //  Uthernet II's host is an IPv4 address.
    static std::wstring  ValidateAddress    (const std::wstring & address);
    static std::wstring  ValidateAddressFor (LinkTransport transport, const std::wstring & text);

    //  Whether every row the settings use is filled in: the address where
    //  there is one, and an ADTPro server's folder.
    static bool  IsComplete (const CassoLinkSettings & settings);

    //  Why a connection cannot be added, as one that another has the name of,
    //  and so the settings of; empty when it can. otherNames are the other
    //  links' name each.
    static std::wstring  ValidateDuplicate (const CassoLinkSettings & settings, const std::vector<std::wstring> & otherNames);

    //  The settings for the chosen values, with CassoLinkSettings::MakeName's name.
    //  An index out of range takes the list's first choice; a row the
    //  transport does not have takes its default.
    static CassoLinkSettings  MakeSettings (int transportIndex, const std::vector<std::wstring> & comPorts, int portIndex, const std::wstring & address,
                                            int speedIndex, int bootstrapIndex, bool hardwareHandshake, const std::wstring & folder);
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkPanel
//
//  The form's rows, one under another, each a label beside its field; an
//  info tip follows its row's label, and a field's error pushes the rows
//  below it down.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerLinkPanel : public DxuiPanel
{
public:
    struct Children
    {
        DxuiLabel       * transportLabel = nullptr;
        DxuiComboBox    * transport      = nullptr;
        DxuiLabel       * note           = nullptr;
        DxuiLabel       * portLabel      = nullptr;
        DxuiComboBox    * port           = nullptr;
        DxuiLabel       * addressLabel   = nullptr;
        DxuiTextInput   * address        = nullptr;
        DxuiFieldError  * addressError   = nullptr;
        DxuiLabel       * bootstrapLabel = nullptr;
        DxuiInfoTip     * bootstrapTip   = nullptr;
        DxuiComboBox    * bootstrap      = nullptr;
        DxuiLabel       * speedLabel     = nullptr;
        DxuiComboBox    * speed          = nullptr;
        DxuiLabel       * handshakeLabel = nullptr;
        DxuiInfoTip     * handshakeTip   = nullptr;
        DxuiCheckbox    * handshake      = nullptr;
        DxuiLabel       * folderLabel    = nullptr;
        DxuiTextInput   * folder         = nullptr;
        DxuiButton      * browse         = nullptr;
        DxuiFieldError  * duplicate      = nullptr;
    };

    void  Init (const Children & children);
    void  SetOnChildPressed (std::function<void (IDxuiControl *)> fn) { m_onPressed = std::move (fn); }

    //  What the panel measures its note, its errors and its info tips with.
    void  SetMeasure (IDxuiTextRenderer * text, const IDxuiTheme * theme) { m_text = text; m_theme = theme; }

    //  The height of the rows at the panel's width, errors included.
    int   GetRequiredHeightPx () const { return ArrangeRows (m_boundsDip, m_scaler, false); }

    void  Layout  (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;

    //  The rows placed again, after a row's visibility changes.
    void  Relayout () { Layout (GetBounds(), m_scaler); }
    bool  OnMouse (const DxuiMouseEvent & ev) override;

    static constexpr int  kRowHeightDip     = 30;
    static constexpr int  kRowGapDip        = 8;
    static constexpr int  kLabelWidthDip    = 150;
    static constexpr int  kNoteHeightDip    = 52;
    static constexpr int  kBrowseWidthDip   = 90;
    static constexpr int  kFallbackErrorDip = 20;
    static constexpr int  kErrorTopDip      = 4;
    static constexpr int  kRowCount         = 8;

private:
    int   ArrangeRows      (const RECT & bounds, const DxuiDpiScaler & scaler, bool place) const;
    int   PlaceRow         (const RECT & bounds, int y, DxuiLabel * label, IDxuiControl * field, DxuiFieldError * error,
                            const DxuiDpiScaler & scaler, bool place, DxuiInfoTip * tip = nullptr, DxuiButton * trailing = nullptr) const;
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
//  Adds a connection or edits one: the transport first, then only the
//  settings it uses. The connection's name is made from them. Each field is
//  checked as it changes, and OK is disabled until every field passes and
//  no other connection has the same name.
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

    //  Picks a folder for an ADTPro server, starting at the folder given;
    //  false when none is picked.
    using BrowseFn = std::function<bool (HWND owner, std::wstring & inOutFolder)>;

    //  Runs the dialog modally over `owner`, starting from `initial`.
    //  otherNames are the other links', which this one's must differ from,
    //  and comPorts the PC's.
    static Outcome  Ask (HWND owner, const IDxuiTheme * theme, const CassoLinkSettings & initial,
                         const std::vector<std::wstring> & otherNames, const std::vector<std::wstring> & comPorts,
                         bool adding, BrowseFn browse);

protected:
    void  OnCreate     () override;
    void  OnDialogTick () override;

private:
    CassoLinkSettings  MakeChosen        () const;
    void               Revalidate        ();
    void               ShowTransportRows ();
    void               FitToContent      ();

    const IDxuiTheme           * m_theme          = nullptr;
    CassoLinkSettings            m_initial;
    std::vector<std::wstring>    m_otherNames;
    std::vector<std::wstring>    m_comPorts;
    BrowseFn                     m_browseFolder;
    bool                         m_adding         = true;
    bool                         m_fitted         = false;
    LinkTransport                m_shownTransport = LinkTransport::Serial;
    DxuiButton                 * m_ok             = nullptr;
    DxuiFieldValidator           m_validator;
    Outcome                      m_outcome;
    DxuiLabel                    m_transportLabel;
    DxuiComboBox                 m_transport;
    DxuiLabel                    m_note;
    DxuiLabel                    m_portLabel;
    DxuiComboBox                 m_port;
    DxuiLabel                    m_addressLabel;
    DxuiTextInput                m_address;
    DxuiFieldError               m_addressError;
    DxuiLabel                    m_bootstrapLabel;
    DxuiInfoTip                  m_bootstrapTip;
    DxuiComboBox                 m_bootstrap;
    DxuiLabel                    m_speedLabel;
    DxuiComboBox                 m_speed;
    DxuiLabel                    m_handshakeLabel;
    DxuiInfoTip                  m_handshakeTip;
    DxuiCheckbox                 m_handshake { L"RTS and CTS" };
    DxuiLabel                    m_folderLabel;
    DxuiTextInput                m_folder;
    DxuiButton                   m_browse { L"Browse..." };
    DxuiFieldError               m_duplicate;
    CassoExplorerLinkPanel     * m_body    = nullptr;
};
