#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "AppleTextCodec.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/Model/CrossVolumeCopy.h"
#include "CommandLineOptions.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/CpmVolume.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/PascalVolume.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CrossVolumeCopyTests
//
//  Files copied through real volumes in every direction: written on one file
//  system in its form, read, converted, written on another, and read again.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CrossVolumeCopyTests)
{
public:

    static constexpr size_t  kImageBytes = 143360;

    static std::vector<Byte>  MakeBlank (VolumeKind kind)
    {
        std::vector<Byte>  image (kImageBytes, 0);

        switch (kind)
        {
            case VolumeKind::Dos33:  AssertSucceeded (Dos33Skeleton::Write (image, 254));        break;
            case VolumeKind::ProDos: AssertSucceeded (ProDosSkeleton::Write (image, "COPY"));    break;
            case VolumeKind::Pascal: PascalVolume::WriteBlank ("COPY", 0, image);                break;
            default:                 CpmVolume::WriteBlank (image);                              break;
        }

        return image;
    }

    //  Host text in `kind`'s form, as a payload to write there.
    static FilePayload  MakeText (VolumeKind kind, const std::string & text)
    {
        FilePayload  payload;
        size_t       bad = 0;

        switch (kind)
        {
            case VolumeKind::Pascal:
                AssertSucceeded (PascalVolume::EncodeText (text, payload.bytes, bad));
                payload.type = PascalVolume::kKindText;
                break;

            case VolumeKind::Cpm:
                CpmVolume::EncodeText (text, payload.bytes);
                break;

            default:
                AssertSucceeded (AppleTextCodec::Encode (text, AppleTextConvention::HighAscii, payload.bytes, bad));
                payload.type = (kind == VolumeKind::Dos33) ? Dos33Volume::kTypeText : ProDosVolume::kTypeText;
                break;
        }

        return payload;
    }

    static void  CopyThrough (VolumeKind source, VolumeKind target, const FilePayload & original, FilePayload & outRead, std::wstring & outNote, const char * name = "NOTES")
    {
        std::vector<Byte>  from      = MakeBlank (source);
        std::vector<Byte>  to        = MakeBlank (target);
        std::vector<Byte>  edited;
        FilePayload        payload;

        AssertSucceeded (VolumeImage::CreateVolume (source, from)->Write (FilePath::FromName (name), original, edited));
        AssertSucceeded (VolumeImage::CreateVolume (source, edited)->Read (FilePath::FromName (name), payload));

        CrossVolumeCopy::Convert (source, target, name, payload, outNote);

        AssertSucceeded (VolumeImage::CreateVolume (target, to)->Write (FilePath::FromName (name), payload, edited));
        AssertSucceeded (VolumeImage::CreateVolume (target, edited)->Read (FilePath::FromName (name), outRead));
    }

    static std::string  DecodeForTest (VolumeKind kind, const FilePayload & payload)
    {
        std::string  text;

        if (kind == VolumeKind::Pascal)
        {
            PascalVolume::DecodeText (payload.bytes, text);
        }
        else if (kind == VolumeKind::Cpm)
        {
            CpmVolume::DecodeText (payload.bytes, text);
        }
        else
        {
            AppleTextCodec::Decode (payload.bytes, AppleTextConvention::HighAscii, text);
        }

        //  Line ends are compared as one form.
        text.erase (std::remove (text.begin(), text.end(), '\r'), text.end());

        return text;
    }

    TEST_METHOD (TextSurvivesEveryDirection)
    {
        const VolumeKind   kinds[] = { VolumeKind::Dos33, VolumeKind::ProDos, VolumeKind::Pascal, VolumeKind::Cpm };
        const std::string  lines   = "FIRST LINE\n    INDENTED\n\nLOWER case 123\n";
        int                checked = 0;

        for (VolumeKind source : kinds)
        {
            for (VolumeKind target : kinds)
            {
                FilePayload   read;
                std::wstring  note;

                if (source == target)
                {
                    continue;
                }

                CopyThrough (source, target, MakeText (source, lines), read, note);

                Assert::AreEqual (lines, DecodeForTest (target, read),
                                  std::format (L"{} to {}", (int) source, (int) target).c_str());
                Assert::IsTrue (note.empty());

                checked++;
            }
        }

        Assert::AreEqual (12, checked);
    }

    TEST_METHOD (TextTakesTheTargetsType)
    {
        FilePayload   read;
        std::wstring  note;

        CopyThrough (VolumeKind::Pascal, VolumeKind::ProDos, MakeText (VolumeKind::Pascal, "A\n"), read, note);
        Assert::AreEqual ((int) 0x04, (int) read.type, L"ProDOS TXT");

        CopyThrough (VolumeKind::Pascal, VolumeKind::Dos33, MakeText (VolumeKind::Pascal, "A\n"), read, note);
        Assert::AreEqual ((int) Dos33Volume::kTypeText, (int) read.type);

        CopyThrough (VolumeKind::ProDos, VolumeKind::Pascal, MakeText (VolumeKind::ProDos, "A\n"), read, note);
        Assert::AreEqual ((int) PascalVolume::kKindText, (int) read.type);
    }

    TEST_METHOD (TheTargetsFormIsOnTheDisk)
    {
        FilePayload   read;
        std::wstring  note;

        CopyThrough (VolumeKind::Pascal, VolumeKind::Dos33, MakeText (VolumeKind::Pascal, "    A\n"), read, note);
        Assert::AreEqual ((int) (' ' | 0x80), (int) read.bytes[0], L"The indent is spaces, with high bits");
        Assert::AreEqual ((int) 0x8D, (int) read.bytes[5], L"An Apple line end");

        CopyThrough (VolumeKind::Dos33, VolumeKind::Pascal, MakeText (VolumeKind::Dos33, "    A\n"), read, note);
        Assert::AreEqual ((size_t) 2048, read.bytes.size(), L"A header of two blocks and a page");
        Assert::AreEqual ((int) 0x10, (int) read.bytes[1024], L"DLE");
        Assert::AreEqual ((int) (0x20 + 4), (int) read.bytes[1025], L"Four spaces");
        Assert::AreEqual ((int) 'A', (int) read.bytes[1026]);
        Assert::AreEqual ((int) '\r', (int) read.bytes[1027]);

        CopyThrough (VolumeKind::Dos33, VolumeKind::Cpm, MakeText (VolumeKind::Dos33, "A\n"), read, note);
        Assert::AreEqual (std::string ("A\r\n\x1A"), std::string (read.bytes.begin(), read.bytes.begin() + 4));
    }

    TEST_METHOD (CpmTextIsANameOrAllTextBytes)
    {
        FilePayload  payload;

        payload.bytes = { 'H', 'I', '\r', '\n', 0x1A, 0xC3, 0x00 };
        Assert::IsTrue (CrossVolumeCopy::IsText (VolumeKind::Cpm, "ANY.DAT", payload), L"The bytes up to the end are text");

        payload.bytes = { 0xC3, 0x00, 0x01, 0x05 };
        Assert::IsFalse (CrossVolumeCopy::IsText (VolumeKind::Cpm, "ANY.COM", payload));
        Assert::IsTrue  (CrossVolumeCopy::IsText (VolumeKind::Cpm, "ANY.TXT", payload), L"The name decides");
    }

    TEST_METHOD (HighBitsOnCpmTextAreRemoved)
    {
        FilePayload   payload;
        std::string   text;

        payload.bytes = { 'H', 'I' | 0x80, '\r', '\n', 0x1A };

        CpmVolume::DecodeText (payload.bytes, text);

        Assert::AreEqual (std::string ("HI\r\n"), text);
    }

    TEST_METHOD (BinariesKeepTheirBytesWithATypeThatIsNotRunnable)
    {
        FilePayload        original;
        FilePayload        read;
        std::wstring       note;
        std::vector<Byte>  bytes (600);

        for (size_t i = 0; i < bytes.size(); i++)
        {
            bytes[i] = (Byte) (i * 3 + 0x80);
        }

        original.bytes = bytes;
        original.type  = PascalVolume::kKindCode;

        CopyThrough (VolumeKind::Pascal, VolumeKind::Dos33, original, read, note, "TOOL.CODE");
        Assert::AreEqual ((int) Dos33Volume::kTypeSpecial, (int) read.type);
        Assert::IsFalse (read.hasLoadAddress);
        Assert::IsTrue (std::equal (bytes.begin(), bytes.end(), read.bytes.begin()));

        CopyThrough (VolumeKind::Pascal, VolumeKind::ProDos, original, read, note, "TOOL.CODE");
        Assert::AreEqual ((int) 0x02, (int) read.type, L"ProDOS's Pascal code type");
        Assert::IsTrue (bytes == read.bytes);

        original.type = 0;

        CopyThrough (VolumeKind::Cpm, VolumeKind::ProDos, original, read, note, "TOOL.COM");
        Assert::AreEqual ((int) 0, (int) read.type);
        Assert::IsTrue (std::equal (bytes.begin(), bytes.end(), read.bytes.begin()));

        CopyThrough (VolumeKind::Cpm, VolumeKind::Dos33, original, read, note, "TOOL.COM");
        Assert::AreEqual ((int) Dos33Volume::kTypeSpecial, (int) read.type);
        Assert::IsTrue (std::equal (bytes.begin(), bytes.end(), read.bytes.begin()));
    }

    TEST_METHOD (APictureIsABinaryAtTheHiResPage)
    {
        FilePayload   payload;
        std::wstring  note;

        payload.type = PascalVolume::kKindFoto;
        payload.bytes.assign (8192, 0);

        CrossVolumeCopy::Convert (VolumeKind::Pascal, VolumeKind::Dos33, "PIC.FOTO", payload, note);

        Assert::AreEqual ((int) Dos33Volume::kTypeBinary, (int) payload.type);
        Assert::AreEqual ((int) 0x2000, (int) payload.loadAddress);
        Assert::IsTrue (payload.hasLoadAddress);
    }

    TEST_METHOD (ALoadAddressIsKeptOrReported)
    {
        FilePayload   original;
        FilePayload   read;
        std::wstring  note;

        original.bytes          = { 1, 2, 3, 4 };
        original.type           = Dos33Volume::kTypeBinary;
        original.loadAddress    = 0x6000;
        original.hasLoadAddress = true;

        CopyThrough (VolumeKind::Dos33, VolumeKind::ProDos, original, read, note);
        Assert::AreEqual ((int) 0x6000, (int) read.loadAddress, L"ProDOS stores it");
        Assert::IsTrue (note.empty());

        CopyThrough (VolumeKind::Dos33, VolumeKind::Pascal, original, read, note);
        Assert::IsTrue (note.find (L"$6000") != std::wstring::npos, note.c_str());

        CopyThrough (VolumeKind::Dos33, VolumeKind::Cpm, original, read, note);
        Assert::IsTrue (note.find (L"CP/M") != std::wstring::npos, note.c_str());
    }

    TEST_METHOD (TextNamesFitTheTarget)
    {
        Assert::AreEqual (std::string ("NOTES.TEXT"),      CrossVolumeCopy::FitTextName ("NOTES", VolumeKind::Pascal));
        Assert::AreEqual (std::string ("NOTES.TEXT"),      CrossVolumeCopy::FitTextName ("NOTES.TXT", VolumeKind::Pascal));
        Assert::AreEqual (std::string ("NOTES.TEXT"),      CrossVolumeCopy::FitTextName ("NOTES.TEXT", VolumeKind::Pascal));
        Assert::AreEqual (std::string ("FIFTEENCHARSLON"), CrossVolumeCopy::FitTextName ("FIFTEENCHARSLON", VolumeKind::Pascal));
        Assert::AreEqual (std::string ("NOTES.TXT"),       CrossVolumeCopy::FitTextName ("NOTES.TEX", VolumeKind::Cpm));
        Assert::AreEqual (std::string ("NOTES"),           CrossVolumeCopy::FitTextName ("NOTES", VolumeKind::ProDos));
    }

    //  To the host and back, through `disk get --text` and `disk put --text`.
    TEST_METHOD (TextGoesToAndFromTheHost)
    {
        struct { const char * format; const char * name; }  rows[] = { { "pascal", "NOTES.TEXT" }, { "cpm", "NOTES.TXT" } };

        for (const auto & row : rows)
        {
            FakeDiskFileIo      io;
            DiskCommandRunner   runner (io);
            CommandLineOptions  options;
            DiskCommandResult   result;
            std::string         lines = "ONE\r\n    TWO\r\n";

            options.disk.command    = CommandLineOptions::DiskOptions::Command::Create;
            options.disk.imagePath  = "C:\\disks\\t.dsk";
            options.disk.formatName = row.format;

            Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

            io.files["C:\\host\\in.txt"].assign (lines.begin(), lines.end());
            io.stamps["C:\\host\\in.txt"] = FileStamp { lines.size(), 1 };

            options                = CommandLineOptions();
            options.disk.command   = CommandLineOptions::DiskOptions::Command::Put;
            options.disk.imagePath = "C:\\disks\\t.dsk";
            options.disk.hostFile  = "C:\\host\\in.txt";
            options.disk.path      = row.name;
            options.disk.encoding  = CommandLineOptions::DiskOptions::Encoding::Text;
            result                 = runner.Run (options);

            Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus, TextEncoding::NarrowToWide (result.diagnostics).c_str());

            options                = CommandLineOptions();
            options.disk.command   = CommandLineOptions::DiskOptions::Command::Get;
            options.disk.imagePath = "C:\\disks\\t.dsk";
            options.disk.path      = row.name;
            options.disk.encoding  = CommandLineOptions::DiskOptions::Encoding::Text;
            result                 = runner.Run (options);

            Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
            Assert::AreEqual (lines, std::string (result.payload.begin(), result.payload.end()));
        }
    }

    TEST_METHOD (ExplorerCopiesConvertCpmTextByName)
    {
        FileEntry  entry;

        entry.name = "NOTES.TXT";
        Assert::IsTrue (CassoExplorerActions::GetEncoding (entry, VolumeKind::Cpm) == ActionsTypes::Encoding::Text);

        entry.name = "PIP.COM";
        Assert::IsTrue (CassoExplorerActions::GetEncoding (entry, VolumeKind::Cpm) == ActionsTypes::Encoding::Verbatim);
    }
};
