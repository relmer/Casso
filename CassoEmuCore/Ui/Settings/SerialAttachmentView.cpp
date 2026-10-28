#include "Pch.h"

#include "Ui/Settings/SerialAttachmentView.h"
#include "Core/TextEncoding.h"
#include "Seams/IHostSerialPort.h"





static constexpr float  s_kNoteFontDp  = 12.0f;
static constexpr int    s_kPortWidthDp = 90;





////////////////////////////////////////////////////////////////////////////////
//
//  SerialAttachmentView::SerialAttachmentView
//
//  None, an empty device, is first in the list, then the loopback plug.
//
////////////////////////////////////////////////////////////////////////////////

SerialAttachmentView::SerialAttachmentView (
    Kind          kind,
    std::wstring  label)
    : m_kind  (kind),
      m_label (std::move (label))
{
    m_devices = { "", kpszSerialLoopback, kpszSerialFileOut, kpszSerialFileIn, kpszSerialComPort, kpszSerialTcp, kpszSerialModem };

    m_attach.SetItems    ({ L"None", L"Loopback plug", L"Write output to file", L"Read input from file", L"COM port", L"TCP connection", L"Modem" });
    m_attach.SetSelected (0);
    m_connect.SetItems   ({ L"Connect to a host", L"Listen for a connection" });

    m_note.SetTextRole    (DxuiTextRole::Muted);
    m_note.SetFontSizeDip (s_kNoteFontDp);
    m_error.SetSeverity   (DxuiInfoBanner::Severity::Warning);
    m_error.SetText       (L"Invalid port number\nA port number is from 1 to 65535.");
    m_error.SetVisible    (false);
    m_listenPort.SetPlaceholder (L"None");

    m_attach.SetSelect      ([this] (int picked) { OnAttachPicked (picked); });
    m_comPort.SetSelect     ([this] (int picked) { OnComPortPicked (picked); });
    m_comPort.SetOnOpen     ([this] () { RefreshComPorts(); });
    m_connect.SetSelect     ([this] (int picked) { OnConnectPicked (picked); });
    m_host.SetOnChange      ([this] (const std::wstring & text) { OnHostChanged (text); });
    m_port.SetOnChange      ([this] (const std::wstring & text) { OnPortChanged (text); });
    m_listenPort.SetOnChange ([this] (const std::wstring & text) { OnListenChanged (text); });
    m_telnet.SetOnChange    ([this] (bool isOn) { OnTelnetChanged (isOn); });
    m_browse.SetOnClick     ([this] () { OnBrowse(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialAttachmentView::AdoptInto
//
////////////////////////////////////////////////////////////////////////////////

void SerialAttachmentView::AdoptInto (DxuiPanel & panel)
{
    panel.Adopt (m_attachLabel);
    panel.Adopt (m_attach);
    panel.Adopt (m_note);
    panel.Adopt (m_pathLabel);
    panel.Adopt (m_pathValue);
    panel.Adopt (m_browse);
    panel.Adopt (m_comPortLabel);
    panel.Adopt (m_comPort);
    panel.Adopt (m_connectLabel);
    panel.Adopt (m_connect);
    panel.Adopt (m_hostLabel);
    panel.Adopt (m_host);
    panel.Adopt (m_portLabel);
    panel.Adopt (m_port);
    panel.Adopt (m_listenPortLabel);
    panel.Adopt (m_listenPort);
    panel.Adopt (m_telnet);
    panel.Adopt (m_error);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialAttachmentView::SetPopupHost
//
////////////////////////////////////////////////////////////////////////////////

void SerialAttachmentView::SetPopupHost (DxuiHwndSource * host)
{
    m_attach.SetPopupHost  (host);
    m_comPort.SetPopupHost (host);
    m_connect.SetPopupHost (host);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialAttachmentView::SetChoice
//
//  Sets the controls to a staged choice; nothing is reported.
//
////////////////////////////////////////////////////////////////////////////////

void SerialAttachmentView::SetChoice (const SerialAttachmentChoice & choice)
{
    m_choice = choice;

    SyncControls();
    ShowError (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialAttachmentView::SyncControls
//
//  Every control to the choice.
//
////////////////////////////////////////////////////////////////////////////////

void SerialAttachmentView::SyncControls()
{
    int  selected = 0;
    int  i        = 0;



    for (i = 0; i < (int) m_devices.size(); i++)
    {
        if (m_choice.device == m_devices[(size_t) i])
        {
            selected = i;
            break;
        }
    }

    m_attach.SetSelected  (selected);
    m_connect.SetSelected (m_choice.options.listen ? kListenChoice : kConnectChoice);
    m_host.SetText        (TextEncoding::NarrowToWide (m_choice.options.host));
    m_port.SetText        (std::format (L"{}", m_choice.options.tcpPort));
    m_listenPort.SetText  ((m_choice.options.modemListenPort == 0) ? std::wstring() : std::format (L"{}", m_choice.options.modemListenPort));
    m_telnet.SetChecked   (m_choice.options.IsTelnetOn (IsDevice (kpszSerialModem)));
    m_pathValue.SetText   (m_choice.path.empty() ? std::wstring (L"(none chosen)") : TextEncoding::NarrowToWide (m_choice.path));

    RefreshComPorts();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialAttachmentView::RefreshComPorts
//
//  The ports present now, with the staged one added when it is not, so a
//  choice made while an adapter was plugged in is listed.
//
////////////////////////////////////////////////////////////////////////////////

void SerialAttachmentView::RefreshComPorts()
{
    HRESULT                    hr       = S_OK;
    std::vector<std::wstring>  items;
    int                        selected = -1;
    size_t                     i        = 0;
    bool                       isListed = false;



    m_comPortNames.clear();

    if (m_ports != nullptr)
    {
        hr = m_ports->EnumeratePorts (m_comPortNames);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    isListed = std::find (m_comPortNames.begin(), m_comPortNames.end(), m_choice.options.comPort) != m_comPortNames.end();

    if (!isListed && !m_choice.options.comPort.empty())
    {
        m_comPortNames.push_back (m_choice.options.comPort);
    }

    for (i = 0; i < m_comPortNames.size(); i++)
    {
        items.push_back (TextEncoding::NarrowToWide (m_comPortNames[i]) + ((!isListed && m_comPortNames[i] == m_choice.options.comPort) ? L" (not present)" : L""));
        selected = (m_comPortNames[i] == m_choice.options.comPort) ? (int) i : selected;
    }

    if (items.empty())
    {
        items.push_back (L"(no COM ports)");
    }

    m_comPort.SetItems    (items);
    m_comPort.SetSelected (selected);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialAttachmentView::Layout
//
//  The attachment row and its note, then the rows of the chosen
//  attachment's options; the rows of the others are hidden.
//
////////////////////////////////////////////////////////////////////////////////

int SerialAttachmentView::Layout (
    int                     x,
    int                     y,
    int                     labelWidth,
    int                     controlsWidth,
    const Metrics         & metrics,
    const DxuiDpiScaler   & scaler)
{
    UINT  dpi        = scaler.GetDpi();
    int   controlsX  = x + labelWidth;
    int   portWidth  = scaler.ToPx (s_kPortWidthDp);
    bool  isFile     = IsDevice (kpszSerialFileOut) || IsDevice (kpszSerialFileIn);
    bool  isComPort  = IsDevice (kpszSerialComPort);
    bool  isTcp      = IsDevice (kpszSerialTcp);
    bool  isModem    = IsDevice (kpszSerialModem);
    bool  isConnect  = isTcp && !m_choice.options.listen;
    int   errorWidth = labelWidth + controlsWidth;
    int   noteWidth  = (metrics.noteWidth > 0) ? metrics.noteWidth : controlsWidth;
    auto  row        = [&] (DxuiLabel & label, const wchar_t * text, IDxuiControl * control, int width, bool isShown)
    {
        label.SetVisible (isShown);
        label.SetText    (text);
        label.SetRect    ({ x, y, x + labelWidth, y + metrics.rowHeight });
        label.SetDpi     (dpi);

        if (control != nullptr)
        {
            control->SetVisible (isShown);
            control->SetBounds  ({ controlsX, y, controlsX + width, y + metrics.rowHeight });
        }

        if (isShown)
        {
            y += metrics.rowHeight + metrics.rowGap;
        }
    };



    row (m_attachLabel, m_label.c_str(), &m_attach, controlsWidth, true);
    m_attach.SetDpi (dpi);

    m_noteText = GetNote (m_choice.device);
    m_note.SetVisible (!m_noteText.empty());
    m_note.SetText    (m_noteText);
    m_note.SetRect    ({ controlsX, y, controlsX + noteWidth, y + metrics.noteHeight });
    m_note.SetDpi     (dpi);

    if (!m_noteText.empty())
    {
        y += metrics.noteHeight + metrics.sectionGap;
    }

    row (m_pathLabel, L"File:", &m_pathValue, controlsWidth, isFile);
    m_pathValue.SetDpi (dpi);

    m_browse.SetLabel   (L"Browse...");
    m_browse.SetVisible (isFile);
    m_browse.Layout     ({ controlsX, y, controlsX + scaler.ToPx (DxuiButtonRow::GetWidthForLabel (L"Browse...")), y + metrics.rowHeight });
    m_browse.SetDpi     (dpi);

    if (isFile)
    {
        y += metrics.rowHeight + metrics.rowGap;
    }

    row (m_comPortLabel, L"COM port:",    &m_comPort,    controlsWidth, isComPort);
    row (m_connectLabel, L"Connection:",  &m_connect,    controlsWidth, isTcp);
    row (m_hostLabel,    L"Host:",        &m_host,       controlsWidth, isConnect);
    row (m_portLabel,    L"Port:",        &m_port,       portWidth,     isTcp);
    row (m_listenPortLabel, L"Listen port:", &m_listenPort, portWidth,  isModem);

    m_comPort.SetDpi    (dpi);
    m_connect.SetDpi    (dpi);
    m_host.SetDpi       (dpi);
    m_port.SetDpi       (dpi);
    m_listenPort.SetDpi (dpi);

    m_telnet.SetVisible (isTcp || isModem);
    m_telnet.SetRect    ({ controlsX, y, controlsX + controlsWidth, y + metrics.rowHeight });
    m_telnet.SetDpi     (dpi);

    if (isTcp || isModem)
    {
        y += metrics.rowHeight + metrics.rowGap;
    }

    if (m_error.IsVisible())
    {
        int  errorHeight = (int) std::ceil (m_error.GetPreferredHeightPx ((float) errorWidth, scaler));

        m_error.SetRect ({ x, y, x + errorWidth, y + errorHeight });
        y += errorHeight + metrics.rowGap;
    }

    m_error.SetDpi (dpi);

    return y;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialAttachmentView::GetNote
//
//  What each attachment does with the connector's lines.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SerialAttachmentView::GetNote (const std::string & device) const
{
    const wchar_t  * owner = (m_kind == Kind::Card) ? L"card" : L"port";
    std::wstring     note;



    if (device == kpszSerialLoopback)
    {
        note = std::format (L"TX is wired back to RX, RTS to CTS, and DTR to DSR and DCD, so what the {} sends comes back into it.", owner);
    }
    else if (device == kpszSerialFileOut)
    {
        note = std::format (L"All data the {} sends is appended to the file below.", owner);
    }
    else if (device == kpszSerialFileIn)
    {
        note = std::format (L"The file below is sent to the {} at its baud rate, once a program turns the {}'s receiver on.", owner, owner);
    }
    else if (device == kpszSerialComPort)
    {
        note = std::format (L"The {}'s data, baud rate, format and handshake lines are wired to a PC serial port.", owner);
    }
    else if (device == kpszSerialTcp)
    {
        note = L"Data passes over a TCP connection, made to a host or accepted on a port. DCD and DSR are on while it is open.";
    }
    else if (device == kpszSerialModem)
    {
        note = L"A Hayes-compatible modem that dials over TCP: ATDT host:port. With a listen port, a call to it rings.";
    }

    return note;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialAttachmentView::TryParsePort
//
////////////////////////////////////////////////////////////////////////////////

bool SerialAttachmentView::TryParsePort (
    const std::wstring  & text,
    bool                  isZeroAllowed,
    int                 & outPort)
{
    constexpr int     kMaxPort   = 65535;
    constexpr int     kDecimal   = 10;
    constexpr size_t  kMaxDigits = 5;
    int               value      = 0;
    bool              isNumeric  = !text.empty() && text.size() <= kMaxDigits && std::all_of (text.begin(), text.end(), [] (wchar_t ch) { return ch >= L'0' && ch <= L'9'; });



    if (text.empty() && isZeroAllowed)
    {
        outPort = 0;
        return true;
    }

    if (!isNumeric)
    {
        return false;
    }

    for (wchar_t ch : text)
    {
        value = value * kDecimal + (ch - L'0');
    }

    if (value < 1 || value > kMaxPort)
    {
        return false;
    }

    outPort = value;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialAttachmentView::Report
//
////////////////////////////////////////////////////////////////////////////////

void SerialAttachmentView::Report()
{
    if (m_onChange)
    {
        m_onChange (m_choice);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialAttachmentView::ShowError
//
////////////////////////////////////////////////////////////////////////////////

void SerialAttachmentView::ShowError (bool isShown)
{
    m_error.SetVisible (isShown);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialAttachmentView::OnAttachPicked
//
//  The path stays with the file attachments only. A COM port with none
//  chosen takes the first one present, which is the one selected in the
//  list.
//
////////////////////////////////////////////////////////////////////////////////

void SerialAttachmentView::OnAttachPicked (int choice)
{
    bool  isValid = choice >= 0 && choice < (int) m_devices.size();



    if (!isValid)
    {
        return;
    }

    m_choice.device = m_devices[(size_t) choice];

    if (!IsDevice (kpszSerialFileOut) && !IsDevice (kpszSerialFileIn))
    {
        m_choice.path.clear();
    }

    SyncControls();

    if (IsDevice (kpszSerialComPort) && m_choice.options.comPort.empty() && !m_comPortNames.empty())
    {
        m_choice.options.comPort = m_comPortNames.front();
        SyncControls();
    }

    ShowError (false);
    Report();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialAttachmentView::OnComPortPicked
//
////////////////////////////////////////////////////////////////////////////////

void SerialAttachmentView::OnComPortPicked (int choice)
{
    if (choice >= 0 && choice < (int) m_comPortNames.size())
    {
        m_choice.options.comPort = m_comPortNames[(size_t) choice];
        Report();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialAttachmentView::OnConnectPicked
//
////////////////////////////////////////////////////////////////////////////////

void SerialAttachmentView::OnConnectPicked (int choice)
{
    m_choice.options.listen = choice == kListenChoice;
    Report();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialAttachmentView::OnHostChanged
//
////////////////////////////////////////////////////////////////////////////////

void SerialAttachmentView::OnHostChanged (const std::wstring & text)
{
    m_choice.options.host = TextEncoding::WideToNarrow (text);
    Report();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialAttachmentView::OnPortChanged
//
//  A port that does not parse is not staged, and the error is displayed
//  until one does.
//
////////////////////////////////////////////////////////////////////////////////

void SerialAttachmentView::OnPortChanged (const std::wstring & text)
{
    int   port     = 0;
    bool  isParsed = TryParsePort (text, false, port);



    ShowError (!isParsed);

    if (isParsed)
    {
        m_choice.options.tcpPort = port;
    }

    Report();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialAttachmentView::OnListenChanged
//
//  An empty listen port means no listening.
//
////////////////////////////////////////////////////////////////////////////////

void SerialAttachmentView::OnListenChanged (const std::wstring & text)
{
    int   port     = 0;
    bool  isParsed = TryParsePort (text, true, port);



    ShowError (!isParsed);

    if (isParsed)
    {
        m_choice.options.modemListenPort = port;
    }

    Report();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialAttachmentView::OnTelnetChanged
//
////////////////////////////////////////////////////////////////////////////////

void SerialAttachmentView::OnTelnetChanged (bool isOn)
{
    m_choice.options.telnet = isOn;
    Report();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialAttachmentView::OnBrowse
//
////////////////////////////////////////////////////////////////////////////////

void SerialAttachmentView::OnBrowse()
{
    std::string  path    = m_choice.path;
    bool         isFound = m_onBrowse && m_onBrowse (IsDevice (kpszSerialFileOut), path);



    if (isFound)
    {
        m_choice.path = path;
        SyncControls();
        Report();
    }
}
