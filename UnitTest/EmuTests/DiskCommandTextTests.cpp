#include "Pch.h"

#include "Devices/Disk/DiskCommandText.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandTextTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskCommandTextTests)
{
public:

    //  A file whose sectors cannot be gathered is damaged in its file
    //  system's terms; any other failure has no damage to name.
    TEST_METHOD (Damage_IsInTheFileSystemsTerms)
    {
        constexpr HRESULT  broken = HRESULT_FROM_WIN32 (ERROR_HANDLE_EOF);

        Assert::AreEqual (std::string ("the track/sector list is damaged"), DiskCommandText::DescribeDamage (broken, VolumeKind::Dos33));
        Assert::AreEqual (std::string ("the index block is damaged"),       DiskCommandText::DescribeDamage (broken, VolumeKind::ProDos));
        Assert::IsTrue   (DiskCommandText::DescribeDamage (HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND), VolumeKind::Dos33).empty());
    }
};
