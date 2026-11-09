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
};
