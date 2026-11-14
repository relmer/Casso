#include "Pch.h"

#include "Core/MemoryBus.h"
#include "Devices/Floppy35/LironCard.h"
#include "Devices/Floppy35/SmartPortBus.h"
#include "Devices/Floppy35/UniDisk35Drive.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FakeSmartPortDevice
//
////////////////////////////////////////////////////////////////////////////////

class FakeSmartPortDevice : public ISmartPortDevice
{
public:
    void       Connect      (SmartPortBus * bus, size_t index) override { position = index; (void) bus; }
    void       CatchUp      (uint64_t nowNs) override { caughtUpTo = nowNs; }
    void       SetBusReset  (bool isReset) override { resets.push_back (isReset); }
    bool       IsDrivingBus() const override { return driving; }
    bool       GetAck() const override { return ack; }
    bool       PassesEnable() const override { return passes; }
    BitLine  & GetInput() override { return input; }
    BitLine  & GetOutput() override { return output; }

    size_t             position   = 0;
    uint64_t           caughtUpTo = 0;
    std::vector<bool>  resets;
    bool               driving    = false;
    bool               ack        = true;
    bool               passes     = false;
    BitLine            input;
    BitLine            output;
};





////////////////////////////////////////////////////////////////////////////////
//
//  SmartPortBusTests
//
//  The bus's lines as the host's IWM drives them, the chain's enable, the
//  bit lines between two IWMs, the UniDisk 3.5's gate array, and the UniDisk
//  3.5 interface card's ROM and IWM.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SmartPortBusTests)
{
public:

    static constexpr uint32_t  kCell = SmartPortBus::kCellNs;


    //  A transition is read in the reader's cell that has the writer's cell
    //  end, whatever the two IWMs' phase.
    TEST_METHOD (BitLine_EachTransitionInOneCell)
    {
        for (uint64_t phase : { 0ull, 1ull, 1999ull, 3999ull })
        {
            BitLine  line;
            int      ones = 0;



            for (uint64_t cell = 1; cell <= 64; cell++)
            {
                line.Write ((Byte) (cell % 3 == 0), cell * kCell, kCell);
            }

            for (uint64_t cell = 1; cell <= 65; cell++)
            {
                ones += line.Read (cell * kCell + phase, kCell);
            }

            Assert::AreEqual (21, ones);
        }
    }


    //  Transitions before the cell read went by unread.
    TEST_METHOD (BitLine_DropsStaleTransitions)
    {
        BitLine  line;



        line.Write (1, 1 * kCell, kCell);
        line.Write (1, 2 * kCell, kCell);

        Assert::AreEqual ((Byte) 0, line.Read (10 * kCell, kCell));
        Assert::AreEqual ((Byte) 0, line.Read (11 * kCell, kCell));
    }


    //  A device's bus is enabled when phases 1 and 3 are on and every
    //  device before it passes it on.
    TEST_METHOD (Bus_EnablePassesDownTheChain)
    {
        SmartPortBus         bus;
        FakeSmartPortDevice  first;
        FakeSmartPortDevice  second;



        bus.AddDevice (&first);
        bus.AddDevice (&second);
        Assert::AreEqual ((size_t) 1, second.position);

        bus.SetPhases (false, true, false, false);
        Assert::IsFalse (bus.IsEnabledAt (0));

        bus.SetPhases (false, true, false, true);
        Assert::IsTrue  (bus.IsEnabledAt (0));
        Assert::IsFalse (bus.IsEnabledAt (1));

        first.passes = true;
        Assert::IsTrue (bus.IsEnabledAt (1));
        Assert::IsTrue (bus.IsLastAt (1));
        Assert::IsFalse (bus.IsLastAt (0));
    }


    //  REQ is phase 0; phases 0 and 2 alone reset every device.
    TEST_METHOD (Bus_ReqAndReset)
    {
        SmartPortBus         bus;
        FakeSmartPortDevice  device;



        bus.AddDevice (&device);

        bus.SetPhases (true, true, false, true);
        Assert::IsTrue (bus.GetReq());
        Assert::IsTrue (device.resets.empty());

        bus.SetPhases (true, false, true, false);
        bus.SetPhases (false, false, false, false);

        Assert::AreEqual ((size_t) 2, device.resets.size());
        Assert::IsTrue  (device.resets[0]);
        Assert::IsFalse (device.resets[1]);
    }


    //  /ACK is high with no device driving the bus, and follows the driving
    //  device; the host reads only a driving device's data.
    TEST_METHOD (Bus_AckAndDataFollowTheDrivingDevice)
    {
        SmartPortBus         bus;
        FakeSmartPortDevice  device;



        bus.AddDevice (&device);

        device.ack = false;
        Assert::IsTrue (bus.GetSense(), L"no device drives /ACK");

        device.driving = true;
        Assert::IsFalse (bus.GetSense());

        device.output.Write (1, kCell, kCell);
        Assert::AreEqual ((Byte) 1, bus.ReadCell (kCell + 1));

        device.driving = false;
        device.output.Write (1, 3 * kCell, kCell);
        Assert::AreEqual ((Byte) 0, bus.ReadCell (3 * kCell + 1));

        bus.WriteCell (1, 5 * kCell);
        Assert::AreEqual ((Byte) 1, device.input.Read (5 * kCell + 1, kCell));

        bus.CatchUp (12345);
        Assert::AreEqual ((uint64_t) 12345, device.caughtUpTo);
    }


    //  The gate array: register 0 has the chain position and the bus
    //  enable; register 1 the sense line and phase 0. IWMDIR moves the IWM
    //  between the bus, at 4 microsecond cells, and the mechanism, at 2.
    TEST_METHOD (UniDisk_GateArray)
    {
        constexpr Word  kReg0 = 0x0800;
        constexpr Word  kReg1 = 0x0801;

        SmartPortBus    bus;
        UniDisk35Drive  drive;



        bus.AddDevice (&drive);

        Assert::AreEqual ((Byte) 0x10, (Byte) (drive.Read (kReg0) & 0x10), L"the only device is the last");
        Assert::AreEqual ((Byte) 0x08, (Byte) (drive.Read (kReg0) & 0x08), L"the bus isn't enabled");

        bus.SetPhases (true, true, false, true);
        Assert::AreEqual ((Byte) 0x00, (Byte) (drive.Read (kReg0) & 0x08));

        drive.Write (kReg0, 0x02);
        Assert::AreEqual (SmartPortBus::kCellNs, drive.GetCellNs());
        Assert::IsTrue (drive.GetSense(), L"the IWM's sense line reads REQ");
        Assert::AreEqual ((Byte) 0x10, (Byte) (drive.Read (kReg1) & 0x10));

        drive.SetPhases (true, false, false, false);
        Assert::IsFalse (drive.IsDrivingBus());
        drive.Write (kReg0, 0x0A);
        Assert::IsTrue (drive.IsDrivingBus());
        Assert::IsTrue (drive.GetAck(), L"/ACK follows phase 0");
        Assert::AreEqual ((Byte) 0x01, (Byte) (drive.Read (kReg1) & 0x01));

        drive.Write (kReg0, 0x08);
        drive.Write (kReg1, 0x1E);
        Assert::IsFalse (drive.GetAck(), L"/ACK is low while the drive works its mechanism");
        Assert::AreEqual (Sony35Drive::kCellNs, drive.GetCellNs());

        drive.Write (kReg0, 0x04);
        Assert::IsTrue (drive.PassesEnable());
    }


    //  Drive 2 reads as not installed; drive 1 is the mechanism.
    TEST_METHOD (UniDisk_DriveSelect)
    {
        constexpr Word  kReg1          = 0x0801;
        constexpr int   kRegInstalled  = 0xD;

        UniDisk35Drive  drive;



        drive.Write (kReg1, 0x1D);
        drive.Write (0x0800, (Byte) ((kRegInstalled >> 1) & 1));
        drive.SetPhases ((kRegInstalled & 4) != 0, (kRegInstalled & 8) != 0, (kRegInstalled & 1) != 0, false);
        Assert::IsTrue (drive.GetSense(), L"drive 2 isn't installed");

        drive.Write (kReg1, 0x1E);
        Assert::IsFalse (drive.GetSense(), L"drive 1's mechanism is installed");
    }


    TEST_METHOD (UniDisk_RejectsAWrongSizeRom)
    {
        UniDisk35Drive  drive;



        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_BAD_LENGTH), drive.SetFirmware (std::vector<Byte> (0x1000, 0)));
        Assert::IsFalse (drive.HasFirmware());
    }


    //  The card's ROM: page n at $Cn00 is ROM $n00, and $C800-$CFFF is ROM
    //  $800-$FFF.
    TEST_METHOD (Liron_RomPages)
    {
        constexpr int  kSlot = 5;

        MemoryBus          bus;
        LironCard          card (kSlot, bus);
        std::vector<Byte>  rom (LironCard::kRomSize);



        for (size_t i = 0; i < rom.size(); i++)
        {
            rom[i] = (Byte) (i >> 4);
        }

        Assert::AreEqual (S_OK, card.SetFirmware (rom));
        Assert::IsFalse (card.IsExpansionRomSelected());

        Assert::AreEqual (rom[0x5FF], card.OnSlotPageAccess (0xC5FF, false, 0));
        Assert::IsTrue (card.IsExpansionRomSelected());
        Assert::AreEqual (rom[0x800], card.ReadExpansionRom (0xC800));
        Assert::AreEqual (rom[0xFFE], card.ReadExpansionRom (0xCFFE));

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_BAD_LENGTH), card.SetFirmware (std::vector<Byte> (0x800, 0)));
    }


    //  The IWM's switches are at $C0n0-$C0nF, and the phases are on the bus with
    //  either drive selected.
    TEST_METHOD (Liron_IwmDrivesTheBus)
    {
        constexpr int  kSlot = 5;

        MemoryBus  bus;
        LironCard  card (kSlot, bus);



        card.Read (0xC0D3);
        card.Read (0xC0D7);
        Assert::IsTrue (card.GetBus().IsEnabledAt (0));

        card.Read (0xC0DB);
        card.Read (0xC0D1);
        Assert::IsTrue (card.GetBus().GetReq());

        card.Read (0xC0D0);
        Assert::IsFalse (card.GetBus().GetReq());
    }
};
