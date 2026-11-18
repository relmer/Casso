#include "Pch.h"

#include "GuestSession.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/Inspector/DiskAnalyzer.h"
#include "Devices/Disk/Inspector/FileMap/FileMap.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapStockDiskTests
//
//  The file map on Apple's own disks: the DOS 3.3 System Master and the
//  ProDOS Users Disk map as their file systems with every file listed and
//  no file system finding (FR-093, SC-014).
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FileMapStockDiskTests)
{
public:

    static FileMap MapOf (const std::vector<Byte> & raw)
    {
        DiskImage        image;
        DiskAnalysis     analysis;
        vector<FileMap>  maps;
        HRESULT          hr       = S_OK;



        hr = NibblizationLayer::NibblizeDsk (raw, image);
        Assert::IsTrue (SUCCEEDED (hr));
        DiskAnalyzer::Analyze (DiskCopy::MakeFromImage (image, 1, "stock.dsk", raw.size(), true), DecodeSettings::MakeStandard(), analysis);
        maps = FileMapBuilder::Build (analysis);
        Assert::AreEqual (static_cast<size_t> (1), maps.size());

        return maps[0];
    }



    static std::wstring ListFindings (const FileMap & map)
    {
        std::wstring  text;



        for (const Finding & finding : map.findings)
        {
            text += TextEncoding::Utf8ToWide (finding.detail) + L"\n";
        }

        return text;
    }



    TEST_METHOD (TheDos33SystemMasterMapsWithNoFindings)
    {
        FileMap  map = MapOf (GuestSession::RequireDos33Master());



        Assert::IsTrue  (map.fileSystem == MapFileSystem::Dos33);
        Assert::IsTrue  (map.isCatalogComplete);
        Assert::IsTrue  (map.files.size() >= 10, L"the master's programs");
        Assert::IsTrue  (map.findings.empty(), ListFindings (map).c_str());
    }



    TEST_METHOD (TheProDosUsersDiskMapsWithNoFindings)
    {
        FileMap  map = MapOf (GuestSession::RequireProDosUsersDisk());



        Assert::IsTrue  (map.fileSystem == MapFileSystem::ProDos);
        Assert::IsTrue  (map.isCatalogComplete);
        Assert::IsFalse (map.files.empty());
        Assert::IsTrue  (map.findings.empty(), ListFindings (map).c_str());
    }
};
