#include "Pch.h"

#include "Devices/Disk/ImageClassCache.h"
#include "../EmuTests/FakeDiskFileIo.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ImageClassCacheTests
//
//  A file is read once while its length and time stay the same, and again
//  after either changes.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ImageClassCacheTests)
{
public:

    static constexpr uint64_t  k140K = 143360;
    static constexpr uint64_t  k800K = 819200;



    static void AddFile (FakeDiskFileIo & io, const std::string & path, uint64_t bytes, int64_t modified)
    {
        io.files[path]  = std::vector<Byte> ((size_t) bytes, 0);
        io.stamps[path] = FileStamp { bytes, modified };
    }



    TEST_METHOD (SecondLookupReadsNothing)
    {
        FakeDiskFileIo   io;
        ImageClassCache  cache;
        std::string      path       = "C:\\Disks\\a.dsk";
        uint64_t         afterFirst = 0;



        AddFile (io, path, k140K, 1);

        Assert::AreEqual ((int) ImageFormFactor::Floppy525, (int) cache.Get (io, path, k140K, 1).formFactor);
        afterFirst = io.bytesReadAt;

        Assert::AreEqual ((int) ImageFormFactor::Floppy525, (int) cache.Get (io, path, k140K, 1).formFactor);
        Assert::AreEqual (afterFirst, io.bytesReadAt, L"no second read");
        Assert::AreEqual ((size_t) 1, cache.GetCount());
    }



    TEST_METHOD (ChangedLengthIsReadAgain)
    {
        FakeDiskFileIo   io;
        ImageClassCache  cache;
        std::string      path = "C:\\Disks\\b.po";



        AddFile (io, path, k140K, 1);
        Assert::AreEqual ((int) ImageFormFactor::Floppy525, (int) cache.Get (io, path, k140K, 1).formFactor);

        AddFile (io, path, k800K, 1);
        Assert::AreEqual ((int) ImageFormFactor::Floppy35, (int) cache.Get (io, path, k800K, 1).formFactor);
    }



    TEST_METHOD (ChangedTimeIsReadAgain)
    {
        FakeDiskFileIo   io;
        ImageClassCache  cache;
        std::string      path       = "C:\\Disks\\c.po";
        uint64_t         afterFirst = 0;



        AddFile (io, path, k140K, 1);
        cache.Get (io, path, k140K, 1);
        afterFirst = io.bytesReadAt;

        cache.Get (io, path, k140K, 2);
        Assert::IsTrue (io.bytesReadAt > afterFirst, L"read again");
    }
};
