#include "Pch.h"

#include "../EhmTestHelper.h"

#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/LinkProgram.h"
#include "CassoLink/Session/CaptureStream.h"
#include "CassoLink/Session/CassoLinkCapture.h"
#include "CassoLink/Session/CassoLinkThirteenSector.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/ThirteenSectorCodec.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskFieldFormat.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/Stamp/FormatStamp.h"
#include "Devices/Disk/Stamp/FormatStampChunk.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CaptureProgramTests
//
//  The capture's encoding and the analysis of its reads, on reads built in
//  memory. The capture module on an emulated //c is tested in
//  CaptureProgramScenarioTests.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CaptureProgramTests)
{
public:

    //  A 16-sector data field's nibbles, its checksum nibble last.
    static void EncodeDataField (const std::vector<Byte> & data, std::vector<Byte> & outNibbles)
    {
        Byte  checksum = 0;



        DiskFieldFormat::EncodeData (DiskFieldKind::Sixteen, data, outNibbles, checksum);
        outNibbles.push_back (checksum);
    }



    static bool DoesDataFieldDecode (const Byte * nibbles)
    {
        DataFieldDecode  decode;



        DiskFieldFormat::DecodeData (DiskFieldKind::Sixteen, std::span<const Byte> (nibbles, DiskFieldFormat::kBodyLength62), nibbles[DiskFieldFormat::kBodyLength62], decode);

        return decode.isChecksumGood && decode.invalidNibbles.empty();
    }


    //  A WOZ's track 0 as its bytes, each with the zero bits after it.
    static std::vector<std::pair<Byte, size_t>>  FrameTrack (const std::vector<Byte> & woz)
    {
        DiskImage                             image;
        std::vector<std::pair<Byte, size_t>>  framed;
        int                                   slot  = 0;
        size_t                                count = 0;
        size_t                                at    = 0;



        AssertSucceeded (WozLoader::Load (woz, image));

        slot  = image.ResolveQuarterTrack (0);
        count = image.GetTrackBitCount (slot);

        while (at < count && image.ReadBit (slot, at) == 0)
        {
            at++;
        }

        while (at + 8 <= count)
        {
            Byte    value = 0;
            size_t  zeros = 0;

            for (int bit = 0; bit < 8; bit++)
            {
                value = (Byte) ((value << 1) | image.ReadBit (slot, at++));
            }

            while (at < count && image.ReadBit (slot, at) == 0)
            {
                zeros++;
                at++;
            }

            framed.push_back ({ value, zeros });
        }

        return framed;
    }


    //  Each token: values as they are, a repeat, and a copy of a revolution
    //  before, then the marks, with the padding after the last ignored. The
    //  copy's values are the same, and its marks aren't.
    TEST_METHOD (Decode_TokensGiveTheValues_AndTheMarksTheTopBits)
    {
        const Byte         encoded[]  = { 3, 0, 0xD5, 0xAA, 0x96, 0x00, 0x81, 0xFF, 0x00, 0x02, 0xEF, 0x60, 0x00, 0x00 };
        const Byte         expected[] = { 0xD5, 0xAA, 0x96, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0xFF, 0xFF };
        std::vector<Byte>  bytes;



        Assert::IsTrue  (CaptureStream::Decode (encoded, std::size (expected), bytes));
        Assert::IsTrue  (bytes == std::vector<Byte> (std::begin (expected), std::end (expected)));
        Assert::IsFalse (CaptureStream::Decode (encoded, std::size (expected) + 3, bytes), L"Too few values");

        const Byte         early[] = { 3, 0, 0x00, 0x01 };

        Assert::IsFalse (CaptureStream::Decode (early, 3, bytes), L"A copy from before the first value");

        const Byte         noMarks[] = { 3, 0, 0xD5, 0xAA };

        Assert::IsFalse (CaptureStream::Decode (noMarks, 2, bytes), L"No marks");
    }


    //  The module fits below the loop's copies, which capbuild writes from
    //  CAPCODE.
    TEST_METHOD (TheCaptureModule_FitsBelowItsLoopsCopies)
    {
        static constexpr size_t  kRoom = 0x5470 - 0x5000;

        std::vector<Byte>  module;
        std::string        errors;
        HRESULT            hr     = S_OK;



        hr = LinkProgram::AssembleModule (LinkProgram::GetCaptureModuleSource(), module, errors);
        Assert::IsTrue (SUCCEEDED (hr), TextEncoding::NarrowToWide (errors).c_str());
        Assert::IsTrue (module.size() <= kRoom, std::format (L"{} bytes", module.size()).c_str());
    }


    //  A track of disk bytes with a C0 in it, as a read at a write splice
    //  has, built into a WOZ: no run of zero bits is longer than the three
    //  a controller reads reliably.
    TEST_METHOD (ASpliceByte_IsWrittenWithNoLongRunOfZeros)
    {
        static constexpr size_t  kRevolution = 6000;
        static constexpr size_t  kReadBytes  = 14000;
        static constexpr size_t  kMaxZeros   = 3;

        std::vector<Byte>                        revolution;
        std::vector<CassoLinkCapture::Position>  positions (1);
        CassoLinkCapture::Result                 result;
        DiskImage                                image;
        uint32_t                                 seed    = 12345;
        size_t                                   run     = 0;
        size_t                                   longest = 0;



        for (size_t at = 0; at < kRevolution; at++)
        {
            seed = seed * 1103515245u + 12345u;
            revolution.push_back (s_kDiskBytes[(seed >> 16) % std::size (s_kDiskBytes)]);
        }

        revolution[kRevolution / 2] = 0xC0;

        for (Byte delay : CassoLinkCapture::kWholeDelays)
        {
            std::vector<Byte>  read;

            for (size_t at = 0; at < kReadBytes; at++)
            {
                read.push_back (revolution[at % kRevolution]);
            }

            positions[0].reads.push_back (read);
            positions[0].delays.push_back (delay);
        }

        AssertSucceeded (CassoLinkCapture::BuildWoz (positions, 0, "2026-11-12T00:00:00Z", 1, result));
        AssertSucceeded (WozLoader::Load (result.woz, image));

        int  slot = image.ResolveQuarterTrack (0);

        Assert::IsTrue (image.GetTrackBitCount (slot) > 0, L"Track 0 has a stream");

        for (size_t at = 0; at < image.GetTrackBitCount (slot); at++)
        {
            run     = image.ReadBit (slot, at) != 0 ? 0 : run + 1;
            longest = std::max (longest, run);
        }

        Assert::IsTrue (longest <= kMaxZeros);
    }


    //  A track whose address fields end in a checksum of $FF, read with
    //  its sync bytes marked as two zero bits follow them: the sync bytes
    //  are written with two zero bits, and the checksum's FF FF with none,
    //  so a read of the field doesn't read an FF twice.
    TEST_METHOD (AnAddressFieldsFFs_AreWrittenWithNoZeroBits)
    {
        static constexpr size_t  kSectors   = 16;
        static constexpr size_t  kSyncs     = 6;
        static constexpr size_t  kDataBytes = 343;
        static constexpr size_t  kReadBytes = 14000;
        static constexpr Byte    kMarked    = 0x7F;

        std::vector<Byte>                        revolution;
        std::vector<bool>                        sync;
        std::vector<CassoLinkCapture::Position>  positions (1);
        CassoLinkCapture::Result                 result;
        DiskImage                                image;
        std::vector<std::pair<Byte, size_t>>     framed;
        uint32_t                                 seed    = 12345;
        size_t                                   checked = 0;



        auto  add = [&revolution, &sync] (std::initializer_list<Byte> values, bool isSync)
        {
            for (Byte value : values)
            {
                revolution.push_back (value);
                sync.push_back (isSync);
            }
        };

        for (size_t sector = 0; sector < kSectors; sector++)
        {
            for (size_t each = 0; each < kSyncs; each++)
            {
                add ({ 0xFF }, true);
            }

            add ({ 0xD5, 0xAA, 0x96, 0xFF, 0xFE, 0xAA, 0xAA, (Byte) (0xAA | (sector >> 1)), (Byte) (0xAA | sector), 0xFF, 0xFF, 0xDE, 0xAA, 0xEB }, false);

            for (size_t each = 0; each < kSyncs; each++)
            {
                add ({ 0xFF }, true);
            }

            add ({ 0xD5, 0xAA, 0xAD }, false);

            for (size_t each = 0; each < kDataBytes; each++)
            {
                seed = seed * 1103515245u + 12345u;
                add ({ s_kDiskBytes[(seed >> 16) % (std::size (s_kDiskBytes) - 1)] }, false);
            }

            add ({ 0xDE, 0xAA, 0xEB }, false);
        }

        for (Byte delay : CassoLinkCapture::kWholeDelays)
        {
            std::vector<Byte>  read;

            for (size_t at = 0; at < kReadBytes; at++)
            {
                size_t  in = at % revolution.size();

                read.push_back ((delay == 1 && sync[in]) ? kMarked : revolution[in]);
            }

            positions[0].reads.push_back (read);
            positions[0].delays.push_back (delay);
        }

        AssertSucceeded (CassoLinkCapture::BuildWoz (positions, 0, "2026-11-12T00:00:00Z", 1, result));
        AssertSucceeded (WozLoader::Load (result.woz, image));

        int     slot  = image.ResolveQuarterTrack (0);
        size_t  count = image.GetTrackBitCount (slot);
        size_t  at    = 0;

        while (at < count && image.ReadBit (slot, at) == 0)
        {
            at++;
        }

        while (at + 8 <= count)
        {
            Byte    value = 0;
            size_t  zeros = 0;

            for (int bit = 0; bit < 8; bit++)
            {
                value = (Byte) ((value << 1) | image.ReadBit (slot, at++));
            }

            while (at < count && image.ReadBit (slot, at) == 0)
            {
                zeros++;
                at++;
            }

            framed.push_back ({ value, zeros });
        }

        for (size_t each = 1; each + 11 < framed.size(); each++)
        {
            if (framed[each].first == 0xD5 && framed[each + 1].first == 0xAA && framed[each + 2].first == 0x96)
            {
                Assert::IsTrue (framed[each - 1].second >= 2, L"The sync byte before the field");
                Assert::AreEqual ((size_t) 0, framed[each + 9].second,  L"The checksum's first FF");
                Assert::AreEqual ((size_t) 0, framed[each + 10].second, L"The checksum's second FF");
                checked++;
            }
        }

        Assert::IsTrue (checked > 0, L"The track has address fields");
    }


    //  A track with Ultima IV's D7 AA 96 address fields, each after a data
    //  field whose last nibbles are DD, a checksum, DE AA EB and sync bytes,
    //  which pass for an address field ending at the D7. Every read at 8
    //  cycles marks the D7, as a //c can by chance: the field is written with
    //  no zero bits inside it, so a read of the field doesn't read a byte twice.
    TEST_METHOD (AnAddressFieldsMarkedBytes_AreWrittenWithNoZeroBits)
    {
        static constexpr size_t  kSectors   = 16;
        static constexpr size_t  kSyncs     = 6;
        static constexpr size_t  kDataBytes = 343;
        static constexpr size_t  kReadBytes = 14000;
        static constexpr size_t  kFieldSize = 11;

        std::vector<Byte>                        revolution;
        std::vector<bool>                        marked;
        std::vector<CassoLinkCapture::Position>  positions (1);
        CassoLinkCapture::Result                 result;
        DiskImage                                image;
        std::vector<std::pair<Byte, size_t>>     framed;
        uint32_t                                 seed    = 12345;
        size_t                                   checked = 0;



        auto  add = [&revolution, &marked] (std::initializer_list<Byte> values, bool isMarked)
        {
            for (Byte value : values)
            {
                revolution.push_back (value);
                marked.push_back (isMarked);
            }
        };

        for (size_t sector = 0; sector < kSectors; sector++)
        {
            for (size_t each = 0; each < kSyncs; each++)
            {
                add ({ 0xFF }, false);
            }

            add ({ 0xD7 }, true);
            add ({ 0xAA, 0x96, 0xAA, 0xAA, 0xAB, 0xBA, (Byte) (0xAA | (sector >> 1)), (Byte) (0xAA | sector) }, false);
            add ({ (Byte) (0xAA | ((0x12 ^ sector) >> 1)), (Byte) (0xAA | (0x12 ^ sector)) }, false);
            add ({ 0xDE, 0xAA, 0xEB }, false);

            for (size_t each = 0; each < kSyncs; each++)
            {
                add ({ 0xFF }, false);
            }

            add ({ 0xD5, 0xAA, 0xAD }, false);

            for (size_t each = 0; each + 2 < kDataBytes; each++)
            {
                seed = seed * 1103515245u + 12345u;
                add ({ s_kDiskBytes[(seed >> 16) % (std::size (s_kDiskBytes) - 1)] }, false);
            }

            add ({ 0xDD, 0xAB, 0xDE, 0xAA, 0xEB }, false);
        }

        for (Byte delay : CassoLinkCapture::kWholeDelays)
        {
            std::vector<Byte>  read;

            for (size_t at = 0; at < kReadBytes; at++)
            {
                size_t  in = at % revolution.size();

                read.push_back ((delay == 0 && marked[in]) ? (Byte) (revolution[in] & 0x7F) : revolution[in]);
            }

            positions[0].reads.push_back (read);
            positions[0].delays.push_back (delay);
        }

        AssertSucceeded (CassoLinkCapture::BuildWoz (positions, 0, "2026-11-23T00:00:00Z", 1, result));
        AssertSucceeded (WozLoader::Load (result.woz, image));

        int     slot  = image.ResolveQuarterTrack (0);
        size_t  count = image.GetTrackBitCount (slot);
        size_t  at    = 0;

        while (at < count && image.ReadBit (slot, at) == 0)
        {
            at++;
        }

        while (at + 8 <= count)
        {
            Byte    value = 0;
            size_t  zeros = 0;

            for (int bit = 0; bit < 8; bit++)
            {
                value = (Byte) ((value << 1) | image.ReadBit (slot, at++));
            }

            while (at < count && image.ReadBit (slot, at) == 0)
            {
                zeros++;
                at++;
            }

            framed.push_back ({ value, zeros });
        }

        for (size_t each = 0; each + kFieldSize < framed.size(); each++)
        {
            if (framed[each].first == 0xD7 && framed[each + 1].first == 0xAA && framed[each + 2].first == 0x96)
            {
                for (size_t k = 0; k < kFieldSize; k++)
                {
                    Assert::AreEqual ((size_t) 0, framed[each + k].second, L"Zero bits inside the address field");
                }

                checked++;
            }
        }

        Assert::IsTrue (checked > 0, L"The track has address fields");
    }


    //  A track as Karateka's track 27 is: sync bytes, then one long field of
    //  pages that each end in a B7 followed by one zero bit, read as a //c's
    //  IWM marks bytes. At 8 cycles it marks a byte with no zero bits after
    //  it one time in ten, the B7 four times in five, and the sync bytes
    //  nearly always. At 10 cycles it marks the B7 three times in eight and
    //  a byte with no zero bits never, but for one catch of every 500th
    //  byte here. Every B7 is written with a zero bit after it, and no other
    //  byte of the field is.
    TEST_METHOD (ALoudTracksMarkedBytes_AreWrittenWithAZeroBit)
    {
        static constexpr size_t  kSyncs     = 40;
        static constexpr size_t  kPages     = 11;
        static constexpr size_t  kPageBytes = 512;
        static constexpr size_t  kReadBytes = 12000;
        static constexpr int     kReads     = CassoLinkCapture::kWholeReads + 2 * CassoLinkCapture::kLoudReads;
        static constexpr size_t  kStrayStep = 500;

        std::vector<Byte>                        revolution;
        std::vector<int>                         share8;
        std::vector<int>                         share12;
        std::vector<int>                         share10;
        std::vector<CassoLinkCapture::Position>  positions (1);
        CassoLinkCapture::Result                 result;
        DiskImage                                image;
        std::vector<std::pair<Byte, size_t>>     framed;
        uint32_t                                 seed    = 4321;
        size_t                                   ends    = 0;
        int                                      stray   = 0;



        auto  next = [&seed] () { seed = seed * 1103515245u + 12345u; return (int) ((seed >> 16) % 100); };

        auto  add = [&revolution, &share8, &share12, &share10] (Byte value, int at8, int at12, int at10)
        {
            revolution.push_back (value);
            share8.push_back (at8);
            share12.push_back (at12);
            share10.push_back (at10);
        };

        for (size_t each = 0; each < kSyncs; each++)
        {
            add (0xFF, 98, 56, 94);
        }

        add (0xAF, 10, 0, 0);
        add (0xB5, 10, 0, 0);
        add (0xB6, 10, 0, 0);

        for (size_t page = 0; page < kPages; page++)
        {
            for (size_t each = 0; each < kPageBytes; each++)
            {
                seed = seed * 1103515245u + 12345u;
                add (s_kDiskBytes[(seed >> 16) % (std::size (s_kDiskBytes) - 1)], 10, 0, 0);
            }

            add (0xB7, 80, 10, 38);
        }

        for (int each = 0; each < kReads; each++)
        {
            bool               loud  = each >= CassoLinkCapture::kWholeReads;
            Byte               delay = loud ? CassoLinkCapture::kLoudDelay : CassoLinkCapture::kWholeDelays[each];
            std::vector<Byte>  read;

            for (size_t at = 0; at < kReadBytes; at++)
            {
                size_t  in     = at % revolution.size();
                bool    stray  = each == CassoLinkCapture::kWholeReads && at < revolution.size() && in % kStrayStep == kStrayStep / 2;
                bool    marked = stray || next() < (loud ? share10[in] : (delay == 0 ? share8[in] : share12[in]));

                read.push_back (marked ? (Byte) (revolution[in] & 0x7F) : revolution[in]);
            }

            positions[0].reads.push_back (read);
            positions[0].delays.push_back (delay);
        }

        AssertSucceeded (CassoLinkCapture::BuildWoz (positions, 0, "2026-11-27T00:00:00Z", 1, result));
        AssertSucceeded (WozLoader::Load (result.woz, image));

        int     slot  = image.ResolveQuarterTrack (0);
        size_t  count = image.GetTrackBitCount (slot);
        size_t  at    = 0;

        while (at < count && image.ReadBit (slot, at) == 0)
        {
            at++;
        }

        while (at + 8 <= count)
        {
            Byte    value = 0;
            size_t  zeros = 0;

            for (int bit = 0; bit < 8; bit++)
            {
                value = (Byte) ((value << 1) | image.ReadBit (slot, at++));
            }

            while (at < count && image.ReadBit (slot, at) == 0)
            {
                zeros++;
                at++;
            }

            framed.push_back ({ value, zeros });
        }

        for (const auto & [value, zeros] : framed)
        {
            ends  += (value == 0xB7 && zeros > 0) ? 1 : 0;
            stray += (value != 0xB7 && value != 0xFF && zeros > 0) ? 1 : 0;
        }

        Assert::AreEqual (kPages, ends, L"Every B7 is written with a zero bit after it");
        Assert::AreEqual (0, stray, L"No other byte of the field is");
    }


    //  A loud track's bytes are counted from its reads at 10 cycles, inside
    //  an address field and after one too. Each page here ends in a field
    //  that passes for an address field. Every read at 8 cycles marks the
    //  first byte of its ending, as a //c can by chance, and no read at 10
    //  does: the byte is written with no zero bits. The field's AE is
    //  followed by one zero bit and marked as the B7 of Karateka's track 27
    //  is: it is written with one.
    TEST_METHOD (ALoudTracksAddressFields_AreCountedFromTheReadsAt10Cycles)
    {
        static constexpr size_t  kSyncs     = 40;
        static constexpr size_t  kPages     = 11;
        static constexpr size_t  kPageBytes = 500;
        static constexpr size_t  kReadBytes = 12000;
        static constexpr int     kReads     = CassoLinkCapture::kWholeReads + 2 * CassoLinkCapture::kLoudReads;
        static constexpr size_t  kZeroAt    = 5;
        static constexpr size_t  kEndingAt  = 11;

        std::vector<Byte>                        revolution;
        std::vector<int>                         share8;
        std::vector<int>                         share12;
        std::vector<int>                         share10;
        std::vector<CassoLinkCapture::Position>  positions (1);
        CassoLinkCapture::Result                 result;
        std::vector<std::pair<Byte, size_t>>     framed;
        uint32_t                                 seed    = 9876;
        size_t                                   fields  = 0;



        auto  next = [&seed] () { seed = seed * 1103515245u + 12345u; return (int) ((seed >> 16) % 100); };

        auto  add = [&revolution, &share8, &share12, &share10] (Byte value, int at8, int at12, int at10)
        {
            revolution.push_back (value);
            share8.push_back (at8);
            share12.push_back (at12);
            share10.push_back (at10);
        };

        for (size_t each = 0; each < kSyncs; each++)
        {
            add (0xFF, 98, 56, 94);
        }

        for (size_t page = 0; page < kPages; page++)
        {
            for (size_t each = 0; each < kPageBytes; each++)
            {
                seed = seed * 1103515245u + 12345u;
                add (s_kDiskBytes[(seed >> 16) % (std::size (s_kDiskBytes) - 1)], 10, 0, 0);
            }

            for (Byte value : { 0xD5, 0xAA, 0xBE, 0xAA, 0xAB })
            {
                add (value, 10, 0, 0);
            }

            add (0xAE, 80, 10, 38);

            for (Byte value : { 0xAF, 0xBA, 0xBB, 0xBE, 0xEA })
            {
                add (value, 10, 0, 0);
            }

            add (0xED, 100, 0, 0);
            add (0xEC, 10, 0, 0);
        }

        for (int each = 0; each < kReads; each++)
        {
            bool               loud  = each >= CassoLinkCapture::kWholeReads;
            Byte               delay = loud ? CassoLinkCapture::kLoudDelay : CassoLinkCapture::kWholeDelays[each];
            std::vector<Byte>  read;

            for (size_t at = 0; at < kReadBytes; at++)
            {
                size_t  in     = at % revolution.size();
                bool    marked = next() < (loud ? share10[in] : (delay == 0 ? share8[in] : share12[in]));

                read.push_back (marked ? (Byte) (revolution[in] & 0x7F) : revolution[in]);
            }

            positions[0].reads.push_back (read);
            positions[0].delays.push_back (delay);
        }

        AssertSucceeded (CassoLinkCapture::BuildWoz (positions, 0, "2026-11-27T00:00:00Z", 1, result));

        framed = FrameTrack (result.woz);

        for (size_t each = 0; each + kEndingAt < framed.size(); each++)
        {
            if (framed[each].first == 0xD5 && framed[each + 1].first == 0xAA && framed[each + 2].first == 0xBE && framed[each + kEndingAt].first == 0xED)
            {
                Assert::AreEqual ((size_t) 1, framed[each + kZeroAt].second, L"The AE is written with its zero bit");
                Assert::AreEqual ((size_t) 0, framed[each + kEndingAt].second, L"The ending's byte is written with no zero bits");

                fields++;
            }
        }

        Assert::AreEqual (kPages, fields, L"Every field is checked");
    }


    //  A read has about two turns and a fifth, and only its whole turns are
    //  aligned. A B7 followed by one zero bit, which the reads at 10 cycles
    //  mark only in their last, part turn, is written with its zero bit: a
    //  part turn's marks count too.
    TEST_METHOD (ALoudTracksPartTurns_AreCountedToo)
    {
        static constexpr size_t  kSyncs     = 40;
        static constexpr size_t  kPages     = 11;
        static constexpr size_t  kPageBytes = 512;
        static constexpr size_t  kReadBytes = 12000;
        static constexpr int     kReads     = CassoLinkCapture::kWholeReads + 2 * CassoLinkCapture::kLoudReads;

        std::vector<Byte>                        revolution;
        std::vector<CassoLinkCapture::Position>  positions (1);
        CassoLinkCapture::Result                 result;
        std::vector<std::pair<Byte, size_t>>     framed;
        uint32_t                                 seed    = 2468;
        size_t                                   target  = 0;
        size_t                                   ends    = 0;



        auto  next = [&seed] () { seed = seed * 1103515245u + 12345u; return (int) ((seed >> 16) % 100); };

        revolution.assign (kSyncs, 0xFF);
        revolution.insert (revolution.end(), { 0xAF, 0xB5, 0xB6 });

        for (size_t page = 0; page < kPages; page++)
        {
            for (size_t each = 0; each < kPageBytes; each++)
            {
                seed = seed * 1103515245u + 12345u;
                revolution.push_back (s_kDiskBytes[(seed >> 16) % (std::size (s_kDiskBytes) - 1)]);
            }

            target = (page == 0) ? revolution.size() : target;
            revolution.push_back (page == 0 ? 0xB7 : 0xB6);
        }

        for (int each = 0; each < kReads; each++)
        {
            bool               loud  = each >= CassoLinkCapture::kWholeReads;
            Byte               delay = loud ? CassoLinkCapture::kLoudDelay : CassoLinkCapture::kWholeDelays[each];
            std::vector<Byte>  read;

            for (size_t at = 0; at < kReadBytes; at++)
            {
                size_t  in     = at % revolution.size();
                bool    sync   = in < kSyncs;
                int     share  = sync ? (loud ? 94 : (delay == 0 ? 98 : 56)) : ((loud || delay != 0) ? 0 : 10);
                bool    marked = next() < share || (loud && in == target && at >= 2 * revolution.size());

                read.push_back (marked ? (Byte) (revolution[in] & 0x7F) : revolution[in]);
            }

            positions[0].reads.push_back (read);
            positions[0].delays.push_back (delay);
        }

        AssertSucceeded (CassoLinkCapture::BuildWoz (positions, 0, "2026-11-27T00:00:00Z", 1, result));

        framed = FrameTrack (result.woz);

        for (const auto & [value, zeros] : framed)
        {
            ends += (value == 0xB7 && zeros == 1) ? 1 : 0;
        }

        Assert::IsTrue (ends >= 1, L"The B7 is written with its zero bit");
    }


    //  A track whose address fields end in AF, as Ultima IV's track 0 does,
    //  with the bytes after it read otherwise on one of three reads, as a
    //  drive that reads a weak stretch steadily does: the bytes after the
    //  ending's first are written weak, so they differ between reads, as a
    //  protection's check requires.
    TEST_METHOD (AProtectedEndingsBytes_ReadOtherwiseOnce_AreWeak)
    {
        static constexpr size_t  kSectors       = 16;
        static constexpr size_t  kSyncs         = 6;
        static constexpr size_t  kDataBytes     = 343;
        static constexpr size_t  kReadBytes     = 14000;
        static constexpr size_t  kTail          = 4;
        static constexpr Byte    kSteady[kTail] = { 0xA4, 0xBF, 0xC8, 0xD3 };
        static constexpr Byte    kOther[kTail]  = { 0xA4, 0x9F, 0xE4, 0xB3 };

        std::vector<Byte>                        revolution;
        std::vector<Byte>                        other;
        std::vector<CassoLinkCapture::Position>  positions (1);
        CassoLinkCapture::Result                 result;
        DiskImage                                image;
        std::vector<Byte>                        framed;
        uint32_t                                 seed    = 12345;
        size_t                                   checked = 0;
        size_t                                   read    = 0;



        auto  add = [&revolution, &other] (std::initializer_list<Byte> values)
        {
            for (Byte value : values)
            {
                revolution.push_back (value);
                other.push_back (value);
            }
        };

        for (size_t sector = 0; sector < kSectors; sector++)
        {
            for (size_t each = 0; each < kSyncs; each++)
            {
                add ({ 0xFF });
            }

            add ({ 0xD5, 0xAA, 0x96, 0xAA, 0xAA, 0xAA, 0xAA, (Byte) (0xAA | (sector >> 1)), (Byte) (0xAA | sector), (Byte) (0xAA | (sector >> 1)), (Byte) (0xAA | sector), 0xAF });

            for (size_t each = 0; each < kTail; each++)
            {
                revolution.push_back (kSteady[each]);
                other.push_back (kOther[each]);
            }

            for (size_t each = 0; each < kSyncs; each++)
            {
                add ({ 0xFF });
            }

            add ({ 0xD5, 0xAA, 0xAD });

            for (size_t each = 0; each < kDataBytes; each++)
            {
                seed = seed * 1103515245u + 12345u;
                add ({ s_kDiskBytes[(seed >> 16) % (std::size (s_kDiskBytes) - 1)] });
            }

            add ({ 0xDE, 0xAA, 0xEB });
        }

        for (Byte delay : CassoLinkCapture::kWholeDelays)
        {
            const std::vector<Byte>  & source = (read == 1) ? other : revolution;
            std::vector<Byte>          bytes;

            for (size_t at = 0; at < kReadBytes; at++)
            {
                bytes.push_back (source[at % source.size()]);
            }

            positions[0].reads.push_back (bytes);
            positions[0].delays.push_back (delay);
            read++;
        }

        AssertSucceeded (CassoLinkCapture::BuildWoz (positions, 0, "2026-11-24T00:00:00Z", 1, result));
        AssertSucceeded (WozLoader::Load (result.woz, image));

        int     slot  = image.ResolveQuarterTrack (0);
        size_t  count = image.GetTrackBitCount (slot);
        size_t  at    = 0;
        Byte    value = 0;

        while (at < count)
        {
            value = (Byte) ((value << 1) | image.ReadBit (slot, at++));

            if ((value & 0x80) != 0)
            {
                framed.push_back (value);
                value = 0;
            }
        }

        for (size_t each = 0; each + 14 < framed.size(); each++)
        {
            if (framed[each] == 0xD5 && framed[each + 1] == 0xAA && framed[each + 2] == 0x96 && framed[each + 11] == 0xAF)
            {
                Assert::AreEqual ((int) 0xA4, (int) framed[each + 12], L"The byte after the ending, the same on every read");
                Assert::AreNotEqual ((int) 0xBF, (int) framed[each + 13], L"The next byte, read otherwise once, is weak");
                checked++;
            }
        }

        Assert::IsTrue (checked > 0, L"The track has address fields");
    }


    //  A track whose data fields, with the D5 AA BC prolog Infocom's disks
    //  have, end in FF FF before the epilog, read with its sync bytes marked:
    //  the data's FFs are written with no zero bits, so a read of the field
    //  doesn't read the checksum's FF again where the epilog's DE is.
    TEST_METHOD (ADataFieldsFFs_AreWrittenWithNoZeroBits)
    {
        static constexpr size_t  kSectors    = 16;
        static constexpr size_t  kSyncs      = 6;
        static constexpr size_t  kDataBytes  = 341;
        static constexpr size_t  kReadBytes  = 14000;
        static constexpr Byte    kMarked     = 0x7F;
        static constexpr size_t  kNoiseEvery = 4;

        std::vector<Byte>                        revolution;
        std::vector<bool>                        sync;
        std::vector<CassoLinkCapture::Position>  positions (1);
        CassoLinkCapture::Result                 result;
        DiskImage                                image;
        std::vector<std::pair<Byte, size_t>>     framed;
        uint32_t                                 seed    = 12345;
        size_t                                   checked = 0;



        auto  add = [&revolution, &sync] (std::initializer_list<Byte> values, bool isSync)
        {
            for (Byte value : values)
            {
                revolution.push_back (value);
                sync.push_back (isSync);
            }
        };

        for (size_t sector = 0; sector < kSectors; sector++)
        {
            for (size_t each = 0; each < kSyncs; each++)
            {
                add ({ 0xFF }, true);
            }

            add ({ 0xD5, 0xAA, 0x96, 0xAA, 0xAA, 0xAA, 0xAB, (Byte) (0xAA | (sector >> 1)), (Byte) (0xAA | sector), 0xAA, 0xAA, 0xDE, 0xAA, 0xEB }, false);

            for (size_t each = 0; each < kSyncs; each++)
            {
                add ({ 0xFF }, true);
            }

            add ({ 0xD5, 0xAA, 0xBC }, false);

            for (size_t each = 0; each < kDataBytes; each++)
            {
                seed = seed * 1103515245u + 12345u;
                add ({ s_kDiskBytes[(seed >> 16) % (std::size (s_kDiskBytes) - 1)] }, false);
            }

            add ({ 0xFF, 0xFF, 0xDE, 0xAA, 0xEB }, false);
        }

        for (Byte delay : CassoLinkCapture::kWholeDelays)
        {
            std::vector<Byte>  read;

            for (size_t at = 0; at < kReadBytes; at++)
            {
                size_t  in = at % revolution.size();

                bool    noise = delay == 0 && !sync[in] && at % kNoiseEvery == 0;

                read.push_back (((delay == 1 && sync[in]) || noise) ? (Byte) (revolution[in] & kMarked) : revolution[in]);
            }

            positions[0].reads.push_back (read);
            positions[0].delays.push_back (delay);
        }

        AssertSucceeded (CassoLinkCapture::BuildWoz (positions, 0, "2026-11-12T00:00:00Z", 1, result));
        AssertSucceeded (WozLoader::Load (result.woz, image));

        int     slot  = image.ResolveQuarterTrack (0);
        size_t  count = image.GetTrackBitCount (slot);
        size_t  at    = 0;

        while (at < count && image.ReadBit (slot, at) == 0)
        {
            at++;
        }

        while (at + 8 <= count)
        {
            Byte    value = 0;
            size_t  zeros = 0;

            for (int bit = 0; bit < 8; bit++)
            {
                value = (Byte) ((value << 1) | image.ReadBit (slot, at++));
            }

            while (at < count && image.ReadBit (slot, at) == 0)
            {
                zeros++;
                at++;
            }

            framed.push_back ({ value, zeros });
        }

        for (size_t each = 2; each + 2 < framed.size(); each++)
        {
            if (framed[each].first == 0xDE && framed[each + 1].first == 0xAA && framed[each + 2].first == 0xEB && framed[each - 1].first == 0xFF)
            {
                Assert::AreEqual ((size_t) 0, framed[each - 2].second, L"The data's first FF");
                Assert::AreEqual ((size_t) 0, framed[each - 1].second, L"The data's second FF");
                checked++;
            }
        }

        Assert::IsTrue (checked > 0, L"The track has data fields");
    }


    //  A track of 16 sectors read three times, with a nibble of sector 5
    //  misread the same way in the first two reads, so most of the passes
    //  have it wrong: the sector is taken from the read that decodes it. A
    //  byte read at the data field's write splice there is a D5, which
    //  isn't taken as the data field's prolog.
    TEST_METHOD (ASectorMisreadInMostReads_IsTakenFromTheReadThatDecodesIt)
    {
        static constexpr int     kSectors    = 16;
        static constexpr int     kBad        = 5;
        static constexpr size_t  kSyncs      = 8;
        static constexpr size_t  kMisread    = 100;
        static constexpr size_t  kAcrossEnd  = 70;      // past the misread, still in the field
        static constexpr size_t  kReadBytes  = 14000;
        static constexpr Byte    kVolume     = 254;

        std::vector<Byte>                        revolution;
        std::vector<Byte>                        misread;
        std::vector<CassoLinkCapture::Position>  positions (1);
        CassoLinkCapture::Result                 result;
        DiskImage                                image;
        std::vector<Byte>                        framed;
        uint32_t                                 seed    = 4321;
        size_t                                   badAt   = 0;
        bool                                     decoded = false;



        auto  oddEven = [&revolution] (Byte value)
        {
            revolution.push_back ((Byte) ((value >> 1) | 0xAA));
            revolution.push_back ((Byte) (value | 0xAA));
        };

        for (int number = 0; number < kSectors; number++)
        {
            std::vector<Byte>  data (NibbleFormat::kSectorByteSize);
            std::vector<Byte>  nibbles;

            revolution.insert (revolution.end(), kSyncs, 0xFF);
            revolution.insert (revolution.end(), { 0xD5, 0xAA, 0x96 });
            oddEven (kVolume);
            oddEven (0);
            oddEven ((Byte) number);
            oddEven ((Byte) (kVolume ^ number));
            revolution.insert (revolution.end(), { 0xDE, 0xAA, 0xEB });

            if (number == kBad)
            {
                revolution.insert (revolution.end(), { 0xD5, 0xFE, 0x9F });
            }

            revolution.insert (revolution.end(), kSyncs, 0xFF);
            revolution.insert (revolution.end(), { 0xD5, 0xAA, 0xAD });

            for (Byte & value : data)
            {
                seed  = seed * 1103515245u + 12345u;
                value = (Byte) (seed >> 16);
            }

            EncodeDataField (data, nibbles);
            badAt = number == kBad ? revolution.size() + kMisread : badAt;
            revolution.insert (revolution.end(), nibbles.begin(), nibbles.end());
            revolution.insert (revolution.end(), { 0xDE, 0xAA, 0xEB });
        }

        misread        = revolution;
        misread[badAt] = misread[badAt] == 0x96 ? 0x97 : 0x96;

        //  The reads start at the track's first sync byte, and then in the
        //  middle of sector 5's data field, so the field crosses the end of
        //  the revolution the stream has.
        for (size_t first : { (size_t) 0, badAt + kAcrossEnd })
        {
            positions.assign (1, CassoLinkCapture::Position());
            decoded = false;

            for (size_t read = 0; read < CassoLinkCapture::kWholeReads; read++)
            {
                const std::vector<Byte>  & source = read < 2 ? misread : revolution;
                std::vector<Byte>          bytes;

                for (size_t at = 0; at < kReadBytes; at++)
                {
                    bytes.push_back (source[(first + at) % source.size()]);
                }

                positions[0].reads.push_back (bytes);
                positions[0].delays.push_back (CassoLinkCapture::kWholeDelays[read]);
            }

            AssertSucceeded (CassoLinkCapture::BuildWoz (positions, 0, "2026-11-12T00:00:00Z", 1, result));
            AssertSucceeded (WozLoader::Load (result.woz, image));

            FrameTrack (image, image.ResolveQuarterTrack (0), framed);

            for (size_t at = 0; !decoded && at + 20 + NibbleFormat::kDataNibbleCount < framed.size(); at++)
            {
                bool  address = framed[at] == 0xD5 && framed[at + 1] == 0xAA && framed[at + 2] == 0x96;
                int   number  = address ? (((framed[at + 7] << 1) | 1) & framed[at + 8]) : -1;

                for (size_t data = at + 14; number == kBad && data < at + 40; data++)
                {
                    bool  prolog = framed[data] == 0xD5 && framed[data + 1] == 0xAA && framed[data + 2] == 0xAD;

                    decoded = decoded || (prolog && DoesDataFieldDecode (&framed[data + 3]));
                }
            }

            Assert::IsTrue (decoded, std::format (L"Sector 5 decodes, the reads starting at byte {}", first).c_str());
        }
    }


    //  A track whose second read waited on the track's sector 15 address field, so
    //  the revolution is timed from bits that are all data: the track is made
    //  that long by its gap's sync bytes, and the sectors' bytes are those
    //  the reads have.
    TEST_METHOD (ATrack_IsMadeARevolutionLong_ByItsGap)
    {
        static constexpr int     kSectors   = 16;
        static constexpr size_t  kSyncs     = 8;
        static constexpr size_t  kGapSyncs  = 120;
        static constexpr size_t  kAfter     = 790;     // the bytes from the marker's end to the second read's start
        static constexpr size_t  kReadBytes = 14000;
        static constexpr Byte    kVolume    = 254;

        std::vector<Byte>                        revolution;
        std::vector<CassoLinkCapture::Position>  positions (1);
        CassoLinkCapture::Result                 result;
        DiskImage                                image;
        std::vector<Byte>                        marker;
        uint32_t                                 seed    = 777;
        size_t                                   markAt  = 0;



        auto  oddEven = [&revolution] (Byte value)
        {
            revolution.push_back ((Byte) ((value >> 1) | 0xAA));
            revolution.push_back ((Byte) (value | 0xAA));
        };

        for (int number = 0; number < kSectors; number++)
        {
            std::vector<Byte>  data (NibbleFormat::kSectorByteSize);
            std::vector<Byte>  nibbles;

            revolution.insert (revolution.end(), number == 0 ? kGapSyncs : kSyncs, 0xFF);
            markAt = number == kSectors - 1 ? revolution.size() : markAt;
            revolution.insert (revolution.end(), { 0xD5, 0xAA, 0x96 });
            oddEven (kVolume);
            oddEven (0);
            oddEven ((Byte) number);
            oddEven ((Byte) (kVolume ^ number));
            revolution.insert (revolution.end(), { 0xDE, 0xAA, 0xEB });
            revolution.insert (revolution.end(), kSyncs, 0xFF);
            revolution.insert (revolution.end(), { 0xD5, 0xAA, 0xAD });

            for (Byte & value : data)
            {
                seed  = seed * 1103515245u + 12345u;
                value = (Byte) (seed >> 16);
            }

            EncodeDataField (data, nibbles);
            revolution.insert (revolution.end(), nibbles.begin(), nibbles.end());
            revolution.insert (revolution.end(), { 0xDE, 0xAA, 0xEB });
        }

        marker.assign (revolution.begin() + (ptrdiff_t) markAt, revolution.begin() + (ptrdiff_t) markAt + 11);

        positions[0].syncFrom   = 0;
        positions[0].syncMarker = marker;

        for (size_t read = 0; read < CassoLinkCapture::kWholeReads; read++)
        {
            size_t             first = read == 1 ? (markAt + marker.size() + kAfter) % revolution.size() : 0;
            std::vector<Byte>  bytes;

            for (size_t at = 0; at < kReadBytes; at++)
            {
                bytes.push_back (revolution[(first + at) % revolution.size()]);
            }

            positions[0].reads.push_back (bytes);
            positions[0].delays.push_back (CassoLinkCapture::kWholeDelays[read]);
        }

        AssertSucceeded (CassoLinkCapture::BuildWoz (positions, 0, "2026-11-12T00:00:00Z", 1, result));
        AssertSucceeded (WozLoader::Load (result.woz, image));

        size_t  bits = image.GetTrackBitCount (image.ResolveQuarterTrack (0));

        Assert::IsTrue (result.revolutionCycles > 0.0, L"The revolution is timed");
        Assert::AreEqual ((size_t) std::llround (result.revolutionCycles / 4.0), bits, L"The track is a revolution long");
        Assert::AreNotEqual (revolution.size() * 8, bits, L"The gap's sync bytes changed");
    }


    //  A track whose first read starts just after the D5 of sector 4's
    //  address field, and latches the AA after it from the middle of the
    //  byte, as 82: the WOZ has every address field as the disk does.
    TEST_METHOD (AReadsFirstByte_LatchedFromTheMiddleOfAByte_IsNotInTheWoz)
    {
        static constexpr int     kSectors    = 16;
        static constexpr int     kStartIn    = 4;
        static constexpr size_t  kSyncs      = 8;
        static constexpr size_t  kReadBytes  = 14000;
        static constexpr Byte    kVolume     = 254;
        static constexpr Byte    kMisframed  = 0x82;

        std::vector<Byte>                        revolution;
        std::vector<CassoLinkCapture::Position>  positions (1);
        CassoLinkCapture::Result                 result;
        DiskImage                                image;
        std::vector<Byte>                        framed;
        std::set<int>                            addressed;
        uint32_t                                 seed    = 2468;
        size_t                                   startAt = 0;



        auto  oddEven = [&revolution] (Byte value)
        {
            revolution.push_back ((Byte) ((value >> 1) | 0xAA));
            revolution.push_back ((Byte) (value | 0xAA));
        };

        for (int number = 0; number < kSectors; number++)
        {
            std::vector<Byte>  data (NibbleFormat::kSectorByteSize);
            std::vector<Byte>  nibbles;

            revolution.insert (revolution.end(), kSyncs, 0xFF);
            startAt = number == kStartIn ? revolution.size() + 1 : startAt;
            revolution.insert (revolution.end(), { 0xD5, 0xAA, 0x96 });
            oddEven (kVolume);
            oddEven (0);
            oddEven ((Byte) number);
            oddEven ((Byte) (kVolume ^ number));
            revolution.insert (revolution.end(), { 0xDE, 0xAA, 0xEB });
            revolution.insert (revolution.end(), kSyncs, 0xFF);
            revolution.insert (revolution.end(), { 0xD5, 0xAA, 0xAD });

            for (Byte & value : data)
            {
                seed  = seed * 1103515245u + 12345u;
                value = (Byte) (seed >> 16);
            }

            EncodeDataField (data, nibbles);
            revolution.insert (revolution.end(), nibbles.begin(), nibbles.end());
            revolution.insert (revolution.end(), { 0xDE, 0xAA, 0xEB });
        }

        for (size_t read = 0; read < CassoLinkCapture::kWholeReads; read++)
        {
            size_t             first = read == 0 ? startAt : 0;
            std::vector<Byte>  bytes;

            for (size_t at = 0; at < kReadBytes; at++)
            {
                bytes.push_back (revolution[(first + at) % revolution.size()]);
            }

            bytes[0] = read == 0 ? kMisframed : bytes[0];

            positions[0].reads.push_back (bytes);
            positions[0].delays.push_back (CassoLinkCapture::kWholeDelays[read]);
        }

        AssertSucceeded (CassoLinkCapture::BuildWoz (positions, 0, "2026-11-12T00:00:00Z", 1, result));
        AssertSucceeded (WozLoader::Load (result.woz, image));

        FrameTrack (image, image.ResolveQuarterTrack (0), framed);

        for (size_t at = 0; at + 11 < framed.size(); at++)
        {
            bool  address  = framed[at + 1] == 0xAA && framed[at + 2] == 0x96;
            int   volume   = ((framed[at + 3] << 1) | 1) & framed[at + 4];
            int   track    = ((framed[at + 5] << 1) | 1) & framed[at + 6];
            int   number   = ((framed[at + 7] << 1) | 1) & framed[at + 8];
            int   checksum = ((framed[at + 9] << 1) | 1) & framed[at + 10];

            if (framed[at] == 0xD5 && address && (volume ^ track ^ number) == checksum)
            {
                addressed.insert (number);
            }
        }

        Assert::AreEqual ((size_t) kSectors, addressed.size(), L"Every sector's address field");
    }


    //  A track with all 16 address fields is a standard track whether or not
    //  its data fields decode, so the positions between two of them aren't
    //  read; one with an address field whose checksum is wrong isn't.
    TEST_METHOD (ATrackWithEveryAddressField_IsStandard)
    {
        static constexpr int     kSectors   = 16;
        static constexpr int     kTrack     = 3;
        static constexpr size_t  kSyncs     = 8;
        static constexpr size_t  kReadBytes = 14000;
        static constexpr Byte    kVolume    = 254;

        for (int variant = 0; variant < 3; variant++)
        {
            bool                        decodes   = variant != 1;
            bool                        addressed = variant != 2;
            std::vector<Byte>           revolution;
            CassoLinkCapture::Position  position;
            uint32_t                    seed      = 1357;



            auto  oddEven = [&revolution] (Byte value)
            {
                revolution.push_back ((Byte) ((value >> 1) | 0xAA));
                revolution.push_back ((Byte) (value | 0xAA));
            };

            for (int number = 0; number < kSectors; number++)
            {
                std::vector<Byte>  data (NibbleFormat::kSectorByteSize);
                std::vector<Byte>  nibbles;

                revolution.insert (revolution.end(), kSyncs, 0xFF);
                revolution.insert (revolution.end(), { 0xD5, 0xAA, 0x96 });
                oddEven (kVolume);
                oddEven (kTrack);
                oddEven ((Byte) number);
                oddEven ((Byte) (kVolume ^ kTrack ^ number ^ ((!addressed && number == 5) ? 1 : 0)));
                revolution.insert (revolution.end(), { 0xDE, 0xAA, 0xEB });
                revolution.insert (revolution.end(), kSyncs, 0xFF);
                revolution.insert (revolution.end(), { 0xD5, 0xBB, 0xCF });

                for (Byte & value : data)
                {
                    seed  = seed * 1103515245u + 12345u;
                    value = (Byte) (seed >> 16);
                }

                EncodeDataField (data, nibbles);
                nibbles.back() = decodes ? nibbles.back() : (Byte) (nibbles.back() == 0x96 ? 0x97 : 0x96);
                revolution.insert (revolution.end(), nibbles.begin(), nibbles.end());
                revolution.insert (revolution.end(), { 0xDE, 0xAA, 0xEB });
            }

            position.position = kTrack * 4;

            for (size_t read = 0; read < CassoLinkCapture::kWholeReads; read++)
            {
                std::vector<Byte>  bytes;

                for (size_t at = 0; at < kReadBytes; at++)
                {
                    bytes.push_back (revolution[at % revolution.size()]);
                }

                position.reads.push_back (bytes);
                position.delays.push_back (CassoLinkCapture::kWholeDelays[read]);
            }

            Assert::AreEqual (addressed, CassoLinkCapture::IsStandardTrack (position),
                              variant == 0 ? L"Data fields that decode" : variant == 1 ? L"Data fields that don't decode" : L"An address field with a wrong checksum");
        }
    }


    //  A sector whose data prolog one read has as D5 BB BF, on a track whose
    //  other data fields begin D5 AA AD, is taken from a read with D5 AA AD.
    //  The stream is the first read's, and the other two reads differ in the
    //  gap before the prolog, as reads of a write splice do, so no two reads
    //  outvote the first. The sector is in the result and in the notes.
    TEST_METHOD (ASectorWithAnotherProlog_IsTakenFromAnotherRead)
    {
        static constexpr int     kSectors   = 16;
        static constexpr int     kTrack     = 1;
        static constexpr int     kOdd       = 3;
        static constexpr size_t  kSyncs     = 8;
        static constexpr size_t  kReadBytes = 14000;
        static constexpr Byte    kVolume    = 254;

        std::vector<CassoLinkCapture::Position>  positions (1);
        CassoLinkCapture::Result                 result;
        DiskImage                                image;
        std::vector<Byte>                        framed;
        std::string                              notes = "Track 1 sector 3 taken from another read.";
        int                                      usual = 0;



        positions[0].position = kTrack * 4;

        for (size_t read = 0; read < CassoLinkCapture::kWholeReads; read++)
        {
            std::vector<Byte>  revolution;
            std::vector<Byte>  bytes;
            uint32_t           seed = 2468;

            auto  oddEven = [&revolution] (Byte value)
            {
                revolution.push_back ((Byte) ((value >> 1) | 0xAA));
                revolution.push_back ((Byte) (value | 0xAA));
            };

            for (int number = 0; number < kSectors; number++)
            {
                std::vector<Byte>  data (NibbleFormat::kSectorByteSize);
                std::vector<Byte>  nibbles;
                bool               odd = number == kOdd;

                revolution.insert (revolution.end(), kSyncs, 0xFF);
                revolution.insert (revolution.end(), { 0xD5, 0xAA, 0x96 });
                oddEven (kVolume);
                oddEven (kTrack);
                oddEven ((Byte) number);
                oddEven ((Byte) (kVolume ^ kTrack ^ number));
                revolution.insert (revolution.end(), { 0xDE, 0xAA, 0xEB });

                //  The first read has a shorter gap and the other prolog; each
                //  of the other two has a different byte in the gap.
                if (odd && read == 0)
                {
                    revolution.insert (revolution.end(), kSyncs - 3, 0xFF);
                    revolution.insert (revolution.end(), { 0xD5, 0xBB, 0xBF });
                }
                else
                {
                    revolution.insert (revolution.end(), kSyncs, 0xFF);
                    revolution[revolution.size() - 4] = odd ? (Byte) (read == 1 ? 0xD4 : 0xDD) : (Byte) 0xFF;
                    revolution.insert (revolution.end(), { 0xD5, 0xAA, 0xAD });
                }

                for (Byte & value : data)
                {
                    seed  = seed * 1103515245u + 12345u;
                    value = (Byte) (seed >> 16);
                }

                EncodeDataField (data, nibbles);
                revolution.insert (revolution.end(), nibbles.begin(), nibbles.end());
                revolution.insert (revolution.end(), { 0xDE, 0xAA, 0xEB });
            }

            for (size_t at = 0; at < kReadBytes; at++)
            {
                bytes.push_back (revolution[at % revolution.size()]);
            }

            positions[0].reads.push_back (bytes);
            positions[0].delays.push_back (CassoLinkCapture::kWholeDelays[read]);
        }

        AssertSucceeded (CassoLinkCapture::BuildWoz (positions, kTrack * 4, "2026-11-12T00:00:00Z", 1, result));
        AssertSucceeded (WozLoader::Load (result.woz, image));

        Assert::AreEqual ((size_t) 1, result.replaced.size(), L"One sector taken from another read");
        Assert::AreEqual (kTrack * 4, result.replaced[0].first);
        Assert::AreEqual (kOdd, result.replaced[0].second);
        Assert::IsTrue (result.weak.empty(), L"No weak bytes");
        Assert::IsTrue (std::search (result.woz.begin(), result.woz.end(), notes.begin(), notes.end()) != result.woz.end(), L"The notes have the sector");

        FrameTrack (image, image.ResolveQuarterTrack (kTrack * 4), framed);

        for (size_t at = 0; at + 2 < framed.size(); at++)
        {
            usual += (framed[at] == 0xD5 && framed[at + 1] == 0xAA && framed[at + 2] == 0xAD) ? 1 : 0;
        }

        Assert::IsTrue (usual >= kSectors, L"Every data field begins D5 AA AD");
    }


    //  A revolution that starts at an address field, the first after the first
    //  read's start, followed by a gap that the other reads have a byte longer,
    //  as reads of a write splice can:
    //  each read is aligned from its copy of that address field, not a byte
    //  past it, so every address field is in the result and no byte is weak.
    TEST_METHOD (AReadWithALongerGapAfterTheFirstField_IsAlignedFromIt)
    {
        static constexpr int     kSectors   = 16;
        static constexpr int     kTrack     = 1;
        static constexpr size_t  kSyncs     = 8;
        static constexpr size_t  kReadBytes = 14000;
        static constexpr Byte    kVolume    = 254;
        static constexpr size_t  kBefore    = 100;
        static constexpr size_t  kStarts[]  = { 0, 1000, 3000 };

        std::vector<CassoLinkCapture::Position>  positions (1);
        CassoLinkCapture::Result                 result;
        DiskImage                                image;
        std::vector<Byte>                        framed;
        std::set<int>                            addressed;



        positions[0].position = kTrack * 4;

        for (size_t read = 0; read < CassoLinkCapture::kWholeReads; read++)
        {
            std::vector<Byte>  revolution;
            std::vector<Byte>  bytes;
            uint32_t           seed = 1357;

            auto  oddEven = [&revolution] (Byte value)
            {
                revolution.push_back ((Byte) ((value >> 1) | 0xAA));
                revolution.push_back ((Byte) (value | 0xAA));
            };

            for (int number = 0; number < kSectors; number++)
            {
                std::vector<Byte>  data (NibbleFormat::kSectorByteSize);
                std::vector<Byte>  nibbles;

                revolution.insert (revolution.end(), { 0xD5, 0xAA, 0x96 });
                oddEven (kVolume);
                oddEven (kTrack);
                oddEven ((Byte) number);
                oddEven ((Byte) (kVolume ^ kTrack ^ number));
                revolution.insert (revolution.end(), { 0xDE, 0xAA, 0xEB });

                //  Sector 0's gap begins with two bytes in the first read, and
                //  in each of the others with six, each set different, in
                //  place of those two and three sync bytes.
                if (number == 0)
                {
                    revolution.insert (revolution.end(), read == 0 ? std::initializer_list<Byte> { 0xD7, 0xF3 } :
                                                         read == 1 ? std::initializer_list<Byte> { 0x9B, 0xC9, 0xBF, 0xCF, 0xF3, 0xFC } :
                                                                     std::initializer_list<Byte> { 0xAB, 0xF9, 0xBF, 0xCF, 0xF3, 0xFC });
                }

                revolution.insert (revolution.end(), number == 0 && read > 0 ? kSyncs - 3 : kSyncs, 0xFF);
                revolution.insert (revolution.end(), { 0xD5, 0xAA, 0xAD });

                for (Byte & value : data)
                {
                    seed  = seed * 1103515245u + 12345u;
                    value = (Byte) (seed >> 16);
                }

                EncodeDataField (data, nibbles);
                revolution.insert (revolution.end(), nibbles.begin(), nibbles.end());
                revolution.insert (revolution.end(), { 0xDE, 0xAA, 0xEB });
                revolution.insert (revolution.end(), kSyncs, 0xFF);
            }

            for (size_t at = 0; at < kReadBytes; at++)
            {
                bytes.push_back (revolution[(kStarts[read] + revolution.size() - kBefore + at) % revolution.size()]);
            }

            positions[0].reads.push_back (bytes);
            positions[0].delays.push_back (CassoLinkCapture::kWholeDelays[read]);
        }

        AssertSucceeded (CassoLinkCapture::BuildWoz (positions, kTrack * 4, "2026-11-18T00:00:00Z", 1, result));
        AssertSucceeded (WozLoader::Load (result.woz, image));

        FrameTrack (image, image.ResolveQuarterTrack (kTrack * 4), framed);

        for (size_t at = 0; at + 9 < framed.size(); at++)
        {
            if (framed[at] == 0xD5 && framed[at + 1] == 0xAA && framed[at + 2] == 0x96)
            {
                addressed.insert (((framed[at + 7] << 1) | 1) & framed[at + 8]);
            }
        }

        Assert::AreEqual ((size_t) kSectors, addressed.size(), L"Every sector's address field");
        Assert::IsTrue (result.weak.empty(), L"No weak bytes");
    }


    //  A track's bytes as a controller frames them from its bits.
    static void  FrameTrack (DiskImage & image, int slot, std::vector<Byte> & outBytes)
    {
        size_t  count = image.GetTrackBitCount (slot);
        size_t  at    = 0;
        Byte    value = 0;



        outBytes.clear();

        for (size_t bit = 0; bit < 2 * count; bit++)
        {
            value = (Byte) ((value << 1) | image.ReadBit (slot, at));
            at    = (at + 1) % count;

            if (value & 0x80)
            {
                outBytes.push_back (value);
                value = 0;
            }
        }
    }


    static constexpr size_t  kWindowBits     = 200;


    //  The disk bytes a 16-sector track's data fields use.
    static constexpr Byte  s_kDiskBytes[] =
    {
        0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6, 0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
        0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
        0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
        0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
    };


    //  An earlier capture's reads go after the reads a position has, and a
    //  position only the earlier capture read is added.
    TEST_METHOD (AnEarlierCapturesReads_AreAddedAfterAPositionsReads)
    {
        std::vector<CassoLinkCapture::Position>  positions (1);
        std::vector<CassoLinkCapture::Position>  earlier (2);

        positions[0].position = 4;
        positions[0].reads    = { { 1 }, { 2 } };
        positions[0].delays   = { 0, 1 };
        earlier[0].position   = 4;
        earlier[0].reads      = { { 3 } };
        earlier[0].delays     = { 0 };
        earlier[1].position   = 0;
        earlier[1].reads      = { { 9 } };
        earlier[1].delays     = { 1 };

        CassoLinkCapture::MergeReads (positions, earlier);

        Assert::AreEqual ((size_t) 2, positions.size());
        Assert::AreEqual (0, positions[0].position, L"In order of position");
        Assert::AreEqual ((size_t) 3, positions[1].reads.size());
        Assert::IsTrue (positions[1].reads[0] == std::vector<Byte> { 1 } && positions[1].reads[2] == std::vector<Byte> { 3 }, L"The reads it had first");
        Assert::IsTrue (positions[1].delays == std::vector<Byte> { 0, 1, 0 });
    }


    //  A whole track's reads of a standard 16-sector track, each read a
    //  revolution and a half, starting at another byte.
    static CassoLinkCapture::Position  MakeStandardPosition (int track)
    {
        static constexpr int     kSectors = 16;
        static constexpr size_t  kSyncs   = 8;
        static constexpr Byte    kVolume  = 254;

        CassoLinkCapture::Position  position;
        std::vector<Byte>           revolution;
        uint32_t                    seed = (uint32_t) (track * 7919 + 1);



        auto  oddEven = [&revolution] (Byte value)
        {
            revolution.push_back ((Byte) ((value >> 1) | 0xAA));
            revolution.push_back ((Byte) (value | 0xAA));
        };

        for (int number = 0; number < kSectors; number++)
        {
            std::vector<Byte>  data (NibbleFormat::kSectorByteSize);
            std::vector<Byte>  nibbles;

            revolution.insert (revolution.end(), kSyncs, 0xFF);
            revolution.insert (revolution.end(), { 0xD5, 0xAA, 0x96 });
            oddEven (kVolume);
            oddEven ((Byte) track);
            oddEven ((Byte) number);
            oddEven ((Byte) (kVolume ^ track ^ number));
            revolution.insert (revolution.end(), { 0xDE, 0xAA, 0xEB });
            revolution.insert (revolution.end(), kSyncs, 0xFF);
            revolution.insert (revolution.end(), { 0xD5, 0xAA, 0xAD });

            for (Byte & value : data)
            {
                seed  = seed * 1103515245u + 12345u;
                value = (Byte) (seed >> 16);
            }

            EncodeDataField (data, nibbles);
            revolution.insert (revolution.end(), nibbles.begin(), nibbles.end());
            revolution.insert (revolution.end(), { 0xDE, 0xAA, 0xEB });
        }

        position.position = track * 4;

        for (size_t read = 0; read < CassoLinkCapture::kWholeReads; read++)
        {
            std::vector<Byte>  bytes;
            size_t             first = read * 997;

            for (size_t at = 0; at < revolution.size() * 3 / 2; at++)
            {
                bytes.push_back (revolution[(first + at) % revolution.size()]);
            }

            position.reads.push_back (bytes);
            position.delays.push_back (CassoLinkCapture::kWholeDelays[read]);
        }

        return position;
    }


    //  A capture's WOZ has a format stamp, with each track's marks and the
    //  capture's facts.
    TEST_METHOD (ACapturesWoz_HasAFormatStamp)
    {
        static constexpr int  kTracks  = 3;
        static constexpr int  kSectors = 16;

        std::vector<CassoLinkCapture::Position>  positions;
        CassoLinkCapture::Result                 result;
        std::string                              text;
        StampState                               state = StampState::Absent;
        FormatStamp                              stamp;



        for (int track = 0; track < kTracks; track++)
        {
            positions.push_back (MakeStandardPosition (track));
        }

        AssertSucceeded (CassoLinkCapture::BuildWoz (positions, (kTracks - 1) * 4, "2026-11-12T00:00:00Z", 1, result));
        AssertSucceeded (FormatStampChunk::GetStampFromFile (result.woz, text, state, stamp));

        Assert::IsTrue   (state == StampState::Readable);
        Assert::IsTrue   (stamp.source == StampSource::Capture);
        Assert::IsTrue   (stamp.hasCapture);
        Assert::AreEqual (kTracks * (int) CassoLinkCapture::kWholeReads, stamp.capture.reads);
        Assert::AreEqual (std::string ("2026-11-12T00:00:00Z"), stamp.written);
        Assert::IsTrue   (stamp.tracks.size() >= (size_t) kTracks);
        Assert::IsTrue   (stamp.tracks[1].addressPrologs == std::vector<std::vector<Byte>> { { 0xD5, 0xAA, 0x96 } });
        Assert::AreEqual (kSectors, stamp.tracks[1].sectorsGood);
        Assert::IsTrue   (stamp.tracks[1].hasCapture);
    }

    //  A capture whose reads found the disk write-protected is a
    //  write-protected WOZ, and the reads file has it; one whose reads didn't
    //  isn't.
    TEST_METHOD (AWriteProtectedDisksReads_AreAWriteProtectedWoz)
    {
        static constexpr size_t  kInfoProtected = 22;

        for (bool protect : { true, false })
        {
            std::vector<CassoLinkCapture::Position>  positions { MakeStandardPosition (0) };
            std::vector<CassoLinkCapture::Position>  parsed;
            std::vector<Byte>                        file;
            CassoLinkCapture::Result                 result;



            positions[0].writeProtected = protect;

            AssertSucceeded (CassoLinkCapture::BuildWoz (positions, 0, "2026-11-12T00:00:00Z", 1, result));

            Assert::AreEqual (protect, result.writeProtected, L"The result has the protection");
            Assert::AreEqual ((Byte) (protect ? 1 : 0), result.woz[kInfoProtected], L"The WOZ's INFO has it");

            CassoLinkCapture::SerializeReads (positions, file);
            AssertSucceeded (CassoLinkCapture::ParseReads (file, parsed));
            Assert::AreEqual (protect, parsed[0].writeProtected, L"The reads file has it");
        }
    }


    //  Two standard whole tracks' reads build a WOZ with a stream for each,
    //  nothing weak or unstable. The quarter tracks next to each whole track
    //  are its stream, and the half track between them doesn't have a stream.
    TEST_METHOD (TwoStandardTracksReads_AreAWozOfTwoStreams)
    {
        std::vector<CassoLinkCapture::Position>  positions { MakeStandardPosition (0), MakeStandardPosition (1) };
        CassoLinkCapture::Result                 result;
        DiskImage                                woz;



        AssertSucceeded (CassoLinkCapture::BuildWoz (positions, 4, "2026-11-12T00:00:00Z", 1, result));
        AssertSucceeded (WozLoader::Load (result.woz, woz));

        Assert::AreEqual ((size_t) 2, result.streams.size(), L"The two whole tracks");
        Assert::IsTrue   (result.unstable.empty());
        Assert::IsTrue   (result.weak.empty());
        Assert::AreEqual (woz.ResolveQuarterTrack (0), woz.ResolveQuarterTrack (1), L"Quarter track 1 is track 0's stream");
        Assert::AreEqual (woz.ResolveQuarterTrack (4), woz.ResolveQuarterTrack (3), L"Quarter track 3 is track 1's stream");
        Assert::AreNotEqual (woz.ResolveQuarterTrack (0), woz.ResolveQuarterTrack (4));
        Assert::AreEqual (-1, woz.ResolveQuarterTrack (2), L"The half track");
    }
};
