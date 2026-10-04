#include "Pch.h"

#include "Devices/Tape/TapeRecordScanner.h"
#include "TapeTestEncoder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordScannerTests
//
//  Records built by the test encoder, turned straight into transitions -- one
//  per half-cycle, at its exact time -- so the scanner is tested on its own,
//  without the signal decoder in front of it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TapeRecordScannerTests)
{
public:

    static constexpr uint32_t  kRate = 44100;


    static void ToTransitions (const std::vector<double> & halvesUs, std::vector<double> & transitions)
    {
        double  t = 0.0;



        transitions.clear();

        for (double half : halvesUs)
        {
            transitions.push_back (t);
            t += half * kRate / 1.0e6;
        }

        transitions.push_back (t);
    }


    static void MakeBytes (size_t count, std::vector<Byte> & bytes)
    {
        bytes.resize (count);

        for (size_t i = 0; i < count; i++)
        {
            bytes[i] = (Byte) (i * 37 + 11);
        }
    }


    TEST_METHOD (FindsARecordWithItsBytesAndAGoodChecksum)
    {
        std::vector<Byte>        data;
        std::vector<double>      halves;
        std::vector<double>      transitions;
        std::vector<TapeRecord>  records;
        TapeEncodeOptions        options;



        MakeBytes (100, data);
        TapeTestEncoder::AppendRecord (halves, data, options);
        halves.push_back (5000.0);   // a gap closes the record
        ToTransitions (halves, transitions);

        TapeRecordScanner::Scan (transitions, kRate, records);

        Assert::AreEqual ((size_t) 1, records.size());
        Assert::AreEqual ((size_t) 101, records[0].byteCount, L"the data and its checksum");
        Assert::IsTrue   (records[0].isValid);
        Assert::IsTrue   (records[0].leaderStart < records[0].dataStart);
        Assert::IsTrue   (records[0].dataStart   < records[0].dataEnd);
    }


    TEST_METHOD (FindsEveryRecordOnATape)
    {
        std::vector<Byte>        header = { 0x10, 0x00, 0x55 };
        std::vector<Byte>        body;
        std::vector<double>      halves;
        std::vector<double>      transitions;
        std::vector<TapeRecord>  records;
        TapeEncodeOptions        options;



        MakeBytes (300, body);
        TapeTestEncoder::AppendRecord (halves, header, options);
        halves.push_back (5000.0);
        TapeTestEncoder::AppendRecord (halves, body, options);
        halves.push_back (5000.0);
        ToTransitions (halves, transitions);

        TapeRecordScanner::Scan (transitions, kRate, records);

        Assert::AreEqual ((size_t) 2, records.size());
        Assert::AreEqual ((size_t) 4,   records[0].byteCount);
        Assert::AreEqual ((size_t) 301, records[1].byteCount);
        Assert::IsTrue   (records[0].isValid && records[1].isValid);
    }


    TEST_METHOD (AMisreadBitFailsTheChecksum)
    {
        std::vector<Byte>        data;
        std::vector<double>      halves;
        std::vector<double>      transitions;
        std::vector<TapeRecord>  records;
        TapeEncodeOptions        options;
        size_t                   bitAt  = 0;
        size_t                   dataAt = 0;



        MakeBytes (100, data);
        TapeTestEncoder::AppendRecord (halves, data, options);
        halves.push_back (5000.0);

        // Turn one 0 bit, a cycle of two short halves, into a 1 bit, two
        // long ones: the bytes still decode, and the checksum no longer
        // matches them.
        // On a bit boundary: the data starts right after the sync's second
        // half-cycle, and each bit is a pair from there.
        dataAt = (size_t) (std::find (halves.begin(), halves.end(), TapeTestEncoder::kSyncSecondUs) - halves.begin()) + 1;

        for (size_t i = dataAt + 400; i + 1 < halves.size(); i += 2)
        {
            if (halves[i] == TapeTestEncoder::kZeroHalfUs && halves[i + 1] == TapeTestEncoder::kZeroHalfUs)
            {
                bitAt = i;
                break;
            }
        }

        Assert::IsTrue (bitAt != 0);
        halves[bitAt]     = TapeTestEncoder::kOneHalfUs;
        halves[bitAt + 1] = TapeTestEncoder::kOneHalfUs;
        ToTransitions (halves, transitions);

        TapeRecordScanner::Scan (transitions, kRate, records);

        Assert::AreEqual ((size_t) 1, records.size());
        Assert::IsFalse  (records[0].isValid);
    }


    TEST_METHOD (NoLeaderNoRecord)
    {
        std::vector<double>      halves (2000, TapeTestEncoder::kZeroHalfUs);
        std::vector<double>      transitions;
        std::vector<TapeRecord>  records;



        ToTransitions (halves, transitions);

        TapeRecordScanner::Scan (transitions, kRate, records);

        Assert::IsTrue (records.empty());
    }
};
