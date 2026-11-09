#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "Devices/Disk/Archives/ContainerPath.h"
#include "Devices/Disk/Archives/GzipFile.h"
#include "Devices/Disk/Archives/NuFxArchive.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Update/ZipArchive.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerPathTests
//
//  A file on an 800K ProDOS image, in a gzip file, in a ZIP on the PC: read
//  through every step, then replaced, with the other ZIP member and the
//  image's other file intact. Then a record in a ShrinkIt archive.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ContainerPathTests)
{
public:

    static constexpr const wchar_t *  kZipPath = L"C:\\Disks\\collection.zip";
    static constexpr const char *     kZipName = "C:\\Disks\\collection.zip";

    static FilePayload MakePayload (size_t bytes, Byte fill)
    {
        FilePayload  payload;



        payload.bytes.assign (bytes, fill);
        payload.type           = ProDosVolume::kTypeBinary;
        payload.loadAddress    = 0x2000;
        payload.hasLoadAddress = true;

        return payload;
    }

    static void MakeCollection (InMemoryFileSystem & fs)
    {
        static constexpr Byte  kEmptyZip[22] = { 0x50, 0x4B, 0x05, 0x06 };

        std::vector<Byte>  volume (1600 * 512, 0);
        std::vector<Byte>  withHello;
        std::vector<Byte>  withBoth;
        std::vector<Byte>  gzip;
        std::vector<Byte>  readme = { 'h', 'i' };
        std::vector<Byte>  zipOne;
        std::vector<Byte>  zipTwo;



        AssertSucceeded (ProDosSkeleton::Write (volume, "NESTED"));
        AssertSucceeded (ProDosVolume (volume).Write (FilePath::Parse ("HELLO"), MakePayload (3000, 0x11), withHello));
        AssertSucceeded (ProDosVolume (withHello).Write (FilePath::Parse ("OTHER"), MakePayload (700, 0x22), withBoth));

        GzipFile::Build (withBoth, "disk.po", gzip);

        AssertSucceeded (ZipArchive::Replace (kEmptyZip, "readme.txt", readme, zipOne));
        AssertSucceeded (ZipArchive::Replace (zipOne, "games/disk.po.gz", gzip, zipTwo));
        AssertSucceeded (fs.WriteAllText (kZipPath, std::string (zipTwo.begin(), zipTwo.end())));
    }

    static ContainerPath MakeHelloPath()
    {
        ContainerPath  path;



        path.steps = { { ContainerStep::Kind::HostFile,  kZipName },
                       { ContainerStep::Kind::ZipMember, "games/disk.po.gz" },
                       { ContainerStep::Kind::Gzip,      "" },
                       { ContainerStep::Kind::ImageFile, "HELLO" } };

        return path;
    }


    TEST_METHOD (Open_ReadsThroughAZipAGzipAndAnImage)
    {
        InMemoryFileSystem  fs;
        OpenedContainer     opened;
        std::string         error;



        MakeCollection (fs);

        AssertSucceeded (ContainerPath::Open (MakeHelloPath(), fs, opened, error));
        Assert::IsTrue (opened.bytes == std::vector<Byte> (3000, 0x11));
        Assert::AreEqual (std::string ("HELLO"), opened.name);
    }


    TEST_METHOD (Open_ReturnsTheImageAtAShallowerStep)
    {
        InMemoryFileSystem  fs;
        ContainerPath       path = MakeHelloPath();
        OpenedContainer     opened;
        std::string         error;



        MakeCollection (fs);
        path.steps.pop_back();

        AssertSucceeded (ContainerPath::Open (path, fs, opened, error));
        Assert::AreEqual ((size_t) 1600 * 512, opened.bytes.size());
        Assert::AreEqual (std::string ("games/disk.po"), opened.name);
    }


    TEST_METHOD (Replace_RebuildsEveryStepAndKeepsTheRest)
    {
        InMemoryFileSystem     fs;
        ContainerPath          path   = MakeHelloPath();
        ContainerPath          other  = MakeHelloPath();
        std::vector<Byte>      edited (5000, 0x33);
        std::string            zip;
        std::vector<ZipEntry>  entries;
        OpenedContainer        opened;
        std::string            error;



        MakeCollection (fs);

        AssertSucceeded (ContainerPath::Replace (path, fs, edited, error));

        AssertSucceeded (ContainerPath::Open (path, fs, opened, error));
        Assert::IsTrue (opened.bytes == edited);

        other.steps.back().name = "OTHER";
        AssertSucceeded (ContainerPath::Open (other, fs, opened, error));
        Assert::IsTrue (opened.bytes == std::vector<Byte> (700, 0x22));

        AssertSucceeded (fs.ReadAllText (kZipPath, zip));
        AssertSucceeded (ZipArchive::Extract (std::vector<Byte> (zip.begin(), zip.end()), entries));
        Assert::AreEqual (std::string ("readme.txt"), entries[0].path);
        Assert::IsTrue (entries[0].data == std::vector<Byte> { 'h', 'i' });
    }


    TEST_METHOD (Replace_LeavesTheHostFileWhenAStepFails)
    {
        InMemoryFileSystem  fs;
        ContainerPath       path = MakeHelloPath();
        std::vector<Byte>   edited (5000, 0x33);
        HRESULT             hr   = S_OK;
        std::string         before;
        std::string         after;
        std::string         error;



        MakeCollection (fs);
        AssertSucceeded (fs.ReadAllText (kZipPath, before));

        path.steps.back().name = "MISSING";
        hr = ContainerPath::Replace (path, fs, edited, error);
        Assert::IsTrue (FAILED (hr));
        Assert::IsFalse (error.empty());

        AssertSucceeded (fs.ReadAllText (kZipPath, after));
        Assert::IsTrue (before == after);
    }


    TEST_METHOD (ArchiveRecord_OpensAndReplacesInAShrinkItArchive)
    {
        InMemoryFileSystem  fs;
        NuFxArchive         archive;
        NuFxRecord          record;
        std::vector<Byte>   file;
        std::vector<Byte>   edited (900, 0x44);
        ContainerPath       path;
        OpenedContainer     opened;
        std::string         error;



        record.fileType = 0x04;
        record.threads.push_back (NuFxArchive::MakeFilenameThread ("DOCS/README"));
        record.threads.push_back (NuFxArchive::MakeDataThread (NuFxThread::kKindDataFork, std::vector<Byte> (400, 'R')));
        archive.records.push_back (record);
        archive.Build (file);
        AssertSucceeded (fs.WriteAllText (L"C:\\Disks\\docs.shk", std::string (file.begin(), file.end())));

        path.steps = { { ContainerStep::Kind::HostFile,      "C:\\Disks\\docs.shk" },
                       { ContainerStep::Kind::ArchiveRecord, "DOCS/README" } };

        AssertSucceeded (ContainerPath::Open (path, fs, opened, error));
        Assert::IsTrue (opened.bytes == std::vector<Byte> (400, 'R'));

        AssertSucceeded (ContainerPath::Replace (path, fs, edited, error));
        AssertSucceeded (ContainerPath::Open (path, fs, opened, error));
        Assert::IsTrue (opened.bytes == edited);
    }


    TEST_METHOD (Id_IsEqualForEqualSteps)
    {
        ContainerPath  a = MakeHelloPath();
        ContainerPath  b = MakeHelloPath();



        Assert::IsTrue (a == b);

        b.steps.back().name = "OTHER";
        Assert::IsFalse (a == b);
    }
};
