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
};





////////////////////////////////////////////////////////////////////////////////
//
//  InputRecord
//
//  One journaled input: the position and cycle at which the CPU thread
//  applied it, what it was, and its arguments. Only a disk mount carries text
//  (the image path), so the payload is empty on nearly every record.
//
////////////////////////////////////////////////////////////////////////////////

struct InputRecord
{
    uint64_t     position = 0;
    uint64_t     cycle    = 0;
    InputKind    kind     = InputKind::KeyPress;
    Byte         value    = 0;
    uint16_t     detail   = 0;
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
////////////////////////////////////////////////////////////////////////////////

class InputJournal
{
public:
    void  SetOn             (bool isOn)                       { m_isOn = isOn; }
    bool  IsOn              () const                          { return m_isOn; }
    void  SetPositionSource (const uint64_t * positionSource) { m_positionSource = positionSource; }

    void  Record (uint64_t cycle, InputKind kind, Byte value, uint16_t detail, std::string_view payload);

    size_t               GetBeginIndex () const { return m_firstIndex; }
    size_t               GetEndIndex   () const { return m_firstIndex + m_records.size(); }
    const InputRecord  & GetRecord     (size_t index) const;

    void  Truncate      (size_t endIndex);
    void  DiscardBefore (size_t beginIndex);
    void  Clear         ();

private:
    std::deque<InputRecord>   m_records;
    size_t                    m_firstIndex     = 0;
    const uint64_t          * m_positionSource = nullptr;
    bool                      m_isOn           = false;
};
