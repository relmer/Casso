# Contract: host seams

## IHostSerialPorts / IHostSerialPort (new, `CassoEmuCore/Seams/IHostSerialPort.h`)

```cpp
struct SerialPortErrors { bool framing, parity, overrun, breakSeen; };

class IHostSerialPort
{
public:
    virtual HRESULT  Configure    (const SerialLineFormat & format, uint32_t & outBaud) = 0;
    virtual HRESULT  SetControl   (bool dtr, bool rts)                                   = 0;
    virtual HRESULT  SetBreak     (bool active)                                          = 0;
    virtual HRESULT  Write        (std::span<const Byte> bytes, size_t & outWritten)     = 0;
    // Never waits. Errors since the last call; the bytes waiting, up to the span.
    virtual HRESULT  Read         (std::span<Byte> buffer, size_t & outRead, SerialPortErrors & outErrors) = 0;
    virtual HRESULT  GetLines     (bool & outCts, bool & outDsr, bool & outDcd)          = 0;
};

class IHostSerialPorts
{
public:
    virtual HRESULT  EnumeratePorts (std::vector<std::string> & outNames)                         = 0;
    // HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND) absent, (ERROR_ACCESS_DENIED) in use.
    virtual HRESULT  Open           (const std::string & name, std::unique_ptr<IHostSerialPort> & outPort) = 0;
};
```

`Configure` returns the rate the port runs at in `outBaud`. Any call failing
after `Open` means the device is gone.

Win32: `Win32HostSerialPorts` (`GetCommPorts`, `CreateFileW` on
`\\.\COMn`, `SetCommState`, `EscapeCommFunction`, `SetCommBreak`,
`ClearCommError`, `ReadFile` with `ReadIntervalTimeout = MAXDWORD`,
`GetCommModemStatus`). The unit test fake is `FakeHostSerialPorts`.

## IHostNetwork addition

```cpp
class IHostNameLookup
{
public:
    virtual ~IHostNameLookup() = default;
    // False while pending; then hr is the result and outIpv4 the address.
    virtual bool  TryGetResult (HRESULT & outHr, uint32_t & outIpv4) = 0;
};

virtual HRESULT  LookUpHost (const std::string & host, std::unique_ptr<IHostNameLookup> & outLookup) = 0;
```

## MachineBuildServices addition

`IHostSerialPorts * hostSerialPorts = nullptr;` beside `hostNetwork`. With a
null seam the attachment is built offline: it reports its problem, DCD is off
for the guest, and nothing is opened. The scratch replay machine and headless
machines are built this way.
