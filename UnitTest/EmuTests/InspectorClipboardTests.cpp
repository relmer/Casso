#include "Pch.h"

#include "Devices/Disk/Inspector/InspectorClipboard.h"
#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "InspectorTrackBuilder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorClipboardTests
//
//  The clipboard text of FR-057 as the window contract gives it: hex 16 to
//  a line, text with the high bit masked, the hex dump of "Copy sector",
//  nibbles across the index, and tab-separated rows under their header.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (InspectorClipboardTests)
{
public:

    TEST_METHOD (HexRunsSixteenToALine)
    {
        vector<Byte>  bytes (18);



        for (size_t i = 0; i < bytes.size(); i++)
        {
            bytes[i] = static_cast<Byte> (i);
        }

        Assert::AreEqual (std::wstring (L"00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\r\n10 11"), InspectorClipboard::FormatHex (bytes));
    }



    TEST_METHOD (TextMasksTheHighBitAndDotsWhatDoesNotPrint)
    {
        const vector<Byte>  bytes = { 0xC8, 0xC9, 0x8D, 0x41, 0x7F, 0x00 };



        Assert::AreEqual (std::wstring (L"HI.A.."), InspectorClipboard::FormatText (bytes));
    }



    TEST_METHOD (CopySectorIsAHexDump)
    {
        vector<Byte>  bytes (256, 0xC1);
        std::wstring  dump;



        dump = InspectorClipboard::FormatHexDump (bytes);

        Assert::AreEqual (static_cast<size_t> (16), static_cast<size_t> (std::count (dump.begin(), dump.end(), L'\n')));
        Assert::AreEqual (std::wstring (L"F0  C1 C1 C1 C1 C1 C1 C1 C1 C1 C1 C1 C1 C1 C1 C1 C1  AAAAAAAAAAAAAAAA\r\n"), dump.substr (dump.rfind (L"F0  ")));
    }



    TEST_METHOD (NibblesCopyAsHexAcrossTheIndex)
    {
        InspectorTrackBuilder  builder;
        TrackContext           context;
        TrackAnalysis          track;
        size_t                 n       = 0;
        std::wstring           text;



        builder.AppendStandardTrack (DiskFieldKind::Sixteen, 254, 0, InspectorTrackBuilder::FillPattern);
        TrackAnalyzer::Analyze (*builder.MakeBitCopy(), context, DecodeSettings::MakeStandard(), track);
        n    = track.framed.nibbles.size();
        text = InspectorClipboard::FormatNibbles (track, static_cast<int> (n) - 1, 2);

        Assert::AreEqual (std::format (L"{:02X} {:02X}", track.framed.nibbles[n - 1].value, track.framed.nibbles[0].value), text);
    }



    TEST_METHOD (RowsAreTabSeparatedUnderTheirHeader)
    {
        Assert::AreEqual (std::wstring (L"Track\tClass\r\n0\t16 sector\r\n0.25\tNothing\r\n"),
                          InspectorClipboard::FormatTable ({ L"Track", L"Class" }, { { L"0", L"16 sector" }, { L"0.25", L"Nothing" } }));
    }



    TEST_METHOD (PastedHexTakesDollarAndZeroXPrefixes)
    {
        using HexParse = InspectorClipboard::HexParse;

        vector<Byte>  bytes;



        Assert::IsTrue  (InspectorClipboard::ParseHex (L"A5 5a\r\n$C1 0x0D 0XFF $7 0x8 $0102", bytes) == HexParse::Bytes);
        Assert::IsTrue  (bytes == vector<Byte> { 0xA5, 0x5A, 0xC1, 0x0D, 0xFF, 0x07, 0x08, 0x01, 0x02 });

        Assert::IsTrue  (InspectorClipboard::ParseHex (L"  \t\r\n", bytes) == HexParse::Empty);
        Assert::IsTrue  (InspectorClipboard::ParseHex (L"A5 XY",    bytes) == HexParse::NotHex);
        Assert::IsTrue  (bytes.empty(), L"a rejected paste returns no bytes");
        Assert::IsTrue  (InspectorClipboard::ParseHex (L"A5,B6",    bytes) == HexParse::NotHex);
        Assert::IsTrue  (InspectorClipboard::ParseHex (L"$",        bytes) == HexParse::NotHex, L"a prefix without digits");
        Assert::IsTrue  (InspectorClipboard::ParseHex (L"0x",       bytes) == HexParse::NotHex);
        Assert::IsTrue  (InspectorClipboard::ParseHex (L"$$A5",     bytes) == HexParse::NotHex);
        Assert::IsTrue  (InspectorClipboard::ParseHex (L"0x0xA5",   bytes) == HexParse::NotHex);
        Assert::IsTrue  (InspectorClipboard::ParseHex (L"A5$B6",    bytes) == HexParse::NotHex, L"a prefix inside a value");
        Assert::IsTrue  (InspectorClipboard::ParseHex (L"$ A5",     bytes) == HexParse::NotHex);
        Assert::IsTrue  (InspectorClipboard::ParseHex (L"ABC",      bytes) == HexParse::OddDigits);
        Assert::IsTrue  (InspectorClipboard::ParseHex (L"$123",     bytes) == HexParse::OddDigits);
        Assert::IsTrue  (InspectorClipboard::ParseHex (L"0",        bytes) == HexParse::OddDigits, L"one digit needs a prefix");
    }
};
