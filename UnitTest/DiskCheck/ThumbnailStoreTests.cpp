#include "Pch.h"

#include "../EhmTestHelper.h"
#include "DiskCheck/ThumbnailStore.h"
#include "Devices/Disk/DurableCommit.h"
#include "Seams/Win32DiskFileIo.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailStoreTests
//
//  A disk image's thumbnail in the cache and in the image file's stream,
//  over a folder of the test's in the temporary folder.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ThumbnailStoreTests)
{
public:

    struct Rig
    {
        std::filesystem::path  root;
        std::filesystem::path  image;

        Rig()
        {
            static int  s_next = 0;

            root  = std::filesystem::temp_directory_path() / std::format (L"CassoThumbs{}_{}", ::GetCurrentProcessId(), s_next++);
            image = root / L"disks" / L"0001.dsk";

            std::filesystem::remove_all (root);
            std::filesystem::create_directories (image.parent_path());

            Write (image, std::vector<Byte> (4096, 0x5A));
        }

        ~Rig()
        {
            std::error_code  error;

            std::filesystem::remove_all (root, error);
        }

        static void  Write (const std::filesystem::path & path, const std::vector<Byte> & bytes)
        {
            std::ofstream  out (path, std::ios::binary | std::ios::trunc);

            out.write ((const char *) bytes.data(), (std::streamsize) bytes.size());
        }

        static std::vector<Byte>  Read (const std::filesystem::path & path)
        {
            std::ifstream  in (path, std::ios::binary);

            return std::vector<Byte> (std::istreambuf_iterator<char> (in), std::istreambuf_iterator<char>());
        }

        std::filesystem::path  GetStream (const std::filesystem::path & file) const
        {
            return std::filesystem::path (file.wstring() + ThumbnailStore::kStreamName);
        }
    };

    //  A thumbnail stored for a file is found for the file renamed and for
    //  a copy of its bytes in another folder, with its record; the stream
    //  is written, and the file's last write time is as it was.
    TEST_METHOD (AThumbnail_IsFoundByContent_ForARenamedFileAndACopy)
    {
        Rig                    rig;
        ThumbnailStore         store (rig.root / L"cache");
        std::vector<Byte>  png       = { 0x89, 'P', 'N', 'G', 1, 2, 3 };
        std::vector<Byte>  found;
        ThumbnailRecord    record;
        ThumbnailRecord    read;
        bool               isUserSet = true;
        std::filesystem::path  renamed = rig.image.parent_path() / L"Arcticfox.dsk";
        std::filesystem::path  copy    = rig.root / L"elsewhere.dsk";
        auto                   before  = std::filesystem::last_write_time (rig.image);



        record.verdict = "booted";
        record.machine = "Apple2c";
        record.title   = "ARCTICFOX";

        AssertSucceeded (store.Find (rig.image, found, read, isUserSet));
        Assert::IsTrue (found.empty(), L"Nothing before a store");

        AssertSucceeded (store.Store (rig.image, png, record));

        Assert::IsTrue (std::filesystem::last_write_time (rig.image) == before, L"The image's last write time is as it was");
        Assert::IsTrue (Rig::Read (rig.GetStream (rig.image)) == png, L"The stream has the thumbnail");

        std::filesystem::rename (rig.image, renamed);
        Rig::Write (copy, Rig::Read (renamed));

        Assert::AreEqual (S_OK, store.Find (renamed, found, read, isUserSet));
        Assert::IsTrue (found == png);
        Assert::AreEqual (std::string ("booted"), read.verdict);
        Assert::AreEqual (std::string ("ARCTICFOX"), read.title);
        Assert::IsFalse (isUserSet);

        Assert::AreEqual (S_OK, store.Find (copy, found, read, isUserSet), L"A copy of the bytes, with no stream");
        Assert::IsTrue (found == png);
        Assert::IsTrue (Rig::Read (rig.GetStream (copy)) == png, L"The copy's stream is written from the cache");
    }

    //  A file that arrives with its stream and no entry in the cache has
    //  the stream's thumbnail, which is taken into the cache.
    TEST_METHOD (AStreamWithNoCacheEntry_IsTakenIntoTheCache)
    {
        Rig                rig;
        ThumbnailStore     first  (rig.root / L"cache1");
        ThumbnailStore     second (rig.root / L"cache2");
        std::vector<Byte>  png       = { 0x89, 'P', 'N', 'G', 9 };
        std::vector<Byte>  found;
        ThumbnailRecord    read;
        bool               isUserSet = false;



        AssertSucceeded (first.Store (rig.image, png, ThumbnailRecord()));

        Assert::AreEqual (S_OK, second.Find (rig.image, found, read, isUserSet), L"Another PC's cache has nothing; the stream has it");
        Assert::IsTrue (found == png);

        Rig::Write (rig.GetStream (rig.image), {});
        std::filesystem::remove (rig.GetStream (rig.image));

        Assert::AreEqual (S_OK, second.Find (rig.image, found, read, isUserSet), L"Now from the second cache");
        Assert::IsTrue (found == png);
    }

    //  A thumbnail the user set is returned in place of the booted one and
    //  outlives Clear until it is included; Remove restores the booted one.
    TEST_METHOD (AThumbnailTheUserSet_ReplacesTheBootedOne_UntilRemoved)
    {
        Rig                rig;
        ThumbnailStore     store (rig.root / L"cache");
        std::vector<Byte>  booted    = { 0x89, 'P', 'N', 'G', 1 };
        std::vector<Byte>  chosen    = { 0x89, 'P', 'N', 'G', 2, 2 };
        std::vector<Byte>  found;
        ThumbnailRecord    read;
        bool               isUserSet = false;
        int                userSet   = 0;
        std::string        hash;



        AssertSucceeded (store.Store (rig.image, booted, ThumbnailRecord()));
        AssertSucceeded (store.SetByUser (rig.image, chosen));

        Assert::AreEqual (S_OK, store.Find (rig.image, found, read, isUserSet));
        Assert::IsTrue (found == chosen);
        Assert::IsTrue (isUserSet);
        Assert::IsTrue (Rig::Read (rig.GetStream (rig.image)) == chosen, L"The stream has the one returned");
        Assert::AreEqual (1, store.CountUserSet());

        AssertSucceeded (store.Clear (false, userSet));
        Assert::AreEqual (1, userSet);
        Assert::AreEqual (S_OK, store.Find (rig.image, found, read, isUserSet), L"The user's outlives a clear that doesn't include it");
        Assert::IsTrue (found == chosen);

        AssertSucceeded (store.Store (rig.image, booted, ThumbnailRecord()));
        AssertSucceeded (store.Remove (rig.image));

        Assert::AreEqual (S_OK, store.Find (rig.image, found, read, isUserSet));
        Assert::IsTrue (found == booted, L"The booted one again");
        Assert::IsFalse (isUserSet);

        AssertSucceeded (store.GetHash (rig.image, hash));
        AssertSucceeded (store.RemoveForDeleted (hash));

        std::filesystem::remove (rig.GetStream (rig.image));

        AssertSucceeded (store.Find (rig.image, found, read, isUserSet));
        Assert::IsTrue (found.empty(), L"Nothing for a deleted image's hash");
    }

    //  After a write that renames a temporary file over the image, as a save
    //  does, the image has its thumbnail: the stream is copied to the temporary, and the
    //  new bytes' hash finds it there.
    TEST_METHOD (AWriteThroughATemporaryFile_KeepsTheThumbnail)
    {
        Rig                   rig;
        ThumbnailStore        store (rig.root / L"cache");
        std::vector<Byte>     png       = { 0x89, 'P', 'N', 'G', 7 };
        std::vector<Byte>     found;
        ThumbnailRecord       read;
        bool                  isUserSet = false;
        Win32DiskFileIo       io;
        CommitPlan::Progress  progress;



        AssertSucceeded (store.Store (rig.image, png, ThumbnailRecord()));

        AssertSucceeded (DurableCommit::Commit (io, rig.image.string(), std::vector<Byte> (4096, 0x11), 1, CommitMode::Replace, progress));

        Assert::IsTrue (Rig::Read (rig.image) == std::vector<Byte> (4096, 0x11), L"The new bytes were written");
        Assert::IsTrue (Rig::Read (rig.GetStream (rig.image)) == png, L"The stream was copied");

        AssertSucceeded (store.Find (rig.image, found, read, isUserSet));
        Assert::IsTrue (found == png, L"Found for the new hash");

        Assert::AreEqual (S_OK, ThumbnailStore::CopyStream (rig.root / L"missing.dsk", rig.image), L"No stream is no error");
    }

    //  A thumbnail the user set is the user's after a write, with the record
    //  of the image's boot.
    TEST_METHOD (AWriteThroughATemporaryFile_KeepsTheUsersMarkAndTheRecord)
    {
        Rig                   rig;
        ThumbnailStore        store (rig.root / L"cache");
        std::vector<Byte>     booted    = { 0x89, 'P', 'N', 'G', 1 };
        std::vector<Byte>     chosen    = { 0x89, 'P', 'N', 'G', 2 };
        std::vector<Byte>     found;
        ThumbnailRecord       record;
        ThumbnailRecord       read;
        bool                  isUserSet = false;
        Win32DiskFileIo       io;
        CommitPlan::Progress  progress;



        record.verdict = "booted";
        record.title   = "ARCHON";

        AssertSucceeded (store.Store (rig.image, booted, record));
        AssertSucceeded (store.SetByUser (rig.image, chosen));
        AssertSucceeded (DurableCommit::Commit (io, rig.image.string(), std::vector<Byte> (4096, 0x22), 1, CommitMode::Replace, progress));

        AssertSucceeded (store.Find (rig.image, found, read, isUserSet));
        Assert::IsTrue (found == chosen);
        Assert::IsTrue (isUserSet, L"Set by the user under the new hash");
        Assert::AreEqual (std::string ("booted"), read.verdict);
        Assert::AreEqual (std::string ("ARCHON"), read.title);
        Assert::AreEqual (2, store.CountUserSet(), L"The old bytes' as well, for a copy of them elsewhere");
    }

    //  An image's streams from before a clear aren't taken into the cache,
    //  and a clear removes only the cache files that begin with an image's hash.
    TEST_METHOD (AClear_IgnoresTheStreamsFromBeforeIt_AndKeepsOtherFiles)
    {
        Rig                    rig;
        ThumbnailStore         store (rig.root / L"cache");
        std::vector<Byte>      chosen    = { 0x89, 'P', 'N', 'G', 3 };
        std::vector<Byte>      found;
        ThumbnailRecord        read;
        bool                   isUserSet = false;
        int                    userSet   = 0;
        std::filesystem::path  given     = rig.root / L"cache" / L"given.json";



        AssertSucceeded (store.SetByUser (rig.image, chosen));
        Rig::Write (given, { '{', '}' });

        AssertSucceeded (store.Clear (true, userSet));
        Assert::AreEqual (1, userSet);
        Assert::IsTrue (std::filesystem::exists (given), L"A file whose name isn't a hash is left");

        AssertSucceeded (store.Find (rig.image, found, read, isUserSet));
        Assert::IsTrue (found.empty(), L"The stream is from before the clear");
        Assert::AreEqual (0, store.CountUserSet());
    }

    //  A change another process makes to the cache is to a thumbnail the user
    //  set, or a clear; a booted thumbnail or a record isn't.
    TEST_METHOD (AUsersChange_IsAThumbnailTheUserSet_OrAClear)
    {
        std::wstring  hash (64, L'a');

        Assert::IsTrue  (ThumbnailStore::IsUserChange (L"C:\\Cache\\" + hash + L".user.png"));
        Assert::IsTrue  (ThumbnailStore::IsUserChange (L"C:\\Cache\\clears.txt"));
        Assert::IsFalse (ThumbnailStore::IsUserChange (L"C:\\Cache\\" + hash + L".png"));
        Assert::IsFalse (ThumbnailStore::IsUserChange (L"C:\\Cache\\" + hash + L".json"));
        Assert::IsFalse (ThumbnailStore::IsUserChange (L"C:\\Cache\\given.json"));
    }

    //  Removing the thumbnail the user set from an image with no booted one
    //  deletes the streams, so it isn't taken from them again.
    TEST_METHOD (Remove_WithNoBootedThumbnail_DeletesTheStreams)
    {
        Rig                rig;
        ThumbnailStore     store (rig.root / L"cache");
        std::vector<Byte>  chosen    = { 0x89, 'P', 'N', 'G', 4 };
        std::vector<Byte>  found;
        ThumbnailRecord    read;
        bool               isUserSet = false;



        AssertSucceeded (store.SetByUser (rig.image, chosen));
        AssertSucceeded (store.Remove (rig.image));

        Assert::IsFalse (std::filesystem::exists (rig.GetStream (rig.image)));

        AssertSucceeded (store.Find (rig.image, found, read, isUserSet));
        Assert::IsTrue (found.empty());
    }
};
