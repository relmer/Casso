#pragma once

#include "Pch.h"

#include "CassoExplorer/CassetteLevelMeter.h"
#include "CassoLink/Audio/WasapiCassetteAudio.h"
#include "CassoLink/CassoLinkSettings.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkChoices
//
//  What the connection dialog offers, the settings a set of choices becomes,
//  and the rule for each field's value, apart from the dialog so they are
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
        { L"Serial port",                  LinkTransport::Serial       },
        { L"TCP",                          LinkTransport::Tcp          },
        { L"Wi-Fi modem",                  LinkTransport::WifiModem    },
        { L"Uthernet II",                  LinkTransport::Uthernet     },
        { L"Uthernet or LANceGS",          LinkTransport::Udp          },
        { L"ADTPro-compatible server",     LinkTransport::AdtProServer },
        { L"Cassette ports (audio)",       LinkTransport::Cassette     },
    };

    static constexpr DWORD  kSpeeds[] = { 230400, 115200, 19200, 9600, 4800, 2400, 1200 };

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
    static bool  UsesDrives    (LinkTransport transport)                 { return transport == LinkTransport::Serial; }
    static bool  UsesSound     (LinkTransport transport)                 { return transport == LinkTransport::Cassette; }

    //  A sound device drop-down: Default device, then the devices found. The
    //  id at an index, empty for the default, and the index of an id, 0 for
    //  the default or one not found.
    static std::vector<std::wstring>  GetSoundLabels (const std::vector<AudioDeviceChoice> & devices);
    static std::wstring               GetSoundId     (const std::vector<AudioDeviceChoice> & devices, int index);
    static int                        GetSoundIndex  (const std::vector<AudioDeviceChoice> & devices, const std::wstring & id);

    //  What the transport connects, for the line under the drop-down.
    static const wchar_t  * GetNote               (LinkTransport transport);
    static const wchar_t  * GetAddressPlaceholder (LinkTransport transport);

    //  The lines under the port, above the bootstrap speed and above the
    //  folder, and the info tip beside the handshake.
    static constexpr const wchar_t *  kPortNote      = L"With Any, each port is searched in turn.";
    static constexpr const wchar_t *  kBootstrapNote = L"Bootstrapping gets CassoLink running on an Apple with no CassoLink boot disk. Choose "
                                                       L"Bootstrap the Apple on the connection's menu, and Casso Explorer types a small loader "
                                                       L"into the Apple at this speed. Transfers then run at the transfer speed.";
    static constexpr const wchar_t *  kFolderNote    = L"ADTPro's client reads and writes disk images in this folder and its subfolders only.";
    static constexpr const wchar_t *  kDrivesNote    = L"Booted on the Apple, a CassoLink ProDOS disk adds two drives in a slot with no card. "
                                                       L"They read and write these disk images on this PC.";
    static constexpr const wchar_t *  kSoundNote     = L"Turn this PC's output most of the way up. The level is red when the Apple's "
                                                       L"output is too quiet to read or loud enough to clip.";
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

class CassoExplorerLinkPanel : public DxuiPanel, public IDxuiMeasuredContent
{
public:
    struct Children
    {
        DxuiLabel       * transportLabel = nullptr;
        DxuiComboBox    * transport      = nullptr;
        DxuiLabel       * note           = nullptr;
        DxuiLabel       * portLabel      = nullptr;
        DxuiComboBox    * port           = nullptr;
        DxuiLabel       * portNote       = nullptr;
        DxuiLabel       * addressLabel   = nullptr;
        DxuiTextInput   * address        = nullptr;
        DxuiFieldError  * addressError   = nullptr;
        DxuiLabel       * bootstrapNote  = nullptr;
        DxuiLabel       * bootstrapLabel = nullptr;
        DxuiComboBox    * bootstrap      = nullptr;
        DxuiLabel       * speedLabel     = nullptr;
        DxuiComboBox    * speed          = nullptr;
        DxuiLabel       * handshakeLabel = nullptr;
        DxuiInfoTip     * handshakeTip   = nullptr;
        DxuiCheckbox    * handshake      = nullptr;
    DxuiLabel       * columnsLabel   = nullptr;
    DxuiCheckbox    * columns        = nullptr;
    DxuiLabel       * verifyLabel    = nullptr;
    DxuiComboBox    * verify         = nullptr;
        DxuiLabel       * folderNote     = nullptr;
        DxuiLabel       * folderLabel    = nullptr;
        DxuiTextInput   * folder         = nullptr;
        DxuiButton      * browse         = nullptr;
        DxuiLabel       * outputLabel    = nullptr;
        DxuiComboBox    * output         = nullptr;
        DxuiLabel       * inputLabel     = nullptr;
        DxuiComboBox    * input          = nullptr;
        DxuiLabel       * levelLabel     = nullptr;
        CassetteLevelMeter * level       = nullptr;
        DxuiLabel       * soundNote      = nullptr;
        DxuiLabel       * drivesNote     = nullptr;
        DxuiLabel       * drive1Label    = nullptr;
        DxuiTextInput   * drive1         = nullptr;
        DxuiButton      * browse1        = nullptr;
        DxuiLabel       * drive2Label    = nullptr;
        DxuiTextInput   * drive2         = nullptr;
        DxuiButton      * browse2        = nullptr;
        DxuiFieldError  * duplicate      = nullptr;
    };

    void  Init              (const Children & children);
    void  SetOnChildPressed (std::function<void (IDxuiControl *)> fn) { m_onPressed = std::move (fn); }

    //  What the panel measures its note, its errors and its info tips with.
    void  SetMeasure (IDxuiTextRenderer * text, const IDxuiTheme * theme) { m_text = text; m_theme = theme; }

    //  The height of the rows at the panel's width, errors included.
    int   GetRequiredHeightPx () const { return ArrangeRows (m_boundsDip, m_scaler, false); }
    bool  TryMeasureHeightPx  (IDxuiTextRenderer & text, const IDxuiTheme & theme, int widthPx, int & heightPx) override;

    void  Layout  (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;

    //  The rows placed again, after a row's visibility changes.
    void  Relayout () { Layout (GetBounds(), m_scaler); }
    bool  OnMouse  (const DxuiMouseEvent & ev) override;

    static constexpr int  kLabelWidthDip    = 150;
    static constexpr int  kFallbackNoteDip  = 40;
    static constexpr int  kBrowseWidthDip   = 90;
    static constexpr int  kRowCount         = 10;

private:
    int   ArrangeRows      (const RECT & bounds, const DxuiDpiScaler & scaler, bool place) const;
    int   PlaceRow         (const RECT & bounds, int y, DxuiLabel * label, IDxuiControl * field, DxuiFieldError * error,
                            const DxuiDpiScaler & scaler, bool place, DxuiInfoTip * tip = nullptr, DxuiButton * trailing = nullptr) const;
    int   PlaceNote        (const RECT & bounds, int y, DxuiLabel * note, const DxuiDpiScaler & scaler, bool place) const;
    int   GetErrorHeightPx (const DxuiFieldError & error, int widthPx, const DxuiDpiScaler & scaler) const;
    int   GetNoteHeightPx  (const DxuiLabel & note, int widthPx, const DxuiDpiScaler & scaler) const;

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

    //  Picks a disk image for a virtual drive, starting at the image given;
    //  false when none is picked.
    using BrowseImageFn = std::function<bool (HWND owner, std::wstring & inOutImage)>;

    //  Runs the dialog modally over `owner`, starting from `initial`.
    //  otherNames are the other links', which this one's must differ from,
    //  and comPorts the PC's.
    static Outcome  Ask (HWND owner, const IDxuiTheme * theme, const CassoLinkSettings & initial,
                         const std::vector<std::wstring> & otherNames, const std::vector<std::wstring> & comPorts,
                         bool adding, BrowseFn browse, BrowseImageFn browseImage = nullptr);

protected:
    void  OnCreate     () override;
    void  OnDialogTick () override;

private:
    CassoLinkSettings  MakeChosen        () const;
    void               Revalidate        ();
    void               ShowTransportRows ();
    void               FitToContent      ();
    void               BrowseImage       (DxuiTextInput & field);
    void               UpdateLevel       ();

    const IDxuiTheme           * m_theme          = nullptr;
    CassoLinkSettings            m_initial;
    std::vector<std::wstring>    m_otherNames;
    std::vector<std::wstring>    m_comPorts;
    BrowseFn                     m_browseFolder;
    BrowseImageFn                m_browseImage;
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
    DxuiLabel                    m_portNote;
    DxuiLabel                    m_addressLabel;
    DxuiTextInput                m_address;
    DxuiFieldError               m_addressError;
    DxuiLabel                    m_bootstrapNote;
    DxuiLabel                    m_bootstrapLabel;
    DxuiComboBox                 m_bootstrap;
    DxuiLabel                    m_speedLabel;
    DxuiComboBox                 m_speed;
    DxuiLabel                    m_handshakeLabel;
    DxuiInfoTip                  m_handshakeTip;
    DxuiCheckbox                 m_handshake { L"RTS and CTS" };
    DxuiLabel                    m_verifyLabel;
    DxuiComboBox                 m_verify;
    DxuiLabel                    m_columnsLabel;
    DxuiCheckbox                 m_columns { L"Use 40-column mode (disable 80-column detection)" };
    DxuiLabel                    m_folderNote;
    DxuiLabel                    m_folderLabel;
    DxuiTextInput                m_folder;
    DxuiButton                   m_browse { L"Browse..." };
    DxuiLabel                    m_drivesNote;
    DxuiLabel                    m_drive1Label;
    DxuiTextInput                m_drive1;
    DxuiButton                   m_browse1 { L"Browse..." };
    DxuiLabel                    m_drive2Label;
    DxuiTextInput                m_drive2;
    DxuiButton                   m_browse2 { L"Browse..." };
    DxuiLabel                               m_outputLabel;
    DxuiComboBox                            m_output;
    DxuiLabel                               m_inputLabel;
    DxuiComboBox                            m_input;
    DxuiLabel                               m_levelLabel;
    CassetteLevelMeter                      m_level;
    DxuiLabel                               m_soundNote;
    std::vector<AudioDeviceChoice>          m_outputs;
    std::vector<AudioDeviceChoice>          m_inputs;
    std::unique_ptr<WasapiCassetteAudio>    m_meterAudio;
    std::wstring                            m_meterInput;
    float                                   m_peak        = 0.0f;
    bool                                    m_meterFailed = false;
    DxuiFieldError                          m_duplicate;
    CassoExplorerLinkPanel                * m_body        = nullptr;
};
