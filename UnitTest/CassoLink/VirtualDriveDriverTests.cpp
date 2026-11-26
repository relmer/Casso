#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../TestHelpers.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/LinkProgram.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  VirtualDriveDriverTests
//
//  The virtual drives' installer and block driver, the CASSOLINK file, on a
//  6502 with flat memory: a //c's identification byte, ProDOS's device table,
//  BASIC.SYSTEM's GETBUFR and the monitor's COUT as stubs, and a fake 6551
//  at the //c's port 2, whose bytes from the Apple go to a scripted PC. The
//  installer puts the driver at GETBUFR's page and lists it for slot 7's
//  drives, and each test calls the driver as ProDOS does.
//
//  A request is the envelope ($C1, the length, Virtual block, the header's
//  EOR), then the drive, the operation, the block and for a write its 512
//  bytes and their CRC, then the payload's EOR. A reply is the envelope,
//  ProDOS's error, then for a status the drive's blocks and for a read the
//  block and its CRC, then the payload's EOR.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (VirtualDriveDriverTests)
{
public:

    static constexpr Word      kFile              = 0x2000;
    static constexpr Word      kStub              = 0x0300;
    static constexpr Word      kGetBufr           = 0xBEF5;
    static constexpr Word      kCout              = 0xFDED;
    static constexpr Word      kDevAdr            = 0xBF10;
    static constexpr Word      kDevCnt            = 0xBF31;
    static constexpr Word      kDevLst            = 0xBF32;
    static constexpr Word      kAciaData          = 0xC0A8;
    static constexpr Word      kAciaStat          = 0xC0A9;
    static constexpr Word      kAciaCmd           = 0xC0AA;
    static constexpr Word      kAciaCtl           = 0xC0AB;
    static constexpr Word      kPort1Data         = 0xC098;   // a //c's port 1, with no cable
    static constexpr Word      kPort1Stat         = 0xC099;
    static constexpr Word      kBuffer            = 0x4000;
    static constexpr Word      kPrinted           = 0x5000;   // COUT's characters, from the stub
    static constexpr Word      kPrintedCount      = 0x5100;
    static constexpr Word      kBottomLine        = 0x07D0;
    static constexpr Word      kRd80Vid           = 0xC01F;
    static constexpr Word      kPage2Off          = 0xC054;
    static constexpr Word      kPage2On           = 0xC055;
    static constexpr Byte      kDriverPage        = 0x90;
    static constexpr Word      kNoDevice          = 0xDEAD;
    static constexpr Word      kOtherDevice       = 0x1234;
    static constexpr Byte      kStart             = 0xC1;
    static constexpr Byte      kStatus            = 0;
    static constexpr Byte      kRead              = 1;
    static constexpr Byte      kWrite             = 2;
    static constexpr Byte      kFormat            = 3;
    static constexpr Byte      kErrIo             = 0x27;
    static constexpr Byte      kErrWriteProtected = 0x2B;
    static constexpr int       kTries             = 4;
    static constexpr uint32_t  kCallCycles        = 60'000'000;
    static constexpr size_t    kBlock             = 512;



    //  A request from the Apple, as the scripted PC reads it.
    struct Request
    {
        std::vector<Byte>  bytes;
        Byte               drive = 0;
        Byte               op    = 0;
        Word               block = 0;
        std::vector<Byte>  data;
        Word               crc   = 0;
    };


    //  The 6551 at slot 2's $C0A8 to $C0AB: the transmitter always empty, and
    //  a byte received while the PC's reply has one left. Each complete
    //  request is passed to `answer`, whose bytes are the reply.
    class DriverCpu : public TestCpu
    {
    public:
        std::function<std::vector<Byte> (const Request &)>  answer;
        std::vector<Request>  requests;
        std::vector<Request>  installRequests;   // the installer's, from Install
        std::deque<Byte>      arriving;
        std::vector<Byte>     sending;
        Byte                  command         = 0;
        Byte                  control         = 0;
        size_t                port1Bytes      = 0;
        bool                  page2           = false;
        std::array<Byte, 40>  auxLine         = {};   // auxiliary memory's bottom line

        Byte  ReadByteSlow (Word address) override
        {
            Byte  value = memory[address];



            if (address == kPort1Stat)
            {
                value = 0x10;
            }
            else if (address == kAciaStat)
            {
                value = (Byte) (0x10 | (arriving.empty() ? 0x00 : 0x08));
            }
            else if (address == kAciaData)
            {
                value = 0;

                if (!arriving.empty())
                {
                    value = arriving.front();
                    arriving.pop_front();
                }
            }

            return value;
        }

        void  WriteByte (Word address, Byte value) override
        {
            if (address == kPort1Data)
            {
                port1Bytes++;
            }
            else if (address == kPage2On || address == kPage2Off)
            {
                page2 = address == kPage2On;
            }
            else if (page2 && address >= kBottomLine && address < kBottomLine + auxLine.size())
            {
                auxLine[(size_t) (address - kBottomLine)] = value;
            }
            else if (address == kAciaData)
            {
                sending.push_back (value);
                TakeRequest();
            }
            else if (address == kAciaCmd)
            {
                command = value;
            }
            else if (address == kAciaCtl)
            {
                control = value;
            }
            else if (address != kAciaStat)
            {
                memory[address] = value;
            }
        }

    private:
        void  TakeRequest()
        {
            size_t   length = 0;
            Request  request;



            if (sending.size() < 5)
            {
                return;
            }

            length = (size_t) sending[1] | ((size_t) sending[2] << 8);

            if (sending.size() < 5 + length + 1)
            {
                return;
            }

            request.bytes = sending;
            request.drive = sending[5];
            request.op    = sending[6];
            request.block = (Word) (sending[7] | (sending[8] << 8));

            if (request.op == kWrite)
            {
                request.data.assign (sending.begin() + 9, sending.begin() + 9 + (ptrdiff_t) kBlock);
                request.crc = (Word) (sending[9 + kBlock] | (sending[10 + kBlock] << 8));
            }

            requests.push_back (request);
            sending.clear();

            if (answer)
            {
                for (Byte b : answer (request))
                {
                    arriving.push_back (b);
                }
            }
        }
    };




    //  A reply: the envelope, then `payload` and its EOR.
    static std::vector<Byte>  MakeReply (const std::vector<Byte> & payload)
    {
        std::vector<Byte>  reply  = { kStart, (Byte) (payload.size() & 0xFF), (Byte) (payload.size() >> 8), CassoLinkCodec::kVirtualBlock };
        Byte               header = 0;



        for (Byte b : reply)
        {
            header ^= b;
        }

        reply.push_back (header);
        reply.insert (reply.end(), payload.begin(), payload.end());
        reply.push_back (CassoLinkCodec::ComputeEor (payload));
        return reply;
    }


    //  A read's reply: no error, the block and its CRC, `crcDelta` added to
    //  the CRC for a damaged one.
    static std::vector<Byte>  MakeReadReply (const std::vector<Byte> & block, Word crcDelta = 0)
    {
        std::vector<Byte>  payload = { 0 };
        Word               crc     = (Word) (CassoLinkCodec::ComputeCrc (block) + crcDelta);



        payload.insert (payload.end(), block.begin(), block.end());
        payload.push_back ((Byte) (crc & 0xFF));
        payload.push_back ((Byte) (crc >> 8));
        return MakeReply (payload);
    }


    static std::vector<Byte>  MakeBlock (Byte seed)
    {
        std::vector<Byte>  block (kBlock);



        for (size_t i = 0; i < block.size(); i++)
        {
            block[i] = (Byte) (seed + i * 7);
        }

        return block;
    }


    static Word  PeekWord (DriverCpu & cpu, Word address)
    {
        return (Word) (cpu.Peek (address) | (cpu.Peek ((Word) (address + 1)) << 8));
    }


    //  A JSR to `entry`, run until it returns.
    static void  Call (DriverCpu & cpu, Word entry)
    {
        cpu.Poke (kStub, 0x20);
        cpu.PokeWord ((Word) (kStub + 1), entry);
        cpu.RegPC() = kStub;
        cpu.RegSP() = 0xFF;

        Assert::IsTrue (cpu.RunUntil ((Word) (kStub + 3), kCallCycles) == TestCpu::StopReason::ReachedTarget, L"the call returns");
    }


    //  The text COUT was given, high bits off.
    static std::string  GetPrinted (DriverCpu & cpu)
    {
        std::string  text;



        for (Word i = 0; i < cpu.Peek (kPrintedCount); i++)
        {
            text += (char) (cpu.Peek ((Word) (kPrinted + i)) & 0x7F);
        }

        return text;
    }


    //  The text screen's bottom line in 40 columns, high bits off, or in 80
    //  columns with the even columns from auxiliary memory.
    static std::string  GetBottomLine (DriverCpu & cpu, bool eighty)
    {
        std::string  text;



        for (size_t i = 0; i < (eighty ? 80u : 40u); i++)
        {
            Byte  b = eighty ? (i % 2 == 0 ? cpu.auxLine[i / 2] : cpu.Peek ((Word) (kBottomLine + i / 2))) : cpu.Peek ((Word) (kBottomLine + i));

            text += (char) (b & 0x7F);
        }

        return text;
    }


    //  The CASSOLINK file at $2000 and the machine it runs on, and the
    //  installer run: ProDOS's slot 0 entries are the no-device entry, which
    //  slot 7's drives have too, GETBUFR returns kDriverPage, and COUT's stub
    //  logs each character at kPrinted. The installer's status call is
    //  has a reply when `pcAnswers`, and is taken out of the requests after.
    static void  Install (DriverCpu & cpu, bool pcAnswers = false)
    {
        std::vector<Byte>  file;
        std::string        errors;
        Byte               speed = 0;



        Assert::IsTrue (LinkProgram::GetSpeedControl (115200, speed));
        AssertSucceeded (LinkProgram::BuildVirtualDrive (speed, file, errors));

        cpu.InitForTest (kFile);

        for (size_t i = 0; i < file.size(); i++)
        {
            cpu.Poke ((Word) (kFile + i), file[i]);
        }

        cpu.Poke (0xFBC0, 0x00);
        cpu.WriteBytes (kGetBufr, { 0xA9, kDriverPage, 0x18, 0x60 });
        cpu.WriteBytes (kCout, { 0x8E, 0x01, 0x51,      // STX $5101
                                 0xAE, 0x00, 0x51,      // LDX $5100
                                 0x9D, 0x00, 0x50,      // STA $5000,X
                                 0xEE, 0x00, 0x51,      // INC $5100
                                 0xAE, 0x01, 0x51,      // LDX $5101
                                 0x60 });               // RTS

        for (Word slot = 0; slot < 8; slot++)
        {
            Word  device = (slot == 0 || slot == 7) ? kNoDevice : kOtherDevice;

            cpu.PokeWord ((Word) (kDevAdr + slot * 2), device);
            cpu.PokeWord ((Word) (kDevAdr + 0x10 + slot * 2), device);
        }

        cpu.Poke (kDevCnt, 0);
        cpu.Poke (kDevLst, 0x60);

        if (pcAnswers)
        {
            cpu.answer = [] (const Request &) { return MakeReply ({ 0, 0x18, 0x01 }); };
        }

        Call (cpu, kFile);
        Assert::AreEqual (speed, cpu.control, L"the 6551 at the connection's speed");

        cpu.answer          = nullptr;
        cpu.installRequests = cpu.requests;
        cpu.requests.clear();
        cpu.port1Bytes = 0;
    }


    //  A ProDOS call to the driver: the command, the unit, the buffer and
    //  the block at $42 to $47.
    static void  CallDriver (DriverCpu & cpu, Byte command, Byte unit, Word block)
    {
        cpu.Poke (0x42, command);
        cpu.Poke (0x43, unit);
        cpu.PokeWord (0x44, kBuffer);
        cpu.PokeWord (0x46, block);
        Call (cpu, (Word) (kDriverPage << 8));
    }


    //  The envelope and both EORs of a request are right.
    static void  AssertWellFormed (const Request & request)
    {
        size_t  length = (size_t) request.bytes[1] | ((size_t) request.bytes[2] << 8);
        Byte    header = (Byte) (request.bytes[0] ^ request.bytes[1] ^ request.bytes[2] ^ request.bytes[3]);



        Assert::AreEqual (kStart, request.bytes[0]);
        Assert::AreEqual (CassoLinkCodec::kVirtualBlock, request.bytes[3]);
        Assert::AreEqual (header, request.bytes[4], L"the header's EOR");
        Assert::AreEqual (CassoLinkCodec::ComputeEor (std::span<const Byte> (request.bytes).subspan (5, length)), request.bytes[5 + length], L"the payload's EOR");
    }




    //  The installer lists the driver for both of slot 7's drives, its first
    //  three pages at GETBUFR's page, and sets up the 6551 with DTR on and no
    //  interrupts.
    TEST_METHOD (TheInstaller_ListsTheDriverForSlot7sDrives)
    {
        DriverCpu  cpu;



        Install (cpu);

        Assert::AreEqual ((Word) (kDriverPage << 8), PeekWord (cpu, (Word) (kDevAdr + 14)), L"drive 1's entry");
        Assert::AreEqual ((Word) (kDriverPage << 8), PeekWord (cpu, (Word) (kDevAdr + 0x10 + 14)), L"drive 2's entry");
        Assert::AreEqual ((Byte) 2, cpu.Peek (kDevCnt), L"two devices added");
        Assert::AreEqual ((Byte) 0x70, cpu.Peek ((Word) (kDevLst + 1)));
        Assert::AreEqual ((Byte) 0xF0, cpu.Peek ((Word) (kDevLst + 2)));
        Assert::AreEqual ((Byte) 0x0B, cpu.command);
    }


    //  A read of drive 1 requests the block, and the block from the PC is in
    //  the buffer, with no error.
    TEST_METHOD (ARead_PutsThePcsBlockInTheBuffer)
    {
        DriverCpu          cpu;
        std::vector<Byte>  block = MakeBlock (3);
        std::vector<Byte>  read  (kBlock);



        Install (cpu);
        cpu.answer = [&] (const Request &) { return MakeReadReply (block); };

        CallDriver (cpu, kRead, 0x70, 0x0123);

        Assert::AreEqual ((size_t) 1, cpu.requests.size());
        AssertWellFormed (cpu.requests[0]);
        Assert::AreEqual ((Byte) 1, cpu.requests[0].drive);
        Assert::AreEqual (kRead, cpu.requests[0].op);
        Assert::AreEqual ((Word) 0x0123, cpu.requests[0].block);

        for (size_t i = 0; i < kBlock; i++)
        {
            read[i] = cpu.Peek ((Word) (kBuffer + i));
        }

        Assert::IsTrue (read == block, L"the block in the buffer");
        Assert::IsFalse (cpu.Status().flags.carry != 0, L"no error");
        Assert::AreEqual ((Word) kBuffer, PeekWord (cpu, 0x44), L"the buffer's address as ProDOS set it");
    }


    //  A write of drive 2 sends the buffer's block and its CRC.
    TEST_METHOD (AWrite_SendsTheBlockAndItsCrc)
    {
        DriverCpu          cpu;
        std::vector<Byte>  block = MakeBlock (9);



        Install (cpu);

        for (size_t i = 0; i < kBlock; i++)
        {
            cpu.Poke ((Word) (kBuffer + i), block[i]);
        }

        cpu.answer = [&] (const Request &) { return MakeReply ({ 0 }); };

        CallDriver (cpu, kWrite, 0xF0, 0x0005);

        Assert::AreEqual ((size_t) 1, cpu.requests.size());
        AssertWellFormed (cpu.requests[0]);
        Assert::AreEqual ((Byte) 2, cpu.requests[0].drive);
        Assert::AreEqual (kWrite, cpu.requests[0].op);
        Assert::AreEqual ((Word) 5, cpu.requests[0].block);
        Assert::IsTrue (cpu.requests[0].data == block, L"the buffer's block");
        Assert::AreEqual (CassoLinkCodec::ComputeCrc (block), cpu.requests[0].crc, L"its CRC");
        Assert::IsFalse (cpu.Status().flags.carry != 0, L"no error");
    }


    //  A status call returns the drive's blocks from the PC in X and Y.
    TEST_METHOD (AStatus_ReturnsTheDrivesBlocks)
    {
        DriverCpu  cpu;



        Install (cpu);
        cpu.answer = [] (const Request &) { return MakeReply ({ 0, 0x18, 0x01 }); };

        CallDriver (cpu, kStatus, 0x70, 0);

        AssertWellFormed (cpu.requests[0]);
        Assert::AreEqual (kStatus, cpu.requests[0].op);
        Assert::AreEqual ((size_t) (5 + 4 + 5 + 1), cpu.requests[0].bytes.size(), L"the ROM's ID bytes after the block");
        Assert::AreEqual ((Byte) 0x18, cpu.RegX(), L"280 blocks, low");
        Assert::AreEqual ((Byte) 0x01, cpu.RegY(), L"280 blocks, high");
        Assert::IsFalse (cpu.Status().flags.carry != 0, L"no error");
    }


    //  ProDOS's error from the PC is returned in A with C set.
    TEST_METHOD (ThePcsError_IsReturnedToProDos)
    {
        DriverCpu  cpu;



        Install (cpu);
        cpu.answer = [] (const Request &) { return MakeReply ({ kErrWriteProtected }); };

        CallDriver (cpu, kWrite, 0x70, 0);

        Assert::IsTrue (cpu.Status().flags.carry != 0, L"an error");
        Assert::AreEqual (kErrWriteProtected, cpu.RegA());
        Assert::AreEqual ((size_t) 1, cpu.requests.size(), L"an error isn't requested again");
    }


    //  A read whose block arrives damaged is requested again; once the reply is
    //  whole, the block is in the buffer.
    TEST_METHOD (ADamagedRead_IsAskedAgain)
    {
        DriverCpu          cpu;
        std::vector<Byte>  block = MakeBlock (5);



        Install (cpu);
        cpu.answer = [&] (const Request &) { return MakeReadReply (block, cpu.requests.size() == 1 ? 1 : 0); };

        CallDriver (cpu, kRead, 0x70, 7);

        Assert::AreEqual ((size_t) 2, cpu.requests.size(), L"requested once more");
        Assert::AreEqual (block[0], cpu.Peek (kBuffer));
        Assert::AreEqual (block[kBlock - 1], cpu.Peek ((Word) (kBuffer + kBlock - 1)));
        Assert::IsFalse (cpu.Status().flags.carry != 0, L"no error");
    }


    //  A read damaged every time is requested four times in all, then is
    //  ProDOS's I/O error.
    TEST_METHOD (AReadDamagedEveryTime_IsAnIoErrorAfterFourTries)
    {
        DriverCpu          cpu;
        std::vector<Byte>  block = MakeBlock (1);



        Install (cpu);
        cpu.answer = [&] (const Request &) { return MakeReadReply (block, 1); };

        CallDriver (cpu, kRead, 0x70, 7);

        Assert::AreEqual ((size_t) kTries / 2, cpu.requests.size(), L"port 2 and port 1 in turn");
        Assert::IsTrue   (cpu.port1Bytes > 0);
        Assert::IsTrue (cpu.Status().flags.carry != 0, L"an error");
        Assert::AreEqual (kErrIo, cpu.RegA());
    }


    //  With no reply, each request times out, and after four the call is
    //  ProDOS's I/O error.
    TEST_METHOD (NoReply_IsAnIoErrorAfterFourTries)
    {
        DriverCpu  cpu;



        Install (cpu);

        CallDriver (cpu, kRead, 0x70, 0);

        Assert::AreEqual ((size_t) kTries / 2, cpu.requests.size(), L"port 2 and port 1 in turn");
        Assert::IsTrue   (cpu.port1Bytes > 0);
        Assert::IsTrue (cpu.Status().flags.carry != 0, L"an error");
        Assert::AreEqual (kErrIo, cpu.RegA());
    }


    //  With a reply to the installer's status call, the installer
    //  prints the drives' slot and no other message.
    TEST_METHOD (TheInstaller_PrintsNoMessage_WhenThePcAnswers)
    {
        DriverCpu    cpu;
        std::string  printed;



        Install (cpu, true);
        printed = GetPrinted (cpu);

        Assert::IsTrue (printed.find ("CASSOLINK DRIVES IN SLOT 7") != std::string::npos);
        Assert::IsTrue (printed.find ("ISN'T CONNECTED") == std::string::npos);
    }


    //  A status call has the ROM's identification bytes after the block:
    //  $FBB3, $FBC0, $FBBF, $FB1E, and 0 on a //c.
    TEST_METHOD (TheInstallersStatus_HasTheRomsIdBytes)
    {
        DriverCpu  cpu;



        Install (cpu, true);

        Assert::AreEqual ((size_t) 1, cpu.installRequests.size());
        AssertWellFormed (cpu.installRequests[0]);
        Assert::AreEqual (kStatus, cpu.installRequests[0].op);
        Assert::AreEqual ((size_t) (5 + 4 + 5 + 1), cpu.installRequests[0].bytes.size(), L"the block, then five ID bytes");
        Assert::AreEqual ((Byte) 0x00, cpu.installRequests[0].bytes[10], L"$FBC0, a //c");
    }


    //  With no reply to the installer's status call, the installer prints
    //  CASSOLINK ISN'T CONNECTED and writes nothing to the screen itself.
    //  The driver's calls after it send each request up to four times again.
    TEST_METHOD (TheInstaller_PrintsIsntConnected_WhenThePcDoesntAnswer)
    {
        DriverCpu  cpu;



        Install (cpu);

        Assert::IsTrue   (GetPrinted (cpu).find ("CASSOLINK ISN'T CONNECTED") != std::string::npos);
        Assert::AreEqual ((Byte) 0, cpu.Peek (kBottomLine), L"no screen message from the installer's call");

        CallDriver (cpu, kRead, 0x70, 0);

        Assert::AreEqual ((size_t) kTries / 2, cpu.requests.size());
    }


    //  With no byte back from the PC, the driver writes CASSOLINK ISN'T
    //  CONNECTED on the 40-column screen's bottom line.
    TEST_METHOD (NoReply_WritesIsntConnectedOnTheBottomLine)
    {
        DriverCpu  cpu;



        Install (cpu);

        CallDriver (cpu, kRead, 0x70, 0);

        Assert::AreEqual (std::string ("CASSOLINK ISN'T CONNECTED"), GetBottomLine (cpu, false).substr (0, 25));
        Assert::AreEqual ((Byte) ('C' | 0x80), cpu.Peek (kBottomLine), L"normal text");
    }


    //  In 80 columns, the message's even columns are in auxiliary memory.
    TEST_METHOD (NoReply_WritesIsntConnectedIn80Columns)
    {
        DriverCpu  cpu;



        Install (cpu);
        cpu.Poke (kRd80Vid, 0x80);

        CallDriver (cpu, kRead, 0x70, 0);

        Assert::AreEqual (std::string ("CASSOLINK ISN'T CONNECTED"), GetBottomLine (cpu, true).substr (0, 25));
        Assert::IsFalse  (cpu.page2, L"PAGE2 off after");
    }


    //  A reply that arrives damaged every time is from a connected PC, so
    //  the screen has no message.
    TEST_METHOD (ADamagedReply_WritesNoMessage)
    {
        DriverCpu          cpu;
        std::vector<Byte>  block = MakeBlock (1);



        Install (cpu);
        cpu.answer = [&] (const Request &) { return MakeReadReply (block, 1); };

        CallDriver (cpu, kRead, 0x70, 7);

        Assert::AreEqual ((Byte) 0, cpu.Peek (kBottomLine));
    }


    //  FORMAT isn't served: it is ProDOS's I/O error, with nothing sent.
    TEST_METHOD (Format_IsAnIoError_WithNothingSent)
    {
        DriverCpu  cpu;



        Install (cpu);

        CallDriver (cpu, kFormat, 0x70, 0);

        Assert::IsTrue (cpu.requests.empty());
        Assert::IsTrue (cpu.sending.empty());
        Assert::IsTrue (cpu.Status().flags.carry != 0, L"an error");
        Assert::AreEqual (kErrIo, cpu.RegA());
    }
};
