#include "Pch.h"

#include "CassoExplorer/CassoExplorerLinkDialog.h"
#include "CassoExplorer/Model/CassoLinkConnections.h"
#include "Core/TextEncoding.h"
#include "Seams/Win32NetworkAdapters.h"
#include "Ui/Settings/SettingsInfoTip.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkChoices::GetTransportLabels
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> CassoExplorerLinkChoices::GetTransportLabels()
{
    std::vector<std::wstring>  labels;



    for (const TransportChoice & choice : kTransports)
    {
        labels.push_back (choice.label);
    }

    return labels;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkChoices::GetSpeedLabels
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> CassoExplorerLinkChoices::GetSpeedLabels()
{
    std::vector<std::wstring>  labels;



    for (DWORD speed : kSpeeds)
    {
        labels.push_back (std::format (L"{} baud", CassoLinkConnections::FormatBaud (speed)));
    }

    return labels;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkChoices::GetTransportIndex
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerLinkChoices::GetTransportIndex (LinkTransport transport)
{
    int  index = 0;



    for (int i = 0; i < (int) ARRAYSIZE (kTransports); i++)
    {
        if (kTransports[i].transport == transport)
        {
            index = i;
        }
    }

    return index;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkChoices::GetSpeedIndex
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerLinkChoices::GetSpeedIndex (DWORD baud)
{
    int  index = 0;



    for (int i = 0; i < (int) ARRAYSIZE (kSpeeds); i++)
    {
        if (kSpeeds[i] == baud)
        {
            index = i;
        }
    }

    return index;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkChoices::GetTransport
//
////////////////////////////////////////////////////////////////////////////////

LinkTransport CassoExplorerLinkChoices::GetTransport (int transportIndex)
{
    bool  known = transportIndex >= 0 && transportIndex < (int) ARRAYSIZE (kTransports);



    return kTransports[known ? transportIndex : 0].transport;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkChoices::GetPortLabels
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> CassoExplorerLinkChoices::GetPortLabels (LinkTransport transport, const std::vector<std::wstring> & comPorts)
{
    std::vector<std::wstring>  labels = { (transport == LinkTransport::AdtProServer) ? L"TCP address" : L"Any" };



    labels.insert (labels.end(), comPorts.begin(), comPorts.end());

    return labels;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkChoices::GetPortIndex
//
//  A port saved but not found now, as an adapter unplugged, is the first
//  choice; the connection is then saved with that choice unless the user
//  picks the port again.
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerLinkChoices::GetPortIndex (const CassoLinkSettings & settings, const std::vector<std::wstring> & comPorts)
{
    std::wstring  port  = TextEncoding::NarrowToWide (settings.port);
    int           index = 0;



    for (int i = 0; i < (int) comPorts.size() && !port.empty(); i++)
    {
        if (_wcsicmp (comPorts[(size_t) i].c_str(), port.c_str()) == 0)
        {
            index = i + 1;
        }
    }

    return index;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkChoices::UsesAddress
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerLinkChoices::UsesAddress (LinkTransport transport, int portIndex)
{
    return transport == LinkTransport::Tcp
        || transport == LinkTransport::Uthernet
        || (transport == LinkTransport::AdtProServer && portIndex <= 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkChoices::UsesSpeed
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerLinkChoices::UsesSpeed (LinkTransport transport, int portIndex)
{
    return transport == LinkTransport::Serial
        || transport == LinkTransport::WifiModem
        || (transport == LinkTransport::AdtProServer && portIndex > 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkChoices::GetNote
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * CassoExplorerLinkChoices::GetNote (LinkTransport transport)
{
    const wchar_t  * note = L"";



    switch (transport)
    {
        case LinkTransport::Serial:
            note = L"A null-modem cable from a //c's serial port, or a Super Serial Card, to a COM port or a USB "
                   L"serial adapter on this PC.";
            break;

        case LinkTransport::Tcp:
            note = L"An Apple's serial port over the network, at an address and port: Casso on another PC with a "
                   L"serial card set to a TCP port, or a network serial adapter on a real Apple. Casso Explorer lists "
                   L"Casso running on this PC automatically.";
            break;

        case LinkTransport::WifiModem:
            note = L"A serial Wi-Fi modem on a //c's port 2 or a Super Serial Card. A CassoLink boot disk made for this "
                   L"connection dials this PC on TCP port 6400; the address is listed under the connection.";
            break;

        case LinkTransport::Uthernet:
            note = L"An Uthernet II card at a static IP address. An Apple on this PC's network that boots a CassoLink "
                   L"disk broadcasts its address, and Casso Explorer lists it automatically.";
            break;

        case LinkTransport::AdtProServer:
            note = L"This connection allows a standard ADTPro client on the Apple to transfer disk images to and from "
                   L"this PC.";
            break;

        default:
            break;
    }

    return note;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkChoices::GetAddressPlaceholder
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * CassoExplorerLinkChoices::GetAddressPlaceholder (LinkTransport transport)
{
    return (transport == LinkTransport::Uthernet) ? L"192.168.1.50:6502" : L"192.168.1.5:6502";
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkChoices::ValidateAddress
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerLinkChoices::ValidateAddress (const std::wstring & address)
{
    std::wstring  error;
    size_t        colon = address.rfind (L':');
    int           port  = 0;
    bool          valid = colon != std::wstring::npos
                       && colon > 0
                       && colon + 1 < address.size()
                       && address.find_first_not_of (L"0123456789", colon + 1) == std::wstring::npos
                       && (port = _wtoi (address.c_str() + colon + 1)) >= 1 && port <= 65535;



    if (!address.empty() && !valid)
    {
        error = L"Enter a host and a port, such as 192.168.1.5:6502.";
    }

    return error;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkChoices::ValidateAddressFor
//
//  The card's own address is set into its chip, so it is an IPv4 address,
//  not a name.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerLinkChoices::ValidateAddressFor (LinkTransport transport, const std::wstring & text)
{
    std::wstring       error = ValidateAddress (text);
    CassoLinkSettings  settings;
    LinkNetworkConfig  network;
    HRESULT            hr    = S_OK;



    if (error.empty() && !text.empty() && transport == LinkTransport::Uthernet)
    {
        settings.transport = LinkTransport::Uthernet;
        settings.address   = TextEncoding::WideToNarrow (text);

        hr    = CassoLinkConnections::MakeNetworkConfig (settings, {}, network);
        error = FAILED (hr) ? std::wstring (L"Enter the card's IPv4 address and a port, such as 192.168.1.50:6502.") : std::wstring();
    }

    return error;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkChoices::IsComplete
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerLinkChoices::IsComplete (const CassoLinkSettings & settings)
{
    bool  addressed = settings.transport == LinkTransport::Tcp
                   || settings.transport == LinkTransport::Uthernet
                   || (settings.transport == LinkTransport::AdtProServer && settings.port.empty());
    bool  foldered  = settings.transport == LinkTransport::AdtProServer;



    return (!addressed || !settings.address.empty()) && (!foldered || !settings.folder.empty());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkChoices::ValidateDuplicate
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerLinkChoices::ValidateDuplicate (const CassoLinkSettings & settings, const std::vector<std::wstring> & otherNames)
{
    std::wstring  name  = TextEncoding::NarrowToWide (settings.name);
    bool          taken = std::any_of (otherNames.begin(), otherNames.end(), [&name] (const std::wstring & other) { return _wcsicmp (other.c_str(), name.c_str()) == 0; });
    std::wstring  error;



    if (taken)
    {
        error = std::format (L"{} is listed already, with these settings.", name);
    }

    return error;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkChoices::MakeSettings
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkSettings CassoExplorerLinkChoices::MakeSettings (int transportIndex, const std::vector<std::wstring> & comPorts, int portIndex, const std::wstring & address,
                                                          int speedIndex, int bootstrapIndex, bool hardwareHandshake, const std::wstring & folder)
{
    CassoLinkSettings  settings;
    int                speeds = (int) ARRAYSIZE (kSpeeds);
    bool               onPort = portIndex >= 1 && portIndex <= (int) comPorts.size();



    settings.transport = GetTransport (transportIndex);
    settings.port      = (UsesPort (settings.transport) && onPort) ? TextEncoding::WideToNarrow (comPorts[(size_t) portIndex - 1]) : std::string();
    settings.address   = UsesAddress (settings.transport, onPort ? portIndex : 0) ? TextEncoding::WideToNarrow (address) : std::string();
    settings.folder    = UsesFolder (settings.transport) ? folder : std::wstring();

    //  A Wi-Fi modem connects from the network, to any of this PC's
    //  addresses, on the port its boot disk dials.
    if (settings.transport == LinkTransport::WifiModem)
    {
        settings.address = std::format ("{}:{}", ByteStreamSettings::kAnyAddress, CassoLinkSettings::kModemPort);
    }

    if (UsesSpeed (settings.transport, onPort ? portIndex : 0))
    {
        settings.baud = kSpeeds[(speedIndex >= 0 && speedIndex < speeds) ? speedIndex : 0];
    }

    if (UsesBootstrap (settings.transport))
    {
        settings.bootstrapBaud = kSpeeds[(bootstrapIndex >= 0 && bootstrapIndex < speeds) ? bootstrapIndex : 0];
    }

    settings.hardwareHandshake = UsesHandshake (settings.transport, onPort ? portIndex : 0) && hardwareHandshake;
    settings.name              = CassoLinkSettings::MakeName (settings);

    return settings;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkPanel::Init
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerLinkPanel::Init (const Children & children)
{
    m_kids = children;

    for (IDxuiControl * control : std::initializer_list<IDxuiControl *> {
             children.transportLabel, children.transport, children.note,
             children.portLabel, children.port, children.portNote,
             children.addressLabel, children.address, children.addressError,
             children.bootstrapNote, children.bootstrapLabel, children.bootstrap,
             children.speedLabel, children.speed,
             children.handshakeLabel, children.handshakeTip, children.handshake,
             children.folderNote, children.folderLabel, children.folder, children.browse,
             children.duplicate })
    {
        Adopt (*control);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkPanel::Layout
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerLinkPanel::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int  used = 0;



    SetBounds (boundsDip);
    m_scaler = scaler;

    used = ArrangeRows (boundsDip, scaler, true);
    IGNORE_RETURN_VALUE (used, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkPanel::TryMeasureHeightPx
//
//  The rows' height at the width, with the window's renderer and theme, so
//  DxuiDialogWindow grows the window wherever a note or an error would
//  otherwise run into the buttons.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerLinkPanel::TryMeasureHeightPx (IDxuiTextRenderer & text, const IDxuiTheme & theme, int widthPx, int & heightPx)
{
    RECT  bounds = { m_boundsDip.left, m_boundsDip.top, m_boundsDip.left + widthPx, m_boundsDip.bottom };



    m_text  = &text;
    m_theme = &theme;

    heightPx = ArrangeRows (bounds, m_scaler, false);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkPanel::ArrangeRows
//
//  The transport's row, its note under it across the field's column, the
//  rows the transport uses with their notes, then the error for a connection
//  listed already.
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerLinkPanel::ArrangeRows (const RECT & bounds, const DxuiDpiScaler & scaler, bool place) const
{
    int  y      = bounds.top;
    int  fieldX = bounds.left + scaler.ToPx (kLabelWidthDip);
    int  fieldW = (std::max) ((int) bounds.right - fieldX, 1);
    int  err    = 0;



    y = PlaceRow  (bounds, y, m_kids.transportLabel, m_kids.transport, nullptr,             scaler, place);
    y = PlaceNote (bounds, y, m_kids.note,                                                  scaler, place);
    y = PlaceRow  (bounds, y, m_kids.portLabel,      m_kids.port,      nullptr,             scaler, place);
    y = PlaceNote (bounds, y, m_kids.portNote,                                              scaler, place);
    y = PlaceRow  (bounds, y, m_kids.addressLabel,   m_kids.address,   m_kids.addressError, scaler, place);
    y = PlaceNote (bounds, y, m_kids.bootstrapNote,                                         scaler, place);
    y = PlaceRow  (bounds, y, m_kids.bootstrapLabel, m_kids.bootstrap, nullptr,             scaler, place);
    y = PlaceRow  (bounds, y, m_kids.speedLabel,     m_kids.speed,     nullptr,             scaler, place);
    y = PlaceRow  (bounds, y, m_kids.handshakeLabel, m_kids.handshake, nullptr,             scaler, place, m_kids.handshakeTip);
    y = PlaceNote (bounds, y, m_kids.folderNote,                                            scaler, place);
    y = PlaceRow  (bounds, y, m_kids.folderLabel,    m_kids.folder,    nullptr,             scaler, place, nullptr, m_kids.browse);

    err = GetErrorHeightPx (*m_kids.duplicate, fieldW, scaler);

    if (place)
    {
        m_kids.duplicate->Layout (RECT { fieldX, y, bounds.right, y + err }, scaler);
    }

    y += err;

    return y - bounds.top;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkPanel::PlaceRow
//
//  A label and its field, an info tip after the label's text when the row
//  has one, a button at the field's end when the row has one, then the
//  field's error under it; returns where the next row starts.
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerLinkPanel::PlaceRow (const RECT & bounds, int y, DxuiLabel * label, IDxuiControl * field, DxuiFieldError * error,
                                      const DxuiDpiScaler & scaler, bool place, DxuiInfoTip * tip, DxuiButton * trailing) const
{
    int  row     = scaler.ToPx (kRowHeightDip);
    int  gap     = scaler.ToPx (kRowGapDip);
    int  fieldX  = bounds.left + scaler.ToPx (kLabelWidthDip);
    int  fieldR  = (trailing != nullptr) ? (int) bounds.right - scaler.ToPx (kBrowseWidthDip) - gap : (int) bounds.right;
    int  fieldW  = (std::max) (fieldR - fieldX, 1);
    int  err     = 0;



    if (!field->IsVisible())
    {
        return y;
    }

    if (place)
    {
        label->Layout (RECT { bounds.left, y, fieldX, y + row }, scaler);
        field->Layout (RECT { fieldX, y, fieldR, y + row }, scaler);

        if (tip != nullptr)
        {
            tip->SetRect (SettingsInfoTip::GetRect (*label, scaler, m_text));
            tip->SetDpi  (scaler.GetDpi());
        }

        if (trailing != nullptr)
        {
            trailing->Layout (RECT { fieldR + gap, y, bounds.right, y + row });
            trailing->SetDpi (scaler.GetDpi());
        }
    }

    y += row;

    if (error != nullptr)
    {
        err = GetErrorHeightPx (*error, fieldW, scaler);
        y  += (err > 0) ? scaler.ToPx (kErrorTopDip) : 0;

        if (place)
        {
            error->Layout (RECT { fieldX, y, fieldR, y + err }, scaler);
        }

        y += err;
    }

    return y + gap;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkPanel::PlaceNote
//
//  A note across the field's column at its wrapped height; returns where
//  the next row starts. A hidden or empty note takes no room.
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerLinkPanel::PlaceNote (const RECT & bounds, int y, DxuiLabel * note, const DxuiDpiScaler & scaler, bool place) const
{
    int  gap    = scaler.ToPx (kRowGapDip);
    int  fieldX = bounds.left + scaler.ToPx (kLabelWidthDip);
    int  fieldW = (std::max) ((int) bounds.right - fieldX, 1);
    int  height = 0;



    if (!note->IsVisible() || note->GetText().empty())
    {
        return y;
    }

    height = GetNoteHeightPx (*note, fieldW, scaler);

    if (place)
    {
        note->Layout (RECT { fieldX, y, bounds.right, y + height }, scaler);
    }

    return y + height + gap;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkPanel::GetErrorHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerLinkPanel::GetErrorHeightPx (const DxuiFieldError & error, int widthPx, const DxuiDpiScaler & scaler) const
{
    if (!error.HasError())
    {
        return 0;
    }

    if (m_text == nullptr || m_theme == nullptr)
    {
        return scaler.ToPx (kFallbackErrorDip);
    }

    return error.GetHeightPx (*m_text, *m_theme, scaler, widthPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkPanel::GetNoteHeightPx
//
//  The note's text wrapped at the width in the body font, as DxuiLabel
//  draws it.
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerLinkPanel::GetNoteHeightPx (const DxuiLabel & note, int widthPx, const DxuiDpiScaler & scaler) const
{
    HRESULT  hr    = S_OK;
    float    textW = 0.0f;
    float    textH = 0.0f;



    if (m_text == nullptr || m_theme == nullptr)
    {
        return scaler.ToPx (kFallbackNoteDip);
    }

    hr = m_text->MeasureStringWrapped (note.GetText().c_str(), scaler.ToPxf (m_theme->BodyFont().sizeDip), DxuiTheme::GetUiFace(),
                                       (float) widthPx, textW, textH);

    if (FAILED (hr))
    {
        return scaler.ToPx (kFallbackNoteDip);
    }

    return (int) std::ceil (textH);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkPanel::OnMouse
//
//  The open dropdowns first, since their lists extend over the rows below.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerLinkPanel::OnMouse (const DxuiMouseEvent & ev)
{
    IDxuiControl *  fields[] = { m_kids.transport, m_kids.port, m_kids.speed, m_kids.bootstrap, m_kids.address, m_kids.handshake,
                                 m_kids.folder, m_kids.browse, m_kids.handshakeTip };
    bool            handled  = false;



    for (IDxuiControl * field : fields)
    {
        if (!field->IsVisible() || !field->IsEnabled())
        {
            continue;
        }

        handled = field->OnMouse (ev);

        if (handled)
        {
            if (ev.kind == DxuiMouseEventKind::Down && m_onPressed)
            {
                m_onPressed (field);
            }

            break;
        }
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkDialog::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerLinkDialog::OnCreate()
{
    using Choices = CassoExplorerLinkChoices;

    CassoExplorerLinkPanel::Children  kids;



    for (DxuiLabel * label : { &m_transportLabel, &m_portLabel, &m_addressLabel, &m_bootstrapLabel, &m_speedLabel, &m_handshakeLabel, &m_folderLabel })
    {
        label->SetTextRole  (DxuiTextRole::Body);
        label->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    }

    m_transportLabel.SetText (L"Connection:");
    m_portLabel.SetText      (L"COM port:");
    m_addressLabel.SetText   (L"Address:");
    m_bootstrapLabel.SetText (L"Bootstrap speed:");
    m_speedLabel.SetText     (L"Transfer speed:");
    m_handshakeLabel.SetText (L"Handshake:");
    m_folderLabel.SetText    (L"Folder:");

    for (DxuiLabel * note : { &m_note, &m_portNote, &m_bootstrapNote, &m_folderNote })
    {
        note->SetTextRole  (DxuiTextRole::Muted);
        note->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Top);
    }

    m_portNote.SetText      (Choices::kPortNote);
    m_bootstrapNote.SetText (Choices::kBootstrapNote);
    m_folderNote.SetText    (Choices::kFolderNote);

    m_handshakeTip.SetText      (Choices::kHandshakeTip);
    m_handshakeTip.SetPopupHost (GetPopupHost());

    for (DxuiTextInput * input : { &m_address, &m_folder })
    {
        input->SetTheme        (m_theme);
        input->SetHwnd         (GetHwnd());
        input->SetTextRenderer (GetTextRenderer());
        input->SetOnChange     ([this] (const std::wstring &) { Revalidate(); });
    }

    m_address.SetText (TextEncoding::NarrowToWide (m_initial.transport == LinkTransport::WifiModem ? std::string() : m_initial.address));
    m_folder.SetText  (m_initial.folder);
    m_shownTransport = m_initial.transport;

    for (DxuiComboBox * combo : { &m_transport, &m_port, &m_speed, &m_bootstrap })
    {
        combo->SetPopupHost (GetPopupHost());
        combo->SetSelect    ([this] (int) { Revalidate(); });
    }

    m_transport.SetItems    (Choices::GetTransportLabels());
    m_transport.SetSelected (Choices::GetTransportIndex (m_initial.transport));
    m_port.SetItems         (Choices::GetPortLabels (m_initial.transport, m_comPorts));
    m_port.SetSelected      (Choices::GetPortIndex (m_initial, m_comPorts));
    m_speed.SetItems        (Choices::GetSpeedLabels());
    m_speed.SetSelected     (Choices::GetSpeedIndex (m_initial.baud));
    m_bootstrap.SetItems    (Choices::GetSpeedLabels());
    m_bootstrap.SetSelected (Choices::GetSpeedIndex (m_initial.bootstrapBaud));

    m_handshake.SetSingleLineLabel (true);
    m_handshake.SetChecked         (m_initial.hardwareHandshake);

    m_browse.SetOnClick ([this]()
    {
        std::wstring  folder = m_folder.GetText();

        if (m_browseFolder && m_browseFolder (GetHwnd(), folder))
        {
            m_folder.SetText (folder);
            Revalidate();
        }
    });

    kids.transportLabel = &m_transportLabel;
    kids.transport      = &m_transport;
    kids.note           = &m_note;
    kids.portLabel      = &m_portLabel;
    kids.port           = &m_port;
    kids.portNote       = &m_portNote;
    kids.addressLabel   = &m_addressLabel;
    kids.address        = &m_address;
    kids.addressError   = &m_addressError;
    kids.bootstrapNote  = &m_bootstrapNote;
    kids.bootstrapLabel = &m_bootstrapLabel;
    kids.bootstrap      = &m_bootstrap;
    kids.speedLabel     = &m_speedLabel;
    kids.speed          = &m_speed;
    kids.handshakeLabel = &m_handshakeLabel;
    kids.handshakeTip   = &m_handshakeTip;
    kids.handshake      = &m_handshake;
    kids.folderNote     = &m_folderNote;
    kids.folderLabel    = &m_folderLabel;
    kids.folder         = &m_folder;
    kids.browse         = &m_browse;
    kids.duplicate      = &m_duplicate;

    m_body = CreateDialogContent<CassoExplorerLinkPanel>();
    m_body->Init (kids);
    m_body->SetMeasure (GetTextRenderer(), m_theme);
    m_body->SetOnChildPressed ([this] (IDxuiControl * child) { FocusControl (child); });

    m_ok = AddDialogButton (m_adding ? L"Add" : L"OK", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    m_ok->SetOnClick ([this]()
    {
        m_outcome.confirmed = true;
        m_outcome.settings  = MakeChosen();
        EndDialog (IDOK);
    });

    m_validator.AddField ([this]()
    {
        return Choices::ValidateAddressFor (Choices::GetTransport (m_transport.GetSelectedIndex()), m_address.GetText());
    }, &m_addressError);

    //  A required row left empty disables Add with no error displayed,
    //  so a form just opened has none.
    m_validator.AddField ([this]()
    {
        return Choices::IsComplete (MakeChosen()) ? std::wstring() : std::wstring (L"incomplete");
    }, nullptr);

    m_validator.AddField ([this]()
    {
        return Choices::ValidateDuplicate (MakeChosen(), m_otherNames);
    }, &m_duplicate);

    m_validator.SetConfirmButton (m_ok);

    SetDialogTickIntervalMs (50);

    Revalidate();

    SetInitialFocus (&m_transport);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkDialog::Ask
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerLinkDialog::Outcome CassoExplorerLinkDialog::Ask (HWND owner, const IDxuiTheme * theme, const CassoLinkSettings & initial,
                                                               const std::vector<std::wstring> & otherNames, const std::vector<std::wstring> & comPorts,
                                                               bool adding, BrowseFn browse)
{
    HRESULT                   hr     = S_OK;
    CassoExplorerLinkDialog   dialog;
    DxuiWindow::CreateParams  params;



    dialog.m_theme        = theme;
    dialog.m_initial      = initial;
    dialog.m_otherNames   = otherNames;
    dialog.m_comPorts     = comPorts;
    dialog.m_adding       = adding;
    dialog.m_browseFolder = std::move (browse);

    params.title                    = adding ? L"Add a CassoLink connection" : L"Edit the CassoLink connection";
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { 560, 190 + CassoExplorerLinkPanel::kRowCount * (CassoExplorerLinkPanel::kRowHeightDip + CassoExplorerLinkPanel::kRowGapDip) };
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);

    if (FAILED (hr))
    {
        return Outcome();
    }

    dialog.SetTheme (theme);
    dialog.ShowModalDialog (IDOK);

    return dialog.m_outcome;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkDialog::MakeChosen
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkSettings CassoExplorerLinkDialog::MakeChosen() const
{
    return CassoExplorerLinkChoices::MakeSettings (m_transport.GetSelectedIndex(), m_comPorts, m_port.GetSelectedIndex(), m_address.GetText(),
                                                   m_speed.GetSelectedIndex(), m_bootstrap.GetSelectedIndex(), m_handshake.IsChecked(),
                                                   m_folder.GetText());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkDialog::Revalidate
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerLinkDialog::Revalidate()
{
    bool  valid = false;



    ShowTransportRows();

    valid = m_validator.Revalidate();
    IGNORE_RETURN_VALUE (valid, true);

    FitToContent();

    if (m_body != nullptr)
    {
        m_body->Relayout();
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkDialog::ShowTransportRows
//
//  Only the chosen transport's rows are visible. A change of
//  transport lists the port drop-down's choices for the new one, keeping
//  the port chosen.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerLinkDialog::ShowTransportRows()
{
    using Choices = CassoExplorerLinkChoices;

    LinkTransport  transport = Choices::GetTransport (m_transport.GetSelectedIndex());
    int            port      = m_port.GetSelectedIndex();



    if (transport != m_shownTransport)
    {
        m_port.SetItems    (Choices::GetPortLabels (transport, m_comPorts));
        m_port.SetSelected (port);
    }

    m_shownTransport = transport;
    port             = m_port.GetSelectedIndex();

    m_note.SetText              (Choices::GetNote (transport));
    m_portLabel.SetText         (transport == LinkTransport::AdtProServer ? L"Line:" : L"COM port:");
    m_address.SetPlaceholder    (Choices::GetAddressPlaceholder (transport));

    m_portLabel.SetVisible      (Choices::UsesPort (transport));
    m_port.SetVisible           (Choices::UsesPort (transport));
    m_portNote.SetVisible       (transport == LinkTransport::Serial);
    m_addressLabel.SetVisible   (Choices::UsesAddress (transport, port));
    m_address.SetVisible        (Choices::UsesAddress (transport, port));
    m_addressError.SetVisible   (Choices::UsesAddress (transport, port));
    m_bootstrapNote.SetVisible  (Choices::UsesBootstrap (transport));
    m_bootstrapLabel.SetVisible (Choices::UsesBootstrap (transport));
    m_bootstrap.SetVisible      (Choices::UsesBootstrap (transport));
    m_speedLabel.SetVisible     (Choices::UsesSpeed (transport, port));
    m_speed.SetVisible          (Choices::UsesSpeed (transport, port));
    m_handshakeLabel.SetVisible (Choices::UsesHandshake (transport, port));
    m_handshakeTip.SetVisible   (Choices::UsesHandshake (transport, port));
    m_handshake.SetVisible      (Choices::UsesHandshake (transport, port));
    m_folderNote.SetVisible     (Choices::UsesFolder (transport));
    m_folderLabel.SetVisible    (Choices::UsesFolder (transport));
    m_folder.SetVisible         (Choices::UsesFolder (transport));
    m_browse.SetVisible         (Choices::UsesFolder (transport));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkDialog::FitToContent
//
//  The window grows or shrinks so the buttons, anchored to its bottom, sit
//  just under the rows.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerLinkDialog::FitToContent()
{
    RECT  content = {};
    RECT  window  = {};
    int   delta   = 0;



    if (m_body == nullptr || GetHwnd() == nullptr)
    {
        return;
    }

    content = m_body->GetBounds();

    if (content.bottom <= content.top || content.right <= content.left)
    {
        return;
    }

    delta = m_body->GetRequiredHeightPx() - (int) (content.bottom - content.top);

    if (delta != 0 && GetWindowRect (GetHwnd(), &window))
    {
        SetWindowPos (GetHwnd(), nullptr, 0, 0, window.right - window.left, window.bottom - window.top + delta,
                      SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkDialog::OnDialogTick
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerLinkDialog::OnDialogTick()
{
    if (!m_fitted && m_body != nullptr && m_body->GetBounds().bottom > m_body->GetBounds().top)
    {
        m_fitted = true;
        FitToContent();
    }
}
