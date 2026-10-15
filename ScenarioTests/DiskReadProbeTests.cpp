#include "Pch.h"

#include "GuestSession.h"
#include "TestMachine.h"
#include "Core/IIoAccessSink.h"
#include "Debugger/DebugHook.h"
#include "Devices/Disk/DiskFieldFormat.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/IDisk2EventSink.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DiskReadProbeTests
{
    static constexpr Word    kDataLatch       = 0xC0EC;    // slot 6's $C08C
    static constexpr Word    kRomSectorEntry  = 0xC65C;    // the boot ROM's sector routine
    static constexpr Word    kRomSector       = 0x3D;
    static constexpr Word    kRomTrack        = 0x41;
    static constexpr Word    kRwtsVector      = 0x03D9;    // JMP to RWTS's entry
    static constexpr Word    kRwtsEntry       = 0xBD00;    // the System Master's RWTS
    static constexpr Word    kLowRwtsEntry    = 0x3D00;    // its copy, which boot stage 2 runs before DOS moves up
    static constexpr Word    kRdadr16         = 0xB944;
    static constexpr Word    kRwtsFirst       = 0xB800;
    static constexpr Word    kRwtsLast        = 0xBFFF;
    static constexpr Word    kLoaderFirst     = 0x0800;    // ProDOS's block 0 loader
    static constexpr Word    kLoaderLast      = 0x0BFF;
    static constexpr Word    kProDosGlobal    = 0xBF00;    // JMP MLI once ProDOS is in
    static constexpr Word    kDriverVector    = 0xBF1C;    // slot 6, drive 1
    static constexpr Word    kDriverCommand   = 0x42;
    static constexpr Word    kDriverBlock     = 0x46;
    static constexpr Word    kIobTrack        = 4;
    static constexpr Word    kIobSector       = 5;
    static constexpr Word    kIobCommand      = 0x0C;
    static constexpr Byte    kJsr             = 0x20;
    static constexpr Byte    kJmp             = 0x4C;
    static constexpr Byte    kJmpIndirect     = 0x6C;
    static constexpr Byte    kReadCommand     = 1;
    static constexpr Byte    kLatchFull       = 0x80;
    static constexpr int     kSectorsPerTrack = 16;
    static constexpr int     kBlocksPerTrack  = 8;
    static constexpr size_t  kSearchLimit     = 48;        // nibbles from an address field to its data prologue
    static constexpr size_t  kNoteLimit       = 40;





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  NibbleRead, Request, DiskEvent
    //
    //  A nibble the CPU took from the latch the first time it was there, with
    //  the instruction that read it and how many nibbles assembled unread
    //  before it; a sector the boot ROM or the operating system requested, in
    //  physical sectors, with its place in the nibble stream; and an event of
    //  the Disk ][ window's listener, with its place.
    //
    ////////////////////////////////////////////////////////////////////////////////

    struct NibbleRead
    {
        Word      pc     = 0;
        Byte      value  = 0;
        uint64_t  missed = 0;
    };

    struct Request
    {
        char    source      = '?';       // R: boot ROM, L: RWTS at $3D00, D: RWTS at $BD00, P: ProDOS driver
        size_t  at          = 0;
        int     track       = 0;
        int     physical[2] = { -1, -1 };
        int     command     = 0;
    };

    struct DiskEvent
    {
        size_t  at     = 0;
        int     track  = 0;
        int     sector = 0;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  Probe
    //
    //  The hook before every instruction, so the instruction running is known
    //  when the latch is read; the listener on reads of the data latch; and
    //  the Disk ][ window's listener, for its "Data read" and cut-off events.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class Probe : public DebugHook, public IIoAccessSink, public IDisk2EventSink
    {
    public:
        std::vector<NibbleRead>   nibbles;
        std::vector<Request>      requests;
        std::vector<DiskEvent>    markReads;
        std::vector<DiskEvent>    cutOffs;
        std::map<Word, size_t>    rwtsCalls;        // calls into RWTS's pages from outside them, by where they land
        std::map<Word, size_t>    otherCallers;     // the instructions that made such a call to anywhere but RWTS's entry
        std::map<Word, size_t>    rdadrCallers;     // the instructions that called RDADR16
        size_t                    vectorEntries = 0;
        Word                      vectorTarget  = 0;
        std::optional<Word>       lowVector;        // the $03D9 JMP's target at the first call of RWTS at $3D00, 0 for no JMP

        Probe (MachineHost & host, bool isProDos) :
            m_host     (host),
            m_isProDos (isProDos)
        {
            m_filter.reads.set (kDataLatch - IoAccessFilter::kFirst);
            m_lastCount = CountNibbles();
        }

        const IoAccessFilter &  GetIoFilter() const { return m_filter; }

        bool  HasPendingStop() const override { return false; }

        //  Called before every instruction, which then runs.
        bool  ShouldStopBefore (Word pc) override
        {
            Word  previous = m_pc;



            m_pc = pc;

            if (pc == kRomSectorEntry)
            {
                AddRomRequest();
            }

            if (m_isProDos)
            {
                NoteDriverCall (pc);
            }
            else
            {
                NoteRwtsCall (previous, pc);
            }

            return false;
        }

        //  A read is the first of its nibble when the drive has assembled a
        //  nibble since the last one taken.
        void  OnIoAccess (Word address, Byte value, bool isWrite) override
        {
            bool      isFull = !isWrite && address == kDataLatch && (value & kLatchFull) != 0;
            uint64_t  count  = isFull ? CountNibbles() : m_lastCount;



            if (count != m_lastCount)
            {
                nibbles.push_back ({ m_pc, value, count - m_lastCount - 1 });
                m_lastCount = count;
            }
        }

        void  OnDataMarkRead    (int track, int sector, int, std::span<const Byte>) override { markReads.push_back ({ nibbles.size(), track, sector }); }
        void  OnDataFieldCutOff (int track, int sector, int) override                         { cutOffs.push_back   ({ nibbles.size(), track, sector }); }

        void  OnMotorCommandOn() override                                      {}
        void  OnMotorEngaged() override                                      {}
        void  OnMotorCommandOff() override                                      {}
        void  OnMotorDisengaged() override                                      {}
        void  OnHeadStep        (int, int) override                              {}
        void  OnHeadBump        (int) override                                   {}
        void  OnAddressMark     (int, int, int) override                         {}
        void  OnDataMarkWrite   (int, int, int, std::span<const Byte>) override  {}
        void  OnDriveSelect     (int) override                                   {}
        void  OnDiskInserted    (int) override                                   {}
        void  OnDiskEjected     (int) override                                   {}

        //  The physical sector a DOS 3.3 logical sector is written under.
        static int GetPhysicalForDosSector (int logical)
        {
            int  found = -1;



            for (int physical = 0; physical < kSectorsPerTrack; physical++)
            {
                if (NibblizationLayer::GetDosFileIndexForPhysicalSector (physical) == logical)
                {
                    found = physical;
                    break;
                }
            }

            return found;
        }

    private:
        Byte  Read   (Word address) { return m_host.GetMemoryBus().ReadByte (address); }
        Word  Read16 (Word address) { return (Word) (Read (address) | (Read ((Word) (address + 1)) << 8)); }

        uint64_t CountNibbles()
        {
            Disk2Controller  * controller = m_host.GetRefs().diskController;



            return (controller == nullptr) ? 0 : controller->GetEngine (0).GetReadNibbles() + controller->GetEngine (1).GetReadNibbles();
        }

        //  The boot ROM enters its routine again for each address field that is
        //  not the one it is looking for, so a request the same as the last one
        //  is that request's retry.
        void Add (const Request & request)
        {
            const Request  * last = requests.empty() ? nullptr : &requests.back();
            bool             same = last != nullptr && last->source == request.source && last->track == request.track
                                    && last->physical[0] == request.physical[0] && last->physical[1] == request.physical[1];



            if (!same)
            {
                requests.push_back (request);
            }
        }

        void AddRomRequest()
        {
            Request  request;



            request.source      = 'R';
            request.at          = nibbles.size();
            request.track       = Read (kRomTrack);
            request.physical[0] = Read (kRomSector);
            request.command     = kReadCommand;
            Add (request);
        }

        //  RWTS's entry, with the IOB at Y (low) and A (high). Every call into
        //  RWTS's pages from outside them, and every call of RDADR16, is
        //  counted by where it lands and where it came from.
        void NoteRwtsCall (Word previous, Word pc)
        {
            Cpu6502Registers  registers = m_host.GetCpu()->GetCpu6502()->GetRegisters();
            Word              iob       = (Word) (registers.y | (registers.a << 8));
            Byte              opcode    = Read (previous);
            bool              isCall    = opcode == kJsr || opcode == kJmp || opcode == kJmpIndirect;
            bool              wasInside = previous >= kRwtsFirst && previous <= kRwtsLast;
            bool              isInto    = pc >= kRwtsFirst && pc <= kRwtsLast && !wasInside && isCall;
            Request           request;



            vectorTarget = (Read (kRwtsVector) == kJmp) ? Read16 ((Word) (kRwtsVector + 1)) : 0;

            if (vectorTarget != 0 && pc == vectorTarget)
            {
                vectorEntries++;
            }

            if (isInto)
            {
                rwtsCalls[pc]++;
            }

            if (isInto && pc != kRwtsEntry)
            {
                otherCallers[previous]++;
            }

            if (pc == kRdadr16)
            {
                rdadrCallers[previous]++;
            }

            if (pc == kLowRwtsEntry && !lowVector.has_value())
            {
                lowVector = vectorTarget;
            }

            if (pc == kRwtsEntry || pc == kLowRwtsEntry)
            {
                request.source      = (pc == kRwtsEntry) ? 'D' : 'L';
                request.at          = nibbles.size();
                request.track       = Read ((Word) (iob + kIobTrack));
                request.physical[0] = GetPhysicalForDosSector (Read ((Word) (iob + kIobSector)) & 0x0F);
                request.command     = Read ((Word) (iob + kIobCommand));
                Add (request);
            }
        }

        //  ProDOS's Disk II driver, through the device vector once ProDOS is
        //  in, with the command at $42 and the block at $46-$47: a block is two
        //  physical sectors of track block / 8.
        void NoteDriverCall (Word pc)
        {
            bool     isCall = Read (kProDosGlobal) == kJmp && pc == Read16 (kDriverVector);
            int      block  = Read16 (kDriverBlock);
            size_t   offset = 0;
            Request  request;



            if (isCall)
            {
                request.source  = 'P';
                request.at      = nibbles.size();
                request.track   = block / kBlocksPerTrack;
                request.command = Read (kDriverCommand);

                for (int half = 0; half < 2; half++)
                {
                    offset                 = ProDosSkeleton::GetBlockByteOffset (block, (size_t) half * DiskFieldFormat::kSectorBytes);
                    request.physical[half] = GetPhysicalForDosSector ((int) ((offset / DiskFieldFormat::kSectorBytes) % kSectorsPerTrack));
                }

                Add (request);
            }
        }

        MachineHost     & m_host;
        bool              m_isProDos  = false;
        IoAccessFilter    m_filter;
        Word              m_pc        = 0;
        uint64_t          m_lastCount = 0;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  Field
    //
    //  One sector found in the nibble stream: its address field's numbers, the
    //  instructions that read the address prologue's D5 and the data field's
    //  checksum nibble, and whether the data field was read to its checksum
    //  nibble with none missed.
    //
    ////////////////////////////////////////////////////////////////////////////////

    struct Field
    {
        size_t  at                = 0;          // the address prologue's D5
        int     track             = 0;
        int     sector            = 0;
        Word    addressPc         = 0;
        Word    checksumPc        = 0;
        bool    isComplete        = false;
        bool    isChecksumSkipped = false;      // the address checksum's first nibble went unread

        bool  IsRead() const { return isComplete && checksumPc != addressPc; }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  Results
    //
    ////////////////////////////////////////////////////////////////////////////////

    struct Results
    {
        std::vector<Field>        fields;
        size_t                    readRequested     = 0;   // read, and a sector in the request under way
        size_t                    readUnrequested   = 0;   // read, and another sector
        size_t                    readByLoader      = 0;   // of those, read by ProDOS's block 0 loader
        size_t                    passedUnrequested = 0;   // passed over, and not that sector
        size_t                    passedThenRead    = 0;   // passed over, that sector, and read later in the same request
        size_t                    passedNeverRead   = 0;   // passed over, that sector, and not read in the request
        size_t                    requestsRead      = 0;
        size_t                    requestsUnread    = 0;
        size_t                    incomplete        = 0;
        std::vector<std::string>  notes;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DiskReadProbeTests
    //
    //  T712 (R-041, FR-145): the read test measured before anything is built
    //  on it. A DOS 3.3 boot on the System Master and a ProDOS boot on the
    //  Users Disk, from a power cycle, with every nibble the CPU took from the
    //  latch and the instruction that took it. A data field counts as read
    //  when the instruction that read its checksum nibble is not the one that
    //  read its address field's first prologue nibble; the fields counted so
    //  are compared with the sectors the boot ROM, RWTS and ProDOS's driver
    //  requested.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DiskReadProbeTests)
    {
    public:

        TEST_METHOD (TheReadTestSeparatesReadSectorsOnADos33Boot)
        {
            Results  results;



            Boot (GuestSession::RequireDos33Master(), false, results, [] (const Probe & probe)
            {
                size_t  outside = 0;
                size_t  entries = probe.rwtsCalls.contains (kRwtsEntry) ? probe.rwtsCalls.at (kRwtsEntry) : 0;



                for (const auto & [caller, count] : probe.rdadrCallers)
                {
                    outside += (caller < kRwtsFirst || caller > kRwtsLast) ? count : 0;
                }

                Assert::AreEqual (probe.vectorEntries, entries, L"every call of RWTS's entry comes through the routine the $03D9 JMP leads to");
                Assert::AreEqual ((size_t) 0, outside, L"RDADR16 is called only from RWTS's own pages");
            });

            Assert::IsTrue   (results.readRequested > 90, L"the boot read DOS");
            Assert::AreEqual ((size_t) 0, results.readUnrequested, L"every field the test counts as read is the sector requested");
            Assert::AreEqual ((size_t) 0, results.passedNeverRead, L"no requested sector is only passed over");
            Assert::AreEqual ((size_t) 0, results.requestsUnread,  L"every sector requested is counted as read");
        }

        TEST_METHOD (TheReadTestSeparatesReadSectorsOnAProDosBoot)
        {
            Results  results;



            Boot (GuestSession::RequireProDosUsersDisk(), true, results, [] (const Probe &) {});

            Assert::IsTrue   (results.readRequested > 100, L"the boot read ProDOS");
            Assert::AreEqual (results.readByLoader, results.readUnrequested, L"every read with no request found is the block 0 loader's, whose requests are not followed");
            Assert::AreEqual ((size_t) 0, results.passedNeverRead, L"no requested sector is only passed over");
            Assert::AreEqual ((size_t) 0, results.requestsUnread,  L"every sector requested is counted as read");
        }

    private:

        static constexpr const char * kpszMachine = "Apple2eEnhanced";

        static void Boot (const std::vector<Byte> & image, bool isProDos, Results & results, const std::function<void (const Probe &)> & check)
        {
            TestMachine  machine (kpszMachine);
            Probe        probe   (machine, isProDos);



            machine.GetDiskStore().SetFlushSink ([] (const std::string &, const std::vector<Byte> &) { return S_OK; });
            machine.SetDebugHook (&probe);
            machine.AttachObservers ({ &probe, nullptr, &probe, &probe.GetIoFilter() }, ObserverSlot::History);

            GuestSession::MountAndBoot (machine, image);

            machine.AttachObservers ({}, ObserverSlot::History);
            machine.SetDebugHook (nullptr);

            Analyze (probe, results);
            Report  (probe, results, isProDos ? "ProDOS" : "DOS 3.3");
            check   (probe);
        }

        //  Every address field with a good checksum, or with the checksum's
        //  first nibble unread, which the boot ROM does not check; and its
        //  data field when the CPU read it to its checksum nibble.
        static void FindFields (const std::vector<NibbleRead> & nibbles, std::vector<Field> & fields)
        {
            constexpr size_t  kAddressEnd = 11;
            size_t            count       = nibbles.size();



            for (size_t i = 0; i + kAddressEnd < count; i++)
            {
                Field  field;
                Byte   volume   = 0;
                Byte   checksum = 0;
                bool   isGood   = false;



                if (!IsPrologue (nibbles, i, 0x96) || !IsUnbroken (nibbles, i + 3, i + 8))
                {
                    continue;
                }

                volume                  = DiskFieldFormat::Decode44 (nibbles[i + 3].value, nibbles[i + 4].value);
                field.track             = DiskFieldFormat::Decode44 (nibbles[i + 5].value, nibbles[i + 6].value);
                field.sector            = DiskFieldFormat::Decode44 (nibbles[i + 7].value, nibbles[i + 8].value);
                checksum                = DiskFieldFormat::Decode44 (nibbles[i + 9].value, nibbles[i + 10].value);
                field.isChecksumSkipped = nibbles[i + 9].missed + nibbles[i + 10].missed == 1;
                isGood                  = checksum == DiskFieldFormat::ComputeAddressChecksum (volume, (Byte) field.track, (Byte) field.sector);

                if (!isGood && !field.isChecksumSkipped)
                {
                    continue;
                }

                field.at        = i;
                field.addressPc = nibbles[i].pc;

                FindData (nibbles, field);
                fields.push_back (field);
            }
        }

        static void FindData (const std::vector<NibbleRead> & nibbles, Field & field)
        {
            constexpr size_t  kBody   = DiskFieldFormat::kBodyLength62;
            size_t            count   = nibbles.size();
            size_t            first   = field.at + (field.isChecksumSkipped ? 10 : 11);
            size_t            dataAt  = SIZE_MAX;
            bool              isWhole = false;



            for (size_t j = first; j + 3 + kBody < count && j < first + kSearchLimit; j++)
            {
                if (IsPrologue (nibbles, j, 0x96))
                {
                    break;
                }

                if (IsPrologue (nibbles, j, 0xAD))
                {
                    dataAt = j;
                    break;
                }
            }

            isWhole = dataAt != SIZE_MAX && IsUnbroken (nibbles, dataAt + 1, dataAt + 3 + kBody);

            if (isWhole)
            {
                field.checksumPc = nibbles[dataAt + 3 + kBody].pc;
                field.isComplete = true;
            }
        }

        static bool IsUnbroken (const std::vector<NibbleRead> & nibbles, size_t first, size_t last)
        {
            bool  isUnbroken = true;



            for (size_t k = first; k <= last; k++)
            {
                if (nibbles[k].missed != 0)
                {
                    isUnbroken = false;
                    break;
                }
            }

            return isUnbroken;
        }

        static bool IsPrologue (const std::vector<NibbleRead> & nibbles, size_t at, Byte third)
        {
            return nibbles[at].value == 0xD5 && nibbles[at + 1].value == 0xAA && nibbles[at + 2].value == third
                && nibbles[at + 1].missed == 0 && nibbles[at + 2].missed == 0;
        }

        //  The request under way at a place in the stream: the last one made
        //  before it.
        static const Request * FindRequest (const std::vector<Request> & requests, size_t at)
        {
            const Request  * found = nullptr;



            for (const Request & request : requests)
            {
                if (request.at > at)
                {
                    break;
                }

                found = &request;
            }

            return found;
        }

        static bool IsRequested (const Request * request, const Field & field)
        {
            return request != nullptr && request->track == field.track
                && (request->physical[0] == field.sector || request->physical[1] == field.sector);
        }

        //  Where the request after this one was made.
        static size_t GetRequestEnd (const std::vector<Request> & requests, const Request & request)
        {
            size_t  index = (size_t) (&request - requests.data());



            return (index + 1 < requests.size()) ? requests[index + 1].at : SIZE_MAX;
        }

        //  A field after the one at `at`, before the next request, with the same
        //  track and sector, counted as read.
        static bool IsReadLater (const std::vector<Request> & requests, const std::vector<Field> & fields, size_t at, const Request & request)
        {
            size_t  end    = GetRequestEnd (requests, request);
            bool    isRead = false;



            for (size_t f = at + 1; f < fields.size() && fields[f].at < end; f++)
            {
                if (fields[f].IsRead() && fields[f].track == fields[at].track && fields[f].sector == fields[at].sector)
                {
                    isRead = true;
                    break;
                }
            }

            return isRead;
        }

        static bool IsSectorRead (const std::vector<Field> & fields, const Request & request, size_t end, int sector)
        {
            bool  isRead = sector < 0;



            for (const Field & field : fields)
            {
                if (field.at >= request.at && field.at < end && field.IsRead() && field.track == request.track && field.sector == sector)
                {
                    isRead = true;
                    break;
                }
            }

            return isRead;
        }

        //  Each sector in the request counted as read before the next
        //  request.
        static bool IsSatisfied (const std::vector<Request> & requests, const std::vector<Field> & fields, const Request & request)
        {
            size_t  end         = GetRequestEnd (requests, request);
            bool    isSatisfied = true;



            for (int sector : request.physical)
            {
                isSatisfied = isSatisfied && IsSectorRead (fields, request, end, sector);
            }

            return isSatisfied;
        }

        static void AddNote (Results & results, std::string && note)
        {
            if (results.notes.size() < kNoteLimit)
            {
                results.notes.push_back (std::move (note));
            }
        }

        static void Analyze (const Probe & probe, Results & results)
        {
            FindFields (probe.nibbles, results.fields);

            for (size_t f = 0; f < results.fields.size(); f++)
            {
                const Field    & field       = results.fields[f];
                const Request  * request     = FindRequest (probe.requests, field.at);
                bool             isRequested = IsRequested (request, field);
                bool             isByLoader  = field.addressPc >= kLoaderFirst && field.addressPc <= kLoaderLast;



                if (!field.isComplete)
                {
                    results.incomplete++;
                    continue;
                }

                if (field.IsRead() && isRequested)
                {
                    results.readRequested++;
                    continue;
                }

                if (field.IsRead())
                {
                    results.readUnrequested++;
                    results.readByLoader += isByLoader ? 1 : 0;
                    AddNote (results, std::format ("read, not requested: T{:02X} S{:X} prologue ${:04X} checksum ${:04X}; under way: {}",
                                                   field.track, field.sector, field.addressPc, field.checksumPc, FormatRequest (request)));
                    continue;
                }

                if (!isRequested)
                {
                    results.passedUnrequested++;
                    continue;
                }

                if (IsReadLater (probe.requests, results.fields, f, *request))
                {
                    results.passedThenRead++;
                    continue;
                }

                results.passedNeverRead++;
                AddNote (results, std::format ("passed over and not read, requested: T{:02X} S{:X} by ${:04X}; under way: {}",
                                               field.track, field.sector, field.addressPc, FormatRequest (request)));
            }

            for (const Request & request : probe.requests)
            {
                bool  isSatisfied = IsSatisfied (probe.requests, results.fields, request);



                if (request.command != kReadCommand)
                {
                    continue;
                }

                if (isSatisfied)
                {
                    results.requestsRead++;
                    continue;
                }

                results.requestsUnread++;
                AddNote (results, std::format ("requested and not counted as read: {}", FormatRequest (&request)));
            }
        }

        //  Each event against the last complete field before it with its track
        //  and sector: read, passed over, or neither.
        static void Classify (const std::vector<DiskEvent> & events, const std::vector<Field> & fields, size_t (& counts)[3])
        {
            for (const DiskEvent & event : events)
            {
                const Field  * found = nullptr;



                for (const Field & field : fields)
                {
                    if (field.at > event.at)
                    {
                        break;
                    }

                    if (field.isComplete && field.track == event.track && field.sector == event.sector)
                    {
                        found = &field;
                    }
                }

                counts[(found == nullptr) ? 2 : found->IsRead() ? 0 : 1]++;
            }
        }

        static std::string FormatRequest (const Request * request)
        {
            return (request == nullptr) ? std::string ("none")
                                        : std::format ("{} T{:02X} S{:X}{} command {}", request->source, request->track, request->physical[0],
                                                       (request->physical[1] >= 0) ? std::format ("+{:X}", request->physical[1]) : std::string(),
                                                       request->command);
        }

        //  For each pair of instructions that read a field's address prologue
        //  and its checksum nibble, how many fields, and how many were the
        //  sector in the request under way.
        static void ReportRoutines (const Probe & probe, const Results & results, std::string & text)
        {
            std::map<std::pair<Word, Word>, std::pair<size_t, size_t>>  routines;



            for (const Field & field : results.fields)
            {
                if (!field.isComplete)
                {
                    continue;
                }

                routines[{ field.addressPc, field.checksumPc }].first++;
                routines[{ field.addressPc, field.checksumPc }].second += IsRequested (FindRequest (probe.requests, field.at), field) ? 1 : 0;
            }

            for (const auto & [pcs, counts] : routines)
            {
                text += std::format ("  prologue read at ${:04X}, checksum at ${:04X}: {} fields, {} the sector requested\n", pcs.first, pcs.second, counts.first, counts.second);
            }
        }

        static void Report (const Probe & probe, const Results & results, const char * boot)
        {
            static constexpr std::string_view  kSources = "RLDP";
            std::string                        text;
            size_t                             sources[4] = {};
            size_t                             marks[3]   = {};
            size_t                             cuts[3]    = {};



            for (const Request & request : probe.requests)
            {
                sources[kSources.find (request.source) & 3]++;
            }

            Classify (probe.markReads, results.fields, marks);
            Classify (probe.cutOffs,   results.fields, cuts);

            text += std::format ("{}: {} nibbles read, {} address fields, {} data fields not read in full\n", boot, probe.nibbles.size(), results.fields.size(), results.incomplete);
            text += std::format ("  requests: {} boot ROM, {} RWTS at $3D00, {} RWTS at $BD00, {} driver; {} counted as read, {} not\n",
                                 sources[0], sources[1], sources[2], sources[3], results.requestsRead, results.requestsUnread);
            text += std::format ("  read: {} the sector requested, {} another ({} by the block 0 loader)\n", results.readRequested, results.readUnrequested, results.readByLoader);
            text += std::format ("  passed over: {} not requested, {} requested and read later, {} requested and not read\n",
                                 results.passedUnrequested, results.passedThenRead, results.passedNeverRead);
            text += std::format ("  Data read events: {} on a read field, {} on a field passed over, {} on neither\n", marks[0], marks[1], marks[2]);
            text += std::format ("  cut-off events: {} on a read field, {} on a field passed over, {} on neither\n", cuts[0], cuts[1], cuts[2]);
            text += std::format ("  entries at the $03D9 vector's target ${:04X}: {}\n", probe.vectorTarget, probe.vectorEntries);

            if (probe.lowVector.has_value())
            {
                text += std::format ("  the $03D9 vector's target at the first call of RWTS at $3D00: ${:04X}\n", *probe.lowVector);
            }

            ReportRoutines (probe, results, text);

            for (const auto & [target, count] : probe.rwtsCalls)
            {
                text += std::format ("  calls into RWTS's pages landing at ${:04X}: {}\n", target, count);
            }

            for (const auto & [caller, count] : probe.otherCallers)
            {
                text += std::format ("  calls into RWTS's pages elsewhere than its entry, from ${:04X}: {}\n", caller, count);
            }

            for (const auto & [caller, count] : probe.rdadrCallers)
            {
                text += std::format ("  RDADR16 called from ${:04X}: {}\n", caller, count);
            }

            for (const std::string & note : results.notes)
            {
                text += "  " + note + "\n";
            }

            Logger::WriteMessage (text.c_str());
        }
    };
}
