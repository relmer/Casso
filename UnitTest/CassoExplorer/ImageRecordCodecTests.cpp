#include "Pch.h"

#include "CassoExplorer/Model/ImageRecordCodec.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ImageRecordCodecTests
//
//  An image record formatted and parsed back, with made-up contents.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ImageRecordCodecTests)
{
public:

    static ImageRecordCodec::Record MakeRecord()
    {
        ImageRecordCodec::Record  record;
        FileEntry                 entry;
        PartitionEntry            partition;



        entry.name         = std::string ("HELLO\x8D\x00X", 7);
        entry.type         = 0xFC;
        entry.eofBytes     = 1234;
        entry.modifiedUnix = 1700000000;
        entry.hasModified  = true;
        entry.catalogIndex = 3;

        partition.partition.index = 2;
        partition.partition.name  = "Apple_PRODOS";
        partition.step            = "#2.po";
        partition.byteCount       = 33554432;

        record.hash                           = "abc123";
        record.savedGame                      = IImageRecords::SavedGame::None;
        record.hasContents                    = true;
        record.contents.result.hr             = HRESULT_FROM_WIN32 (ERROR_INVALID_DATA);
        record.contents.result.message        = "no file system";
        record.contents.kind                  = VolumeKind::ProDos;
        record.contents.listing.volumeName    = "GAMES";
        record.contents.listing.hasVolumeName = true;
        record.contents.listing.totalUnits    = 280;
        record.contents.listing.entries.push_back (entry);
        record.contents.listing.damage.push_back ("block 7 unreadable");
        record.contents.subdirectories.push_back ({ "SAVES", "SAVES", true });
        record.contents.partitions.push_back (partition);
        record.contents.error                 = L"The disk has no file system.";
        record.contents.broken                = true;

        return record;
    }



    TEST_METHOD (AFormattedRecord_ParsesBackTheSame)
    {
        ImageRecordCodec::Record  record   = MakeRecord();
        ImageRecordCodec::Record  parsed;
        bool                      isParsed = false;



        isParsed = ImageRecordCodec::Parse (ImageRecordCodec::Format (record), parsed);

        Assert::IsTrue   (isParsed);
        Assert::AreEqual (record.hash, parsed.hash);
        Assert::IsTrue   (parsed.savedGame == IImageRecords::SavedGame::None);
        Assert::IsTrue   (parsed.hasContents);
        Assert::AreEqual (record.contents.result.hr, parsed.contents.result.hr);
        Assert::AreEqual (record.contents.result.message, parsed.contents.result.message);
        Assert::IsTrue   (parsed.contents.kind == VolumeKind::ProDos);
        Assert::AreEqual (std::string ("GAMES"), parsed.contents.listing.volumeName);
        Assert::AreEqual ((size_t) 1, parsed.contents.listing.entries.size());
        Assert::AreEqual (record.contents.listing.entries[0].name, parsed.contents.listing.entries[0].name);
        Assert::AreEqual ((int64_t) 1700000000, parsed.contents.listing.entries[0].modifiedUnix);
        Assert::AreEqual ((size_t) 3, parsed.contents.listing.entries[0].catalogIndex);
        Assert::AreEqual (std::string ("block 7 unreadable"), parsed.contents.listing.damage[0]);
        Assert::AreEqual (std::string ("SAVES"), parsed.contents.subdirectories[0].name);
        Assert::AreEqual (std::string ("#2.po"), parsed.contents.partitions[0].step);
        Assert::AreEqual ((uint64_t) 33554432, parsed.contents.partitions[0].byteCount);
        Assert::AreEqual (record.contents.error, parsed.contents.error);
        Assert::IsTrue   (parsed.contents.broken);
    }



    TEST_METHOD (ARecordOfAnotherVersion_DoesNotParse)
    {
        std::string               json   = ImageRecordCodec::Format (MakeRecord());
        ImageRecordCodec::Record  parsed;
        size_t                    at     = json.find ("\"version\"");



        Assert::AreNotEqual (std::string::npos, at);

        json.replace (json.find_first_of ("0123456789", at), 1, "9");

        Assert::IsFalse (ImageRecordCodec::Parse (json, parsed));
    }



    TEST_METHOD (ARecordWithoutContents_ParsesWithNone)
    {
        ImageRecordCodec::Record  record;
        ImageRecordCodec::Record  parsed;



        record.hash      = "ff";
        record.savedGame = IImageRecords::SavedGame::Present;

        Assert::IsTrue  (ImageRecordCodec::Parse (ImageRecordCodec::Format (record), parsed));
        Assert::IsFalse (parsed.hasContents);
        Assert::IsTrue  (parsed.savedGame == IImageRecords::SavedGame::Present);
    }



    TEST_METHOD (DamagedText_DoesNotParse)
    {
        std::string               text;

        Assert::IsFalse (ImageRecordCodec::DecodeHex ("4g", text));
        Assert::IsFalse (ImageRecordCodec::DecodeHex ("414", text));
        Assert::IsTrue  (ImageRecordCodec::DecodeHex ("4142", text));
        Assert::AreEqual (std::string ("AB"), text);
    }
};
