#pragma once

#include "Pch.h"

#include "Core/IMachineState.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Mm58167
//
//  National's MM58167A real time clock: BCD counters from thousandths of a
//  second to months, a latch (RAM) register behind each counter, interrupt
//  status and control, the counter and RAM resets, the status bit and the GO
//  command. Registers 0 to 7 are the counters: thousandths in bits 7-4,
//  hundredths and tenths, seconds, minutes, hours, the day of the week (1 to
//  7), the day of the month and the month. Registers 8 to 15 are the latches.
//  The chip has no year counter, and February is 28 days.
//
//  The counters run on host time, through the time source: each access
//  advances them by the milliseconds since the last. The status bit is set
//  when an access advances a counter, and reading it clears it. A status bit
//  of 1 after reading the counters indicates that a counter changed during
//  the reads.
//
//  A latch nibble of $C or more is ignored in the comparison. SOS stores
//  the year in the day and month latches; at power on they are set to the
//  host's year in SOS's form, as if SOS had set the clock this year.
//
//  The interrupt output is high while an enabled interrupt is pending.
//
////////////////////////////////////////////////////////////////////////////////

class HostInputGate;
class InputJournal;
struct InputRecord;

class Mm58167 : public IMachineState
{
public:
    static constexpr int       kRegisterCount     = 32;
    static constexpr int       kCounterCount      = 8;
    static constexpr Byte      kThousandths       = 0x00;
    static constexpr Byte      kHundredths        = 0x01;
    static constexpr Byte      kSeconds           = 0x02;
    static constexpr Byte      kMinutes           = 0x03;
    static constexpr Byte      kHours             = 0x04;
    static constexpr Byte      kWeekday           = 0x05;
    static constexpr Byte      kDay               = 0x06;
    static constexpr Byte      kMonth             = 0x07;
    static constexpr Byte      kFirstLatch        = 0x08;
    static constexpr Byte      kDayLatch          = 0x0E;
    static constexpr Byte      kMonthLatch        = 0x0F;
    static constexpr Byte      kInterruptStatus   = 0x10;
    static constexpr Byte      kInterruptControl  = 0x11;
    static constexpr Byte      kCounterReset      = 0x12;
    static constexpr Byte      kRamReset          = 0x13;
    static constexpr Byte      kStatus            = 0x14;
    static constexpr Byte      kGo                = 0x15;
    static constexpr Byte      kStandby           = 0x16;
    static constexpr Byte      kTestMode          = 0x1F;

    //  Interrupt status and control bits.
    static constexpr Byte      kIrqCompare        = 0x01;
    static constexpr Byte      kIrqTenth          = 0x02;
    static constexpr Byte      kIrqSecond         = 0x04;
    static constexpr Byte      kIrqMinute         = 0x08;
    static constexpr Byte      kIrqHour           = 0x10;
    static constexpr Byte      kIrqDay            = 0x20;
    static constexpr Byte      kIrqWeek           = 0x40;
    static constexpr Byte      kIrqMonth          = 0x80;

    static constexpr uint32_t  kStateTag     = IMachineState::MakeTag ('M', '5', '8', 'K');
    static constexpr uint16_t  kStateVersion = 2;

    //  Local time in milliseconds since 1 January 1970.
    using TimeSource = std::function<int64_t ()>;

    struct LocalTime
    {
        int  year        = 1970;
        int  month       = 1;      // 1 to 12
        int  day         = 1;      // 1 to 31
        int  weekday     = 1;      // 1 to 7, Sunday 1
        int  hour        = 0;
        int  minute      = 0;
        int  second      = 0;
        int  millisecond = 0;
    };

    Mm58167();

    //  Reverse execution. With a journal attached, a ClockTime entry is added
    //  when a read finds the host's time changed. While the host input gate is
    //  closed, during a replay or behind live, the counters run on the time
    //  from the journal.
    void        SetInputJournal  (InputJournal * journal, HostInputGate * gate);
    bool        ApplyInput       (const InputRecord & record);

    //  Null selects the host's local time.
    void        SetTimeSource (TimeSource source);

    //  Sets the counters, and the year in the day and month latches, from the
    //  time source.
    void        PowerOn       ();

    Byte        Read          (Byte reg);
    void        Write         (Byte reg, Byte value);
    Byte        Peek          (Byte reg) const;

    bool        IsInterruptAsserted () const { return (m_interruptStatus & m_interruptControl) != 0; }

    //  The time source's time, split into fields.
    static LocalTime  SplitLocalTime (int64_t localMs);

    //  The day and month latch values with a two-digit BCD year in the form
    //  SOS reads back: SEC, the month latch shifted left twice with ROL, then
    //  ANDed with the day latch.
    static void       EncodeSosYear  (Byte bcdYear, Byte & outDayLatch, Byte & outMonthLatch);

    HRESULT     SaveState     (StateWriter & writer) const override;
    HRESULT     LoadState     (StateReader & reader) override;

private:
    void        Advance       ();
    void        AdvanceDays   (int64_t days, Byte & outRolled);
    void        CheckCompare  ();
    int64_t     GetNow        ();
    int64_t     ReadSource    () const;

    static int  ToBcd         (int value) { return ((value / 10) << 4) | (value % 10); }
    static int  FromBcd       (Byte value) { return ((value >> 4) & 0x0F) * 10 + (value & 0x0F); }
    static int  GetMonthDays  (int month);

    TimeSource                          m_source;
    InputJournal                      * m_inputJournal     = nullptr;
    HostInputGate                     * m_gate             = nullptr;
    int64_t                             m_sampledMs        = 0;
    std::array<Byte, kCounterCount>     m_counters         = {};
    std::array<Byte, kCounterCount>     m_latches          = {};
    int64_t                             m_lastMs           = 0;
    Byte                                m_interruptStatus  = 0;
    Byte                                m_interruptControl = 0;
    Byte                                m_standby          = 0;
    bool                                m_status           = false;
};
