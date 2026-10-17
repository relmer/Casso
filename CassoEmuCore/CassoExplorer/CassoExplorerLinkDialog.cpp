#include "Pch.h"

#include "CassoExplorer/CassoExplorerLinkDialog.h"
#include "Core/TextEncoding.h"





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
        labels.push_back (std::format (L"{} baud", speed));
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
//  CassoExplorerLinkChoices::ValidateName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerLinkChoices::ValidateName (const std::wstring & name, const std::vector<std::wstring> & otherNames)
{
    std::wstring  error;
    bool          taken = std::any_of (otherNames.begin(), otherNames.end(), [&name] (const std::wstring & other) { return _wcsicmp (other.c_str(), name.c_str()) == 0; });



    if (name.find_first_not_of (L' ') == std::wstring::npos)
    {
        error = L"Enter a name for the machine.";
    }
    else if (name.find_first_of (L"\\|:") != std::wstring::npos)
    {
        error = L"A machine's name cannot contain \\, | or :.";
    }
    else if (taken)
    {
        error = L"Another machine has this name.";
    }

    return error;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkChoices::ValidatePort
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerLinkChoices::ValidatePort (const std::wstring & port)
{
    std::wstring  error;
    int           number = 0;
    bool          valid  = port.empty()
                        || (_wcsnicmp (port.c_str(), L"COM", 3) == 0
                            && port.size() > 3
                            && port.find_first_not_of (L"0123456789", 3) == std::wstring::npos
                            && (number = _wtoi (port.c_str() + 3)) >= 1 && number <= 256);



    if (!valid)
    {
        error = L"Enter a port such as COM3, or leave it blank to search the ports for the Apple.";
    }

    return error;
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



    if (!valid)
    {
        error = L"Enter a host and a port, such as 127.0.0.1:6502.";
    }

    return error;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkChoices::ValidateTcpPort
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerLinkChoices::ValidateTcpPort (const std::wstring & port)
{
    std::wstring  error;
    bool          digits = !port.empty() && port.size() <= 5 && port.find_first_not_of (L"0123456789") == std::wstring::npos;
    int           value  = digits ? _wtoi (port.c_str()) : 0;



    if (value < 1 || value > 65535)
    {
        error = L"Enter a TCP port from 1 to 65535.";
    }

    return error;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkChoices::GetAddressLabel
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * CassoExplorerLinkChoices::GetAddressLabel (LinkTransport transport)
{
    return transport == LinkTransport::WifiModem ? L"TCP port:" : L"Address:";
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkChoices::GetDefaultAddress
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerLinkChoices::GetDefaultAddress (LinkTransport transport)
{
    std::wstring  address;



    switch (transport)
    {
        case LinkTransport::Emulated:  address = L"127.0.0.1:6502"; break;
        case LinkTransport::WifiModem: address = L"6502";           break;
        default:                                                    break;
    }

    return address;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkChoices::GetAddressText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerLinkChoices::GetAddressText (const AppleLinkSettings & settings)
{
    std::wstring  address = TextEncoding::NarrowToWide (settings.address);
    size_t        colon   = address.rfind (L':');



    if (settings.transport == LinkTransport::WifiModem && colon != std::wstring::npos)
    {
        address = address.substr (colon + 1);
    }

    return address.empty() ? GetDefaultAddress (settings.transport) : address;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkChoices::ValidateAddressFor
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerLinkChoices::ValidateAddressFor (LinkTransport transport, const std::wstring & text)
{
    std::wstring  error;



    if (transport == LinkTransport::WifiModem)
    {
        error = ValidateTcpPort (text);
    }
    else if (UsesAddress (transport))
    {
        error = ValidateAddress (text);
    }

    return error;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkChoices::MakeSettings
//
////////////////////////////////////////////////////////////////////////////////

AppleLinkSettings CassoExplorerLinkChoices::MakeSettings (const std::wstring & name, int transportIndex, const std::wstring & port, const std::wstring & address,
                                                          int speedIndex, int bootstrapIndex, bool hardwareHandshake)
{
    AppleLinkSettings  settings;
    int                transports = (int) ARRAYSIZE (kTransports);
    int                speeds     = (int) ARRAYSIZE (kSpeeds);



    settings.name              = TextEncoding::WideToNarrow (name);
    settings.transport         = kTransports[(transportIndex >= 0 && transportIndex < transports) ? transportIndex : 0].transport;
    settings.port              = UsesPort (settings.transport) ? TextEncoding::WideToNarrow (port) : std::string();
    settings.address           = UsesAddress (settings.transport) ? TextEncoding::WideToNarrow (address) : std::string();

    //  A Wi-Fi modem connects from the network, to any of this PC's addresses.
    if (settings.transport == LinkTransport::WifiModem)
    {
        settings.address = std::string (ByteStreamSettings::kAnyAddress) + ":" + settings.address;
    }

    //  A row the transport does not have takes its default.
    if (UsesSpeed (settings.transport))
    {
        settings.baud = kSpeeds[(speedIndex >= 0 && speedIndex < speeds) ? speedIndex : 0];
    }

    if (UsesBootstrap (settings.transport))
    {
        settings.bootstrapBaud = kSpeeds[(bootstrapIndex >= 0 && bootstrapIndex < speeds) ? bootstrapIndex : 0];
    }

    settings.hardwareHandshake = UsesHandshake (settings.transport) && hardwareHandshake;

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
             children.nameLabel, children.name, children.nameError,
             children.transportLabel, children.transport,
             children.portLabel, children.port, children.portError,
             children.addressLabel, children.address, children.addressError,
             children.speedLabel, children.speed,
             children.bootstrapLabel, children.bootstrap,
             children.handshake })
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
//  CassoExplorerLinkPanel::ArrangeRows
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerLinkPanel::ArrangeRows (const RECT & bounds, const DxuiDpiScaler & scaler, bool place) const
{
    int  y      = bounds.top;
    int  row    = scaler.ToPx (kRowHeightDip);
    int  fieldX = bounds.left + scaler.ToPx (kLabelWidthDip);



    y = PlaceRow (bounds, y, m_kids.nameLabel,      m_kids.name,      m_kids.nameError,    scaler, place);
    y = PlaceRow (bounds, y, m_kids.transportLabel, m_kids.transport, nullptr,             scaler, place);
    y = PlaceRow (bounds, y, m_kids.portLabel,      m_kids.port,      m_kids.portError,    scaler, place);
    y = PlaceRow (bounds, y, m_kids.addressLabel,   m_kids.address,   m_kids.addressError, scaler, place);
    y = PlaceRow (bounds, y, m_kids.speedLabel,     m_kids.speed,     nullptr,             scaler, place);
    y = PlaceRow (bounds, y, m_kids.bootstrapLabel, m_kids.bootstrap, nullptr,             scaler, place);

    if (place && m_kids.handshake->IsVisible())
    {
        m_kids.handshake->Layout (RECT { fieldX, y, bounds.right, y + row }, scaler);
    }

    y += m_kids.handshake->IsVisible() ? row : 0;

    return y - bounds.top;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkPanel::PlaceRow
//
//  A label and its field, then the field's error under it when it has one;
//  returns where the next row starts.
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerLinkPanel::PlaceRow (const RECT & bounds, int y, DxuiLabel * label, IDxuiControl * field, DxuiFieldError * error,
                                      const DxuiDpiScaler & scaler, bool place) const
{
    int  row    = scaler.ToPx (kRowHeightDip);
    int  gap    = scaler.ToPx (kRowGapDip);
    int  fieldX = bounds.left + scaler.ToPx (kLabelWidthDip);
    int  fieldW = (std::max) ((int) bounds.right - fieldX, 1);
    int  err    = 0;



    if (!field->IsVisible())
    {
        return y;
    }

    if (place)
    {
        label->Layout (RECT { bounds.left, y, fieldX, y + row }, scaler);
        field->Layout (RECT { fieldX, y, bounds.right, y + row }, scaler);
    }

    y += row;

    if (error != nullptr)
    {
        err = GetErrorHeightPx (*error, fieldW, scaler);
        y  += (err > 0) ? scaler.ToPx (kErrorTopDip) : 0;

        if (place)
        {
            error->Layout (RECT { fieldX, y, bounds.right, y + err }, scaler);
        }

        y += err;
    }

    return y + gap;
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
//  CassoExplorerLinkPanel::OnMouse
//
//  The open dropdowns first, since their lists extend over the rows below.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerLinkPanel::OnMouse (const DxuiMouseEvent & ev)
{
    IDxuiControl *  fields[] = { m_kids.transport, m_kids.speed, m_kids.bootstrap, m_kids.name, m_kids.port, m_kids.address, m_kids.handshake };
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
    CassoExplorerLinkPanel::Children  kids;



    for (DxuiLabel * label : { &m_nameLabel, &m_transportLabel, &m_portLabel, &m_addressLabel, &m_speedLabel, &m_bootstrapLabel })
    {
        label->SetTextRole  (DxuiTextRole::Body);
        label->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    }

    m_nameLabel.SetText      (L"Name:");
    m_transportLabel.SetText (L"Connection:");
    m_portLabel.SetText      (L"Port:");
    m_addressLabel.SetText   (L"Address:");
    m_speedLabel.SetText     (L"Speed:");
    m_bootstrapLabel.SetText (L"Bootstrap speed:");

    for (DxuiTextInput * input : { &m_name, &m_port, &m_address })
    {
        input->SetTheme        (m_theme);
        input->SetHwnd         (GetHwnd());
        input->SetTextRenderer (GetTextRenderer());
        input->SetOnChange     ([this] (const std::wstring &) { Revalidate(); });
    }

    m_name.SetMaxLength (CassoExplorerLinkChoices::kMaxNameLength);
    m_name.SetText      (TextEncoding::NarrowToWide (m_initial.name));
    m_name.SelectAll();

    m_port.SetText           (TextEncoding::NarrowToWide (m_initial.port));
    m_port.SetPlaceholder    (L"Search");
    m_address.SetText        (CassoExplorerLinkChoices::GetAddressText (m_initial));
    m_shownTransport = m_initial.transport;

    for (DxuiComboBox * combo : { &m_transport, &m_speed, &m_bootstrap })
    {
        combo->SetPopupHost (GetPopupHost());
        combo->SetSelect    ([this] (int) { Revalidate(); });
    }

    m_transport.SetItems    (CassoExplorerLinkChoices::GetTransportLabels());
    m_transport.SetSelected (CassoExplorerLinkChoices::GetTransportIndex (m_initial.transport));
    m_speed.SetItems        (CassoExplorerLinkChoices::GetSpeedLabels());
    m_speed.SetSelected     (CassoExplorerLinkChoices::GetSpeedIndex (m_initial.baud));
    m_bootstrap.SetItems    (CassoExplorerLinkChoices::GetSpeedLabels());
    m_bootstrap.SetSelected (CassoExplorerLinkChoices::GetSpeedIndex (m_initial.bootstrapBaud));

    m_handshake.SetSingleLineLabel (true);
    m_handshake.SetChecked         (m_initial.hardwareHandshake);

    kids.nameLabel      = &m_nameLabel;
    kids.name           = &m_name;
    kids.nameError      = &m_nameError;
    kids.transportLabel = &m_transportLabel;
    kids.transport      = &m_transport;
    kids.portLabel      = &m_portLabel;
    kids.port           = &m_port;
    kids.portError      = &m_portError;
    kids.addressLabel   = &m_addressLabel;
    kids.address        = &m_address;
    kids.addressError   = &m_addressError;
    kids.speedLabel     = &m_speedLabel;
    kids.speed          = &m_speed;
    kids.bootstrapLabel = &m_bootstrapLabel;
    kids.bootstrap      = &m_bootstrap;
    kids.handshake      = &m_handshake;

    m_body = CreateDialogContent<CassoExplorerLinkPanel>();
    m_body->Init (kids);
    m_body->SetMeasure (GetTextRenderer(), m_theme);
    m_body->SetOnChildPressed ([this] (IDxuiControl * child) { FocusControl (child); });

    m_ok = AddDialogButton (m_adding ? L"Add" : L"OK", IDOK);
    AddDialogButton (L"Cancel", IDCANCEL);

    m_ok->SetOnClick ([this]()
    {
        m_outcome.confirmed = true;
        m_outcome.settings  = CassoExplorerLinkChoices::MakeSettings (m_name.GetText(), m_transport.GetSelectedIndex(), m_port.GetText(), m_address.GetText(),
                                                                      m_speed.GetSelectedIndex(), m_bootstrap.GetSelectedIndex(), m_handshake.IsChecked());
        EndDialog (IDOK);
    });

    m_validator.AddField ([this]() { return CassoExplorerLinkChoices::ValidateName (m_name.GetText(), m_otherNames); }, &m_nameError);

    m_validator.AddField ([this]()
    {
        bool  serial = CassoExplorerLinkChoices::UsesPort (CassoExplorerLinkChoices::kTransports[m_transport.GetSelectedIndex()].transport);

        return serial ? CassoExplorerLinkChoices::ValidatePort (m_port.GetText()) : std::wstring();
    }, &m_portError);

    m_validator.AddField ([this]()
    {
        return CassoExplorerLinkChoices::ValidateAddressFor (CassoExplorerLinkChoices::kTransports[m_transport.GetSelectedIndex()].transport, m_address.GetText());
    }, &m_addressError);

    m_validator.SetConfirmButton (m_ok);

    SetDialogTickIntervalMs (50);

    Revalidate();

    SetInitialFocus (&m_name);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLinkDialog::Ask
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerLinkDialog::Outcome CassoExplorerLinkDialog::Ask (HWND owner, const IDxuiTheme * theme, const AppleLinkSettings & initial,
                                                               const std::vector<std::wstring> & otherNames, bool adding)
{
    HRESULT                   hr     = S_OK;
    CassoExplorerLinkDialog   dialog;
    DxuiWindow::CreateParams  params;



    dialog.m_theme      = theme;
    dialog.m_initial    = initial;
    dialog.m_otherNames = otherNames;
    dialog.m_adding     = adding;

    params.title                    = adding ? L"Add a CassoLink machine" : L"Edit the CassoLink machine";
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { 480, 130 + CassoExplorerLinkPanel::kRowCount * (CassoExplorerLinkPanel::kRowHeightDip + CassoExplorerLinkPanel::kRowGapDip) };
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
//  CassoExplorerLinkDialog::Revalidate
//
//  A serial link has a port and no address; every other transport the
//  reverse. The field out of use is disabled.
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
//  Only the chosen transport's rows are visible (FR-083c). A change of
//  transport replaces an address left empty or at the old transport's
//  default with the new one's.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerLinkDialog::ShowTransportRows()
{
    using Choices = CassoExplorerLinkChoices;

    LinkTransport  transport = Choices::kTransports[m_transport.GetSelectedIndex()].transport;
    LinkTransport  previous  = m_shownTransport;
    std::wstring   address   = m_address.GetText();



    m_shownTransport = transport;

    if (transport != previous && (address.empty() || address == Choices::GetDefaultAddress (previous)))
    {
        m_address.SetText (Choices::GetDefaultAddress (transport));
    }

    m_addressLabel.SetText (Choices::GetAddressLabel (transport));

    m_portLabel.SetVisible      (Choices::UsesPort (transport));
    m_port.SetVisible           (Choices::UsesPort (transport));
    m_portError.SetVisible      (Choices::UsesPort (transport));
    m_addressLabel.SetVisible   (Choices::UsesAddress (transport));
    m_address.SetVisible        (Choices::UsesAddress (transport));
    m_addressError.SetVisible   (Choices::UsesAddress (transport));
    m_speedLabel.SetVisible     (Choices::UsesSpeed (transport));
    m_speed.SetVisible          (Choices::UsesSpeed (transport));
    m_bootstrapLabel.SetVisible (Choices::UsesBootstrap (transport));
    m_bootstrap.SetVisible      (Choices::UsesBootstrap (transport));
    m_handshake.SetVisible      (Choices::UsesHandshake (transport));
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
