# Contract: 047's attachments on 033's serial layer (T046)

Owner decision, 2026-10-28: 033-casso-explorer's serial layer is the base, and
047's attachments are rebuilt on it. This is the design for that work, agreed
before the code arrives.

## When

1. 033-casso-explorer merges to master.
2. 035-debugger merges master and resolves the 033-against-035 conflicts there,
   then the 035 session sends the commit.
3. 047 merges `origin/035-debugger` at that commit. No direct merge of 033 into
   047: a trial on 2026-10-28 hit 156 conflicted files, nearly all 033 against
   035.

## What each branch has

| Capability | 033 | 047 | Result |
|---|---|---|---|
| Device string in `PortConfig.device` | `SerialPortDevice` | `serial-*` device strings plus option keys | 033's, extended (below) |
| Attach or change with no reset | `MachineHost::ConnectSerialPort`, `SettingsConnectors` | rebuild | 033's |
| ImageWriter on a serial port | `AciaPrinterEndpoint`, card printer mode | none | 033's |
| TCP listen | `IByteStream` read on a separate thread, `AciaBridgeEndpoint` | `TcpSerialLink`, polled on the emulation thread | 047's endpoint, 033's registry publish |
| TCP connect out, Telnet, retry | none | `AciaTcpEndpoint`, `TelnetCodec` | 047's |
| Modem | none | `HayesModem`, `AciaModemEndpoint` | 047's |
| COM port | fixed rate from the device string | follows the 6551's format, DTR, RTS, break and errors | 047's endpoint, both rate modes (below) |
| History | none: the bridge feeds the 6551 directly | `AciaByte`, `AciaLines`, `AciaConnection` journaled; `HostInputGate` | 047's |
| Problem reports | none | `SerialLinkReports` to the infobar | 047's |
| Traffic text capture | `SerialTrafficLog`, `--serial-log` | none | 033's, fed by 047's endpoints |
| Settings | Machine tab port drop-downs, Serial card page, Printing page connector | `SerialAttachmentView`, Serial ports page, Serial card page | one control set (below) |

## Device strings

Parsed and written by `SerialPortDevice`. `Format` writes only what differs
from the default, as 033 does now.

| String | Meaning |
|---|---|
| `""` | Nothing attached (owner decision, T042). 033 reads it as the loopback plug, so `ConnectSerialPort` changes to install no endpoint. |
| `serial-loopback` | The loopback plug, on any port. |
| `serial-file-out`, `serial-file-in` with `path` | As in both branches; now on the //c's ports as well. |
| `imagewriter` | 033's ImageWriter II. |
| `tcp:6502` | Listen on TCP port 6502, as in 033. |
| `tcp:host:port` | Connect to `host` on `port`; retry every 5 seconds. |
| `?telnet` suffix on either TCP form | Telnet on. Off by default for TCP. |
| `modem`, `modem:6400` | The Hayes modem; optional listen port for incoming calls. |
| `?telnet=0` suffix on a modem | Telnet off. On by default for the modem. |
| `com:COM3` | A PC COM port. Rate: see question 1. |
| `com:COM3@9600` | A PC COM port fixed at 9600 baud. |

The loader converts 047's `serial-tcp`, `serial-modem` and `serial-com-port`
devices and the `host`, `port`, `listen`, `telnet`, `listenPort` and `comPort`
keys to these strings, then drops them. `SerialAttachmentOptions` goes once
nothing reads it.

## Emulator side

- `IAciaEndpoint`: 047's interface, a superset of 033's.
- `Acia6551`: 047's journaled host-input methods. `EnqueueReceivedBytes` is
  left for the loopback and file sources only.
- `MachineHost`: one set of per-port arrays, 033's `m_serialPorts` and
  `m_serialOnCard` with the endpoint each port has. `InstallSerialEndpoint`
  takes any `IAciaEndpoint`. 047's `m_ownedAciaEndpoints` and
  `CloseSerialAttachments` fold into it. `DropHostConnections` and
  `ResyncHostConnections` walk the same arrays.
- `ConnectSerialPort`: parses the device and installs 047's endpoint for `tcp`,
  `modem` and `com`, 033's for `imagewriter`, and the card endpoints for the
  loopback and files. A listening `tcp` port is published to the running
  registry as 033 does, so Casso Explorer finds it. Problems go to
  `SerialLinkReports`. `MachineBuilder::CreateSerialAttachment` calls the same
  code at build time.
- `AciaBridgeEndpoint` and the emulated `IByteStream` path are removed once
  nothing uses them; CassoLink's PC side runs in Casso Explorer and connects
  to the listening TCP port, so that path is left as it is.
- `SerialTrafficLog` is fed from the 6551's comm events, so every attachment
  is captured.

## Settings

One control set per port: `SerialAttachmentView` on the Serial ports page
(//c) and the Serial card page, applied through `SettingsConnectors` with no
reset. The Machine tab's port drop-downs from 033 are removed in favor of the
pages, which have room for the options. The Printing page's connector chooser
stays and selects `imagewriter` through the same staging.

## Tests

- `SerialPortDevice` parse and format for every row above, and the loader's
  conversion of 047's keys.
- `ConnectSerialPort` with each kind, on a //c port and on the card, with the
  fakes 047 already has (`FakeHostNetwork`, `FakeHostSerialPorts`).
- A change applied with no reset is installed in the running machine and is
  not journaled; the next poll sets the lines.
- 047's replay, resume and state-file checks run on the new wiring.
- The running registry has a listening port and drops it on a change.

## Questions for the owner

1. What `com:COM3` with no rate means. In 033 it is a fixed 115,200 baud,
   which CassoLink over a null-modem cable depends on for speed. In 047 it
   follows the rate the Apple program sets, as a real cable does. Proposal:
   `com:COM3` follows the 6551, and `com:COM3@115200` is a fixed rate, which
   CassoLink writes when it sets a port up. 033 is not released, so no saved
   configuration changes meaning.
2. CassoLink over a link that stays open across a resume from the past.
   Casso Explorer's CassoLink session has separate protocol state and times
   its reads by the wall clock, so after a resume the Apple and Casso Explorer
   are at different points. Proposal: the emulator closes a connection on a
   registry-published port on a resume, so Casso Explorer reconnects and
   starts its session again; other TCP and modem links stay open.
