#include "Pch.h"

#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/Stamp/FormatStamp.h"
#include "Devices/Disk/Stamp/FormatStampChunk.h"
#include "Devices/Disk/Stamp/FormatStampJson.h"
#include "Machines/Apple2/Common/WozChunkWalker.h"
#include "Machines/Apple2/Common/WozLoader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampChunkTests
//
//  The CSSO chunk in a loaded image's WOZ metadata and in a WOZ file's
//  bytes, on synthetic images built in memory.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FormatStampChunkTests)
{
public:

    static constexpr size_t  kTrackBits = 51200;



    static vector<Byte> MakeWoz()
    {
        vector<Byte>  bits ((kTrackBits + 7) / 8, 0xFF);
        vector<Byte>  woz;



        WozLoader::BuildSyntheticV2 (WozLoader::kDiskType525, false, bits, kTrackBits, woz);

        return woz;
    }



    static FormatStamp MakeStamp (const char * writer)
    {
        FormatStamp  stamp;
        StampTrack   track;



        stamp.writer   = writer;
        track.position = 0;
        track.encoding = StampEncoding::Sixteen;
        track.addressPrologs = { { 0xD5, 0xAA, 0x96 } };
        stamp.tracks.push_back (track);

        return stamp;
    }



    static int CountStampChunks (const vector<Byte> & file)
    {
        vector<WozChunkSpan>  chunks;
        size_t                end   = 0;
        int                   count = 0;



        WozChunkWalker::Walk (file, chunks, end);

        for (const WozChunkSpan & chunk : chunks)
        {
            count += WozChunkWalker::IsChunk (chunk, FormatStampChunk::kId) ? 1 : 0;
        }

        return count;
    }



    TEST_METHOD (GetStamp_ImageWithoutAChunk_IsAbsent)
    {
        DiskImage    image;
        FormatStamp  stamp;



        WozLoader::Load (MakeWoz(), image);

        Assert::IsTrue (FormatStampChunk::GetStamp (image.GetWozMetadata(), stamp) == StampState::Absent);
    }



    TEST_METHOD (SetStamp_ThenSaveAndLoad_ReadsTheStamp)
    {
        DiskImage     image;
        DiskImage     reloaded;
        WozMetadata   meta;
        FormatStamp   read;
        vector<Byte>  saved;
        HRESULT       hr    = S_OK;



        WozLoader::Load (MakeWoz(), image);

        meta = image.GetWozMetadata();
        hr = FormatStampChunk::SetStamp (meta, MakeStamp ("first"));
        Assert::IsTrue (SUCCEEDED (hr));
        image.SetWozMetadata (meta);

        hr = WozLoader::Serialize (image, saved);
        Assert::IsTrue (SUCCEEDED (hr));
        hr = WozLoader::Load (saved, reloaded);
        Assert::IsTrue (SUCCEEDED (hr));

        Assert::IsTrue   (FormatStampChunk::GetStamp (reloaded.GetWozMetadata(), read) == StampState::Readable);
        Assert::AreEqual (std::string ("first"), read.writer);
        Assert::AreEqual ((size_t) 1, read.tracks.size());
    }



    TEST_METHOD (SetStamp_Twice_ReplacesTheChunk)
    {
        WozMetadata  meta;
        FormatStamp  read;



        FormatStampChunk::SetStamp (meta, MakeStamp ("first"));
        FormatStampChunk::SetStamp (meta, MakeStamp ("second"));

        Assert::AreEqual ((size_t) 1, meta.passThrough.size());
        Assert::IsTrue   (FormatStampChunk::GetStamp (meta, read) == StampState::Readable);
        Assert::AreEqual (std::string ("second"), read.writer);
    }



    TEST_METHOD (GetStamp_UnreadableText_IsUnreadableAndKeepsTheChunk)
    {
        DiskImage     image;
        DiskImage     reloaded;
        WozMetadata   meta;
        WozChunk      chunk;
        FormatStamp   read;
        vector<Byte>  saved;
        std::string   text  = "{ not a stamp";



        WozLoader::Load (MakeWoz(), image);

        meta = image.GetWozMetadata();
        memcpy (chunk.id, FormatStampChunk::kId, sizeof (chunk.id));
        chunk.payload.assign (text.begin(), text.end());
        meta.passThrough.push_back (chunk);
        image.SetWozMetadata (meta);

        Assert::IsTrue (FormatStampChunk::GetStamp (image.GetWozMetadata(), read) == StampState::Unreadable);

        WozLoader::Serialize (image, saved);
        WozLoader::Load (saved, reloaded);

        Assert::IsTrue (reloaded.GetWozMetadata().passThrough.back().payload == chunk.payload, L"a save writes an unreadable stamp back byte for byte");
    }



    TEST_METHOD (GetStamp_TwoChunks_UsesTheFirst)
    {
        WozMetadata  meta;
        WozMetadata  second;
        FormatStamp  read;



        FormatStampChunk::SetStamp (meta,   MakeStamp ("first"));
        FormatStampChunk::SetStamp (second, MakeStamp ("second"));
        meta.passThrough.push_back (second.passThrough[0]);

        Assert::IsTrue   (FormatStampChunk::GetStamp (meta, read) == StampState::Readable);
        Assert::AreEqual (std::string ("first"), read.writer);
    }



    TEST_METHOD (RemoveStamp_RemovesEveryStampChunk)
    {
        WozMetadata  meta;
        WozMetadata  second;



        FormatStampChunk::SetStamp (meta,   MakeStamp ("first"));
        FormatStampChunk::SetStamp (second, MakeStamp ("second"));
        meta.passThrough.push_back (second.passThrough[0]);

        FormatStampChunk::RemoveStamp (meta);

        Assert::IsFalse (FormatStampChunk::HasStampChunk (meta));
    }



    TEST_METHOD (SetStampInFile_ChangesOnlyTheChunkAndTheCrc)
    {
        vector<Byte>  original = MakeWoz();
        vector<Byte>  file     = original;
        std::string   text;
        StampState    state    = StampState::Absent;
        FormatStamp   read;
        HRESULT       hr       = S_OK;



        hr = FormatStampChunk::SetStampInFile (file, MakeStamp ("first"));
        Assert::IsTrue (SUCCEEDED (hr));

        Assert::IsTrue (std::equal (original.begin() + WozChunkWalker::kHeaderSize, original.end(), file.begin() + WozChunkWalker::kHeaderSize), L"every byte before the new chunk is the original's");
        Assert::IsTrue (std::equal (original.begin(), original.begin() + WozChunkWalker::kCrcOffset, file.begin()), L"the signature is the original's");
        Assert::AreEqual (WozChunkWalker::ComputeCrc (file), WozChunkWalker::ReadSize (file.data() + WozChunkWalker::kCrcOffset));

        hr = FormatStampChunk::GetStampFromFile (file, text, state, read);
        Assert::IsTrue   (SUCCEEDED (hr));
        Assert::IsTrue   (state == StampState::Readable);
        Assert::AreEqual (std::string ("first"), read.writer);
    }



    TEST_METHOD (SetStampInFile_Twice_LeavesOneChunk)
    {
        vector<Byte>  file  = MakeWoz();
        std::string   text;
        StampState    state = StampState::Absent;
        FormatStamp   read;



        FormatStampChunk::SetStampInFile (file, MakeStamp ("first"));
        FormatStampChunk::SetStampInFile (file, MakeStamp ("second"));

        Assert::AreEqual (1, CountStampChunks (file));

        FormatStampChunk::GetStampFromFile (file, text, state, read);
        Assert::AreEqual (std::string ("second"), read.writer);
    }



    TEST_METHOD (SetStampInFile_BadCrc_LeavesTheFileAlone)
    {
        vector<Byte>  file   = MakeWoz();
        vector<Byte>  before;
        HRESULT       hr     = S_OK;



        file[WozChunkWalker::kCrcOffset] ^= 0x01;
        before = file;

        hr = FormatStampChunk::SetStampInFile (file, MakeStamp ("first"));

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_CRC), hr);
        Assert::IsTrue   (file == before);
    }



    TEST_METHOD (GetStampFromFile_NotAWoz_IsInvalidData)
    {
        vector<Byte>  file (64, 0);
        std::string   text;
        StampState    state = StampState::Absent;
        FormatStamp   read;



        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), FormatStampChunk::GetStampFromFile (file, text, state, read));
    }
};
