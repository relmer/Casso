#include "Pch.h"

#include "Devices/Disk/DiskFieldFormat.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SixAndTwoCodecEquivalenceTests
//
//  NibblizationLayer's data field codec against DiskFieldFormat's 16-sector
//  one: the same nibbles for every sector, and the same verdict on every
//  field, good or damaged.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SixAndTwoCodecEquivalenceTests)
{
public:

    static constexpr int       kSectorBytes   = DiskFieldFormat::kSectorBytes;
    static constexpr int       kBodyLength    = DiskFieldFormat::kBodyLength62;
    static constexpr int       kRandomSectors = 500;
    static constexpr int       kRandomFields  = 4000;
    static constexpr Byte      kIllegalNibble = 0xD5;
    static constexpr Byte      kOtherIllegal  = 0xAA;
    static constexpr uint32_t  kSeed          = 0x6A2C0DECu;



    static Byte NextByte (uint32_t & state)
    {
        constexpr uint32_t  kMultiplier = 1664525u;
        constexpr uint32_t  kIncrement  = 1013904223u;
        constexpr int       kHighByte   = 24;



        state = state * kMultiplier + kIncrement;

        return static_cast<Byte> (state >> kHighByte);
    }



    static vector<vector<Byte>> MakeSectors()
    {
        vector<vector<Byte>>  sectors;
        vector<Byte>          sector (kSectorBytes, 0);
        uint32_t              state  = kSeed;
        int                   i      = 0;
        int                   b      = 0;



        sectors.push_back (vector<Byte> (kSectorBytes, 0x00));
        sectors.push_back (vector<Byte> (kSectorBytes, 0xFF));

        for (b = 0; b < kSectorBytes; b++)
        {
            sector[(size_t) b] = static_cast<Byte> (b);
        }

        sectors.push_back (sector);

        for (i = 0; i < kRandomSectors; i++)
        {
            for (b = 0; b < kSectorBytes; b++)
            {
                sector[(size_t) b] = NextByte (state);
            }

            sectors.push_back (sector);
        }

        return sectors;
    }



    //  DiskFieldFormat's body and checksum nibble as one 343-nibble field.
    static vector<Byte> EncodeWithFieldFormat (const vector<Byte> & sector)
    {
        vector<Byte>  field;
        Byte          checksum = 0;



        DiskFieldFormat::EncodeData (DiskFieldKind::Sixteen, sector, field, checksum);
        field.push_back (checksum);

        return field;
    }



    //  The verdict compared between the codecs: every nibble legal and the
    //  checksum good.
    static bool DecodeWithFieldFormat (const vector<Byte> & field, vector<Byte> & outSector)
    {
        DataFieldDecode  decode;



        DiskFieldFormat::DecodeData (DiskFieldKind::Sixteen,
                                     std::span<const Byte> (field).first (kBodyLength),
                                     field[kBodyLength],
                                     decode);
        outSector.assign (decode.bytes.begin(), decode.bytes.end());

        return decode.isChecksumGood && decode.invalidNibbles.empty();
    }



    static void AssertSameVerdict (const vector<Byte> & field, const wchar_t * what)
    {
        vector<Byte>  layerSector;
        vector<Byte>  formatSector;
        bool          layerGood  = NibblizationLayer::DecodeDataNibbles (field.data(), layerSector);
        bool          formatGood = DecodeWithFieldFormat (field, formatSector);



        Assert::AreEqual (layerGood, formatGood, what);

        if (layerGood)
        {
            Assert::IsTrue (layerSector == formatSector, what);
        }
    }



    TEST_METHOD (BothEncodersWriteTheSameNibbles)
    {
        vector<vector<Byte>>  sectors = MakeSectors();
        vector<Byte>          layerField;
        vector<Byte>          formatField;



        Assert::AreEqual ((size_t) kRandomSectors + 3, sectors.size());

        for (const vector<Byte> & sector : sectors)
        {
            NibblizationLayer::EncodeDataNibbles (sector.data(), layerField);
            formatField = EncodeWithFieldFormat (sector);

            Assert::AreEqual (NibblizationLayer::kDataNibbleCount, layerField.size());
            Assert::IsTrue (layerField == formatField);
        }
    }



    TEST_METHOD (BothDecodersAcceptAGoodFieldWithTheSameBytes)
    {
        vector<vector<Byte>>  sectors = MakeSectors();
        vector<Byte>          field;
        vector<Byte>          layerSector;
        vector<Byte>          formatSector;



        for (const vector<Byte> & sector : sectors)
        {
            field = EncodeWithFieldFormat (sector);

            Assert::IsTrue (NibblizationLayer::DecodeDataNibbles (field.data(), layerSector));
            Assert::IsTrue (DecodeWithFieldFormat (field, formatSector));
            Assert::IsTrue (layerSector == sector);
            Assert::IsTrue (formatSector == sector);
        }
    }



    TEST_METHOD (BothDecodersRejectTheSameDamage)
    {
        vector<vector<Byte>>  sectors = MakeSectors();
        vector<Byte>          good;
        vector<Byte>          field;
        uint32_t              state   = kSeed;
        size_t                at      = 0;



        for (const vector<Byte> & sector : sectors)
        {
            good = EncodeWithFieldFormat (sector);
            at   = NextByte (state) % kBodyLength;

            field     = good;
            field[at] = kIllegalNibble;
            AssertSameVerdict (field, L"an illegal body nibble");

            field          = good;
            field[at]      = kOtherIllegal;
            field[at / 2]  = kIllegalNibble;
            AssertSameVerdict (field, L"two illegal body nibbles");

            field     = good;
            field[at] = DiskFieldFormat::TranslateValue (DiskFieldKind::Sixteen, static_cast<Byte> (DiskFieldFormat::TranslateNibble (DiskFieldKind::Sixteen, good[at]) + 1));
            AssertSameVerdict (field, L"a wrong legal body nibble");

            field              = good;
            field[kBodyLength] = DiskFieldFormat::TranslateValue (DiskFieldKind::Sixteen, static_cast<Byte> (DiskFieldFormat::TranslateNibble (DiskFieldKind::Sixteen, good[kBodyLength]) + 1));
            AssertSameVerdict (field, L"a wrong checksum nibble");

            field              = good;
            field[kBodyLength] = kIllegalNibble;
            AssertSameVerdict (field, L"an illegal checksum nibble");
        }
    }



    TEST_METHOD (BothDecodersAgreeOnRandomFields)
    {
        constexpr int  kIllegalOneIn = 64;



        vector<Byte>  field     (NibblizationLayer::kDataNibbleCount, 0);
        vector<Byte>  sector;
        uint32_t      state     = kSeed;
        int           i         = 0;
        size_t        n         = 0;
        int           good      = 0;
        int           bad       = 0;
        bool          isAnyByte = false;



        //  Half the fields have only legal nibbles, so about one in 64 of
        //  those has a good checksum and both decoders accept it.
        for (i = 0; i < kRandomFields; i++)
        {
            for (n = 0; n < field.size(); n++)
            {
                isAnyByte = (i % 2 != 0) && (NextByte (state) % kIllegalOneIn == 0);
                field[n]  = isAnyByte ? NextByte (state)
                                      : DiskFieldFormat::TranslateValue (DiskFieldKind::Sixteen, NextByte (state));
            }

            AssertSameVerdict (field, L"a random field");

            if (DecodeWithFieldFormat (field, sector))
            {
                good++;
            }
            else
            {
                bad++;
            }
        }

        Assert::IsTrue (good > 0, L"some random fields decode");
        Assert::IsTrue (bad > 0,  L"some random fields fail to decode");
    }
};
