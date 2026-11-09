#include "Pch.h"

#include "InMemoryCassoLinkFolder.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/LinkAudioBootstrap.h"
#include "CassoLink/LinkFault.h"
#include "CassoLink/LinkProgram.h"
#include "Core/TextEncoding.h"
#include "Seams/FaultInjectingByteStream.h"
#include "Seams/InProcessByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  LinkFaultTests
//
//  Faults injected into a link: their text, the bytes they change, the
//  Apple's Fault command, and the cassette record.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (LinkFaultTests)
{
public:

    static constexpr DWORD  kWaitMs = 5000;

    //  The faults armed in this process, cleared when the test ends.
    struct Enabled
    {
        Enabled()  { LinkFaults::Enable(); }
        ~Enabled() { LinkFaults::Disable(); }
    };

    static LinkFault  ParseOne (const char * text)
    {
        LinkFault  fault;

        Assert::AreEqual (S_OK, LinkFault::ParseOne (text, fault), TextEncoding::NarrowToWide (text).c_str());
        return fault;
    }


    static std::vector<Byte>  ReadAll (InProcessByteStream & stream, size_t count)
    {
        std::vector<Byte>  bytes;
        Byte               chunk[256];
        size_t             got   = 0;
        auto               until = std::chrono::steady_clock::now() + std::chrono::milliseconds (kWaitMs);

        while (bytes.size() < count && std::chrono::steady_clock::now() < until)
        {
            Assert::AreEqual (S_OK, stream.Read (chunk, sizeof (chunk), 50, got));
            bytes.insert (bytes.end(), chunk, chunk + got);
        }

        return bytes;
    }



    TEST_METHOD (Parse_TakesEachForm)
    {
        std::vector<LinkFault>  faults;
        LinkFault               fault;



        fault = ParseOne ("pc:flip@40");
        Assert::IsTrue (fault.source == LinkFault::Source::Pc && fault.kind == LinkFault::Kind::Flip);
        Assert::AreEqual (40u, fault.offset);

        fault = ParseOne ("PC:Stall@$20:3");
        Assert::IsTrue (fault.kind == LinkFault::Kind::Stall);
        Assert::AreEqual (0x20u, fault.offset);
        Assert::AreEqual<int> (3, fault.quarters);

        fault = ParseOne ("pc:stall@7");
        Assert::AreEqual<int> (LinkFault::kDefaultQuarters, fault.quarters, L"a stall's default length");

        fault = ParseOne ("apple:drop@300#2");
        Assert::IsTrue (fault.source == LinkFault::Source::Apple && fault.kind == LinkFault::Kind::Drop);
        Assert::AreEqual (300u, fault.offset);
        Assert::AreEqual (2, fault.request);

        fault = ParseOne ("apple:unreadable@T17S3");
        Assert::IsTrue (fault.kind == LinkFault::Kind::Unreadable && fault.IsSectorFault());
        Assert::AreEqual<int> (17, fault.track);
        Assert::AreEqual<int> (3,  fault.sector);

        Assert::IsTrue (ParseOne ("apple:baddata@t0s15").kind == LinkFault::Kind::BadData);
        Assert::IsTrue (ParseOne ("apple:badcrc@T34S0").kind  == LinkFault::Kind::BadCrc);
        Assert::IsTrue (ParseOne ("bootstrap:drop@12").source == LinkFault::Source::Bootstrap);
        Assert::IsTrue (ParseOne ("cassette:flip@0x10").source == LinkFault::Source::Cassette);

        Assert::AreEqual (S_OK, LinkFault::Parse (" pc:flip@1 ; apple:badcrc@T1S2;", faults));
        Assert::AreEqual ((size_t) 2, faults.size(), L"spaces and an empty last entry are skipped");
    }



    TEST_METHOD (Parse_RejectsTextThatIsNotAFault)
    {
        static const char *  kBad[] =
        {
            "pc",                       // no kind or where
            "pc:flip",                  // no where
            "disk:flip@1",              // no such source
            "pc:melt@1",                // no such kind
            "pc:flip@",                 // no offset
            "pc:flip@-1",               // not a number
            "pc:flip@1#2",              // a request is the Apple's
            "pc:flip@1:4",              // a length is a stall's
            "pc:unreadable@T1S1",       // a sector is the Apple's
            "cassette:stall@1",         // the cassette's bytes are flipped or left out
            "apple:unreadable@T35S0",   // no track 35
            "apple:unreadable@T1S16",   // no sector 16
            "apple:unreadable@1",       // not a track and sector
            "apple:flip@$1000000",      // past the Apple's 24 bits
            "apple:stall@1:256",        // longer than a byte
        };

        std::vector<LinkFault>  faults;
        LinkFault               fault;



        for (const char * text : kBad)
        {
            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), LinkFault::ParseOne (text, fault), TextEncoding::NarrowToWide (text).c_str());
        }

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), LinkFault::Parse ("pc:flip@1;nonsense", faults));
    }



    TEST_METHOD (MakePayload_HasTheKindAndWhere)
    {
        std::vector<Byte>  byte   = LinkFault::MakePayload (ParseOne ("apple:stall@$123456:9"));
        std::vector<Byte>  sector = LinkFault::MakePayload (ParseOne ("apple:badcrc@T17S3"));



        Assert::IsTrue (byte   == std::vector<Byte> ({ 3, 0x56, 0x34, 0x12, 9 }), L"the offset least significant first, then the stall");
        Assert::IsTrue (sector == std::vector<Byte> ({ 6, 17, 3, 0, 0 }),          L"the track and the sector");
        Assert::AreEqual (LinkFault::kPayloadSize, byte.size());
    }



    TEST_METHOD (ByteFault_ChangesTheByteAtItsOffset_AcrossWrites)
    {
        const Byte         first[]  = { 0x10, 0x11, 0x12 };
        const Byte         second[] = { 0x20, 0x21, 0x22 };
        LinkByteFault      fault;
        std::vector<Byte>  out;
        size_t             stallAt  = 0;
        DWORD              stallMs  = 0;
        std::string        report;



        fault.Arm (ParseOne ("pc:flip@4"));
        fault.Apply (first, out, stallAt, stallMs, report);
        Assert::IsTrue (out == std::vector<Byte> ({ 0x10, 0x11, 0x12 }) && report.empty(), L"the first write is past");

        fault.Apply (second, out, stallAt, stallMs, report);
        Assert::IsTrue (out == std::vector<Byte> ({ 0x20, 0x20, 0x22 }), L"offset 4 is the second write's second byte");
        Assert::IsFalse (report.empty());
        Assert::IsFalse (fault.IsArmed(), L"once");

        fault.Apply (second, out, stallAt, stallMs, report);
        Assert::IsTrue (out == std::vector<Byte> ({ 0x20, 0x21, 0x22 }) && report.empty());

        fault.Arm (ParseOne ("pc:drop@1"));
        fault.Apply (first, out, stallAt, stallMs, report);
        Assert::IsTrue (out == std::vector<Byte> ({ 0x10, 0x12 }));

        fault.Arm (ParseOne ("pc:stall@2:2"));
        fault.Apply (first, out, stallAt, stallMs, report);
        Assert::IsTrue (out == std::vector<Byte> ({ 0x10, 0x11, 0x12 }));
        Assert::AreEqual ((size_t) 2, stallAt);
        Assert::AreEqual (2 * LinkFault::kQuarterMs, stallMs);
    }



    TEST_METHOD (FaultInjectingByteStream_WritesTheChangedBytes)
    {
        std::shared_ptr<InProcessByteStream>  writer;
        std::shared_ptr<InProcessByteStream>  reader;
        std::vector<std::string>              reports;
        const Byte                            bytes[] = { 1, 2, 3, 4 };



        InProcessByteStream::CreatePair (writer, reader);

        {
            FaultInjectingByteStream  stream (*writer, ParseOne ("bootstrap:flip@2"), [&] (const std::string & text) { reports.push_back (text); });

            Assert::AreEqual (S_OK, stream.Write (bytes, sizeof (bytes)));
            Assert::AreEqual (S_OK, stream.Write (bytes, sizeof (bytes)));
        }

        Assert::IsTrue (ReadAll (*reader, 8) == std::vector<Byte> ({ 1, 2, 2, 4, 1, 2, 3, 4 }));
        Assert::AreEqual ((size_t) 1, reports.size());
        Assert::IsTrue (reports[0].find ("bootstrap") != std::string::npos);
    }



    TEST_METHOD (LinkFaults_AreTakenBySource_OnlyWhileEnabled)
    {
        LinkFault  fault;



        LinkFaults::Disable();
        LinkFaults::Arm ({ ParseOne ("pc:flip@1") });
        Assert::IsFalse (LinkFaults::TryTake ({ LinkFault::Source::Pc }, fault), L"not taken while disabled");

        {
            Enabled  enabled;

            LinkFaults::Arm ({ ParseOne ("cassette:flip@3"), ParseOne ("apple:flip@4"), ParseOne ("pc:drop@5") });

            Assert::IsTrue (LinkFaults::TryTake ({ LinkFault::Source::Pc, LinkFault::Source::Apple }, fault));
            Assert::AreEqual (1u, fault.offset, L"the first armed, from before");

            Assert::IsTrue (LinkFaults::TryTake ({ LinkFault::Source::Pc, LinkFault::Source::Apple }, fault));
            Assert::IsTrue (fault.source == LinkFault::Source::Apple, L"the cassette's is left");

            Assert::IsTrue (LinkFaults::TryTake ({ LinkFault::Source::Cassette }, fault));
            Assert::AreEqual (3u, fault.offset);
        }

        Assert::IsFalse (LinkFaults::IsEnabled());
        Assert::IsFalse (LinkFaults::TryTake ({ LinkFault::Source::Pc }, fault), L"Disable clears the rest");
    }



    //  A cassette fault changes the byte, and the checksum is the one for
    //  the data before the change, so the monitor's check of it fails.
    TEST_METHOD (CassetteRecord_WithAFault_KeepsTheChecksumOfTheData)
    {
        constexpr size_t     kHalvesPerByte = 16;
        const Byte           data[]         = { 0x00, 0x5A, 0xFF };
        LinkFault            flip           = ParseOne ("cassette:flip@0");
        LinkFault            drop           = ParseOne ("cassette:drop@1");
        std::vector<double>  plain;
        std::vector<double>  flipped;
        std::vector<double>  dropped;
        size_t               first          = 0;



        LinkAudioBootstrap::AppendRecord (data, plain);
        LinkAudioBootstrap::AppendRecord (data, flipped, &flip);
        LinkAudioBootstrap::AppendRecord (data, dropped, &drop);

        Assert::AreEqual (plain.size(), flipped.size(), L"each bit is a cycle");

        //  The data starts after the leader and the two sync half-cycles.
        first = plain.size() - (std::size (data) + 1) * kHalvesPerByte - 1;

        Assert::IsFalse (std::equal (plain.begin() + (ptrdiff_t) first, plain.begin() + (ptrdiff_t) (first + kHalvesPerByte), flipped.begin() + (ptrdiff_t) first), L"the first byte changed");
        Assert::IsTrue  (std::equal (plain.end() - (ptrdiff_t) (kHalvesPerByte + 1), plain.end(), flipped.end() - (ptrdiff_t) (kHalvesPerByte + 1)), L"the checksum is not");
        Assert::AreEqual (plain.size() - kHalvesPerByte, dropped.size(), L"a byte left out");
    }



    TEST_METHOD (CassetteWav_HasTheLengthItPlaysFor)
    {
        std::vector<Byte>  wav;
        std::string        command;
        LinkShimOptions    options;
        DWORD              ms      = 0;



        Assert::AreEqual (S_OK, LinkAudioBootstrap::BuildWav (options, wav, command));

        ms = LinkAudioBootstrap::GetLengthMs (wav);
        Assert::IsTrue (ms > (DWORD) (LinkAudioBootstrap::kLeaderSeconds * 1000), std::format (L"{} ms, past the leader", ms).c_str());
        Assert::AreEqual ((DWORD) 0, LinkAudioBootstrap::GetLengthMs (std::vector<Byte> (16, 0)), L"not a WAV");
    }


    TEST_METHOD (ProgramWithFaults_AssemblesForBothChips)
    {
        std::vector<Byte>  plain;
        std::vector<Byte>  faults;
        std::vector<Byte>  standard;
        std::string        errors;



        for (LinkChip chip : { LinkChip::Acia, LinkChip::W5100 })
        {
            Assert::AreEqual (S_OK, LinkProgram::AssembleProgram (plain,    errors, chip),        TextEncoding::NarrowToWide (errors.c_str()).c_str());
            Assert::AreEqual (S_OK, LinkProgram::AssembleProgram (standard, errors, chip, false), TextEncoding::NarrowToWide (errors.c_str()).c_str());
            Assert::AreEqual (S_OK, LinkProgram::AssembleProgram (faults,   errors, chip, true),  TextEncoding::NarrowToWide (errors.c_str()).c_str());

            Assert::IsTrue (plain == standard,          L"without faults by default");
            Assert::IsTrue (faults.size() > plain.size(), L"the Fault command's code");
        }
    }



    //  The host's side of the Apple's fault: ERROR_NOT_SUPPORTED for a program with no
    //  Fault command, and otherwise sent ahead of the request it is for.
    TEST_METHOD (AppleFault_IsSentAheadOfItsRequest)
    {
        std::shared_ptr<InProcessByteStream>  hostEnd;
        std::shared_ptr<InProcessByteStream>  appleEnd;
        InMemoryCassoLinkFolder               folder;
        CassoLinkPacket                       packet;
        std::vector<Byte>                     buffer;



        InProcessByteStream::CreatePair (hostEnd, appleEnd);

        CassoLinkHost  host (hostEnd, folder, [] () { return std::array<Byte, 4> {}; });

        host.SetOperationTimeout (kWaitMs);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED), host.ArmFault (ParseOne ("apple:flip@5#1")), L"the program has no Fault command");
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED), host.ArmFault (ParseOne ("cassette:flip@5")), L"not a link's");

        host.SetFaultProgram (true);
        Assert::AreEqual (S_OK, host.ArmFault (ParseOne ("apple:flip@5#1")));

        auto  reading = std::async (std::launch::async, [&] ()
        {
            const Byte         sectors[] = { 0 };
            std::vector<Byte>  data;
            CassoLinkStatus    status;
            HRESULT            hr        = host.ReadSectors (1, 0, sectors, data, status);

            return SUCCEEDED (hr) ? host.ReadSectors (1, 0, sectors, data, status) : hr;
        });

        for (int request = 0; request < 2; request++)
        {
            SendReady (*appleEnd);
            packet = ReceivePacket (*appleEnd, buffer);

            if (request == 1)
            {
                Assert::AreEqual<int> (CassoLinkCodec::kFault, packet.command, L"the fault ahead of the second request");
                Assert::IsTrue (packet.payload == std::vector<Byte> ({ 1, 5, 0, 0, 0 }));

                SendReady (*appleEnd);
                packet = ReceivePacket (*appleEnd, buffer);
            }

            Assert::AreEqual<int> (CassoLinkCodec::kReadSectors, packet.command);
            SendSectorAndStatus (*appleEnd);
        }

        Assert::AreEqual (S_OK, reading.get());
    }



    //  The PC's fault changes a byte the host sends, counted from when it is
    //  armed.
    TEST_METHOD (PcFault_ChangesAByteTheHostSends)
    {
        std::shared_ptr<InProcessByteStream>  hostEnd;
        std::shared_ptr<InProcessByteStream>  appleEnd;
        InMemoryCassoLinkFolder               folder;
        std::vector<Byte>                     expected;
        std::vector<Byte>                     sent;



        InProcessByteStream::CreatePair (hostEnd, appleEnd);

        CassoLinkHost  host (hostEnd, folder, [] () { return std::array<Byte, 4> {}; });

        host.SetOperationTimeout (kWaitMs);
        Assert::AreEqual (S_OK, host.ArmFault (ParseOne ("pc:flip@4")));
        Assert::AreEqual (S_OK, CassoLinkCodec::Encode (CassoLinkCodec::kLabel, std::vector<Byte> ({ 'A', 'B', 'C' }), expected));

        auto  labeling = std::async (std::launch::async, [&] () { return host.SendLabel ("abc"); });

        SendReady (*appleEnd);
        Assert::AreEqual (S_OK, labeling.get());

        sent = ReadAll (*appleEnd, expected.size());
        Assert::AreEqual (expected.size(), sent.size());

        expected[4] ^= 0x01;
        Assert::IsTrue (sent == expected, L"the fifth byte the host sent, flipped");
    }



    static void  SendReady (InProcessByteStream & appleEnd)
    {
        std::vector<Byte>  wire;
        std::vector<Byte>  payload = { (Byte) (LinkProgram::kProgramVersion & 0xFF), (Byte) (LinkProgram::kProgramVersion >> 8), LinkProgram::kShimVersion, 0x03 };



        Assert::AreEqual (S_OK, CassoLinkCodec::Encode (CassoLinkCodec::kReady, payload, wire));
        Assert::AreEqual (S_OK, appleEnd.Write (wire.data(), wire.size()));
    }

    static void  SendSectorAndStatus (InProcessByteStream & appleEnd)
    {
        std::vector<Byte>  wire;
        std::vector<Byte>  data (CassoLinkHost::kSectorSize, 0x5A);



        Assert::AreEqual (S_OK, CassoLinkCodec::EncodeSectorData (0, 0, data, wire));
        Assert::AreEqual (S_OK, appleEnd.Write (wire.data(), wire.size()));

        wire.clear();
        Assert::AreEqual (S_OK, CassoLinkCodec::Encode (CassoLinkCodec::kStatus, std::vector<Byte> ({ 1, 1, 0, 1, CassoLinkHost::kErrorNone }), wire));
        Assert::AreEqual (S_OK, appleEnd.Write (wire.data(), wire.size()));
    }

    static CassoLinkPacket  ReceivePacket (InProcessByteStream & appleEnd, std::vector<Byte> & buffer)
    {
        CassoLinkPacket  packet;
        size_t           consumed = 0;
        bool             complete = false;
        Byte             chunk[512];
        size_t           got      = 0;
        auto             until    = std::chrono::steady_clock::now() + std::chrono::milliseconds (kWaitMs);



        for (;;)
        {
            Assert::AreEqual (S_OK, CassoLinkCodec::Decode (buffer, consumed, packet, complete));

            if (complete)
            {
                buffer.erase (buffer.begin(), buffer.begin() + (ptrdiff_t) consumed);
                return packet;
            }

            Assert::IsTrue (std::chrono::steady_clock::now() < until, L"a packet from the host");
            Assert::AreEqual (S_OK, appleEnd.Read (chunk, sizeof (chunk), 50, got));
            buffer.insert (buffer.end(), chunk, chunk + got);
        }
    }
};
