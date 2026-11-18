#include "Pch.h"

#include "Apple3TestFixture.h"
#include "Devices/Via6522.h"
#include "Machines/Apple3/Apple3CharacterSet.h"
#include "Machines/Apple3/Apple3Screen.h"
#include "Machines/Apple3/Apple3SystemVias.h"
#include "Machines/Apple3/Apple3Video.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3VideoTests
//
//  The display switches and the mode they select, video enable, the
//  vertical blanking signal on the E VIA, the character download at the
//  start of vertical blanking, and the flash phase.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Apple3VideoTests)
{
public:

    struct Rig : Apple3TestFixture
    {
        std::array<Byte, 0x2000>  ram = {};
        Apple3CharacterSet        characterSet;
        Apple3SystemVias          systemVias;
        Apple3Video               video;
        Apple3Screen              screen;

        Rig()
            : systemVias (mmu),
              video      (characterSet)
        {
            video.AttachVias      (&systemVias);
            video.SetSystemReader ([this] (Word address) { return ram[address]; });
        }

        void RunFrame()
        {
            video.OnBeamLine (0);
            video.OnBeamLine (Apple3Video::kDisplayedLines);
        }
    };


    TEST_METHOD (Switches_SetOnTheOddAddressAndClearOnTheEven)
    {
        auto  rig = std::make_unique<Rig>();



        rig->video.HandleSwitch (0xC051);
        rig->video.HandleSwitch (0xC053);
        rig->video.HandleSwitch (0xC055);
        rig->video.HandleSwitch (0xC057);
        rig->video.HandleSwitch (0xC0D9);

        Assert::IsTrue (rig->video.IsColor() && rig->video.Is80Columns() && rig->video.IsPage2() && rig->video.IsGraphics());
        Assert::IsTrue (rig->video.IsSmoothScroll());

        rig->video.HandleSwitch (0xC050);
        rig->video.HandleSwitch (0xC056);

        Assert::IsFalse (rig->video.IsColor());
        Assert::IsFalse (rig->video.IsGraphics());
    }


    TEST_METHOD (Mode_FollowsThe40And80AndColorSwitches)
    {
        auto  rig = std::make_unique<Rig>();



        Assert::AreEqual ("apple3-text40", rig->video.GetMode()->GetModeName());

        rig->video.HandleSwitch (0xC051);
        Assert::AreEqual ("apple3-text40-color", rig->video.GetMode()->GetModeName());

        rig->video.HandleSwitch (0xC053);
        Assert::AreEqual ("apple3-text80", rig->video.GetMode()->GetModeName());

        rig->video.HandleSwitch (0xC057);
        Assert::AreEqual ("apple3-color140", rig->video.GetMode()->GetModeName());

        rig->video.HandleSwitch (0xC050);
        Assert::AreEqual ("apple3-hires560", rig->video.GetMode()->GetModeName());

        rig->video.HandleSwitch (0xC052);
        Assert::AreEqual ("apple3-hires", rig->video.GetMode()->GetModeName());

        rig->video.HandleSwitch (0xC051);
        Assert::AreEqual ("apple3-hires-color", rig->video.GetMode()->GetModeName());
    }


    TEST_METHOD (Download_SwitchesReachTheCharacterSet)
    {
        auto  rig = std::make_unique<Rig>();



        rig->video.HandleSwitch (Apple3CharacterSet::kDownloadOn);
        Assert::IsTrue (rig->characterSet.IsDownloadEnabled());

        rig->video.HandleSwitch (Apple3CharacterSet::kDownloadOff);
        Assert::IsFalse (rig->characterSet.IsDownloadEnabled());
    }


    TEST_METHOD (VerticalBlank_RunsTheDownloadOncePerFrame)
    {
        auto  rig = std::make_unique<Rig>();



        rig->ram[Apple3CharacterSet::GetCodeAddress (0, 0)] = 0x41;
        rig->ram[Apple3CharacterSet::GetDotsAddress (0, 0)] = 0x5A;
        rig->video.HandleSwitch (Apple3CharacterSet::kDownloadOn);

        rig->video.OnBeamLine (100);
        Assert::AreNotEqual ((Byte) 0x5A, rig->characterSet.GetGlyphRow (0x41, 0));

        rig->video.OnBeamLine (Apple3Video::kDisplayedLines);
        Assert::AreEqual ((Byte) 0x5A, rig->characterSet.GetGlyphRow (0x41, 0));
    }


    TEST_METHOD (VerticalBlank_DrivesTheEViaCb1AndCb2)
    {
        auto  rig = std::make_unique<Rig>();
        Byte  pcr = (Byte) (Via6522::kC2InRisingIndependent << Via6522::kPcrCb2Shift);



        rig->systemVias.GetE().WriteRegister (Via6522::kRegPcr, pcr);
        rig->video.OnBeamLine (0);
        rig->systemVias.GetE().SetCb1 (false);
        rig->systemVias.GetE().SetCb2 (false);
        rig->systemVias.GetE().WriteRegister (Via6522::kRegIfr, 0x7F);

        rig->video.OnBeamLine (Apple3Video::kDisplayedLines);
        Assert::IsTrue (rig->video.IsVerticalBlank());
        Assert::AreEqual ((int) Via6522::kIrqCb2, (int) (rig->systemVias.GetE().GetIfr() & Via6522::kIrqCb2), L"CB2 rises at the start");

        rig->video.OnBeamLine (0);
        Assert::AreEqual ((int) Via6522::kIrqCb1, (int) (rig->systemVias.GetE().GetIfr() & Via6522::kIrqCb1), L"CB1 falls at the end");
    }


    TEST_METHOD (Flash_ChangesPhaseEvery15Frames)
    {
        auto  rig = std::make_unique<Rig>();



        for (uint32_t frame = 1; frame < Apple3Video::kFlashFrames; frame++)
        {
            rig->RunFrame();
        }

        Assert::IsFalse (rig->video.IsFlashOn());

        rig->RunFrame();
        Assert::IsTrue (rig->video.IsFlashOn());
    }


    TEST_METHOD (Render_IsBlackWithTheVideoOff_AndTheTextModeWithItOn)
    {
        auto  rig = std::make_unique<Rig>();



        rig->ram[0x0400] = 0x00;

        rig->video.Render (rig->screen);
        Assert::AreEqual (Apple3Screen::kBlack, rig->screen.Get (0, 0));

        rig->video.SetScreenOn (true);
        rig->video.Render (rig->screen);
        Assert::AreEqual (Apple3Screen::kWhite, rig->screen.Get (0, 0), L"An inverse blank cell is lit");
    }
};
