#include "Pch.h"
#include "../EhmTestHelper.h"
#include "CassoExplorer/Model/PreviewDecoder.h"
#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"
#include "CassoExplorer/Window/Managers/ExplorerPreviewManager.h"
#include "Core/MemoryBus.h"
#include "FileTypes/FileFormatTable.h"
#include "FileTypes/PascalText.h"
#include "FileTypes/TextCharset.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CharsetTests
//
//  The character set and record length a file is decoded with: the set a
//  format is read in when none is chosen, a chosen set, and a replaced
//  record length, through the table and through a preview.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CharsetTests)
{
public:

    TEST_METHOD (HasCharsetChoice_IsTrueForTheTextFormatsOnly)
    {
        Assert::IsTrue  (FileFormatTable::HasCharsetChoice (FileFormat::PascalText));
        Assert::IsTrue  (FileFormatTable::HasCharsetChoice (FileFormat::RandomText));
        Assert::IsTrue  (FileFormatTable::HasCharsetChoice (FileFormat::Teach));
        Assert::IsTrue  (FileFormatTable::HasCharsetChoice (FileFormat::AppleWorksGsWp));
        Assert::IsFalse (FileFormatTable::HasCharsetChoice (FileFormat::Merlin));
        Assert::IsFalse (FileFormatTable::HasCharsetChoice (FileFormat::None));
    }


    TEST_METHOD (Resolve_DefaultIsMacOsRomanForAIigsFormatOnly)
    {
        Assert::IsTrue (TextCharset::Resolve (Charset::Default,   false) == Charset::AppleText);
        Assert::IsTrue (TextCharset::Resolve (Charset::Default,   true)  == Charset::MacOsRoman);
        Assert::IsTrue (TextCharset::Resolve (Charset::Iso8859_1, true)  == Charset::Iso8859_1);
        Assert::IsTrue (TextCharset::Resolve (Charset::AppleText, true)  == Charset::AppleText);
    }


    TEST_METHOD (Decode_APascalTextFile_HasTheSetItWasReadIn)
    {
        std::vector<Byte>  bytes;
        DecodedFile        file;
        std::wstring       error;



        AssertSucceeded (PascalText::Encode ({ "BEGIN", "END." }, bytes, error));

        AssertSucceeded (FileFormatTable::Decode (FileFormat::PascalText, bytes, FileFacts(), DecodeOptions(), file, error));
        Assert::IsTrue  (file.charset == Charset::AppleText);
        Assert::AreEqual ((size_t) 0, file.recordLength);

        file = DecodedFile();
        AssertSucceeded (FileFormatTable::Decode (FileFormat::PascalText, bytes, FileFacts(), DecodeOptions { 0, Charset::Iso8859_1 }, file, error));
        Assert::IsTrue  (file.charset == Charset::Iso8859_1);
        Assert::AreEqual (std::wstring (L"BEGIN"), file.lines[0]);
    }


    TEST_METHOD (Decode_ARandomTextFile_InEachSet)
    {
        std::vector<Byte>  bytes (64, 0);
        FileFacts          facts;
        DecodedFile        file;
        std::wstring       error;



        bytes[0]  = 'A';
        bytes[1]  = 0x8E;
        bytes[2]  = 0x0D;
        bytes[32] = 0xE9;
        bytes[33] = 0x0D;

        facts.volume     = VolumeKind::ProDos;
        facts.auxType    = 32;
        facts.hasAuxType = true;

        AssertSucceeded (FileFormatTable::Decode (FileFormat::RandomText, bytes, facts, DecodeOptions { 0, Charset::MacOsRoman }, file, error));
        Assert::IsTrue   (file.charset == Charset::MacOsRoman);
        Assert::AreEqual ((size_t) 32, file.recordLength);
        Assert::AreEqual (std::wstring (L"A\x00E9"), file.grid.rows[0][1]);

        file = DecodedFile();
        AssertSucceeded (FileFormatTable::Decode (FileFormat::RandomText, bytes, facts, DecodeOptions { 0, Charset::Iso8859_1 }, file, error));
        Assert::IsTrue   (file.charset == Charset::Iso8859_1);
        Assert::AreEqual (std::wstring (L"\x00E9"), file.grid.rows[1][1]);

        file = DecodedFile();
        AssertSucceeded (FileFormatTable::Decode (FileFormat::RandomText, bytes, facts, DecodeOptions(), file, error));
        Assert::IsTrue   (file.charset == Charset::AppleText);
        Assert::AreEqual (std::wstring (L"i"), file.grid.rows[1][1]);
    }


    TEST_METHOD (Decode_ARandomTextFile_ByAReplacedRecordLength)
    {
        std::vector<Byte>  bytes (64, 0);
        FileFacts          facts;
        DecodedFile        file;
        std::wstring       error;



        for (size_t at = 0; at < bytes.size(); at += 16)
        {
            bytes[at]     = (Byte) (0xC1 + at / 16);
            bytes[at + 1] = 0x8D;
        }

        facts.volume     = VolumeKind::ProDos;
        facts.auxType    = 32;
        facts.hasAuxType = true;

        AssertSucceeded (FileFormatTable::Decode (FileFormat::RandomText, bytes, facts, DecodeOptions(), file, error));
        Assert::AreEqual ((size_t) 32, file.recordLength);
        Assert::AreEqual ((size_t) 2,  file.grid.rows.size());

        file = DecodedFile();
        AssertSucceeded (FileFormatTable::Decode (FileFormat::RandomText, bytes, facts, DecodeOptions { 16, Charset::Default }, file, error));
        Assert::AreEqual ((size_t) 16, file.recordLength);
        Assert::AreEqual ((size_t) 4,  file.grid.rows.size());
        Assert::AreEqual (std::wstring (L"D"), file.grid.rows[3][1]);
    }


    TEST_METHOD (Decode_AFormatWithOneSet_HasNoChoice)
    {
        std::vector<Byte>  bytes = { 0xC1, 0x8D };
        DecodedFile        file;
        std::wstring       error;



        AssertSucceeded (FileFormatTable::Decode (FileFormat::Merlin, bytes, FileFacts(), DecodeOptions { 0, Charset::MacOsRoman }, file, error));
        Assert::IsTrue  (file.charset == Charset::Default);
    }


    TEST_METHOD (Render_ARandomTextFile_HasItsSetAndRecordLength)
    {
        FileEntry       entry;
        FilePayload     payload;
        PreviewContent  content;
        MemoryBus       bus;



        entry.name       = "DATA";
        entry.type       = 0x04;
        entry.auxType    = 32;
        entry.hasAuxType = true;
        payload.bytes.assign (64, 0);
        payload.bytes[0]  = 0xC1;
        payload.bytes[1]  = 0x8D;
        payload.bytes[32] = 0xC2;
        payload.bytes[33] = 0x8D;

        AssertSucceeded (PreviewDecoder::Render (entry, VolumeKind::ProDos, payload, PreviewDecoder::HexForm::Auto, bus, content));
        Assert::IsTrue   (content.kind == PreviewContent::Kind::Grid);
        Assert::IsTrue   (content.charset == Charset::AppleText);
        Assert::AreEqual ((size_t) 32, content.recordLength);

        AssertSucceeded (PreviewDecoder::Render (entry, VolumeKind::ProDos, payload, PreviewDecoder::HexForm::Auto, bus, content, PicturePreview::Monitor::Color, PicturePreview::kDefaultCustomArgb, DecodeOptions { 16, Charset::Iso8859_1 }));
        Assert::IsTrue   (content.charset == Charset::Iso8859_1);
        Assert::AreEqual ((size_t) 16, content.recordLength);
    }


    TEST_METHOD (Render_AppleTextByType_IsReadAgainInAChosenSet)
    {
        FileEntry       entry;
        FilePayload     payload;
        PreviewContent  content;
        MemoryBus       bus;



        entry.name       = "NOTES";
        entry.type       = 0x04;
        payload.bytes    = { 'c', 'a', 'f', 0xC4, 0x0D, 'o', 'k', 0x0D };
        payload.textForm = PayloadTextForm::AppleHighAscii;

        AssertSucceeded (PreviewDecoder::Render (entry, VolumeKind::ProDos, payload, PreviewDecoder::HexForm::Auto, bus, content));
        Assert::IsTrue  (content.kind == PreviewContent::Kind::Text);
        Assert::IsTrue  (content.charset == Charset::AppleText);

        AssertSucceeded (PreviewDecoder::Render (entry, VolumeKind::ProDos, payload, PreviewDecoder::HexForm::Auto, bus, content, PicturePreview::Monitor::Color, PicturePreview::kDefaultCustomArgb, DecodeOptions { 0, Charset::MacOsRoman }));
        Assert::IsTrue   (content.charset == Charset::MacOsRoman);
        Assert::AreEqual (std::wstring (L"caf\x0192"), content.lines[0]);
        Assert::AreEqual (std::wstring (L"ok"),        content.lines[1]);
    }


    TEST_METHOD (TryParseRecordLength_TakesDigitsUpToTheLongestLength)
    {
        size_t  length = 7;



        Assert::IsTrue   (ExplorerPreviewManager::TryParseRecordLength (L"128", length));
        Assert::AreEqual ((size_t) 128, length);
        Assert::IsTrue   (ExplorerPreviewManager::TryParseRecordLength (L"", length));
        Assert::AreEqual ((size_t) 0, length);
        Assert::IsTrue   (ExplorerPreviewManager::TryParseRecordLength (L"65535", length));
        Assert::IsFalse  (ExplorerPreviewManager::TryParseRecordLength (L"65536", length));
        Assert::IsFalse  (ExplorerPreviewManager::TryParseRecordLength (L"12a", length));
        Assert::IsFalse  (ExplorerPreviewManager::TryParseRecordLength (L"-1", length));
        Assert::AreEqual ((size_t) 65535, length);
    }
};