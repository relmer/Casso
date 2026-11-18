#include "Pch.h"

#include "CassoLink/RunningCassoRegistry.h"
#include "Core/MachineConfig.h"
#include "Devices/Acia6551.h"
#include "Devices/AciaByteSourceEndpoint.h"
#include "Devices/AciaEndpoints.h"
#include "Devices/Printer/PrinterByteRing.h"
#include "Devices/SerialPortDevice.h"
#include "Devices/SerialTrafficLog.h"
#include "Machines/Apple2/Common/PrinterCard.h"
#include "Seams/IByteStreamFactory.h"
#include "Shell/MachineBuilder.h"
#include "Shell/MachineConnectors.h"
#include "Shell/MachineRefs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConnectors::MachineConnectors
//
////////////////////////////////////////////////////////////////////////////////

MachineConnectors::MachineConnectors (MachineRefs & refs, PrinterByteRing & printerRing, const MachineConfig & config) :
    m_refs        (refs),
    m_printerRing (printerRing),
    m_config      (config)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConnectors::~MachineConnectors
//
////////////////////////////////////////////////////////////////////////////////

MachineConnectors::~MachineConnectors() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConnectors::SetSerialPort
//
////////////////////////////////////////////////////////////////////////////////

void MachineConnectors::SetSerialPort (int port, Acia6551 * acia, bool onCard)
{
    if (port >= 1 && port <= kPortCount)
    {
        m_serialPorts[(size_t) port]  = acia;
        m_serialOnCard[(size_t) port] = onCard;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConnectors::ConnectSerialPort
//
//  The port loops back before the new device is opened, so after a device
//  that fails to open, the port is a //c's with nothing plugged in.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConnectors::ConnectSerialPort (int port, const std::string & device, IByteStreamFactory * factory, SerialTrafficLog * log,
                                        const std::string & path)
{
    HRESULT           hr     = S_OK;
    bool              known  = port >= 1 && port <= kPortCount;
    Acia6551        * acia   = known ? m_serialPorts[(size_t) port] : nullptr;
    bool              onCard = known && m_serialOnCard[(size_t) port];
    bool              cardly = onCard && (device == kpszSerialLoopback || device == kpszSerialFileOut || device == kpszSerialFileIn);
    SerialPortDevice  parsed;



    CBRAEx (known, E_INVALIDARG);
    CBREx (acia != nullptr, HRESULT_FROM_WIN32 (ERROR_DEV_NOT_EXIST));

    InstallSerialEndpoint (port, onCard ? nullptr : std::make_unique<AciaLoopbackEndpoint> (acia), nullptr);

    if (m_runningRegistry != nullptr)
    {
        m_runningRegistry->Withdraw (GetCurrentProcessId(), port);
    }

    if (cardly)
    {
        hr = AttachCardEndpoint (port, device, path);
        CHR (hr);
    }
    else
    {
        hr = SerialPortDevice::Parse (device, parsed);
        CHR (hr);
    }

    BAIL_OUT_IF (cardly || parsed.kind == SerialPortDevice::Kind::Loopback, S_OK);

    if (parsed.kind == SerialPortDevice::Kind::Printer)
    {
        hr = AttachSerialPrinter (port);
        CHR (hr);
    }
    else
    {
        hr = OpenSerialStream (port, parsed, factory, log);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConnectors::AttachCardEndpoint
//
//  A card connector's loopback plug, or a file that receives what the card
//  sends, or a file whose bytes are sent into the card.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConnectors::AttachCardEndpoint (int port, const std::string & device, const std::string & path)
{
    HRESULT                                  hr      = S_OK;
    Acia6551                               * acia    = m_serialPorts[(size_t) port];
    std::unique_ptr<AciaFileEndpoint>        fileOut;
    std::unique_ptr<AciaByteSourceEndpoint>  fileIn;
    AciaByteSourceEndpoint                 * source  = nullptr;
    std::vector<Byte>                        bytes;



    if (device == kpszSerialLoopback)
    {
        InstallSerialEndpoint (port, std::make_unique<AciaLoopbackEndpoint> (acia), nullptr);
    }
    else if (device == kpszSerialFileOut)
    {
        fileOut = std::make_unique<AciaFileEndpoint>();

        hr = fileOut->Open (path);
        CHR (hr);

        InstallSerialEndpoint (port, std::move (fileOut), nullptr);
    }
    else
    {
        hr = MachineBuilder::ReadRomFileBytes (path, bytes);
        CHR (hr);

        fileIn = std::make_unique<AciaByteSourceEndpoint> (acia, std::move (bytes));
        source = fileIn.get();

        InstallSerialEndpoint (port, std::move (fileIn), nullptr);
        source->Start();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConnectors::DescribeAttachmentFailure
//
////////////////////////////////////////////////////////////////////////////////

std::string MachineConnectors::DescribeAttachmentFailure (const std::string & device, const std::string & path)
{
    std::string  text;



    if (device == kpszImageWriterDevice)
    {
        text = "The ImageWriter II is attached to another connector.";
    }
    else if (device.starts_with ("tcp:"))
    {
        text = "Cannot listen on TCP port " + device.substr (4) + " for the serial connector.";
    }
    else if (device.starts_with ("com:"))
    {
        text = "Cannot open " + device.substr (4) + " for the serial connector.";
    }
    else if (device == kpszSerialFileIn)
    {
        text = "Cannot read " + path + " for the serial connector.";
    }
    else
    {
        text = "Cannot open " + path + " for the serial connector.";
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConnectors::GetSerialPortOnTcp
//
////////////////////////////////////////////////////////////////////////////////

int MachineConnectors::GetSerialPortOnTcp (uint16_t tcpPort) const
{
    int  found = 0;



    for (int port = 1; port <= kPortCount && found == 0 && tcpPort != 0; port++)
    {
        if (m_serialTcpPorts[(size_t) port] == tcpPort)
        {
            found = port;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConnectors::IsSerialPortOnCard
//
////////////////////////////////////////////////////////////////////////////////

bool MachineConnectors::IsSerialPortOnCard (int port) const
{
    return port >= 1 && port <= kPortCount && m_serialOnCard[(size_t) port];
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConnectors::SetParallelCard
//
////////////////////////////////////////////////////////////////////////////////

void MachineConnectors::SetParallelCard (int slot, PrinterCard * card)
{
    if (slot >= 1 && slot <= kPortCount)
    {
        m_parallelCards[(size_t) slot] = card;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConnectors::ConnectParallelPrinter
//
//  Bytes in the ring when the printer comes off the card are printed; the
//  ring is the host's.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConnectors::ConnectParallelPrinter (int slot, bool attached)
{
    HRESULT        hr    = S_OK;
    bool           known = slot >= 1 && slot <= kPortCount;
    PrinterCard  * card  = known ? m_parallelCards[(size_t) slot] : nullptr;



    CBRAEx (known, E_INVALIDARG);
    CBREx (card != nullptr, HRESULT_FROM_WIN32 (ERROR_DEV_NOT_EXIST));

    if (attached)
    {
        CBREx (m_refs.printerRing == nullptr || m_printerSlot == slot, HRESULT_FROM_WIN32 (ERROR_BUSY));

        card->SetByteRing (&m_printerRing);
        m_printerSlot      = slot;
        m_refs.printerRing = &m_printerRing;
    }
    else if (m_printerSlot == slot)
    {
        card->SetByteRing (nullptr);
        m_printerSlot      = 0;
        m_refs.printerRing = nullptr;
    }
    else
    {
        card->SetByteRing (nullptr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConnectors::AttachSerialPrinter
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConnectors::AttachSerialPrinter (int port)
{
    HRESULT                               hr       = S_OK;
    bool                                  attached = m_refs.printerRing != nullptr;
    std::unique_ptr<AciaPrinterEndpoint>  printer;
    AciaPrinterEndpoint                 * raw      = nullptr;



    CBREx (!attached, HRESULT_FROM_WIN32 (ERROR_BUSY));

    printer = std::make_unique<AciaPrinterEndpoint> (m_serialPorts[(size_t) port], &m_printerRing);
    raw     = printer.get();

    InstallSerialEndpoint (port, std::move (printer), nullptr);

    m_printerEndpoint  = raw;
    m_printerPort      = port;
    m_refs.printerRing = &m_printerRing;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConnectors::OpenSerialStream
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConnectors::OpenSerialStream (int port, const SerialPortDevice & device, IByteStreamFactory * factory, SerialTrafficLog * log)
{
    HRESULT                       hr     = S_OK;
    std::unique_ptr<IByteStream>  stream;
    AciaBridgeEndpoint          * bridge = nullptr;



    CBREx (factory != nullptr, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

    if (device.kind == SerialPortDevice::Kind::Tcp)
    {
        hr = factory->ListenTcp (device.tcpPort, LinkTransport::Emulated, stream);
    }
    else
    {
        hr = factory->OpenComPort (device.comPort, ComSettings { device.baud }, stream);
    }

    CHR (hr);

    hr = AttachSerialStream (port, std::shared_ptr<IByteStream> (std::move (stream)), &bridge);
    CHR (hr);

    m_serialTcpPorts[(size_t) port] = (device.kind == SerialPortDevice::Kind::Tcp) ? device.tcpPort : (uint16_t) 0;

    //  Casso Explorer finds the port by its file; one that cannot be written
    //  does not stop the port; the port is only unlisted.
    if (m_runningRegistry != nullptr && device.kind == SerialPortDevice::Kind::Tcp)
    {
        HRESULT  published = m_runningRegistry->Publish (GetCurrentProcessId(), port, m_config.name, device.tcpPort, m_serialOnCard[(size_t) port]);

        IGNORE_RETURN_VALUE (published, S_OK);
    }

    if (log != nullptr)
    {
        bridge->SetByteLog ([log, port] (bool fromGuest, Byte value, uint64_t cycle)
        {
            log->Append (port, fromGuest, value, cycle);
        });
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConnectors::AttachSerialStream
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConnectors::AttachSerialStream (int port, std::shared_ptr<IByteStream> stream, AciaBridgeEndpoint ** outBridge)
{
    HRESULT                              hr     = S_OK;
    Acia6551                           * acia   = nullptr;
    std::unique_ptr<AciaBridgeEndpoint>  bridge;
    AciaBridgeEndpoint                 * raw    = nullptr;



    CBRAEx (port >= 1 && port <= kPortCount, E_INVALIDARG);
    CBRAEx (stream != nullptr, E_INVALIDARG);

    acia = m_serialPorts[(size_t) port];
    CBREx (acia != nullptr, HRESULT_FROM_WIN32 (ERROR_DEV_NOT_EXIST));

    bridge = std::make_unique<AciaBridgeEndpoint> (acia, std::move (stream));
    raw    = bridge.get();

    InstallSerialEndpoint (port, std::move (bridge), raw);

    if (outBridge != nullptr)
    {
        *outBridge = raw;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConnectors::InstallSerialEndpoint
//
////////////////////////////////////////////////////////////////////////////////

void MachineConnectors::InstallSerialEndpoint (int port, std::unique_ptr<IAciaEndpoint> endpoint, AciaBridgeEndpoint * bridge)
{
    AciaBridgeEndpoint  * old = m_serialBridges[(size_t) port];



    if (old != nullptr && old->GetStream() != nullptr)
    {
        old->GetStream()->Close();
    }

    // The printer the port had is unplugged with it. Its ring is the host's,
    // so bytes not yet drained are printed.
    if (m_printerPort == port)
    {
        m_printerPort      = 0;
        m_printerEndpoint  = nullptr;
        m_refs.printerRing = nullptr;
    }

    m_serialPorts[(size_t) port]->SetEndpoint (endpoint.get());

    m_serialBridges[(size_t) port]   = bridge;
    m_serialEndpoints[(size_t) port] = std::move (endpoint);
    m_serialTcpPorts[(size_t) port]  = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConnectors::DetachSerialPorts
//
//  Before the devices go, so no bridge is ticked against a port that no
//  longer exists. Each stream is closed, which releases its TCP or COM port.
//
////////////////////////////////////////////////////////////////////////////////

void MachineConnectors::DetachSerialPorts()
{
    for (int port = 1; port <= kPortCount; port++)
    {
        AciaBridgeEndpoint  * bridge = m_serialBridges[(size_t) port];
        Acia6551            * acia   = m_serialPorts[(size_t) port];

        if (bridge != nullptr && bridge->GetStream() != nullptr)
        {
            bridge->GetStream()->Close();
        }

        if (acia != nullptr)
        {
            acia->SetEndpoint (nullptr);
        }

    }

    WithdrawSerialPorts();

    m_serialPorts     = {};
    m_serialBridges   = {};
    m_serialEndpoints = {};
    m_serialOnCard    = {};
    m_printerPort     = 0;
    m_printerEndpoint = nullptr;
    m_parallelCards   = {};
    m_printerSlot     = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConnectors::WithdrawSerialPorts
//
////////////////////////////////////////////////////////////////////////////////

void MachineConnectors::WithdrawSerialPorts()
{
    for (int port = 1; port <= kPortCount && m_runningRegistry != nullptr; port++)
    {
        m_runningRegistry->Withdraw (GetCurrentProcessId(), port);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConnectors::Tick
//
//  The bridges and the ImageWriter II's endpoint, after every instruction.
//
////////////////////////////////////////////////////////////////////////////////

void MachineConnectors::Tick (Byte cycles)
{
    for (AciaBridgeEndpoint * bridge : m_serialBridges)
    {
        if (bridge != nullptr)
        {
            bridge->Tick (cycles);
        }
    }

    if (m_printerEndpoint != nullptr)
    {
        m_printerEndpoint->Tick (cycles);
    }
}
