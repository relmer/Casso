#pragma once

#include "Pch.h"

class Acia6551;
class AciaBridgeEndpoint;
class AciaPrinterEndpoint;
class IAciaEndpoint;
class IByteStream;
class IByteStreamFactory;
class PrinterByteRing;
class PrinterCard;
class RunningCassoRegistry;
class SerialTrafficLog;
struct MachineConfig;
struct MachineRefs;
struct SerialPortDevice;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConnectors
//
//  The machine's serial connectors and parallel cards, and what is plugged
//  into each: a loopback plug, a file, a TCP or COM stream, or the
//  ImageWriter II, which is on one connector at a time.
//
////////////////////////////////////////////////////////////////////////////////

class MachineConnectors
{
public:
    static constexpr int  kPortCount = 7;

    using ParallelCards = std::array<PrinterCard *, kPortCount + 1>;

    MachineConnectors (MachineRefs & refs, PrinterByteRing & printerRing, const MachineConfig & config);
    ~MachineConnectors ();

    MachineConnectors (const MachineConnectors &)             = delete;
    MachineConnectors & operator= (const MachineConnectors &) = delete;

    //  The serial ports, by number: a //c's built-in ports 1 and 2, or a
    //  Super Serial Card's slot. A stream attached to one replaces the port's
    //  endpoint with a bridge that runs at the line's speed, ticked after
    //  every instruction.
    //
    //  ConnectSerialPort plugs a port's device in while the machine runs, as
    //  a cable is: the old device's stream is closed first, so a TCP port can
    //  be opened again at once. A //c's port loops back when the new device
    //  is empty or cannot be opened; a card's connector is then empty. A
    //  card's connector also takes a loopback plug and the two file
    //  attachments, whose file is `path`.
    //  The ImageWriter II is attached to one connector at a time, so it
    //  fails with ERROR_BUSY on a port while another connector has it.
    //  Called on the CPU thread.
    void     SetSerialPort      (int port, Acia6551 * acia, bool onCard = false);
    HRESULT  ConnectSerialPort  (int port, const std::string & device, IByteStreamFactory * factory, SerialTrafficLog * log,
                                 const std::string & path = std::string());

    //  The text a card reports for a connector ConnectSerialPort could not
    //  attach `device` to.
    static std::string  DescribeAttachmentFailure (const std::string & device, const std::string & path);

    //  The serial port whose connector listens on `tcpPort`, 0 for none, and
    //  whether a port is a Super Serial Card's. Called on the CPU thread.
    int   GetSerialPortOnTcp (uint16_t tcpPort) const;
    bool  IsSerialPortOnCard (int port) const;

    //  The parallel card in each slot, and the ImageWriter II attached to the
    //  card in `slot` or taken off it while the machine runs. Attaching fails
    //  with ERROR_BUSY while another connector has the printer. Called on the
    //  CPU thread.
    void     SetParallelCard        (int slot, PrinterCard * card);
    HRESULT  ConnectParallelPrinter (int slot, bool attached);
    HRESULT  AttachSerialStream     (int port, std::shared_ptr<IByteStream> stream, AciaBridgeEndpoint ** outBridge = nullptr);
    void     DetachSerialPorts      ();

    //  Where a file for each port connected to TCP is written for Casso Explorer:
    //  written when the port connects, deleted when it is connected
    //  elsewhere or detached. With null, no file is written.
    void  SetRunningRegistry  (RunningCassoRegistry * registry) { m_runningRegistry = registry; }
    void  WithdrawSerialPorts ();

    //  The bridges and the printer endpoint, after every instruction.
    void  Tick (Byte cycles);

    const ParallelCards  & GetParallelCards () const noexcept { return m_parallelCards; }

private:
    //  Puts `endpoint` on the port in place of whatever was there, closing
    //  the old bridge's stream. `bridge` is the endpoint when it is one.
    void  InstallSerialEndpoint (int port, std::unique_ptr<IAciaEndpoint> endpoint, AciaBridgeEndpoint * bridge);

    //  The two kinds of device ConnectSerialPort opens: the ImageWriter II,
    //  and a stream to a TCP or COM port.
    HRESULT  AttachSerialPrinter (int port);
    HRESULT  AttachCardEndpoint  (int port, const std::string & device, const std::string & path);
    HRESULT  OpenSerialStream    (int port, const SerialPortDevice & device, IByteStreamFactory * factory, SerialTrafficLog * log);

    MachineRefs           & m_refs;
    PrinterByteRing       & m_printerRing;
    const MachineConfig   & m_config;

    std::array<Acia6551 *, kPortCount + 1>                        m_serialPorts     = {};
    std::array<std::unique_ptr<IAciaEndpoint>, kPortCount + 1>   m_serialEndpoints;
    std::array<AciaBridgeEndpoint *, kPortCount + 1>              m_serialBridges   = {};
    std::array<bool, kPortCount + 1>                              m_serialOnCard    = {};
    std::array<uint16_t, kPortCount + 1>                          m_serialTcpPorts  = {};   // each connector's TCP port, 0 for none

    //  The serial port the ImageWriter II is attached to, or 0, and its
    //  endpoint, ticked with the bridges.
    int                                  m_printerPort     = 0;
    AciaPrinterEndpoint                * m_printerEndpoint = nullptr;

    //  The parallel cards by slot, and the slot whose card has the printer,
    //  or 0.
    ParallelCards                        m_parallelCards   = {};
    int                                  m_printerSlot     = 0;

    RunningCassoRegistry  * m_runningRegistry = nullptr;
};
