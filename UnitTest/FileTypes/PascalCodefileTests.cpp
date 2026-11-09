#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FileTypes/FileFormatTable.h"
#include "FileTypes/PascalCodefile.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PascalCodefileTests
//
//  Codefiles built in each test: a dictionary block, an unused block, and
//  one segment in block 2.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (PascalCodefileTests)
{
public:

    static constexpr Byte    kProDosCode     = 0x02;
    static constexpr size_t  kSegmentAt      = 1024;
    static constexpr Byte    kPCodeLowFirst  = 2;
    static constexpr Byte    kPCodeHighFirst = 1;
    static constexpr Byte    kNative         = 7;



    static std::vector<Byte> MakeCodefile (const std::vector<Byte> & segment, Byte machine)
    {
        std::vector<Byte>  bytes (kSegmentAt + 512, 0);
        const char *       name = "LINEFEED";

        bytes[0]     = 2;
        bytes[2]     = (Byte) segment.size();
        bytes[0x100] = 1;
        bytes[0x101] = (Byte) (0x40 | machine);

        memcpy (&bytes[0x40], name, 8);
        memset (&bytes[0x48], ' ', 15 * 8);
        std::copy (segment.begin(), segment.end(), bytes.begin() + kSegmentAt);

        return bytes;
    }



    static bool HasLine (const DecodedFile & file, const std::wstring & text)
    {
        for (const std::wstring & line : file.lines)
        {
            if (line.find (text) != std::wstring::npos)
            {
                return true;
            }
        }

        return false;
    }



    static std::vector<Byte> MakeLinefeedSegment()
    {
        return
        {
            0xD7, 0xD7, 0xC7, 0xF1, 0x40, 0x91, 0xAB, 0x03, 0xEA, 0x00, 0x00, 0x01, 0x88, 0xC7, 0xFF, 0x00,
            0x00, 0xC7, 0xFF, 0x00, 0x88, 0xBF, 0xC1, 0x00, 0x02, 0x00, 0x04, 0x00, 0x06, 0x00, 0x1E, 0x00,
            0x01, 0x00, 0x02, 0x00, 0x01, 0x01,
        };
    }



    TEST_METHOD (Identify_ByProDosAndPascalType)
    {
        std::vector<Byte>  bytes = MakeCodefile (MakeLinefeedSegment(), kPCodeLowFirst);
        FileEntry          entry;



        entry.name = "LINEFEED.CODE";
        entry.type = kProDosCode;

        Assert::IsTrue (FileFormatTable::Identify (entry, VolumeKind::ProDos, bytes) == FileFormat::PascalCode);
        Assert::IsTrue (FileFormatTable::Identify (entry, VolumeKind::Pascal, bytes) == FileFormat::PascalCode);
        Assert::IsTrue (FileFormatTable::Identify (entry, VolumeKind::Dos33,  bytes) != FileFormat::PascalCode);
        Assert::IsTrue (FileFormatTable::Identify (entry, VolumeKind::Pascal, std::span<const Byte> (bytes).first (512)) != FileFormat::PascalCode);
    }


    TEST_METHOD (Decode_PCodeSegment)
    {
        std::vector<Byte>  bytes = MakeCodefile (MakeLinefeedSegment(), kPCodeLowFirst);
        DecodedFile        file;
        std::wstring       error;



        AssertSucceeded (PascalCodefile::Decode (bytes, FileFacts(), DecodeOptions(), file, error));

        Assert::IsTrue (file.lines.size() == 24);
        Assert::IsTrue (file.lines[0] == L"Apple Pascal codefile, segments: 1");
        Assert::IsTrue (file.lines[1] == L"Intrinsic units: none");
        Assert::IsTrue (file.lines[3] == L"Segment 0: LINEFEED (LINKED)");
        Assert::IsTrue (file.lines[4] == L"  Block 2, 38 bytes, segment number 1, version 2");
        Assert::IsTrue (file.lines[5] == L"  Machine type: p-code, low byte first");
        Assert::IsTrue (file.lines[7] == L"  Procedure 1: lex level 0, parameters 4 bytes, data 2 bytes");
        Assert::IsTrue (file.lines[8] == L"    0000  D7           NOP");
        Assert::IsTrue (file.lines.back() == L"    0016  C1 00        RBP   0");
        Assert::IsTrue (file.notes.empty());
    }


    TEST_METHOD (Decode_NativeProcedureEndsAtItsRelocationTables)
    {
        //  Four instruction bytes, four empty relocation tables, the
        //  distance back to the first instruction, and an attribute table
        //  with a procedure number of 0.
        std::vector<Byte>  segment =
        {
            0xA9, 0x01, 0xEA, 0x60,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x0C, 0x00,
            0x00, 0x00,
            0x02, 0x00,
            0x01, 0x01,
        };
        std::vector<Byte>  bytes = MakeCodefile (segment, kNative);
        DecodedFile        file;
        std::wstring       error;



        AssertSucceeded (PascalCodefile::Decode (bytes, FileFacts(), DecodeOptions(), file, error));

        Assert::IsTrue (file.lines.size() == 11);
        Assert::IsTrue (file.lines[7] == L"  Procedure 1: 6502");
        Assert::IsTrue (file.lines[8].find  (L"LDA #$01") != std::wstring::npos);
        Assert::IsTrue (file.lines[9].find  (L"NOP")      != std::wstring::npos);
        Assert::IsTrue (file.lines[10].find (L"RTS")      != std::wstring::npos);
        Assert::IsTrue (file.notes.empty());
    }


    TEST_METHOD (Decode_OtherMachineIsListedInHexWithItsVersion)
    {
        std::vector<Byte>  bytes = MakeCodefile (MakeLinefeedSegment(), kPCodeHighFirst);
        DecodedFile        file;
        std::wstring       error;



        AssertSucceeded (PascalCodefile::Decode (bytes, FileFacts(), DecodeOptions(), file, error));

        Assert::IsTrue  (HasLine (file, L"  0000: D7 D7 C7 F1 40 91 AB 03 EA 00 00 01 88 C7 FF 00"));
        Assert::IsTrue  (HasLine (file, L"  0020: 01 00 02 00 01 01"));
        Assert::IsFalse (HasLine (file, L"Procedure"));
        Assert::IsTrue  (file.notes == std::vector<std::wstring> { L"Segment LINEFEED is listed in hex: it is p-code, high byte first, version 2." });
    }


    TEST_METHOD (Decode_DamagedDictionaryAndSegmentPastTheEnd)
    {
        std::vector<Byte>  damaged = MakeLinefeedSegment();
        std::vector<Byte>  bytes;
        DecodedFile        hex;
        DecodedFile        past;
        std::wstring       error;



        damaged[34] = 0xFF;
        bytes       = MakeCodefile (damaged, kPCodeLowFirst);

        AssertSucceeded (PascalCodefile::Decode (bytes, FileFacts(), DecodeOptions(), hex, error));
        Assert::IsTrue  (HasLine (hex, L"  0000: D7 D7"));
        Assert::IsTrue  (hex.notes == std::vector<std::wstring> { L"Segment LINEFEED is listed in hex: its procedure dictionary is damaged." });

        bytes[0] = 9;

        AssertSucceeded (PascalCodefile::Decode (bytes, FileFacts(), DecodeOptions(), past, error));
        Assert::IsTrue  (past.notes == std::vector<std::wstring> { L"Segment LINEFEED ends past the end of the file." });
    }


    TEST_METHOD (Decode_DataSegmentAndEmptyDictionary)
    {
        std::vector<Byte>  bytes (1024, 0);
        DecodedFile        empty;
        DecodedFile        data;
        std::wstring       error;



        AssertSucceeded (PascalCodefile::Decode (bytes, FileFacts(), DecodeOptions(), empty, error));
        Assert::IsTrue  (empty.notes == std::vector<std::wstring> { L"The segment dictionary is empty." });

        bytes[2]    = 0x40;
        bytes[0xC0] = 7;

        AssertSucceeded (PascalCodefile::Decode (bytes, FileFacts(), DecodeOptions(), data, error));
        Assert::IsTrue  (HasLine (data, L"(DATASEG)"));
        Assert::IsTrue  (HasLine (data, L"A data segment has no bytes in the file."));
        Assert::IsTrue  (data.notes.empty());
    }
};
