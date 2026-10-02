#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  InputKind
//
//  Every host input that can change what the emulated machine computes.
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
////////////////////////////////////////////////////////////////////////////////

struct InputRecord
{
    uint64_t     position = 0;
    uint64_t     cycle    = 0;
    InputKind    kind     = InputKind::KeyPress;
    Byte         value    = 0;
    uint16_t     detail   = 0;
    uint64_t     data     = 0;
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
////////////////////////////////////////////////////////////////////////////////

class InputJournal
{
public:
    void  SetOn             (bool isOn)                       { m_isOn = isOn; }
    bool  IsOn              () const                          { return m_isOn; }
    void  SetPositionSource (const uint64_t * positionSource) { m_positionSource = positionSource; }
    void  SetCycleSource    (const uint64_t * cycleSource)    { m_cycleSource = cycleSource; }

    uint64_t  GetCycle() const { return (m_cycleSource != nullptr) ? *m_cycleSource : 0; }

    void  Record         (uint64_t cycle, InputKind kind, Byte value, uint16_t detail, std::string_view payload);
    void  RecordObserved (uint64_t cycle, InputKind kind, Byte value, uint16_t detail, uint64_t data);

    size_t               GetBeginIndex () const { return m_firstIndex; }
    size_t               GetEndIndex   () const { return m_firstIndex + m_records.size(); }
    const InputRecord  & GetRecord     (size_t index) const;

    void  Truncate      (size_t endIndex);
    void  DiscardBefore (size_t beginIndex);
    void  Clear         ();

private:
    std::deque<InputRecord>    m_records;
    size_t                     m_firstIndex     = 0;
    const uint64_t           * m_positionSource = nullptr;
    const uint64_t           * m_cycleSource    = nullptr;
    bool                       m_isOn           = false;
};
