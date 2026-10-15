#pragma once

#include "Pch.h"

#include "DiskMarkPattern.h"
#include "Debugger/Reply.h"
#include "Machines/Apple2/Common/Disk2NibbleRecord.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldTracker
//
//  Follows the fields in the Disk II's recent nibbles for one set of marks:
//  each address field to its checksum, each data field after one to its
//  checksum nibble, and each data prologue written after an address field
//  was read. A disk breakpoint's own marks replace the standard ones, so a
//  breakpoint with custom marks has a tracker of its own.
//
//  An address field whose only missed nibble is in its checksum pair is
//  reported with the checksum unread: the boot ROM checks no address
//  checksum, and can miss its first nibble. An address field with a nibble
//  missed before that, a data field with any nibble missed, a field crossing
//  a disk change and a field any of whose nibbles passed during the motor's
//  spin-up are not reported.
//
////////////////////////////////////////////////////////////////////////////////

class DiskFieldTracker
{
public:
    struct Marks
    {
        std::optional<DiskMarkPattern>  address;                            // absent: D5 AA 96 and D5 AA B5
        std::optional<DiskMarkPattern>  data;                               // absent: D5 AA AD
        DiskFieldKind                   format = DiskFieldKind::Sixteen;    // a data field's length after custom address marks

        bool  operator== (const Marks & other) const = default;
    };

    explicit DiskFieldTracker (const Marks & marks = {}) : m_marks (marks) {}

    const Marks &  GetMarks() const { return m_marks; }

    //  Starts again and follows every entry the record has, reporting
    //  nothing: what happened before a breakpoint was armed, or before a
    //  move through history, does not stop it.
    void  Prime (const Disk2NibbleRecord & record);

    //  Follows the entries added since the last Prime or Feed, and adds an
    //  event for each field that ends among them. A record that started
    //  again since, after a power cycle, is followed from its first entry.
    void  Feed  (const Disk2NibbleRecord & record, std::vector<DiskFieldEvent> & outEvents);

    //  The gap a data prologue may follow its address field by, in nibbles;
    //  a standard track has about ten between them.
    static constexpr int  kMaxAddressToData = 48;

private:
    static constexpr int  kAddressNibbles = 8;
    static constexpr int  kWindow         = 3;

    enum class Phase
    {
        Idle,
        Address,
        Data,
    };

    struct Recent
    {
        Byte  nibble   = 0;
        Word  address  = 0;
        Byte  flags    = 0;
    };

    struct DriveState
    {
        std::array<Recent, kWindow>        window       = {};
        int                                inWindow     = 0;
        std::array<Byte, kWindow>          written      = {};
        int                                inWritten    = 0;
        Phase                              phase        = Phase::Idle;
        DiskFieldKind                      format       = DiskFieldKind::Sixteen;
        int                                collected    = 0;
        std::array<Byte, kAddressNibbles>  addressBytes = {};
        Word                               prologueAt   = 0;
        Byte                               running      = 0;
        bool                               isTainted    = false;
        std::optional<DiskFieldEvent>      lastAddress;
        int                                sinceAddress = 0;
    };

    void  Reset();
    void  Step            (const Disk2NibbleRecord::Entry & entry, std::vector<DiskFieldEvent> * outEvents);
    void  StepWritten     (DriveState & state, int drive, const Disk2NibbleRecord::Entry & entry, std::vector<DiskFieldEvent> * outEvents);
    void  StepAddress     (DriveState & state, int drive, const Disk2NibbleRecord::Entry & entry, std::vector<DiskFieldEvent> * outEvents);
    void  StepData        (DriveState & state, const Disk2NibbleRecord::Entry & entry, std::vector<DiskFieldEvent> * outEvents);
    void  FindPrologue    (DriveState & state);
    void  FinishAddress   (DriveState & state, int drive, Word lastInstruction, bool isChecksumRead, std::vector<DiskFieldEvent> * outEvents);

    int   MatchAddress    (const std::array<Byte, kWindow> & nibbles, int inWindow, DiskFieldKind & outFormat) const;
    int   MatchData       (const std::array<Byte, kWindow> & nibbles, int inWindow) const;

    static bool  IsAtEnd  (const DiskMarkPattern & pattern, const std::array<Byte, kWindow> & nibbles, int inWindow);
    static bool  IsBroken (const DriveState & state, int first);
    static bool  IsSpinUp (const DriveState & state, int first);

    Marks                        m_marks;
    std::array<DriveState, 2>    m_drives  = {};
    uint64_t                     m_fedTo   = 0;
};
