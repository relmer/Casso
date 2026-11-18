#include "Pch.h"

#include "../EhmTestHelper.h"
#include "CassoExplorer/Picker/StartupDiskPicker.h"
#include "Devices/Disk/DriveImageRule.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/StockDisk800K.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  StockDisk800KTests
//
//  The 800K stock disk built from a 140K ProDOS disk, over a synthetic
//  ProDOS-order disk.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (StockDisk800KTests)
{
public:

    //  A 140K ProDOS disk in ProDOS order with a volume name, marked boot
    //  blocks, both system files and a text file.
    static vector<Byte> MakeSource (vector<Byte> & outProdos, vector<Byte> & outBasic)
    {
        vector<Byte>  dosOrder (143360, 0);
        vector<Byte>  proDosOrder (143360, 0);
        size_t        i = 0;



        AssertSucceeded (ProDosSkeleton::Write (dosOrder, "PRODOS.2.4.3"));

        for (i = 0; i < 1024; i++)
        {
            dosOrder[ProDosSkeleton::GetBlockByteOffset ((int) (i / 512), i % 512)] = (Byte) (i * 7 + 1);
        }

        outProdos.assign (1500, 0xA5);
        outBasic.assign (2600, 0x5A);

        AssertSucceeded (ProDosFileWriter::WriteFile (dosOrder, "PRODOS", 0xFF, 0x0000, outProdos));
        AssertSucceeded (ProDosFileWriter::WriteFile (dosOrder, "BASIC.SYSTEM", 0xFF, 0x2000, outBasic));
        AssertSucceeded (ProDosFileWriter::WriteFile (dosOrder, "README", 0x04, 0x0000, vector<Byte> (700, 0xC1)));

        for (i = 0; i < proDosOrder.size(); i++)
        {
            proDosOrder[i] = dosOrder[ProDosSkeleton::GetBlockByteOffset ((int) (i / 512), i % 512)];
        }

        return proDosOrder;
    }


    TEST_METHOD (Build_IsAn800KVolume_WithTheSourcesNameBootBlocksAndFiles)
    {
        vector<Byte>  prodos;
        vector<Byte>  basic;
        vector<Byte>  source   = MakeSource (prodos, basic);
        vector<Byte>  volume;
        vector<Byte>  readBack;
        std::string   name;
        Byte          fileType = 0;
        Word          auxType  = 0;



        AssertSucceeded (StockDisk800K::Build (source, volume));

        Assert::AreEqual (StockDisk800K::kBytes, volume.size());
        Assert::AreEqual ((int) StockDisk800K::kBlocks, ProDosSkeleton::GetBlockCount (volume.size()));
        Assert::IsTrue (ProDosSkeleton::TryGetVolumeName (volume, name));
        Assert::AreEqual (std::string ("PRODOS.2.4.3"), name);

        //  Block order: block b is at b * 512, and the source's first two
        //  blocks are copied as they are.
        Assert::IsTrue (std::equal (source.begin(), source.begin() + 1024, volume.begin()), L"the boot blocks must be copied as they are");

        AssertSucceeded (ProDosReader::ExtractFile (volume, "PRODOS", readBack, fileType, auxType));
        Assert::IsTrue (readBack == prodos);

        AssertSucceeded (ProDosReader::ExtractFile (volume, "BASIC.SYSTEM", readBack, fileType, auxType));
        Assert::IsTrue (readBack == basic);
        Assert::AreEqual ((Word) 0x2000, auxType);

        AssertSucceeded (ProDosReader::ExtractFile (volume, "README", readBack, fileType, auxType));
        Assert::IsTrue (readBack == vector<Byte> (700, 0xC1), L"every file is copied");
        Assert::AreEqual ((Byte) 0x04, fileType);
    }


    TEST_METHOD (Build_WithoutAProDosVolume_Fails)
    {
        vector<Byte>  source (143360, 0);
        vector<Byte>  volume;



        AssertFailed (StockDisk800K::Build (source, volume));
        Assert::IsTrue (volume.empty());
    }


    TEST_METHOD (StockDisks_ForA35Drive_AreThe800KDisk)
    {
        std::vector<StockDiskRow>  rows = StartupDiskPicker::GetStockDisks (ReceivingDrive { ReceivingDriveKind::Apple35, 0 });



        Assert::AreEqual ((size_t) 1, rows.size());
        Assert::AreEqual (std::wstring (StockDisk800K::kpszDisplayName), rows[0].label);
        Assert::AreEqual (StartupDiskPicker::GetStockDisk800KPath().wstring(), rows[0].path);
        Assert::AreEqual ((uint64_t) StockDisk800K::kBytes, rows[0].imageClass.formattedBytes);
    }
};
