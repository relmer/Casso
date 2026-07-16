# //c Serial Printer Integration — Handoff from Spec 015

**Status**: brief for the //c effort. Written 2026-07-08 from spec 015 (printer support).
**Depends on**: spec 015 merged to `master` (provides the printer sink this bridges to).
**Owns**: this work belongs to **issue #87** (`Apple //c serial printer integration + per-port device selection`) — it needs BOTH spec 015 and spec 016 on `master`, so it is downstream of each (016 never waits on it; 016 ships the ACIA + loopback/file ports). Originally drafted as 016 task T024 before the deferral.

---

## TL;DR

The //c has **no expansion slots** — its "printer port" is a built-in serial port backed by a 6551 ACIA (port 1 / slot-1 I/O; port 2 / modem on slot 2). So on a //c the printer **must** be driven over serial; there is no parallel card to attach.

Spec 015 built the **entire platform-neutral printer pipeline** and validated it on the //e through a *parallel* card (The Print Shop Color drives it; real capture preserved — see Reference below). Everything downstream of a single seam — `PrinterByteRing` — is card-agnostic and already done and tested. To light up printing on the //c, you add **one endpoint** that tees the 6551's transmit stream into that ring, plus a small ownership refactor so the ring is reachable without a `PrinterCard`.

**You are not building a printer.** You are adding a second *front door* to a finished pipeline.

---

## What spec 015 already delivered (all in `CassoEmuCore/Devices/Printer/`, unit-tested)

The data path is: **guest bytes → `PrinterByteRing` → `ImageWriterInterpreter` → `PrintRaster` → `PaperRenderer` → PNG (`PngCodec` / `PrintDelivery`) → file (Eject command)**.

- `PrinterByteRing` — 64 KB lock-free SPSC ring. **This is the seam.** Producer = the interface (today the parallel card); consumer = the drain worker. Whoever produces bytes just fills it (`TryPush`).
- `ImageWriterInterpreter` — pure byte-stream → raster. Grammar **confirmed against a real Print Shop Color stream** (ESC T / ESC K / ESC G, CR/LF, color passes). Interface-agnostic: it does not know or care whether bytes arrived parallel or serial.
- `PrintRaster`, `PaperRenderer`, `RgbaImage`, `PngCodec`, `PrintDelivery`, `PrintFileNaming`, `PrintJobPersistence` — raster, deterministic ink rendering, PNG encode/decode, naming, save/load. All core, all tested.
- `PrinterJob` — the consumer half: drains the ring, runs the interpreter, exposes the raster + a byte-observer tap.
- `PrinterWorker` (`Casso/Print/`) — the exe-side background thread that paces `PrinterJob::Drain`. Started on machine build, stopped on teardown/shutdown.
- **Eject command** — `File → Finish Printing (Eject Paper)` renders the strip to `<Pictures>\Casso Prints\*.png`. Interface-agnostic — works the moment the ring has content, regardless of front door.

**Key fact:** none of the above changes for the //c. The interpreter and everything after it are reused verbatim.

## What the 6551 side already has (from 016 US3, done)

- `Acia6551` — complete dual-port ACIA (registers, RX+TX, IRQ). Tested.
- `IAciaEndpoint` with `AciaLoopbackEndpoint` (comms) and `AciaFileEndpoint` (raw TX → host file).

Note: `AciaFileEndpoint` dumps **raw** serial bytes to a file. That is *not* the printer pipeline — it does not render, page, or produce the //e-quality PNG. It's fine as a capture/debug sink, but printing on the //c should feed the **rich pipeline**, not a raw byte dump.

---

## The work

### 1. Decouple the ring from `PrinterCard` (small refactor — do this first)

Today `PrinterByteRing` lives **inside** `PrinterCard`, and `PrinterWorker::Start` takes `card->ByteRing()`. On a //c there is no `PrinterCard`. So hoist the ring (and the `PrinterJob`/worker it feeds) into a **machine-level printer sink** that *both* front doors target:

- **//e (slotted):** the `parallel-printer` card writes into the shared sink's ring (unchanged behavior; the card just no longer *owns* the ring).
- **//c (slotless):** the serial endpoint (below) writes into the same sink's ring.

Concretely: introduce a `PrinterSink` (owns `PrinterByteRing` + `PrinterJob`), owned at the `EmulatorShell`/machine level. `PrinterCard` gets a pointer to the sink's ring instead of owning it; `PrinterWorker` drains the sink. This is a mechanical hoist — **no logic change to the pipeline** — and it's the only structural pre-work. Keep it minimal.

### 2. Add `AciaPrinterEndpoint` (the actual bridge)

An `IAciaEndpoint` implementation whose TX path pushes each byte into the printer sink's ring. That's essentially the whole class — the ACIA already delivers TX bytes to its endpoint; you're just routing them to the ring instead of a file. (RX can be a stub / "printer ready"; a real ImageWriter's return channel is status the printer driver rarely reads for graphics dumps — confirm against Print Shop's serial driver if it stalls.)

### 3. Wire the //c config

In the //c machine profile, bind **serial port 1** to the `AciaPrinterEndpoint` (port 2 stays comms/loopback). The //c Hardware settings page will then show the two serial ports as internal devices (there is no "parallel printer" slot row on a //c — see the settings note below). No slot entry; the printer rides the built-in port.

### 4. Configure Print Shop on the //c and validate

Boot The (New) Print Shop on the //c, set its printer to **ImageWriter II**, interface = the //c serial/printer port (Print Shop's //c build targets it directly). Print. The bytes should flow ACIA → ring → interpreter → raster, and `File → Finish Printing` should produce the same PNG the //e produces.

---

## Acceptance

- A //c boots Print Shop; printing a page produces a non-empty raster via the serial port (not the file endpoint).
- The resulting PNG is equivalent to the //e parallel path for the same document (same interpreter, same renderer).
- The parallel //e path is unaffected by the ring-ownership hoist (regression: //e Print Shop still prints).
- A unit test drives `AciaPrinterEndpoint` with the reference capture bytes and asserts they land in the ring / produce the expected interpreter events (mocked, no real serial — constitution Test Isolation).

## Reference

- **`reference/printshop-color-testpage.bin`** — 2097 bytes, a real Print Shop **Color** stream captured off the //e parallel card. Because the interface is byte-transparent, **the //c serial port should deliver the same ImageWriter command bytes**, so this is a valid cross-check fixture for the serial path. Decoded structure:

  ```
  CR CR
  ESC T 24                          ; line spacing 24/144"
  LF CR
  ESC K 1  ESC > ESC P  ESC G 0682  <682 bytes>   ; color pass 1
  CR
  ESC K 2  ESC > ESC P  ESC G 0682  <682 bytes>   ; color pass 2
  CR
  ESC K 3  ESC > ESC P  ESC G 0682  <682 bytes>   ; color pass 3
  CR LF
  ```

- **Confirmed grammar:** `ESC T nn` (2 digits, n/144"), `ESC K n` (1 ASCII digit, color pass — 3-pass CR-overlay is how the color ribbon works), `ESC G nnnn` (4-digit column count + that many 8-dot bytes), CR = carriage return (left margin, no advance), LF = advance.
- **Still provisional in 015 (not your problem, but a serial capture is a useful second data point):** `ESC >` / `ESC P` semantics (consumed harmlessly today; likely graphics horizontal density), graphics bit order (assumed MSB = top pin), 8-pin vertical spacing, and the `K1/2/3 → ink` color mapping (US2 of 015).

## Boundary note (resolves a 015/016 seam)

016 spec.md:182 says "the serial-printing driver … is spec 015's scope." In practice 015 shipped the *parallel* path and its serial fallback never triggered (Print Shop drives parallel fine on the //e). So the serial front door is **#87's** to build (downstream of 015 + 016) — it is untestable without a bootable //c — reusing 015's finished, card-agnostic pipeline behind `PrinterByteRing`. This document is that assignment.

## Settings / Hardware UX note (//c)

On slotted machines the Hardware settings page (`Casso/Ui/Settings/HardwarePage.cpp`) builds a tree with an "Internal devices" group and a **"Slots"** group; the parallel printer shows as an optional slot-1 row (you *insert* a card).

The //c has no slots, so mirror the concept with a **"Ports" group** representing the back-panel peripheral ports and showing what is *connected* to each — same tree UX, different metaphor (connected vs inserted). The serial printer appears as the device connected to **serial port 1**; port 2 shows the modem/comms endpoint. This keeps the //c Hardware page consistent with the //e one while matching the //c's real, slotless topology.
