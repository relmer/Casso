#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/Archives/ZipAppleDouble.h"
#include "Core/AppleSingleCodec.h"
#include "Machines/Apple2/Common/VolumeTypes.h"
#include "Update/ZipArchive.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ZipAppleDoubleTests
//
//  ZIPs built with a member and its AppleDouble file in each place one is
//  written, with the ProDOS type in a file info entry or in the Finder type.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ZipAppleDoubleTests)
{
public:

    static std::vector<ZipEntry> MakeZip (const std::string & sidecarPath, const AppleSingleFile & single)
    {
        static constexpr Byte  kEmptyZip[22] = { 0x50, 0x4B, 0x05, 0x06 };

        std::vector<Byte>      data   = { 1, 2, 3 };
        std::vector<Byte>      sidecar;
        std::vector<Byte>      one;
        std::vector<Byte>      two;
        std::vector<ZipEntry>  entries;



        AppleSingleCodec::EncodeDouble (single, sidecar);

        AssertSucceeded (ZipArchive::Replace (kEmptyZip, "GAMES/WIZ", data, one));
        AssertSucceeded (ZipArchive::Replace (one, sidecarPath, sidecar, two));
        AssertSucceeded (ZipArchive::Extract (two, entries));

        return entries;
    }


    TEST_METHOD (Apply_ReadsTheFileInfoEntryAndResourceFork)
    {
        AppleSingleFile  single;
        FilePayload      payload;



        single.hasProDosInfo   = true;
        single.fileType        = 0xB3;
        single.auxType         = 0xDB07;
        single.resourceFork    = { 9, 8, 7, 6 };
        single.hasResourceFork = true;

        Assert::IsTrue (ZipAppleDouble::Apply (MakeZip ("__MACOSX/GAMES/._WIZ", single), "GAMES/WIZ", payload));
        Assert::AreEqual ((Byte) 0xB3, payload.type);
        Assert::AreEqual ((Word) 0xDB07, payload.auxType);
        Assert::IsTrue (payload.hasResourceFork);
        Assert::IsTrue (payload.resourceFork == std::vector<Byte> { 9, 8, 7, 6 });
    }


    TEST_METHOD (Apply_ReadsThePFormFinderTypeBesideTheMember)
    {
        AppleSingleFile  single;
        FilePayload      payload;



        single.finderInfo = { 'p', 0xFC, 0x08, 0x01, 'p', 'd', 'o', 's' };
        single.finderInfo.resize (32, 0);

        Assert::IsTrue (ZipAppleDouble::Apply (MakeZip ("GAMES/._WIZ", single), "GAMES/WIZ", payload));
        Assert::AreEqual ((Byte) 0xFC, payload.type);
        Assert::AreEqual ((Word) 0x0801, payload.auxType);
        Assert::IsFalse (payload.hasResourceFork);
    }


    TEST_METHOD (Apply_IsFalseForAMemberWithoutAnAppleDoubleFile)
    {
        AppleSingleFile  single;
        FilePayload      payload;



        payload.type = 0x06;

        Assert::IsFalse (ZipAppleDouble::Apply (MakeZip ("__MACOSX/OTHER/._WIZ", single), "GAMES/WIZ", payload));
        Assert::AreEqual ((Byte) 0x06, payload.type);
    }


    TEST_METHOD (TryGetProDosType_MapsTextAndBinary)
    {
        std::vector<Byte>  text   = { 'T', 'E', 'X', 'T', 'M', 'A', 'C', 'S' };
        std::vector<Byte>  binary = { 'B', 'I', 'N', 'A', 'M', 'A', 'C', 'S' };
        std::vector<Byte>  other  = { 'A', 'P', 'P', 'L', 'M', 'A', 'C', 'S' };
        Byte               type   = 0;
        Word               aux    = 0;



        Assert::IsTrue (ZipAppleDouble::TryGetProDosType (text, type, aux));
        Assert::AreEqual ((Byte) 0x04, type);
        Assert::IsTrue (ZipAppleDouble::TryGetProDosType (binary, type, aux));
        Assert::AreEqual ((Byte) 0x06, type);
        Assert::IsFalse (ZipAppleDouble::TryGetProDosType (other, type, aux));
    }


    TEST_METHOD (IsSidecar_MatchesBothForms)
    {
        Assert::IsTrue  (ZipAppleDouble::IsSidecar ("__MACOSX/GAMES/._WIZ"));
        Assert::IsTrue  (ZipAppleDouble::IsSidecar ("GAMES/._WIZ"));
        Assert::IsFalse (ZipAppleDouble::IsSidecar ("GAMES/WIZ"));
    }
};
