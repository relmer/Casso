#include "Pch.h"

#include "Ui/Settings/SettingsConnectors.h"
#include "Ui/Settings/SettingsPanelState.h"
#include "Ui/Settings/SettingsJson.h"
#include "Config/MonitorCatalog.h"
#include "Controllers/ControllerTokens.h"
#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"
#include "Machines/MachineDefinitions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsConnectors::SettingsConnectors
//
////////////////////////////////////////////////////////////////////////////////

SettingsConnectors::SettingsConnectors (SettingsPanelState & owner)
    : m_owner (owner)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  AreSerialPortsChanged
//
//  A serial port's device is a staged edit like any other: a change makes
//  the state dirty, and Apply connects the new device.
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsConnectors::AreSerialPortsChanged() const
{
    bool  changed = false;



    for (int port = 1; !changed && port <= (int) std::size (SettingsPanelState::kpszSerialPortNames); ++port)
    {
        changed = GetSerialPortDevice (port) != SettingsJson::GetPortDevice (m_owner.m_original.machinePorts, SettingsPanelState::kpszSerialPortNames[port - 1]);
    }

    return changed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasSerialPorts
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsConnectors::HasSerialPorts() const
{
    for (const SettingsMachinePort & entry : m_owner.m_current.machinePorts)
    {
        if (entry.name == SettingsPanelState::kpszSerialPortNames[0])
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSerialPortDevice
//
////////////////////////////////////////////////////////////////////////////////

std::string SettingsConnectors::GetSerialPortDevice (int port) const
{
    bool  valid = port >= 1 && port <= (int) std::size (SettingsPanelState::kpszSerialPortNames);



    return valid ? SettingsJson::GetPortDevice (m_owner.m_current.machinePorts, SettingsPanelState::kpszSerialPortNames[port - 1]) : std::string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSerialPortDevice
//
//  Only a machine with that back-panel port has one to set; the port list is
//  never extended, as the disk port's is not.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsConnectors::SetSerialPortDevice (int port, const std::string & device)
{
    if (device == kpszImageWriterDevice)
    {
        SetPrinterConnection (PrinterConnection { PrinterConnection::Kind::SerialPort, port });
    }
    else
    {
        StageSerialPortDevice (port, device);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StageSerialPortDevice
//
////////////////////////////////////////////////////////////////////////////////

void SettingsConnectors::StageSerialPortDevice (int port, const std::string & device)
{
    if (port < 1 || port > (int) std::size (SettingsPanelState::kpszSerialPortNames))
    {
        return;
    }

    for (SettingsMachinePort & entry : m_owner.m_current.machinePorts)
    {
        if (entry.name == SettingsPanelState::kpszSerialPortNames[port - 1])
        {
            entry.device = device;
            break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPrinterChoices
//
////////////////////////////////////////////////////////////////////////////////

std::vector<PrinterConnection> SettingsConnectors::GetPrinterChoices() const
{
    std::vector<PrinterConnection>  choices = { PrinterConnection() };



    for (const HardwareEntry & entry : m_owner.m_current.hardware)
    {
        if (entry.kind == HardwareEntryKind::Slot && entry.enabled && entry.type == kpszParallelPrinterDevice)
        {
            choices.push_back (PrinterConnection { PrinterConnection::Kind::ParallelCard, entry.slot });
        }
        else if (entry.kind == HardwareEntryKind::Slot && entry.enabled && entry.type == kpszSuperSerialCardDevice)
        {
            choices.push_back (PrinterConnection { PrinterConnection::Kind::SerialCard, entry.slot });
        }
    }

    for (int port = 1; HasSerialPorts() && port <= (int) std::size (SettingsPanelState::kpszSerialPortNames); port++)
    {
        choices.push_back (PrinterConnection { PrinterConnection::Kind::SerialPort, port });
    }

    return choices;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPrinterConnection
//
////////////////////////////////////////////////////////////////////////////////

PrinterConnection SettingsConnectors::GetPrinterConnection() const
{
    return FindPrinterConnection (m_owner.m_current.hardware, m_owner.m_current.machinePorts);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetPrinterConnection
//
//  Every connector is set, chosen or not, so a turned-off parallel card that
//  lists nothing does not take the printer back when it is turned on. A card
//  whose connector was read listing nothing lists nothing again where that
//  has the same meaning, so moving the printer back is no change.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsConnectors::SetPrinterConnection (const PrinterConnection & connection)
{
    PrinterConnection  previous = FindPrinterConnection (m_owner.m_current.hardware, m_owner.m_current.machinePorts);



    for (size_t i = 0; i < m_owner.m_current.hardware.size(); i++)
    {
        HardwareEntry &  entry    = m_owner.m_current.hardware[i];
        bool             readBare = i < m_owner.m_original.hardware.size() && m_owner.m_original.hardware[i].ports.empty();
        bool             parallel = entry.kind == HardwareEntryKind::Slot && entry.type == kpszParallelPrinterDevice;
        bool             serial   = entry.kind == HardwareEntryKind::Slot && entry.type == kpszSuperSerialCardDevice;
        bool             has      = !entry.ports.empty() && entry.ports.front() == kpszImageWriterDevice;
        bool             chosen   = entry.slot == connection.number
                                 && ((parallel && connection.kind == PrinterConnection::Kind::ParallelCard)
                                  || (serial   && connection.kind == PrinterConnection::Kind::SerialCard));

        if (parallel && chosen)
        {
            entry.ports = readBare ? std::vector<std::string>() : std::vector<std::string> { kpszImageWriterDevice };
        }
        else if (parallel && (has || entry.ports.empty()))
        {
            entry.ports = { std::string() };
        }
        else if (serial && chosen)
        {
            //  The connector the printer displaces, to put back when it goes.
            if (!has)
            {
                entry.portBeforePrinter     = entry.ports.empty() ? std::string() : entry.ports.front();
                entry.portPathBeforePrinter = entry.portPath;
            }

            entry.ports    = { kpszImageWriterDevice };
            entry.portPath.clear();
        }
        else if (serial && has)
        {
            if (!entry.portBeforePrinter.empty())
            {
                entry.ports    = { entry.portBeforePrinter };
                entry.portPath = entry.portPathBeforePrinter;
            }
            else
            {
                entry.ports = readBare ? std::vector<std::string>() : std::vector<std::string> { std::string() };
            }

            entry.portBeforePrinter.clear();
            entry.portPathBeforePrinter.clear();
        }
    }

    for (int port = 1; port <= (int) std::size (SettingsPanelState::kpszSerialPortNames); port++)
    {
        bool  chosen = connection.kind == PrinterConnection::Kind::SerialPort && connection.number == port;

        if (chosen)
        {
            StageSerialPortDevice (port, kpszImageWriterDevice);
        }
        else if (GetSerialPortDevice (port) == kpszImageWriterDevice)
        {
            StageSerialPortDevice (port, std::string());
        }
    }

    StageCardPrinterChange (previous, connection);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TakeNotice
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SettingsConnectors::TakeNotice()
{
    std::wstring  notice = std::move (m_notice);



    m_notice.clear();

    return notice;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadSwitches
//
////////////////////////////////////////////////////////////////////////////////

SscSwitches SettingsConnectors::ReadSwitches (const HardwareEntry & entry)
{
    SscSwitches  switches = SscSwitches::MakeDefault();
    bool         parsed   = false;



    parsed = switches.TryParseBankText (SscBank::Sw1, entry.switchesSw1);
    IGNORE_RETURN_VALUE (parsed, false);
    parsed = switches.TryParseBankText (SscBank::Sw2, entry.switchesSw2);
    IGNORE_RETURN_VALUE (parsed, false);

    return switches;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteSwitches
//
////////////////////////////////////////////////////////////////////////////////

void SettingsConnectors::WriteSwitches (HardwareEntry & entry, const SscSwitches & switches)
{
    entry.switchesSw1 = switches.GetBankText (SscBank::Sw1);
    entry.switchesSw2 = switches.GetBankText (SscBank::Sw2);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetModeName
//
//  As a notice's sentence has it, after "switched from" or "switched back to".
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * SettingsConnectors::GetModeName (SscMode mode)
{
    const wchar_t  * name = L"communications mode";



    switch (mode)
    {
        case SscMode::Printer:  name = L"printer mode";                       break;
        case SscMode::SicP8:    name = L"its Serial Interface Card P8 mode";  break;
        case SscMode::SicP8A:   name = L"its Serial Interface Card P8A mode"; break;
        default:                                                              break;
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindSlotEntry
//
////////////////////////////////////////////////////////////////////////////////

HardwareEntry * SettingsConnectors::FindSlotEntry (int slot, const char * type)
{
    HardwareEntry  * found = nullptr;



    for (HardwareEntry & entry : m_owner.m_current.hardware)
    {
        if (found == nullptr && entry.kind == HardwareEntryKind::Slot && entry.slot == slot && entry.type == type)
        {
            found = &entry;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetCardPrinterMode
//
//  The card's SW1 bank is saved first, so the mode can be restored.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsConnectors::SetCardPrinterMode (int slot)
{
    HardwareEntry  * entry    = FindSlotEntry (slot, kpszSuperSerialCardDevice);
    SscSwitches      switches = (entry != nullptr) ? ReadSwitches (*entry) : SscSwitches::MakeDefault();



    if (entry != nullptr && switches.GetMode() != SscMode::Printer)
    {
        const wchar_t  * before = GetModeName (switches.GetMode());

        entry->switchesSw1BeforePrinter = switches.GetBankText (SscBank::Sw1);

        switches.SetOn (SscBank::Sw1, 5, false);
        switches.SetOn (SscBank::Sw1, 6, true);
        WriteSwitches (*entry, switches);

        m_notice = std::format (L"The ImageWriter II requires printer mode, so the Super Serial Card in slot {} was switched from {}. "
                                L"If you connect the printer somewhere else, the card will be switched back to {}.", slot, before, before);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RestoreCardMode
//
//  Only a card in printer mode gets its saved mode back. A mode the user
//  chose since then is left as it is. Switches restored to what was
//  read are staged as they were read, so the round trip is no change.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsConnectors::RestoreCardMode (int slot)
{
    HardwareEntry  * entry    = FindSlotEntry (slot, kpszSuperSerialCardDevice);
    bool             saved    = entry != nullptr && !entry->switchesSw1BeforePrinter.empty();
    SscSwitches      switches = saved ? ReadSwitches (*entry) : SscSwitches::MakeDefault();
    SscSwitches      before   = SscSwitches::MakeDefault();
    bool             parsed   = saved && before.TryParseBankText (SscBank::Sw1, entry->switchesSw1BeforePrinter);



    if (parsed && switches.GetMode() == SscMode::Printer)
    {
        switches.SetOn (SscBank::Sw1, 5, before.IsOn (SscBank::Sw1, 5));
        switches.SetOn (SscBank::Sw1, 6, before.IsOn (SscBank::Sw1, 6));
        WriteSwitches (*entry, switches);

        for (const HardwareEntry & read : m_owner.m_original.hardware)
        {
            if (read.kind == entry->kind && read.slot == slot && read.type == entry->type && ReadSwitches (read) == switches)
            {
                entry->switchesSw1 = read.switchesSw1;
                entry->switchesSw2 = read.switchesSw2;
            }
        }

        m_notice = std::format (L"The Super Serial Card in slot {} was switched back to {}, the mode it had before the "
                                L"ImageWriter II was connected to it.", slot, GetModeName (switches.GetMode()));
    }

    if (saved)
    {
        entry->switchesSw1BeforePrinter.clear();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StageCardPrinterChange
//
////////////////////////////////////////////////////////////////////////////////

void SettingsConnectors::StageCardPrinterChange (const PrinterConnection & from, const PrinterConnection & to)
{
    bool  moved = from != to;



    if (moved && from.kind == PrinterConnection::Kind::SerialCard)
    {
        RestoreCardMode (from.number);
    }

    if (moved && to.kind == PrinterConnection::Kind::SerialCard)
    {
        SetCardPrinterMode (to.number);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindPrinterConnection
//
////////////////////////////////////////////////////////////////////////////////

PrinterConnection SettingsConnectors::FindPrinterConnection (const std::vector<HardwareEntry> & hardware, const std::vector<SettingsMachinePort> & machinePorts)
{
    PrinterConnection  found;



    for (const HardwareEntry & entry : hardware)
    {
        bool  slot     = entry.kind == HardwareEntryKind::Slot && entry.enabled;
        bool  has      = !entry.ports.empty() && entry.ports.front() == kpszImageWriterDevice;
        bool  parallel = slot && entry.type == kpszParallelPrinterDevice && (has || entry.ports.empty());
        bool  serial   = slot && entry.type == kpszSuperSerialCardDevice && has;

        if (found.kind == PrinterConnection::Kind::None && (parallel || serial))
        {
            found.kind   = parallel ? PrinterConnection::Kind::ParallelCard : PrinterConnection::Kind::SerialCard;
            found.number = entry.slot;
        }
    }

    for (int port = 1; found.kind == PrinterConnection::Kind::None && port <= (int) std::size (SettingsPanelState::kpszSerialPortNames); port++)
    {
        if (SettingsJson::GetPortDevice (machinePorts, SettingsPanelState::kpszSerialPortNames[port - 1]) == kpszImageWriterDevice)
        {
            found.kind   = PrinterConnection::Kind::SerialPort;
            found.number = port;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindSerialCard
//
////////////////////////////////////////////////////////////////////////////////

int SettingsConnectors::FindSerialCard() const
{
    int  index = -1;



    for (size_t i = 0; index < 0 && i < m_owner.m_current.hardware.size(); ++i)
    {
        if (m_owner.m_current.hardware[i].kind == HardwareEntryKind::Slot &&
            m_owner.m_current.hardware[i].type == kpszSuperSerialCardDevice)
        {
            index = static_cast<int> (i);
        }
    }

    return index;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasEnabledSerialCard
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsConnectors::HasEnabledSerialCard() const
{
    int  index = FindSerialCard();



    return index >= 0 && m_owner.m_current.hardware[static_cast<size_t> (index)].enabled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSerialCardSwitches
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsConnectors::SetSerialCardSwitches (const std::string & sw1, const std::string & sw2)
{
    HRESULT            hr         = S_OK;
    int                index      = FindSerialCard();
    bool               hasCard    = index >= 0;
    PrinterConnection  connection;
    int                slot       = 0;
    bool               leaves     = false;



    CBR (hasCard);

    m_owner.m_current.hardware[static_cast<size_t> (index)].switchesSw1 = sw1;
    m_owner.m_current.hardware[static_cast<size_t> (index)].switchesSw2 = sw2;

    connection = FindPrinterConnection (m_owner.m_current.hardware, m_owner.m_current.machinePorts);
    slot       = m_owner.m_current.hardware[static_cast<size_t> (index)].slot;
    leaves     = connection.kind == PrinterConnection::Kind::SerialCard
              && connection.number == slot
              && ReadSwitches (m_owner.m_current.hardware[static_cast<size_t> (index)]).GetMode() != SscMode::Printer;

    if (leaves)
    {
        m_owner.m_current.hardware[static_cast<size_t> (index)].switchesSw1BeforePrinter.clear();
        SetPrinterConnection (PrinterConnection());

        m_notice = L"The ImageWriter II has been disconnected from the Super Serial Card because it requires printer mode. "
                   L"You can reconnect it on the Printing tab.";
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSerialCardAttachment
//
//  The card has one connector, so the staged port list is that one entry;
//  an empty device stages an empty connector.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsConnectors::SetSerialCardAttachment (const std::string & device, const std::string & path)
{
    HRESULT          hr      = S_OK;
    int              index   = FindSerialCard();
    bool             hasCard = index >= 0;
    HardwareEntry  * entry   = nullptr;



    CBR (hasCard);

    entry = &m_owner.m_current.hardware[static_cast<size_t> (index)];

    if (device == kpszImageWriterDevice)
    {
        SetPrinterConnection (PrinterConnection { PrinterConnection::Kind::SerialCard, entry->slot });
    }
    else
    {
        entry->ports    = { device };
        entry->portPath = path;
        entry->portBeforePrinter.clear();
        entry->portPathBeforePrinter.clear();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyConnectors
//
//  The //c's serial ports, each Super Serial Card's connector and each
//  parallel card's printer. The connectors whose new device is not the
//  ImageWriter II go first, and the one whose new device is the printer
//  last, so the printer is free when it is attached.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsConnectors::ApplyConnectors (ISettingsApplySink & sink) const
{
    PrinterConnection  before = FindPrinterConnection (m_owner.m_original.hardware, m_owner.m_original.machinePorts);
    PrinterConnection  after  = FindPrinterConnection (m_owner.m_current.hardware, m_owner.m_current.machinePorts);



    for (int pass = 0; pass < 2; ++pass)
    {
        bool  printerPass = pass == 1;

        for (int port = 1; port <= (int) std::size (SettingsPanelState::kpszSerialPortNames); ++port)
        {
            std::string  device  = GetSerialPortDevice (port);
            bool         changed = device != SettingsJson::GetPortDevice (m_owner.m_original.machinePorts, SettingsPanelState::kpszSerialPortNames[port - 1]);

            if (changed && (device == kpszImageWriterDevice) == printerPass)
            {
                sink.ApplySerialPortDevice (port, device, std::string());
            }
        }

        for (size_t i = 0; i < m_owner.m_current.hardware.size() && i < m_owner.m_original.hardware.size(); ++i)
        {
            const HardwareEntry &  now      = m_owner.m_current.hardware[i];
            const HardwareEntry &  was      = m_owner.m_original.hardware[i];
            bool                   slot     = now.kind == HardwareEntryKind::Slot && now.enabled;
            bool                   serial   = slot && now.type == kpszSuperSerialCardDevice;
            bool                   parallel = slot && now.type == kpszParallelPrinterDevice;
            std::string            device   = now.ports.empty() ? std::string() : now.ports.front();
            bool                   had      = before.kind == PrinterConnection::Kind::ParallelCard && before.number == now.slot;
            bool                   has      = after.kind  == PrinterConnection::Kind::ParallelCard && after.number  == now.slot;

            if (serial && (now.ports != was.ports || now.portPath != was.portPath) && (device == kpszImageWriterDevice) == printerPass)
            {
                sink.ApplySerialPortDevice (now.slot, device, now.portPath);
            }

            if (parallel && had != has && has == printerPass)
            {
                sink.ApplyParallelPrinter (now.slot, has);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindNetworkCard
//
////////////////////////////////////////////////////////////////////////////////

int SettingsConnectors::FindNetworkCard() const
{
    size_t  i = 0;



    for (i = 0; i < m_owner.m_current.hardware.size(); ++i)
    {
        if (m_owner.m_current.hardware[i].kind == HardwareEntryKind::Slot && m_owner.m_current.hardware[i].type == SettingsPanelState::kpszNetworkCardDevice)
        {
            return (int) i;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetNetworkCard
//
////////////////////////////////////////////////////////////////////////////////

const HardwareEntry * SettingsConnectors::GetNetworkCard() const
{
    int  index = FindNetworkCard();



    return (index < 0) ? nullptr : &m_owner.m_current.hardware[(size_t) index];
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasEnabledNetworkCard
//
//  The Network tab is present only while this is true, as the Storage tab
//  follows the disk controller.
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsConnectors::HasEnabledNetworkCard() const
{
    const HardwareEntry *  card = GetNetworkCard();



    return card != nullptr && card->enabled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetNetworkCardMac
//
//  On bad text, `outError` gets the error in the fixed two-part format and
//  the stored address stays in effect.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsConnectors::SetNetworkCardMac (
    const std::string  & text,
    std::string        & outError)
{
    HRESULT     hr     = S_OK;
    int         index  = FindNetworkCard();
    MacAddress  parsed;
    bool        valid  = false;



    outError.clear();

    CBR (index >= 0);

    valid = MacAddress::TryParse (text, parsed) && parsed.IsUnicast();

    CBRFEx (valid,
            HRESULT_FROM_WIN32 (ERROR_INVALID_DATA),
            outError = "Error: invalid MAC address\n"
                       "A MAC address is six pairs of hexadecimal digits separated by colons or "
                       "hyphens, such as 02:CA:55:12:34:56, and cannot be a multicast address.");

    m_owner.m_current.hardware[(size_t) index].network.mac = parsed;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetNetworkLanAccess
//
////////////////////////////////////////////////////////////////////////////////

void SettingsConnectors::SetNetworkLanAccess (bool lanAccess)
{
    int  index = FindNetworkCard();



    if (index >= 0)
    {
        m_owner.m_current.hardware[(size_t) index].network.lanAccess = lanAccess;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddPortForward
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsConnectors::AddPortForward (
    const PortForward  & forward,
    std::string        & outError)
{
    HRESULT        hr      = S_OK;
    int            index   = FindNetworkCard();
    std::string    problem;
    bool           valid   = false;



    outError.clear();

    CBR (index >= 0);

    problem = NetworkCardSettings::ValidatePortForward (forward, m_owner.m_current.hardware[(size_t) index].network.portForwards);
    valid   = problem.empty();

    CBRFEx (valid,
            HRESULT_FROM_WIN32 (ERROR_INVALID_DATA),
            outError = "Error: invalid port forward\n" + problem);

    m_owner.m_current.hardware[(size_t) index].network.portForwards.push_back (forward);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RemovePortForward
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsConnectors::RemovePortForward (size_t forwardIndex)
{
    HRESULT                     hr       = S_OK;
    int                         index    = FindNetworkCard();
    std::vector<PortForward>  * forwards = nullptr;
    size_t                      count    = 0;



    CBR (index >= 0);

    forwards = &m_owner.m_current.hardware[(size_t) index].network.portForwards;
    count    = forwards->size();
    CBR (forwardIndex < count);

    forwards->erase (forwards->begin() + (ptrdiff_t) forwardIndex);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeRandomMac
//
////////////////////////////////////////////////////////////////////////////////

MacAddress SettingsConnectors::MakeRandomMac()
{
    std::random_device   source;
    MacAddress::Octets   octets = {};



    for (Byte & octet : octets)
    {
        octet = (Byte) (source() & 0xFF);
    }

    return MacAddress::MakeRandomLocal (octets);
}
