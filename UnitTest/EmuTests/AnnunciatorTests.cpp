#include "Pch.h"

#include "Core/Prng.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleMouse.h"
#include "Machines/Apple2/Common/AppleSoftSwitchBank.h"

#include "TestMachine.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AnnunciatorTests
//
//  AN0-AN2 at $C058-$C05D: the even address turns an output off, the odd one
//  turns it on, by read or by write. Nothing read them before the Joyport, so
//  nothing stored them; these tests pin down the store and the two things
//  that share the addresses and must not change -- the //e's double hi-res on
//  AN3, and the //c's IOU switches while IOU access is on.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AnnunciatorTests)
{
public:

    TEST_METHOD (EachAddressSetsOrClearsItsOwnAnnunciator_IIPlusBank)
    {
        AppleSoftSwitchBank  bank;

        CheckEveryAddress (bank, &ReadAccess);
        CheckEveryAddress (bank, &WriteAccess);
    }


    TEST_METHOD (EachAddressSetsOrClearsItsOwnAnnunciator_IIeBank)
    {
        Apple2eSoftSwitchBank  bank;

        CheckEveryAddress (bank, &ReadAccess);
        CheckEveryAddress (bank, &WriteAccess);
    }


    TEST_METHOD (TheIIeKeepsDoubleHiResOnAn3)
    {
        Apple2eSoftSwitchBank  bank;

        bank.Read (0xC05E);
        Assert::IsTrue (bank.IsDoubleHiRes(), L"$C05E turns double hi-res on");

        bank.Read (0xC05F);
        Assert::IsFalse (bank.IsDoubleHiRes(), L"$C05F turns it off");

        for (int index = 0; index < AppleSoftSwitchBank::kAnnunciatorCount; index++)
        {
            Assert::IsFalse (bank.IsAnnunciatorOn (index), L"AN3 traffic leaves AN0-AN2 alone");
        }
    }


    TEST_METHOD (TheIIcWithIouAccessOnProgramsTheMouseInstead)
    {
        TestMachine              machine ("Apple2c", TestMachine::Slots::Empty);
        Apple2eSoftSwitchBank  * bank    = machine.GetRefs().iieSoftSwitches;
        AppleMouse             * mouse   = machine.GetMouse();

        Assert::IsNotNull (bank);
        Assert::IsNotNull (mouse);

        mouse->WriteIouAccess (true);
        machine.GetMemoryBus().ReadByte (0xC059);   // ENBXY
        machine.GetMemoryBus().ReadByte (0xC05B);   // ENVBL

        Assert::IsTrue (mouse->AreXyInterruptsEnabled(),  L"$C059 is ENBXY while IOU access is on");
        Assert::IsTrue (mouse->AreVblInterruptsEnabled(), L"$C05B is ENVBL while IOU access is on");
        Assert::IsFalse (bank->IsAnnunciatorOn (0), L"and AN0 does not move");
        Assert::IsFalse (bank->IsAnnunciatorOn (1), L"nor AN1");
    }


    TEST_METHOD (APowerCycleTurnsThemOff)
    {
        AppleSoftSwitchBank  bank;
        Prng                 prng (kSeed);

        bank.Read (0xC059);
        bank.Read (0xC05B);
        bank.Read (0xC05D);
        bank.PowerCycle (prng);

        for (int index = 0; index < AppleSoftSwitchBank::kAnnunciatorCount; index++)
        {
            Assert::IsFalse (bank.IsAnnunciatorOn (index), L"power-on state is all off");
        }
    }


    TEST_METHOD (ACtrlResetLeavesThem)
    {
        Apple2eSoftSwitchBank  bank;

        bank.Read (0xC059);
        bank.Read (0xC05B);
        bank.SoftReset();

        Assert::IsTrue (bank.IsAnnunciatorOn (0), L"a reset changes an annunciator only if the reset code writes it");
        Assert::IsTrue (bank.IsAnnunciatorOn (1));
    }


private:

    static constexpr uint64_t  kSeed = 0xCA550036ULL;

    using AccessFn = void (*) (AppleSoftSwitchBank &, Word);


    static void ReadAccess (AppleSoftSwitchBank & bank, Word address)
    {
        bank.Read (address);
    }


    static void WriteAccess (AppleSoftSwitchBank & bank, Word address)
    {
        bank.Write (address, 0);
    }


    //  Turns each annunciator on and then off through its own pair, and
    //  checks after every access that no other annunciator moved.
    static void CheckEveryAddress (AppleSoftSwitchBank & bank, AccessFn access)
    {
        for (int index = 0; index < AppleSoftSwitchBank::kAnnunciatorCount; index++)
        {
            Word  offAddress = static_cast<Word> (0xC058 + 2 * index);

            access (bank, static_cast<Word> (offAddress + 1));
            ExpectOnly (bank, index, true);

            access (bank, offAddress);
            ExpectOnly (bank, index, false);
        }
    }


    static void ExpectOnly (const AppleSoftSwitchBank & bank, int index, bool isOn)
    {
        for (int other = 0; other < AppleSoftSwitchBank::kAnnunciatorCount; other++)
        {
            bool  expected = (other == index) ? isOn : false;

            Assert::AreEqual (expected, bank.IsAnnunciatorOn (other),
                std::format (L"AN{} after an access to AN{}", other, index).c_str());
        }
    }
};
