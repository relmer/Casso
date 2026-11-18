#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "Core/AppleSingleCodec.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/IVolume.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ResourceForkTests
//
//  A Teach document from CiderPress II's 800K IIgs disk, an extended file,
//  copied to another 800K ProDOS volume and to the PC as AppleSingle and
//  back, with both forks and the Finder information compared at each step.
//  Skipped when scripts/FetchCiderPressTestFiles.ps1 hasn't been run.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ResourceForkTests)
{
public:

    static constexpr size_t  k800KBytes = 1600 * 512;

    static void AssertSameFile (const FilePayload & source, const FilePayload & actual, const wchar_t * step)
    {
        Assert::IsTrue (actual.bytes == source.bytes,                 step);
        Assert::IsTrue (actual.hasResourceFork,                       step);
        Assert::IsTrue (actual.resourceFork == source.resourceFork,   step);
        Assert::IsTrue (actual.finderInfo == source.finderInfo,       step);
        Assert::AreEqual (source.type, actual.type,                   step);
        Assert::AreEqual (source.auxType, actual.auxType,             step);
    }

    static void WriteToNewVolume (const FilePayload & payload, FilePayload & outRead)
    {
        std::vector<Byte>  volume (k800KBytes, 0);
        std::vector<Byte>  written;



        AssertSucceeded (ProDosSkeleton::Write (volume, "COPY"));
        AssertSucceeded (ProDosVolume (volume).Write (FilePath::Parse ("TEACH.SAMPLE"), payload, written));
        AssertSucceeded (ProDosVolume (written).Read (FilePath::Parse ("TEACH.SAMPLE"), outRead));
    }


    TEST_METHOD (TeachDocument_KeepsBothForksAcrossVolumesAndThePc)
    {
        FixtureProvider           fixtures;
        std::vector<Byte>         bytes;
        std::vector<Byte>         sectors;
        std::vector<Byte>         single;
        SectorDecodeReport        report;
        std::unique_ptr<IVolume>  volume;
        FilePayload               original;
        FilePayload               copied;
        FilePayload               fromPc;
        FilePayload               copiedBack;
        AppleSingleFile           file;
        AppleSingleFile           decoded;
        std::string               error;
        HRESULT                   hr = fixtures.OpenFixture ("CiderPress/fileconv/test-files.sdk", bytes);



        if (FAILED (hr))
        {
            Logger::WriteMessage ("SKIPPED: test-files.sdk is not on this machine; run scripts/FetchCiderPressTestFiles.ps1\n");
            return;
        }

        AssertSucceeded (VolumeImage::Load (bytes, "test-files.sdk", sectors, report));
        volume = VolumeImage::CreateVolume (VolumeImage::DetectFilesystem (sectors), sectors);
        Assert::IsNotNull (volume.get());

        AssertSucceeded (volume->Read (FilePath::Parse ("Docs/TEACH.SAMPLE"), original));
        Assert::IsTrue (original.hasResourceFork, L"A Teach document is an extended file");
        Assert::IsFalse (original.resourceFork.empty());

        WriteToNewVolume (original, copied);
        AssertSameFile (original, copied, L"Copied to another volume");

        file.data            = original.bytes;
        file.resourceFork    = original.resourceFork;
        file.hasResourceFork = true;
        file.finderInfo      = original.finderInfo;
        file.hasProDosInfo   = true;
        file.fileType        = original.type;
        file.auxType         = original.auxType;
        file.access          = original.access;
        AppleSingleCodec::Encode (file, single);

        AssertSucceeded (AppleSingleCodec::Decode (single, decoded, error));
        fromPc.bytes           = decoded.data;
        fromPc.resourceFork    = decoded.resourceFork;
        fromPc.hasResourceFork = decoded.hasResourceFork;
        fromPc.finderInfo      = decoded.finderInfo;
        fromPc.type            = (Byte) decoded.fileType;
        fromPc.auxType         = (Word) decoded.auxType;
        fromPc.hasAuxType      = true;

        WriteToNewVolume (fromPc, copiedBack);
        AssertSameFile (original, copiedBack, L"Copied to the PC as AppleSingle and back");
    }
};
