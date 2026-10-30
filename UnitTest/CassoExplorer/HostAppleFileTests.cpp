#include "Pch.h"
#include "CassoExplorer/Model/HostAppleFile.h"
#include "CassoExplorer/Model/HostFileNaming.h"
#include "CassoExplorer/CassoExplorerActions.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HostAppleFileTests
//
//  An Apple file on the PC in the properties dialog, with no window: what is
//  read from its name or container, what an edit makes of it, and which name
//  a copy to a disk takes.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (HostAppleFileTests)
{
public:

    static std::vector<Byte>  MakeSingle (const std::string & name, bool nameSetByUser = false)
    {
        AppleSingleFile    file;
        std::vector<Byte>  bytes;

        file.data          = { 1, 2, 3 };
        file.realName      = name;
        file.hasProDosInfo = true;
        file.access        = 0xC3;
        file.fileType      = 0x06;
        file.auxType       = 0x0803;
        file.nameSetByUser = nameSetByUser;
        AppleSingleCodec::Encode (file, bytes);

        return bytes;
    }



    TEST_METHOD (Read_KnowsEachForm_AndOffersNoOtherFile)
    {
        HostAppleFile  descriptive = HostAppleFile::Read (L"GAME.Binary.$0803.bin", {});
        HostAppleFile  ciderPress  = HostAppleFile::Read (L"GAME#060803", {});
        HostAppleFile  single      = HostAppleFile::Read (L"GAME.as", MakeSingle ("GAME"));
        HostAppleFile  plain       = HostAppleFile::Read (L"notes.txt", { 'h', 'i' });

        Assert::IsTrue   (descriptive.form == HostAppleFile::Form::Descriptive);
        Assert::AreEqual (std::string ("GAME"), descriptive.entry.name);
        Assert::AreEqual ((Word) 0x0803, descriptive.entry.auxType);
        Assert::IsTrue   (ciderPress.form == HostAppleFile::Form::CiderPress);
        Assert::AreEqual ((Byte) 0x06, ciderPress.entry.type);
        Assert::IsTrue   (single.form == HostAppleFile::Form::AppleSingle);
        Assert::AreEqual ((Byte) 0xC3, single.entry.access);
        Assert::IsTrue   (plain.form == HostAppleFile::Form::None);
    }



    //  A Descriptive or CiderPress file's properties are its name, in its form.
    TEST_METHOD (Apply_RenamesANamedFile_InItsForm)
    {
        HostAppleFile               file = HostAppleFile::Read (L"GAME#060803", {});
        AppleFileProperties::Edits  edits;
        HostAppleFile::Change       change;

        edits.name       = L"ARCADE";
        edits.nameEdited = true;
        edits.auxEdited  = true;
        edits.aux        = 0x2000;

        AssertSucceeded (file.Apply (edits, false, change));
        Assert::AreEqual (std::wstring (L"ARCADE#062000"), change.leafName);
        Assert::IsTrue   (change.bytes.empty(), L"nothing to write but the name");
    }



    //  An AppleSingle file's header changes; its name on the PC only when
    //  the box is checked, and the new name is marked as set in the dialog.
    TEST_METHOD (Apply_RewritesAnAppleSingleHeader_AndRenamesOnlyWhenAsked)
    {
        HostAppleFile               file = HostAppleFile::Read (L"GAME.as", MakeSingle ("GAME"));
        AppleFileProperties::Edits  edits;
        HostAppleFile::Change       change;
        AppleSingleFile             written;
        std::string                 error;

        edits.name       = L"ARCADE";
        edits.nameEdited = true;

        AssertSucceeded (file.Apply (edits, false, change));
        Assert::AreEqual (std::wstring (L"GAME.as"), change.leafName);
        AssertSucceeded (AppleSingleCodec::Decode (change.bytes, written, error));
        Assert::AreEqual (std::string ("ARCADE"), written.realName);
        Assert::IsTrue   (written.nameSetByUser);
        Assert::IsTrue   (written.data == std::vector<Byte> ({ 1, 2, 3 }), L"the data as it was");

        AssertSucceeded (file.Apply (edits, true, change));
        Assert::AreEqual (std::wstring (L"ARCADE.as"), change.leafName);
    }



    TEST_METHOD (Dates_RoundTripBetweenAppleSingleAndProDos)
    {
        Word     date = 0;
        Word     time = 0;
        int32_t  back = 0;

        HostAppleFile::ToProDosDate (12 * 365 * 86400 + 3 * 3600 + 25 * 60, date, time);
        back = HostAppleFile::FromProDosDate (date, time);

        Assert::AreEqual (12 * 365 * 86400 + 3 * 3600 + 25 * 60, back);
        Assert::AreEqual ((Word) ((3 << 8) | 25), time);
    }



    //  The header's name is used when the PC name is it with .as added, or
    //  when it was set in the dialog; otherwise the PC name.
    TEST_METHOD (CopyBack_TakesTheHeaderName_OnlyWhenItStillApplies)
    {
        AppleSingleFile  file;
        std::string      error;

        AssertSucceeded (AppleSingleCodec::Decode (MakeSingle ("GAME"), file, error));
        Assert::AreEqual (std::string ("GAME"), CassoExplorerActions::PlanAppleSingle (L"C:\\Out\\GAME.as", file, VolumeKind::ProDos).catalogName);
        Assert::AreEqual (std::string ("RENAMED"), CassoExplorerActions::PlanAppleSingle (L"C:\\Out\\RENAMED.as", file, VolumeKind::ProDos).catalogName);

        AssertSucceeded (AppleSingleCodec::Decode (MakeSingle ("GAME", true), file, error));
        Assert::AreEqual (std::string ("GAME"), CassoExplorerActions::PlanAppleSingle (L"C:\\Out\\RENAMED.as", file, VolumeKind::ProDos).catalogName);
    }
};
