# Research: Disk II Debug Window

**Feature**: Disk II Debug Window (`feature/006-disk-ii-debug`)
**Source**: Research notes compiled 2026-05-17 from the Disk II nibble-format
literature, Win32 ListView documentation, and SPSC lock-free queue prior art.
File:line references at the bottom of each section are authoritative.

This document is intentionally short. Unlike Feature 005, which required a
deep multi-emulator survey to ground architectural decisions, Feature 006 is
a passive observer with three well-trodden building blocks: a documented
on-disk byte format, a documented Win32 control idiom, and a 30-year-old
concurrent-data-structure pattern. The research focuses on confirming each
building block's contract and identifying the small set of Casso-specific
touch points.

---

## Part 1 — Disk II Nibble Format (for the address-mark watcher)

### 1.1 Address mark (sector header)

Every sector on a stock DOS 3.3 disk has an address mark immediately
preceding its data mark. The address mark is a fixed-format byte sequence
written into the inter-sector gap by `INIT` (or by the copy-protection
mastering tool):

```
   D5 AA 96    (prologue, 3 nibbles, never appears in encoded data)
   <vol_hi> <vol_lo>     (4-and-4 encoded volume number, 0..255)
   <trk_hi> <trk_lo>     (4-and-4 encoded track number, 0..34)
   <sec_hi> <sec_lo>     (4-and-4 encoded sector number, 0..15)
   <chk_hi> <chk_lo>     (4-and-4 encoded checksum)
   DE AA EB              (epilogue)
```

**4-and-4 decode**: each pair (hi, lo) decodes to
`byte = ((hi << 1) | 1) & lo`. Equivalent: each source byte's odd bits
go into `hi` (shifted right, top bit forced high), even bits into `lo`
(top bit forced high). The "top bit forced high" rule is what makes
every encoded nibble a "valid" disk nibble (high bit set is a hardware
prerequisite of the Disk II shift register accepting a byte).

**Checksum**: `chk == vol XOR trk XOR sec`. Verifying the checksum is the
single most important false-positive guard in the watcher; garbage byte
sequences routinely contain spurious `D5 AA 96` triplets and the checksum
rejects every one that isn't a real address mark.

### 1.2 Data mark (sector contents)

Immediately following the address mark's epilogue (or after a small gap):

```
   D5 AA AD              (data-mark prologue)
   <342 6-and-2 nibbles> (encoded 256 bytes)
   <checksum nibble>     (XOR-fold checksum)
   DE AA EB              (epilogue)
```

**6-and-2 encoding**: every 3 source bytes become 4 disk nibbles. The
exact encoding table is in the DOS 3.3 source (RWTS source in *Beneath
Apple DOS* p. 3-21). For this feature the watcher does NOT decode the
data nibbles; it only counts them between prologue and epilogue. The
emitted `DATA READ` row shows `(256 bytes)` regardless of the actual
nibble count, because the count-to-byte mapping is fixed (342 nibbles +
checksum = 256 bytes).

### 1.3 Why the watcher must verify the address-mark checksum but not the data-mark checksum

The address-mark prologue is `D5 AA 96` (3 bytes); the data-mark prologue
is `D5 AA AD` (also 3 bytes). Both share the `D5 AA` lead-in, which is
sufficiently common in random nibble noise that a 3-byte match is not a
reliable detection. The address-mark checksum (XOR of three small
integers) is what makes the address-mark match a true positive.

The data-mark prologue does not have an immediately-following checksum;
the checksum is at the end of the 342-nibble body, ~342 nibbles later.
v1 of the watcher treats the data mark as "seen" on prologue match
without waiting for the body checksum, because:

- A false positive on the data mark merely emits a spurious `DATA READ`
  row with `S?` — visible to the user, easy to dismiss, no logic error.
- Waiting for the body checksum would require buffering ~342 nibbles
  before any event fires, defeating the live-streaming UX.

The watcher does verify that the epilogue (`DE AA EB`) appears within the
expected window (342 + small slack nibbles); if the body never terminates,
the state machine resets without firing an event.

### 1.4 Sources

- Worth, S. & Lechner, D., *Beneath Apple DOS* (1981), Ch. 3 §"The
  Address Field" (p. 3-18) and §"The Data Field" (p. 3-21).
- Apple, *Disk II Reference Manual* (1978), Appendix B.
- AppleWin source: `Disk.cpp` and `DiskFormatTrack.cpp` (3-byte prologue
  scanning, 4-and-4 decoder helper, XOR checksum verification).
- `https://retrocomputing.stackexchange.com/q/15693` (concise modern
  summary of the format with citations).

---

## Part 2 — Win32 Virtual-Mode ListView

### 2.1 Why virtual mode is mandatory here

A standard `LVS_REPORT` ListView stores one `LV_ITEM` per row internally;
adding 100,000 rows blows past the control's documented practical limit
(~10,000 rows before insert latency becomes user-perceptible) and balloons
memory by ~120 bytes per row of internal bookkeeping on top of whatever
strings the caller supplied.

`LVS_OWNERDATA` (virtual mode) inverts the ownership: the control stores
nothing per row, and asks the caller for row contents via `LVN_GETDISPINFO`
on demand (only for visible rows). Setting the row count is
`ListView_SetItemCountEx`, which is O(1).

Microsoft documents the pattern in *Virtual List-View Controls*
(`learn.microsoft.com/en-us/windows/win32/controls/list-view-controls-overview#virtual-list-views`).

### 2.2 The `LVSICF_NOSCROLL` flag

`ListView_SetItemCountEx (hwnd, count, flags)` accepts two flag bits:

- `LVSICF_NOINVALIDATEALL` — don't repaint every row; the control invalidates
  only the rows whose state changes.
- `LVSICF_NOSCROLL` — don't move the scroll position when the row count
  changes.

Pass **both** flags for an append-only log. Without `LVSICF_NOSCROLL`, the
control may auto-scroll on count change in ways that fight the auto-tail
heuristic; with `LVSICF_NOSCROLL`, the only scroll movement comes from the
explicit `ListView_EnsureVisible` call in the auto-tail handler — which is
exactly what FR-013 requires.

### 2.3 The auto-tail heuristic

The capture must be done **before** the count update:

```cpp
int oldCount       = ListView_GetItemCount (hwnd);
int firstVisible   = ListView_GetTopIndex (hwnd);
int visibleCount   = ListView_GetCountPerPage (hwnd);
bool wasAtTail     = (firstVisible + visibleCount >= oldCount);

// ... drain ring into deque, deque grows by N ...

ListView_SetItemCountEx (hwnd, newCount, LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
if (wasAtTail)
    ListView_EnsureVisible (hwnd, newCount - 1, FALSE);
```

The `>= oldCount` comparison (not `==`) handles the edge case where the
last visible row is also the very last row in the list (`firstVisible +
visibleCount` may equal or slightly exceed `oldCount` depending on partial
last-row visibility).

### 2.4 Sources

- Microsoft Docs: *List-View Controls — Virtual List-View Controls*
  (`learn.microsoft.com/en-us/windows/win32/controls/list-view-controls-overview`).
- Microsoft Docs: *LVM_SETITEMCOUNT message* (and its `Ex` wrapper macro).
- Process Explorer (Sysinternals) source-leak references — same idiom,
  same `LVSICF_NOSCROLL` usage for its 30 Hz process-list refresh.

---

## Part 3 — Casso-Specific Touch Points

Pre-touch fingerprinting (read-only) of the Disk II controller and the
existing dialog patterns Casso uses. These line ranges are advisory; the
implementer should re-verify against current HEAD before each PR.

### 3.1 `CassoEmuCore/Devices/DiskIIController.h`

Current public surface includes:

- `HandleSwitch(uint8_t addr)` — soft-switch dispatcher; cases `0x9` (motor on),
  `0xA`/`0xB` (drive select), `0x0..0x7` (phase magnets).
- `HandlePhase(int phaseIndex, bool energized)` — computes `qtDelta`, clamps
  `m_quarterTrack` to `[0, kMaxQuarterTrack]`.
- `Tick(uint64_t cycles)` — runs the spindown counter; transitions
  `m_motorOn` from true to false when the counter expires.
- `MountImage(int drive, /* image */)` / `EjectImage(int drive)` — mount/eject.
- `NibbleReadByte()` / `NibbleWriteByte(uint8_t)` — nibble-stream access from
  the CPU.

**Required additions**:

- `IDiskIIEventSink* m_eventSink = nullptr;`
- `void SetEventSink(IDiskIIEventSink* sink) noexcept;`
- `DiskIIAddressMarkWatcher m_addrMarkWatcher;` (direct member, ~100 bytes
  for state)
- Fire sites: 9 of them (10 event types, but `OnHeadStep` and `OnHeadBump`
  share a call site).
- One call to `m_addrMarkWatcher.ObserveNibble(...)` in `NibbleReadByte`
  (and optionally in `NibbleWriteByte` if write-event detection is wanted
  in v1; the plan defers write-path nibble observation to Phase 4 if the
  write path doesn't lend itself to passive observation).

### 3.2 `Casso/EmulatorShell.{h,cpp}`

Current shell already owns:

- `OptionsDialog m_optionsDialog;` (or pointer) — pattern Feature 005 set.
- `MachinePickerDialog m_machinePickerDialog;` — analogous.
- Open-dialog menu handlers for `IDM_VIEW_OPTIONS` etc.

**Required additions**:

- `std::unique_ptr<DiskIIDebugDialog> m_diskIIDebugDialog;` (lazy-created on
  first open).
- `void OpenDiskIIDebugDialog();` (called by the View → Disk II Debug...
  menu handler).
- On close: revoke the sink (`controller.SetEventSink(nullptr)`) before
  the dialog tears down the ring.

### 3.3 `Casso/MenuSystem.{h,cpp}` + `Casso/Casso.rc` + `Casso/resource.h`

- New `#define IDM_VIEW_DISKII_DEBUG ...` in `resource.h`.
- New menu item under the existing `View` popup in `Casso.rc`.
- New accelerator entry in `Casso.rc`'s `ACCELERATORS` block:
  `"D", IDM_VIEW_DISKII_DEBUG, VIRTKEY, CONTROL, SHIFT`.
- `MenuSystem::HandleCommand` routes the new id to `EmulatorShell::OpenDiskIIDebugDialog`.

### 3.4 Existing test infrastructure

`UnitTest/Devices/DiskIIControllerTests.cpp` already exercises every
existing controller behavior. The new `DiskIIControllerEventTests.cpp`
adds a recording-sink mock and asserts:

- Event ordering matches `HandlePhase` / `HandleSwitch` / `Tick` sequencing.
- No-sink build is byte-identical (the existing tests must pass without
  modification, with the new watcher member present but the sink
  unattached).
- Watcher with sink attached but every fire site stubbed to no-op also
  leaves the nibble stream byte-identical.

---

## Part 4 — SPSC Lock-Free Ring Buffer

### 4.1 Why SPSC (and not a general MPMC queue)

Two threads only:

- **Producer**: the CPU emulation thread. Every fire-site call goes through
  it. There is exactly one CPU emulation thread (Casso runs single-core
  emulation in v1).
- **Consumer**: the UI thread. Drains on the 30 Hz `WM_TIMER`. There is
  exactly one UI thread.

The "single producer + single consumer" constraint allows the simplest
lock-free queue formulation: two `std::atomic<uint32_t>` indices (head /
tail), no compare-exchange, no exponential backoff, no hazard pointers.
General MPMC queues (Michael-Scott, Vyukov bounded MPMC) add complexity
that's wasted here.

### 4.2 The canonical formulation

Folly's `folly::ProducerConsumerQueue`, Boost's `spsc_queue`, and Dmitry
Vyukov's classic write-up
(`https://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue`
also covers SPSC) all implement the same pattern:

```cpp
template<class T, size_t Capacity>
class SpscRing {
    static_assert((Capacity & (Capacity - 1)) == 0, "power of 2");
    alignas(64) std::atomic<uint32_t> m_head{0};   // consumer side
    alignas(64) std::atomic<uint32_t> m_tail{0};   // producer side
    T m_slots[Capacity];

public:
    bool TryPush (const T& v) {
        uint32_t t = m_tail.load (std::memory_order_relaxed);
        uint32_t h = m_head.load (std::memory_order_acquire);
        if (t - h == Capacity) return false;  // full
        m_slots[t & (Capacity - 1)] = v;
        m_tail.store (t + 1, std::memory_order_release);
        return true;
    }

    bool TryPop (T& out) {
        uint32_t h = m_head.load (std::memory_order_relaxed);
        uint32_t t = m_tail.load (std::memory_order_acquire);
        if (h == t) return false;             // empty
        out = m_slots[h & (Capacity - 1)];
        m_head.store (h + 1, std::memory_order_release);
        return true;
    }
};
```

The `alignas(64)` cache-line padding prevents the producer's tail update
from invalidating the consumer's head cache line (false sharing). On
x86-64 the release/acquire pairing is free (every store has release
semantics, every load has acquire semantics); on ARM64 it generates the
correct `STLR` / `LDAR` instructions.

### 4.3 The drop-oldest-with-marker policy (FR-010)

The plan's policy: when `TryPush` returns `false`, the producer increments
a `std::atomic<uint32_t> m_droppedSinceLastDrain` counter (with
`memory_order_relaxed` since the consumer reads it with
`exchange(0, acq_rel)`). On the next drain, the consumer:

1. Reads-and-zeros the dropped counter (`exchange(0, acq_rel)`).
2. If the counter was > 0, inserts a single synthetic
   `DiskIIEventDisplay{ type = LOST, payload = count }` row into the deque
   **before** the first newly-drained event of this batch. (Approximate
   chronological insertion; precise per-event interleaving is not feasible
   since the dropped events have no recorded data.)
3. Drains the ring normally.

This satisfies FR-010: exactly one `[N events lost]` row per drain that
followed any number of overflow events.

### 4.4 Sources

- Vyukov, D., *Single-Producer/Single-Consumer Queue*
  (`https://www.1024cores.net/home/lock-free-algorithms/queues/spsc`).
- Folly source: `folly/ProducerConsumerQueue.h`
  (`github.com/facebook/folly/blob/main/folly/ProducerConsumerQueue.h`).
- Boost.Lockfree: `boost::lockfree::spsc_queue`.
- Herlihy, M. & Shavit, N., *The Art of Multiprocessor Programming*
  (2nd ed.), §10.5 on bounded SPSC FIFOs.

---

## Part 5 — Performance Budget (Sanity Check)

### 5.1 Event rate

- DOS 3.3 cold boot: ~150 events/sec peak (motor on once, ~40 head steps
  during recalibration in ~200 ms, ~16 address marks + 16 data reads per
  track per ~120 ms, drive select rare, disk insert/eject not during boot).
- Typical steady-state read: ~80 events/sec (address mark + data read per
  sector, 16 sectors/track, ~5 tracks/sec = 160 events/sec at peak, ~50 at
  cruise).
- Tight-loop torture (CPU hammering `$C0E0`-`$C0E7`): can produce
  10,000+ head-step events/sec if the CPU spins on the soft switch. This
  is the FR-010 lossage scenario — the ring fills, the marker fires,
  and the developer sees what happened.

### 5.2 Memory

- Ring: 4,096 × ~24 bytes = ~96 KB. Resident the entire time the dialog
  is open; freed on dialog close.
- Deque: 100,000 × ~80 bytes formatted = ~8 MB worst case. Same lifetime.
- Watcher state: ~100 bytes including the 3-nibble rolling window.

Total: ~8.1 MB worst case, transient (only while the dialog is open),
acceptable per NFR-003.

### 5.3 CPU

- Per fire site: ~5 ns for the `nullptr` check (mispredicted branch on the
  first call after attach, otherwise predicted-not-taken) plus ~30 ns for
  one SPSC push (two atomic loads, one store, one release update — all
  uncontended on x86-64) when attached.
- Per nibble: ~10 ns for the watcher (one switch statement, one state
  update). At a peak nibble rate of ~32k nibbles/sec (Disk II's bitrate
  divided by 8), that's ~320 µs/sec — < 0.1% of a 1 MHz CPU thread.
- Drain timer: a 4,096-entry drain at ~50 ns/entry = ~200 µs every 33 ms
  = ~0.6% UI thread.

All well within NFR-001's "< 1 %" budget.

---

## Part 6 — Decisions to Validate at Implementation Time

The spec encodes the design; these are the small implementation choices
that the plan defers to the implementer:

1. **Filter projection storage**: maintain a parallel `std::vector<uint32_t>`
   of deque indices that match the current filter mask, or filter lazily
   inside `LVN_GETDISPINFO`. The latter is simpler; the former is faster
   for very large deques. Both satisfy FR-014; pick one in Phase 7 based
   on whether `LVN_GETDISPINFO` benchmarks acceptably at full deque size.
2. **Data-mark write-path observation**: the read-path watcher is
   straightforward (nibbles flow CPU-ward); the write-path requires the
   watcher to also observe nibbles flowing controller-ward, which may
   require a second `ObserveNibble` invocation in `NibbleWriteByte`. If
   the write path's nibble sequencing is significantly different from
   the read path, the implementer may simplify by deferring write events
   to a follow-up feature and shipping v1 with `OnDataMarkWrite` as a
   stub that's wired but never fires.
3. **`DiskIIEvent` POD layout**: pack event-type enum + cycle counter +
   payload union into 24 bytes. The exact layout (which union members,
   what alignment) is a Phase 1 implementation detail.

None of these block spec finalization; all are bounded-cost choices
that don't change the user-visible behavior.
