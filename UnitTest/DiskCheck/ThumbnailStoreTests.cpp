#include "Pch.h"

#include "../EhmTestHelper.h"
#include "DiskCheck/ThumbnailStore.h"

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
};
