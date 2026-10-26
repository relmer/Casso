# Contract: Host Network Seam

`CassoEmuCore/Seams/IHostNetwork.h`. Every call is non-blocking and returns an `HRESULT`; a would-block outcome is reported through an out-parameter, never as a failure.

```cpp
struct HostEndpoint
{
    uint32_t  ipv4 = 0;     // host byte order
    uint16_t  port = 0;
};

enum class HostSocketKind { Tcp, Udp };

struct HostSocketReadiness
{
    bool  readable    = false;   // data, a pending accept, or EOF
    bool  writable    = false;   // connect finished or send space
    bool  failed      = false;   // connect failed or the socket errored
};

class IHostSocket
{
public:
    virtual          ~IHostSocket() = default;

    virtual HRESULT  Bind        (const HostEndpoint & local, bool allowBroadcast) = 0;
    virtual HRESULT  GetLocal    (HostEndpoint & outLocal) const = 0;
    virtual HRESULT  Listen      () = 0;
    virtual HRESULT  Accept      (std::unique_ptr<IHostSocket> & outAccepted, HostEndpoint & outPeer, bool & outWouldBlock) = 0;
    virtual HRESULT  Connect     (const HostEndpoint & remote) = 0;          // starts; completion via Poll
    virtual HRESULT  Send        (std::span<const Byte> bytes, size_t & outSent) = 0;
    virtual HRESULT  Receive     (std::span<Byte> buffer, size_t & outReceived, bool & outEof) = 0;
    virtual HRESULT  SendTo      (const HostEndpoint & remote, std::span<const Byte> bytes) = 0;
    virtual HRESULT  ReceiveFrom (std::span<Byte> buffer, size_t & outReceived, HostEndpoint & outFrom) = 0;
    virtual HRESULT  ShutdownSend() = 0;
    virtual HRESULT  Poll        (HostSocketReadiness & outReadiness) = 0;
};

class IHostNetwork
{
public:
    virtual          ~IHostNetwork() = default;

    virtual HRESULT  CreateSocket (HostSocketKind kind, std::unique_ptr<IHostSocket> & outSocket) = 0;
    virtual HRESULT  GetDnsServer (uint32_t & outIpv4) = 0;     // E_NOT_SET when none
};
```

Rules:

- `Receive` and `ReceiveFrom` with nothing pending return `S_OK` with zero bytes; `outEof` distinguishes an orderly close.
- `Send` may send fewer bytes than given; `outSent` reports how many.
- A bind conflict returns `HRESULT_FROM_WIN32 (WSAEADDRINUSE)`.
- Destruction closes the socket.
- `Win32HostNetwork` initializes Winsock once in its constructor (`WSAStartup` 2.2) and cleans up in its destructor; sockets are created non-blocking (`FIONBIO`).

## FakeHostNetwork (UnitTest)

A test acts as the far side of each socket the code under test creates: complete or fail a connect, offer an inbound connection on a bound port, push received bytes or datagrams, set EOF, read what the code under test sent, and limit how many bytes a `Send` accepts. It makes no Winsock calls.
