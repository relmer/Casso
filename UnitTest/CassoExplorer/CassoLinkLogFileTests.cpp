#include "Pch.h"

#include "CassoExplorer/Model/CassoLinkLogFile.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/CassoLinkPacketText.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkLogFileTests
//
//  The CassoLink log's CSV rows, the rows it drops past 30 days, and the
//  packets as CassoLinkPacketText describes them for it.
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




    TEST_METHOD (Row_HasTheDateAndTimeTheConnectionTheKindAndTheMessage)
    {
        Assert::AreEqual (std::string ("2026-11-03 09:05:07.042,Serial (COM3),Sent,Idle\r\n"),
                          CassoLinkLogFile::FormatRow (Time(), "Serial (COM3)", CassoLinkLogFile::kSent, "Idle"));
    }




    TEST_METHOD (Row_QuotesAFieldWithACommaOrAQuote)
    {
        Assert::AreEqual (std::string ("2026-11-03 09:05:07.042,Serial,Activity,\"Disk 0008 is \"\"8\"\", a duplicate\"\r\n"),
                          CassoLinkLogFile::FormatRow (Time(), "Serial", CassoLinkLogFile::kActivity, "Disk 0008 is \"8\", a duplicate"));
    }




    TEST_METHOD (KeepFrom_DropsRowsBeforeTheCutoff_AndKeepsTheHeader)
    {
        std::string  text = std::string (CassoLinkLogFile::kHeader) + "\r\n"
                          + "2026-10-02 23:59:59.000,A,Sent,Idle\r\n"
                          + "2026-10-03 00:00:00.000,A,Activity,\"two\r\nlines\"\r\n"
                          + "2026-11-03 09:00:00.000,A,Received,Ready\r\n";

        Assert::AreEqual (std::string (CassoLinkLogFile::kHeader) + "\r\n"
                          + "2026-10-03 00:00:00.000,A,Activity,\"two\r\nlines\"\r\n"
                          + "2026-11-03 09:00:00.000,A,Received,Ready\r\n",
                          CassoLinkLogFile::KeepFrom (text, "2026-10-03"));
    }




    TEST_METHOD (Packet_ReadsAsPlainEnglish)
    {
        const Byte  request[] = { 1, 17, 2, 5, 9, 0, 0 };
        const Byte  sector[]  = { 17, 5, 0xAA, 0xBB };
        const Byte  prompt[]  = { CassoLinkCodec::kWaitKeys, 'P', 'R', 'E', 'S', 'S', 0, '1', 0 };
        const Byte  key[]     = { CassoLinkCodec::kKeySpace };
        const Byte  status[]  = { 1, 2, 30, 0, 0x20 };
        const Byte  ack[]     = { CassoLinkCodec::kAck, 3, 17 };
        const Byte  nak[]     = { CassoLinkCodec::kNak, 3, 17 };

        Assert::AreEqual (std::string ("Read sectors: drive 1, track 17, sectors 5, 9"),             CassoLinkPacketText::Describe (CassoLinkCodec::kReadSectors,   request));
        Assert::AreEqual (std::string ("Sector data: track 17, sector 5"),                           CassoLinkPacketText::Describe (CassoLinkCodec::kSectorData,    sector));
        Assert::AreEqual (std::string ("Wait for a session key: \"PRESS / 1\""),                     CassoLinkPacketText::Describe (CassoLinkCodec::kWaitForKey,    prompt));
        Assert::AreEqual (std::string ("Key: Space"),                                                CassoLinkPacketText::Describe (CassoLinkCodec::kWaitForKey,    key));
        Assert::AreEqual (std::string ("Status: drive 2, track 30, 0 sectors done, sector not found"), CassoLinkPacketText::Describe (CassoLinkCodec::kStatus,        status));
        Assert::AreEqual (std::string ("Acknowledge: ACK, 3 bytes"),                                 CassoLinkPacketText::Describe (CassoLinkCodec::kAcknowledge,   ack));
        Assert::AreEqual (std::string ("Acknowledge: NAK, 3 bytes"),                                 CassoLinkPacketText::Describe (CassoLinkCodec::kAcknowledge,   nak));
        Assert::AreEqual (std::string ("Command 7F, 0 bytes"),                                       CassoLinkPacketText::Describe (0x7F, {}));
    }




    TEST_METHOD (KrunchPackets_ReadAsPlainEnglish)
    {
        const Byte  request[]  = { 1, 0x34, 0x12 };
        const Byte  progress[] = { 45, 3 };
        const Byte  changed[]  = { 2, 1, 0, 0, CassoLinkHost::kErrorChanged };

        Assert::AreEqual (std::string ("Krunch: drive 1, directory CRC 1234"),       CassoLinkPacketText::Describe (CassoLinkCodec::kKrunch,         request));
        Assert::AreEqual (std::string ("Krunch progress: 45%, 3 files moved"),       CassoLinkPacketText::Describe (CassoLinkCodec::kKrunchProgress, progress));
        Assert::AreEqual (std::string ("Status: drive 1, track 0, 0 sectors done, the directory differs from the one checked"),
                          CassoLinkPacketText::Describe (CassoLinkCodec::kStatus, changed));
    }
};
