#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "Cassque/Model/KnownFolderStore.h"
#include "Core/JsonParser.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  KnownFolderStoreTests
//
//  The shared folder list over an in-memory host, with the cross-process
//  lock off: there is no second process here to hold off.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (KnownFolderStoreTests)
{
public:

    static constexpr const wchar_t *  kBase = L"C:\\Users\\me\\AppData\\Local\\Casso";



    static std::vector<DiskMru::Entry> MakeMru()
    {
        std::vector<DiskMru::Entry>  mru;

        mru.push_back (DiskMru::Entry { L"C:\\Apple\\Disks\\one.dsk", 10 });
        mru.push_back (DiskMru::Entry { L"C:\\Apple\\Disks\\two.dsk", 11 });
        mru.push_back (DiskMru::Entry { L"D:\\Games\\three.woz",      12 });

        return mru;
    }



    TEST_METHOD (Seed_FromTheMrusDistinctFolders_OnlyWhenAbsent)
    {
        InMemoryFileSystem              fs;
        KnownFolderStore                store (fs, kBase, false);
        std::vector<KnownFolderStore::Entry>  entries;

        Assert::IsFalse (store.Exists());

        AssertSucceeded (store.SeedFromMru (MakeMru(), 500));
        AssertSucceeded (store.Load (entries));

        Assert::IsTrue   (store.Exists());
        Assert::AreEqual ((size_t) 2, entries.size(), L"two recents share one folder");
        Assert::AreEqual ((int64_t) 500, entries[0].lastUsedUnix);

        //  A second seed changes nothing: the list is the user's now.
        AssertSucceeded (store.Remove (entries[1].path));
        AssertSucceeded (store.SeedFromMru (MakeMru(), 600));
        AssertSucceeded (store.Load (entries));

        Assert::AreEqual ((size_t) 1, entries.size());
    }



    TEST_METHOD (Append_DedupesCaseInsensitivelyAndRestamps)
    {
        InMemoryFileSystem              fs;
        KnownFolderStore                store (fs, kBase, false);
        std::vector<KnownFolderStore::Entry>  entries;

        AssertSucceeded (store.Append (L"C:\\Apple\\Disks", 100));
        AssertSucceeded (store.Append (L"c:\\apple\\DISKS\\", 200));
        AssertSucceeded (store.Append (L"E:\\Other", 300));
        AssertSucceeded (store.Load (entries));

        Assert::AreEqual ((size_t) 2, entries.size());
        Assert::AreEqual (std::wstring (L"C:\\Apple\\Disks"), entries[0].path, L"the first spelling is kept");
        Assert::AreEqual ((int64_t) 200, entries[0].lastUsedUnix, L"the stamp follows the latest hand-off");
        Assert::AreEqual (std::wstring (L"E:\\Other"), entries[1].path);
    }



    TEST_METHOD (Remove_DropsOnlyThatFolder_AndAMissingFolderIsRetained)
    {
        InMemoryFileSystem              fs;
        KnownFolderStore                store (fs, kBase, false);
        std::vector<KnownFolderStore::Entry>  entries;

        //  Nothing at these paths exists on the in-memory host, and Load
        //  still lists them: existence is the tree's question, not the store's.
        AssertSucceeded (store.Append (L"C:\\Gone", 1));
        AssertSucceeded (store.Append (L"C:\\Kept", 2));
        AssertSucceeded (store.Remove (L"c:\\gone"));
        AssertSucceeded (store.Load (entries));

        Assert::AreEqual ((size_t) 1, entries.size());
        Assert::AreEqual (std::wstring (L"C:\\Kept"), entries[0].path);

        AssertSucceeded (store.Remove (L"C:\\Never"));
        AssertSucceeded (store.Load (entries));
        Assert::AreEqual ((size_t) 1, entries.size());
    }



    TEST_METHOD (Write_LeavesOneWholeFileAndNoTemporary)
    {
        InMemoryFileSystem  fs;
        KnownFolderStore    store (fs, kBase, false);
        JsonValue           root;
        JsonParseError      error;
        const JsonValue *   folders = nullptr;

        AssertSucceeded (store.Append (L"C:\\Apple\\Disks", 100));

        Assert::AreEqual ((size_t) 1, fs.FileCount(), L"one file, no leftover temporary");
        AssertSucceeded  (JsonParser::Parse (fs.PeekContent (KnownFolderStore::GetFilePath (kBase)), root, error));
        Assert::IsTrue   (root.HasArray ("folders", folders));
        Assert::AreEqual ((size_t) 1, folders->GetArraySize());
    }



    TEST_METHOD (Load_AnAbsentFileIsAnEmptyList)
    {
        InMemoryFileSystem              fs;
        KnownFolderStore                store (fs, kBase, false);
        std::vector<KnownFolderStore::Entry>  entries;

        AssertSucceeded (store.Load (entries));

        Assert::IsTrue (entries.empty());
        Assert::AreEqual (std::wstring (L"C:\\Base\\KnownFolders.json"), KnownFolderStore::GetFilePath (L"C:\\Base"));
        Assert::AreEqual (std::wstring (L"C:\\Base\\KnownFolders.json"), KnownFolderStore::GetFilePath (L"C:\\Base\\"));
    }



    TEST_METHOD (RootFolders_FallBackToTheEmulatorsDisksFolder)
    {
        InMemoryFileSystem                    fs;
        std::vector<KnownFolderStore::Entry>  entries;
        std::vector<std::wstring>             folders;

        //  A machine where the emulator has never opened a disk: nothing is
        //  recorded, but its own disk folder is there to start from.
        AssertSucceeded (fs.WriteAllText (std::wstring (kBase) + L"\\Disks\\blank.dsk", "x"));

        folders = KnownFolderStore::ListRootFolders (fs, kBase, entries);

        Assert::AreEqual ((size_t) 1, folders.size());
        Assert::AreEqual (std::wstring (kBase) + L"\\Disks", folders[0]);

        //  Once a folder is recorded, only what was recorded is listed.
        entries.push_back (KnownFolderStore::Entry { L"C:\\Apple\\Disks", 10 });
        folders = KnownFolderStore::ListRootFolders (fs, kBase, entries);

        Assert::AreEqual ((size_t) 1, folders.size());
        Assert::AreEqual (std::wstring (L"C:\\Apple\\Disks"), folders[0]);
    }



    TEST_METHOD (RelativeFolders_AreNeitherSeededNorListed)
    {
        InMemoryFileSystem                    fs;
        KnownFolderStore                      store (fs, kBase, false);
        std::vector<DiskMru::Entry>           mru;
        std::vector<KnownFolderStore::Entry>  entries;
        std::vector<std::wstring>             folders;

        //  A recent disk recorded by a relative path: the folder it yields
        //  means nothing to whichever process reads the list back.
        mru.push_back (DiskMru::Entry { L"Apple2\\Demos\\demo.dsk",    10 });
        mru.push_back (DiskMru::Entry { L"C:\\Apple\\Disks\\one.dsk",  11 });

        AssertSucceeded (store.SeedFromMru (mru, 100));
        AssertSucceeded (store.Load (entries));

        Assert::AreEqual ((size_t) 1, entries.size());
        Assert::AreEqual (std::wstring (L"C:\\Apple\\Disks"), entries[0].path);

        //  And a list already written with one is listed without it.
        entries.push_back (KnownFolderStore::Entry { L"Apple2\\Demos", 12 });
        folders = KnownFolderStore::ListRootFolders (fs, kBase, entries);

        Assert::AreEqual ((size_t) 1, folders.size());
        Assert::AreEqual (std::wstring (L"C:\\Apple\\Disks"), folders[0]);
    }


    TEST_METHOD (RootFolders_AreEmptyWithNothingRecordedAndNoDisksFolder)
    {
        InMemoryFileSystem                    fs;
        std::vector<KnownFolderStore::Entry>  entries;

        Assert::IsTrue (KnownFolderStore::ListRootFolders (fs, kBase, entries).empty());
    }
};
