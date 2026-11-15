#include "Pch.h"

#include "Shell/Layout/DriveLabel.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DriveLabelTests
//
//  A drive's label on the drive bar and its tip on the desk.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DriveLabelTests)
{
public:

    //  A DRIVE'S FIRST LINE IS ITS NAME: a Disk II numbered as DOS numbers it,
    //  an HD20SC with its SCSI ID, and a 3.5-inch drive with its ProDOS slot
    //  and drive only when the class has two of its kind.
    TEST_METHOD (Title_NamesEachDrive)
    {
        Assert::AreEqual (std::wstring (L"Drive 1 - Disk ]["),    DriveLabel::GetTitle ({ DeskDriveModel::DiskII,       6, 0 }, false));
        Assert::AreEqual (std::wstring (L"Drive 1 - Built-in"),   DriveLabel::GetTitle ({ DeskDriveModel::Disk2c,       6, 0, true }, false));
        Assert::AreEqual (std::wstring (L"Drive 1 - Disk IIc"),   DriveLabel::GetTitle ({ DeskDriveModel::Disk2c,       6, 0 }, false), L"the //c Plus's drive 1 is on its port");
        Assert::AreEqual (std::wstring (L"Drive 2 - Disk IIc"),   DriveLabel::GetTitle ({ DeskDriveModel::Disk2c,       6, 1 }, false));
        Assert::AreEqual (std::wstring (L"UniDisk 3.5"),          DriveLabel::GetTitle ({ DeskDriveModel::UniDisk35,    5, 0 }, false));
        Assert::AreEqual (std::wstring (L"UniDisk 3.5 - S5,D2"),  DriveLabel::GetTitle ({ DeskDriveModel::UniDisk35,    5, 1 }, true));
        Assert::AreEqual (std::wstring (L"Built-in 3.5 - S5,D1"), DriveLabel::GetTitle ({ DeskDriveModel::Apple35Plus,  0, 0 }, true), L"the //c port is slot 5 to ProDOS");
        Assert::AreEqual (std::wstring (L"UniDisk 3.5 - unit 3"), DriveLabel::GetTitle ({ DeskDriveModel::UniDisk35,    5, 2 }, true));
        Assert::AreEqual (std::wstring (L"HD20SC - ID 6"),        DriveLabel::GetTitle ({ DeskDriveModel::HardDisk20SC, 7, 6 }, true), L"the SCSI ID is the number");
    }


    //  A DRIVE'S SECOND LINE IS ITS FORM FACTOR AND CAPACITY, with a smaller
    //  disk's size after a 3.5-inch drive's, and a hard disk's image size.
    TEST_METHOD (Detail_FormFactorAndCapacity)
    {
        Assert::AreEqual (std::wstring (L"5.25\" (140K)"),          DriveLabel::GetDetail (DeskDriveModel::DiskII,       0,     0));
        Assert::AreEqual (std::wstring (L"3.5\" (800K)"),           DriveLabel::GetDetail (DeskDriveModel::UniDisk35,    0,     0), L"empty");
        Assert::AreEqual (std::wstring (L"3.5\" (800K)"),           DriveLabel::GetDetail (DeskDriveModel::Apple35Drive, 1600,  0), L"an 800K disk");
        Assert::AreEqual (std::wstring (L"3.5\" (800K), 400K disk"), DriveLabel::GetDetail (DeskDriveModel::UniDisk35,    800,   0));
        Assert::AreEqual (std::wstring (L"Hard disk (32 MB)"),       DriveLabel::GetDetail (DeskDriveModel::HardDisk20SC, 65535, 0));
        Assert::AreEqual (std::wstring (L"Hard disk"),               DriveLabel::GetDetail (DeskDriveModel::HardDisk20SC, 0,     0), L"no image");
        Assert::AreEqual (std::wstring (L"Hard disk (5 MB)"),        DriveLabel::GetDetail (DeskDriveModel::ProFile,      0,     9728));
        Assert::AreEqual (std::wstring (L"Hard disk (10 MB)"),       DriveLabel::GetDetail (DeskDriveModel::ProFile,      19456, 19456));
        Assert::AreEqual (std::wstring (L"140K"),                    DriveLabel::FormatSize (280));
        Assert::AreEqual (std::wstring (L"1 MB"),                    DriveLabel::FormatSize (2048));
    }

    //  A DESK DRIVE'S TIP IS ITS TWO LINES, THEN ITS IMAGE: the image's name
    //  and whether it is write-protected, or "No disk" when the drive is empty.
    TEST_METHOD (Tip_LinesThenImage)
    {
        Assert::AreEqual (std::wstring (L"UniDisk 3.5\n3.5\" (800K)\nGames.po\nWrite-protected"),
                          DriveLabel::GetTip (L"UniDisk 3.5", L"3.5\" (800K)", L"Games.po", false, true));
        Assert::AreEqual (std::wstring (L"UniDisk 3.5\n3.5\" (800K)\nNo disk"),
                          DriveLabel::GetTip (L"UniDisk 3.5", L"3.5\" (800K)", L"", false, false));
        Assert::AreEqual (std::wstring (L"HD20SC - ID 6\nHard disk\nNo image"),
                          DriveLabel::GetTip (L"HD20SC - ID 6", L"Hard disk", L"", true, false));
    }
};
