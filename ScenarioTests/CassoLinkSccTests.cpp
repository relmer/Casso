#include "Pch.h"

#include "EhmTestHelper.h"
#include "GuestSession.h"
#include "Machines/Apple2/Common/SectorInterleave.h"
#include "TestMachine.h"
#include "TextScreenScraper.h"
#include "CassoLink/FakeScc8530.h"
#include "CassoLink/InMemoryCassoLinkFolder.h"
#include "CassoLink/SteppedByteStream.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/LinkProgram.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Machines/Apple2/Common/LanguageCard.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Seams/InProcessByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSccScenarioTests
//
//  The link over a IIgs's modem port on an enhanced //e made to look like a
//  IIgs: an ID call at $FE1F that clears the C flag, a port's firmware in
//  slots 1 and 2, and a fake 8530 at $C036 to
//  $C03B. The universal disk boots, waits on both ports, and starts the
//  shim on port 2 at the host's prompt; the host loads the program and
//  reads a track of drive 2 over the 8530's channel B. The host runs on the
//  test's thread through a SteppedByteStream.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkSccScenarioTests)
{
public:

    static constexpr uint64_t  kBootCycles = 30'000'000;
    static constexpr DWORD     kBaud       = 9600;
    static constexpr Byte      kTrack      = 1;
    static constexpr size_t    kDiskBytes  = 143'360;
    static constexpr Word      kRomStart   = 0xD000;



    //  A port's firmware for slots 1 and 2: the Pascal signature and the
    //  Super Serial Card's ID byte, each entry an RTS.
    static std::vector<Byte>  MakePortFirmware()
    {
        std::vector<Byte>  rom (256, 0);



        rom[0x05] = 0x38;
        rom[0x07] = 0x18;
        rom[0x0B] = 0x01;
        rom[0x0C] = 0x31;

        for (size_t entry = 0x0D; entry <= 0x10; entry++)
        {
            rom[entry] = 0x40;
        }

        rom[0x40] = 0x60;
        return rom;
    }


    //  The //e's keyboard device covers $C000 to $C063, $C036 to $C03B with
    //  it, and starts below the 8530. This device has its range, so it is
    //  dispatched ahead of it, and sends the 8530's addresses to the fake and
    //  the rest to the keyboard.
    class IigsIoPage : public MemoryDevice
    {
    public:
        IigsIoPage (MemoryDevice & keyboard, FakeScc8530 & scc) : m_keyboard (keyboard), m_scc (scc) {}

        Word  GetStart() const override { return m_keyboard.GetStart(); }
        Word  GetEnd() const override { return m_keyboard.GetEnd(); }
        void  Reset() override {}

        Byte  Read (Word address) override
        {
            return IsScc (address) ? m_scc.Read (address) : m_keyboard.Read (address);
        }

        void  Write (Word address, Byte value) override
        {
            if (IsScc (address))
            {
                m_scc.Write (address, value);
            }
            else
            {
                m_keyboard.Write (address, value);
            }
        }

    private:
        bool  IsScc (Word address) const { return address >= m_scc.GetStart() && address <= m_scc.GetEnd(); }

        MemoryDevice  & m_keyboard;
        FakeScc8530   & m_scc;
    };


    struct Rig
    {
        TestMachine                           machine;
        FakeScc8530                           scc;
        std::unique_ptr<IigsIoPage>           ioPage;
        std::shared_ptr<InProcessByteStream>  appleEnd;
        std::shared_ptr<InProcessByteStream>  pcEnd;
        std::shared_ptr<SteppedByteStream>    stream;
        InMemoryCassoLinkFolder               folder;
        std::unique_ptr<CassoLinkHost>        host;
        std::vector<Byte>                     disk;

        Rig()
            : machine ("Apple2eEnhanced", [] (MachineConfig & config)
              {
                  std::erase_if (config.slots, [] (const SlotConfig & slot) { return slot.slot == 1 || slot.slot == 2 || slot.device == kpszSuperSerialCardDevice; });
              })
        {
            LinkShimOptions    options;
            std::vector<Byte>  boot;
            std::vector<Byte>  blank (kDiskBytes, 0);
            FilePayload        file;



            machine.SetVideoTiming (std::make_unique<VideoTiming>());
            machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());

            PatchIdCall();
            ioPage = std::make_unique<IigsIoPage> (*machine.GetRefs().keyboard, scc);
            machine.GetMemoryBus().AddDevice (ioPage.get());
            machine.GetMmu()->AttachSlotRom (1, MakePortFirmware());
            machine.GetMmu()->AttachSlotRom (2, MakePortFirmware());

            InProcessByteStream::CreatePair (appleEnd, pcEnd);
            scc.Attach (FakeScc8530::kB, appleEnd);

            options.baud = kBaud;
            AssertSucceeded (LinkProgram::BuildUniversalBootDisk (options, boot));

            file.bytes.resize (2000);
            file.type           = ProDosVolume::kTypeBinary;
            file.loadAddress    = 0x2000;
            file.hasLoadAddress = true;

            for (size_t i = 0; i < file.bytes.size(); i++)
            {
                file.bytes[i] = (Byte) (i * 7);
            }

            AssertSucceeded (ProDosSkeleton::Write (blank, "DRIVETWO"));
            AssertSucceeded (ProDosVolume (blank).Write (FilePath::Parse ("DATA"), file, disk));

            GuestSession::Mount (machine, boot);
            AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 1, "", DiskFormat::Dsk, disk));

            stream = std::make_shared<SteppedByteStream> (machine, pcEnd);
            host   = std::make_unique<CassoLinkHost> (stream, folder, [] () { return std::array<Byte, 4> {}; });
        }

        ~Rig()
        {
            machine.GetMemoryBus().RemoveDevice (ioPage.get());
        }

        //  The ROM's ID call at $FE1F, an RTS on a //e, made a IIgs's: CLC
        //  and RTS. $FE20, the monitor's LT, is overwritten.
        void  PatchIdCall()
        {
            LanguageCard       * lc  = machine.GetRefs().languageCard;
            std::vector<Byte>    rom (0x10000 - kRomStart);



            for (size_t i = 0; i < rom.size(); i++)
            {
                rom[i] = lc->ReadRom ((Word) (kRomStart + i));
            }

            rom[0xFE1F - kRomStart] = 0x18;
            rom[0xFE20 - kRomStart] = 0x60;

            lc->SetRomData (rom);
            lc->RebindWindow();
        }

        //  A track's 16 physical sectors from drive 2's image, in order.
        std::vector<Byte>  ImageTrack (Byte track)
        {
            std::vector<Byte>  data;



            for (int p = 0; p < 16; p++)
            {
                size_t  offset = (size_t) (track * 16 + SectorInterleave::GetDosFileIndexForPhysicalSector (p)) * 256;

                data.insert (data.end(), disk.begin() + (ptrdiff_t) offset, disk.begin() + (ptrdiff_t) offset + 256);
            }

            return data;
        }

        std::string  ScreenText()
        {
            std::string  joined;



            for (const std::string & row : TextScreenScraper::Scrape40 (machine.GetMemoryBus(), 0x0400))
            {
                joined += row + "|";
            }

            return joined;
        }
    };




    //  The universal disk lists both ports, the shim starts on port 2 at
    //  the host's prompt, and the program reads a track over channel B.
    TEST_METHOD (AIIgsModemPort_ReadsADisk)
    {
        Rig                rig;
        std::vector<Byte>  sectors (16);
        std::vector<Byte>  data;
        CassoLinkStatus    status;
        std::string        screen;
        HRESULT            hr = S_OK;



        rig.machine.RunCycles (kBootCycles);
        screen = rig.ScreenText();

        Assert::IsTrue (screen.find ("SERIAL PORT 1, 9600") != std::string::npos, TextEncoding::NarrowToWide (screen).c_str());
        Assert::IsTrue (screen.find ("SERIAL PORT 2, 9600") != std::string::npos, TextEncoding::NarrowToWide (screen).c_str());

        for (size_t i = 0; i < sectors.size(); i++)
        {
            sectors[i] = (Byte) i;
        }

        AssertSucceeded (rig.host->PromptReady());
        hr = rig.host->ReadSectors (2, kTrack, sectors, data, status);
        Assert::IsTrue (SUCCEEDED (hr), std::format (L"{:08X}: {} bytes sent on port 2 at {} baud, {} garbled | {}", (uint32_t) hr, rig.scc.GetSent (FakeScc8530::kB).size(), rig.scc.GetBaud (FakeScc8530::kB), rig.scc.GetGarbled (FakeScc8530::kB), TextEncoding::NarrowToWide (rig.ScreenText())).c_str());
        Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error);
        Assert::IsTrue (data == rig.ImageTrack (kTrack), L"drive 2's track over the modem port");
        Assert::IsTrue (rig.scc.GetSent (FakeScc8530::kA).empty(), L"nothing on port 1");
        Assert::AreEqual (0, rig.scc.GetGarbled (FakeScc8530::kB), L"every byte at the port's rate");
    }
};
