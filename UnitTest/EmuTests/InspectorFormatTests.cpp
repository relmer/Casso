#include "Pch.h"

#include "Core/UnicodeSymbols.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorFormatTests
//
//  The inspector's number formats (FR-006): tracks, counts and angles in
//  decimal; sectors, byte values and offsets in hex with a "$"; byte sequences
//  as bare hex; and the parsers the Go to box uses.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (InspectorFormatTests)
{
public:

    TEST_METHOD (QuarterTracksAreDecimalWithTheirFraction)
    {
        Assert::AreEqual (std::wstring (L"0"),     InspectorFormat::FormatQuarterTrack (0));
        Assert::AreEqual (std::wstring (L"17.25"), InspectorFormat::FormatQuarterTrack (69));
        Assert::AreEqual (std::wstring (L"17.5"),  InspectorFormat::FormatQuarterTrack (70));
        Assert::AreEqual (std::wstring (L"39.75"), InspectorFormat::FormatQuarterTrack (159));
    }



    TEST_METHOD (AdjacentQuarterTracksJoinIntoARun)
    {
        Assert::AreEqual (std::wstring (L"2.25-2.75, 4"), InspectorFormat::FormatQuarterTrackList ({ 9, 10, 11, 16 }));
        Assert::AreEqual (std::wstring (L"1, 3"),         InspectorFormat::FormatQuarterTrackList ({ 4, 12 }));
        Assert::AreEqual (std::wstring(),                 InspectorFormat::FormatQuarterTrackList ({}));
    }



    TEST_METHOD (HexValuesTakeADollarSignAndSequencesDoNot)
    {
        const Byte  marks[] = { 0xD5, 0xAA, 0x96 };



        Assert::AreEqual (std::wstring (L"$F"),       InspectorFormat::FormatSector (15));
        Assert::AreEqual (std::wstring (L"$1A"),      InspectorFormat::FormatSector (26));
        Assert::AreEqual (std::wstring (L"$05"),      InspectorFormat::FormatHexByte (5));
        Assert::AreEqual (std::wstring (L"$155"),     InspectorFormat::FormatHexOffset (0x155));
        Assert::AreEqual (std::wstring (L"D5 AA 96"), InspectorFormat::FormatByteSequence (marks));
    }



    TEST_METHOD (CountsAnglesTimesAndPercentsAreDecimal)
    {
        Assert::AreEqual (std::wstring (L"999"),       InspectorFormat::FormatCount (999));
        Assert::AreEqual (std::wstring (L"51,007"),    InspectorFormat::FormatCount (51007));
        Assert::AreEqual (std::wstring (L"1,000,000"), InspectorFormat::FormatCount (1000000));
        Assert::AreEqual (std::wstring (L"90") + s_kpszDegree,         InspectorFormat::FormatDegrees (0.25));
        Assert::AreEqual (std::wstring (L"4.00") + s_kpszMicro + L"s", InspectorFormat::FormatMicroseconds (32));
        Assert::AreEqual (std::wstring (L"+4.8%"), InspectorFormat::FormatPercent (0.048));
        Assert::AreEqual (std::wstring (L"-5.0%"), InspectorFormat::FormatPercent (-0.05));
    }



    TEST_METHOD (QuarterTracksParseAsTheyAreShown)
    {
        int  quarterTrack = -1;



        Assert::IsTrue (InspectorFormat::TryParseQuarterTrack (L"17", quarterTrack));
        Assert::AreEqual (68, quarterTrack);
        Assert::IsTrue (InspectorFormat::TryParseQuarterTrack (L"17.75", quarterTrack));
        Assert::AreEqual (71, quarterTrack);
        Assert::IsTrue (InspectorFormat::TryParseQuarterTrack (L"0.5", quarterTrack));
        Assert::AreEqual (2, quarterTrack);

        Assert::IsFalse (InspectorFormat::TryParseQuarterTrack (L"17.3", quarterTrack));
        Assert::IsFalse (InspectorFormat::TryParseQuarterTrack (L"40",   quarterTrack));
        Assert::IsFalse (InspectorFormat::TryParseQuarterTrack (L"",     quarterTrack));
        Assert::IsFalse (InspectorFormat::TryParseQuarterTrack (L"x",    quarterTrack));
    }



    TEST_METHOD (HexAndDecimalParse)
    {
        int  value = 0;



        Assert::IsTrue (InspectorFormat::TryParseHex (L"$1f", value));
        Assert::AreEqual (0x1F, value);
        Assert::IsTrue (InspectorFormat::TryParseHex (L"C0", value));
        Assert::AreEqual (0xC0, value);
        Assert::IsFalse (InspectorFormat::TryParseHex (L"$", value));
        Assert::IsFalse (InspectorFormat::TryParseHex (L"G1", value));

        Assert::IsTrue (InspectorFormat::TryParseDecimal (L"254", value));
        Assert::AreEqual (254, value);
        Assert::IsFalse (InspectorFormat::TryParseDecimal (L"2a", value));
    }
};
