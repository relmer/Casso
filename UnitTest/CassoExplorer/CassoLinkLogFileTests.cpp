#include "Pch.h"

#include "CassoExplorer/Model/CassoLinkLogFile.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/CassoLinkHost.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkLogFileTests
//
//  The CassoLink log's lines, its file names and which old files it deletes,
//  and the packets as the host describes them for it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkLogFileTests)
{
public:

    static SYSTEMTIME  Time()
    {
        SYSTEMTIME  time = {};

        time.wYear         = 2026;
        time.wMonth        = 11;
        time.wDay          = 3;
        time.wHour         = 9;
        time.wMinute       = 5;
        time.wSecond       = 7;
        time.wMilliseconds = 42;

        return time;
    }




    TEST_METHOD (Line_HasTheTimeTheConnectionAndTheText_OnOneLine)
    {
        Assert::AreEqual (std::string ("09:05:07.042  //c on COM3  Disk 0008 is a duplicate\r\n"),
                          CassoLinkLogFile::FormatLine (Time(), "//c on COM3", "Disk 0008\nis a duplicate"));
    }




    TEST_METHOD (FileName_HasTheDay)
    {
        Assert::AreEqual (std::wstring (L"CassoLink-2026-11-03.log"), CassoLinkLogFile::GetFileName (Time()));
    }




    TEST_METHOD (Expired_IsALogBeforeTheCutoff_AndNothingElse)
    {
        Assert::IsTrue  (CassoLinkLogFile::IsExpired (L"CassoLink-2026-10-03.log", 20261004));
        Assert::IsFalse (CassoLinkLogFile::IsExpired (L"CassoLink-2026-10-04.log", 20261004));
        Assert::IsFalse (CassoLinkLogFile::IsExpired (L"CassoLink-2026-10-03.txt", 20261004));
        Assert::IsFalse (CassoLinkLogFile::IsExpired (L"notes-2026-10-03.log",     20261004));
    }




    TEST_METHOD (Packet_HasItsCommand_ItsSector_OrItsText)
    {
        const Byte  request[] = { 1, 17, 2, 5, 9, 0, 0 };
        const Byte  sector[]  = { 17, 5, 0xAA, 0xBB };
        const Byte  prompt[]  = { 'P', 'R', 'E', 'S', 'S', 0, '1', 0 };
        const Byte  none[]    = { 0 };

        Assert::AreEqual (std::string ("> ReadSectors 01 11 02 05 09 00 00"), CassoLinkHost::DescribePacket (true,  CassoLinkCodec::kReadSectors, request));
        Assert::AreEqual (std::string ("< SectorData T17 S5"),                CassoLinkHost::DescribePacket (false, CassoLinkCodec::kSectorData,  sector));
        Assert::AreEqual (std::string ("> WaitForKey PRESS | 1 | "),          CassoLinkHost::DescribePacket (true,  CassoLinkCodec::kWaitForKey,  prompt));
        Assert::AreEqual (std::string ("< WaitForKey 00"),                    CassoLinkHost::DescribePacket (false, CassoLinkCodec::kWaitForKey,  none));
        Assert::AreEqual (std::string ("< 7F"),                               CassoLinkHost::DescribePacket (false, 0x7F, {}));
    }
};
