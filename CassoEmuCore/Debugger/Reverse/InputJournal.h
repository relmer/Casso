#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  InputKind
//
//  Every host input that can change what the emulated machine computes.
//
//  HostState is not an input but a sync point: the saved state of every
//  device another thread writes, taken where reverse execution snapshots the
//  machine. A snapshot can catch a value the host wrote that no read has seen
//  yet; a replay crossing that point loads the same values, so it reaches the
//  snapshot's state exactly.
//
////////////////////////////////////////////////////////////////////////////////

enum class InputKind : uint8_t
{
    KeyPress,
    AutoRepeat,
    PasteChar,
    KeyDown,
    Paddle,
    Button,
    MouseMove,
    MouseButton,
    AciaByte,
    Reset,
    PowerCycle,
    DiskMount,
    DiskEject,
    DriveWriteProtect,
    ImageWriteProtect,
    KeyLatch,
    MouseTarget,
    JoyportJack,
    HostState,
};





////////////////////////////////////////////////////////////////////////////////
//
//  InputLine
//
//  Which switch a Button record holds, in its detail. The game port's three
//  pushbuttons on a ][ / ][+, the //e keys that share those lines, the //c
//  80/40 case switch, and the two Joyport settings a read depends on.
//
////////////////////////////////////////////////////////////////////////////////

enum class InputLine : uint16_t
{
    OpenApple,
    ClosedApple,
    Shift,
    EightyColumnSwitch,
    GamePortButton0,
    GamePortButton1,
    GamePortButton2,
    JoyportAttached,
    JoyportPaddlesConnected,
};





////////////////////////////////////////////////////////////////////////////////
//
//  InputRecord
//
//  One journaled input: the position and cycle at which the CPU thread
//  applied it, what it was, and its arguments. Only a disk mount carries text
//  (the image path), so the payload is empty on nearly every record. Data
//  holds what does not fit a byte: both mouse deltas, a mouse target, or a
//  Joyport jack's switches.
//
//  An observed record was made by a device during the instruction at its
//  position; any other was applied at the boundary before that instruction.
//  A replay landing on a position has applied the second kind there but not
//  yet the first.
//
////////////////////////////////////////////////////////////////////////////////

struct InputRecord
{
    uint64_t     position   = 0;
    uint64_t     cycle      = 0;
    InputKind    kind       = InputKind::KeyPress;
    Byte         value      = 0;
    uint16_t     detail     = 0;
    uint64_t     data       = 0;
    bool         isObserved = false;
    std::string  payload;
};





////////////////////////////////////////////////////////////////////////////////
//
//  InputJournal
//
//  The ordered list of host inputs applied on the CPU thread, each stamped
//  with its position and cycle before it takes effect, so a replay from a
//  snapshot can apply the same inputs at the same points.
//
//  Indices are absolute: DiscardBefore drops old records without changing
//  the index of any record kept, so a snapshot can hold GetEndIndex as its
//  cursor into the journal. Records are kept in the order they were made;
//  the cycle counter can restart on a power cycle, so the order, not the
//  cycle, is what a replay walks.
//
//  A journal that is off ignores every Record call. CPU-thread only.
//
//  Devices whose state another thread writes record through RecordObserved,
//  at the first CPU-thread read that sees a new value, stamped with the
//  cycle the instruction making that read began at. A replay applies each
//  such record just before the instruction that starts at its cycle. A
//  device holds a pointer to the journal only while it is on, so with the
//  journal off its reads pay one null test and nothing else.
//
//  The CPU thread also samples those devices at the start of each execution
//  slice (MachineHost::SampleHostInputs), so a value another thread wrote is
//  journaled where the machine first held it, not only where a read first saw
//  it. While sampling is set, RecordObserved makes a boundary record instead,
//  which a replay applies on landing at its position. A sample updates the
//  same last-seen value a read compares with, so a read after it records
//  nothing for the same change.
//
//  The records sit in one vector from m_head on. Dropped records leave their
//  slots behind, which a later record is written over, payload string and
//  all, so a journal that has reached its working size records without
//  allocating.
//
////////////////////////////////////////////////////////////////////////////////

class InputJournal
{
public:
    void  SetOn             (bool isOn)                       { m_isOn = isOn; }
    bool  IsOn              () const                          { return m_isOn; }
    void  SetPositionSource (const uint64_t * positionSource) { m_positionSource = positionSource; }
    void  SetCycleSource    (const uint64_t * cycleSource)    { m_cycleSource = cycleSource; }
    void  SetSampling       (bool isSampling)                 { m_isSampling = isSampling; }

    uint64_t  GetCycle() const { return (m_cycleSource != nullptr) ? *m_cycleSource : 0; }

    void  Record         (uint64_t cycle, InputKind kind, Byte value, uint16_t detail, std::string_view payload);
    void  RecordObserved (uint64_t cycle, InputKind kind, Byte value, uint16_t detail, uint64_t data);

    size_t               GetBeginIndex () const { return m_firstIndex; }
    size_t               GetEndIndex   () const { return m_firstIndex + m_count; }
    const InputRecord  & GetRecord     (size_t index) const;

    void  Truncate      (size_t endIndex);
    void  DiscardBefore (size_t beginIndex);
    void  Clear         ();

private:
    InputRecord  & AppendSlot();

    std::vector<InputRecord>   m_records;                   // live records start at m_head; slots past them are kept for reuse
    size_t                     m_head           = 0;
    size_t                     m_count          = 0;
    size_t                     m_firstIndex     = 0;
    const uint64_t           * m_positionSource = nullptr;
    const uint64_t           * m_cycleSource    = nullptr;
    bool                       m_isOn           = false;
    bool                       m_isSampling     = false;
};
