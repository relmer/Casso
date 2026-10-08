#include "Pch.h"

#include "Shell/EmulatorShell.h"

#include "FakeControllerBackend.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PlayerPickTests
//
//  What a pick in the picker hands the players. The keys and the mouse are
//  Player 1's entries, each turned on through its own setter, which turns the
//  other off. A switch between them goes straight from one to the other: an
//  entry of Automatic on the way lets Automatic give Player 1 a controller for
//  a moment, and the notice stack announces it as "Player 1: ...".
//
//  The shell is driven without Initialize, as ShellKeyWiringTests drives it,
//  with a controller service over a scripted backend so the entries it is
//  handed can be watched. With no window the paddle capture does nothing, so
//  no test here takes the pointer.
//
////////////////////////////////////////////////////////////////////////////////

namespace ControllerTests
{
    TEST_CLASS (PlayerPickTests)
    {
    public:

        //  Installs the service and starts Player 1 on a stand-in, the way
        //  the saved prefs would have left it.
        class TestShell : public EmulatorShell
        {
        public:
            void  InstallControllerService (IControllerBackend & backend)
            {
                m_controllerService = std::make_unique<ControllerInputService> (backend, m_gamePortMixer);
            }

            void  StartOn (PlayerEntryKind kind)
            {
                PlayerEntries  entries;



                entries[0].kind  = kind;
                m_arrowsJoystick = kind == PlayerEntryKind::ArrowKeys;
                m_pointerMode    = (kind == PlayerEntryKind::MousePaddle) ? InputMappingMode::Paddle : InputMappingMode::Off;
                m_controllerService->SetPlayerEntries (entries);
            }

            ControllerInputService &  GetService()             { return *m_controllerService; }
            bool                      IsArrowsJoystick() const { return m_arrowsJoystick; }
            InputMappingMode          GetPointerMode() const   { return m_pointerMode; }
        };


        //  Player 1's entry each time the service reports a change in the
        //  players, and every notice it raised.
        struct Recorder
        {
            std::vector<PlayerEntryKind>  playerOne;
            std::vector<std::wstring>     notices;
        };


        static ControllerDeviceInfo MakeStick()
        {
            ControllerDeviceInfo  info;

            info.unit.model  = { ControllerKind::DirectInput, 0x231d, 0x0121 };
            info.unit.unitId = "{01661270-ADF7-11F1-8005-444553540000}";
            info.unit.source = ControllerUnitSource::InstanceGuid;
            info.description = L"VKBsim Gladiator";
            info.formFactor  = ControllerFormFactor::Joystick;
            info.controls    = { { ControlKind::Axis, 0 }, { ControlKind::Axis, 1 },
                                 { ControlKind::Button, 0 }, { ControlKind::Button, 1 } };
            return info;
        }


        //  A stick attached at startup, which Automatic would hand Player 1 the
        //  moment Player 1's entry read Automatic, then Player 1 on `from`,
        //  then a pick of `to` from Player 1's submenu.
        static void RunSwitch (TestShell & shell, FakeControllerBackend & backend, PlayerEntryKind from, PlayerEntryKind to, Recorder & recorder)
        {
            PlayerEntry  picked;



            backend.AddDevice (MakeStick());
            shell.InstallControllerService (backend);
            shell.StartOn (from);
            shell.GetService().Tick();

            shell.GetService().SetSlotsChangedFn ([&recorder] (const ControllerInputService::SlotsChange & change)
            {
                recorder.playerOne.push_back (change.entries[0].kind);
                recorder.notices.insert (recorder.notices.end(), change.notices.begin(), change.notices.end());
            });

            picked.kind = to;
            shell.PickPlayer (0, picked);
        }


        TEST_METHOD (MouseToKeys_GoesStraightFromOneToTheOther)
        {
            FakeControllerBackend       backend;
            Recorder                    recorder;
            std::unique_ptr<TestShell>  shell    = std::make_unique<TestShell>();



            RunSwitch (*shell, backend, PlayerEntryKind::MousePaddle, PlayerEntryKind::ArrowKeys, recorder);

            for (PlayerEntryKind kind : recorder.playerOne)
            {
                Assert::IsTrue (kind != PlayerEntryKind::Automatic, L"Player 1 never reads Automatic on the way");
            }

            Assert::IsTrue (recorder.notices.empty(), L"so no controller is announced as Player 1");
            Assert::IsTrue (shell->GetService().GetPlayerEntries()[0].kind == PlayerEntryKind::ArrowKeys, L"Player 1 is on the keys");
            Assert::IsTrue (shell->IsArrowsJoystick());
            Assert::IsTrue (shell->GetPointerMode() == InputMappingMode::Off, L"and the mouse has let the paddle go");
        }


        TEST_METHOD (KeysToMouse_GoesStraightFromOneToTheOther)
        {
            FakeControllerBackend       backend;
            Recorder                    recorder;
            std::unique_ptr<TestShell>  shell    = std::make_unique<TestShell>();



            RunSwitch (*shell, backend, PlayerEntryKind::ArrowKeys, PlayerEntryKind::MousePaddle, recorder);

            for (PlayerEntryKind kind : recorder.playerOne)
            {
                Assert::IsTrue (kind != PlayerEntryKind::Automatic, L"Player 1 never reads Automatic on the way");
            }

            Assert::IsTrue  (recorder.notices.empty(), L"so no controller is announced as Player 1");
            Assert::IsTrue  (shell->GetService().GetPlayerEntries()[0].kind == PlayerEntryKind::MousePaddle, L"Player 1 is on the mouse");
            Assert::IsFalse (shell->IsArrowsJoystick(), L"and the keys have let the stick go");
            Assert::IsTrue  (shell->GetPointerMode() == InputMappingMode::Paddle);
        }


        //  A stick attached at startup, Player 1 on `from`, then Player 1's
        //  mode set to `mode`.
        static void RunModeChange (TestShell & shell, FakeControllerBackend & backend, PlayerEntryKind from, PlayerMode mode, Recorder & recorder)
        {
            backend.AddDevice (MakeStick());
            shell.InstallControllerService (backend);
            shell.StartOn (from);
            shell.GetService().Tick();

            shell.GetService().SetSlotsChangedFn ([&recorder] (const ControllerInputService::SlotsChange & change)
            {
                recorder.playerOne.push_back (change.entries[0].kind);
                recorder.notices.insert (recorder.notices.end(), change.notices.begin(), change.notices.end());
            });

            shell.SetPlayerMode (0, mode);
        }


        //  The keys are a joystick, so Paddle mode turns them off, and Player
        //  1 goes to Automatic in its new mode.
        TEST_METHOD (PaddleMode_TurnsTheKeysOff)
        {
            FakeControllerBackend       backend;
            Recorder                    recorder;
            std::unique_ptr<TestShell>  shell    = std::make_unique<TestShell>();



            RunModeChange (*shell, backend, PlayerEntryKind::ArrowKeys, PlayerMode::Paddle, recorder);

            Assert::IsFalse (shell->IsArrowsJoystick(), L"the keys have let the stick go");
            Assert::IsTrue  (shell->GetService().GetPlayerEntries()[0].kind == PlayerEntryKind::Automatic, L"Player 1 is on Automatic");
            Assert::IsTrue  (shell->GetService().GetPlayerEntries()[0].mode == PlayerMode::Paddle,         L"in Paddle mode");

            for (PlayerEntryKind kind : recorder.playerOne)
            {
                Assert::IsTrue (kind != PlayerEntryKind::ArrowKeys, L"and the keys are not picked again on the way");
            }
        }


        //  The mouse is a paddle, so Joystick mode turns it off.
        TEST_METHOD (JoystickMode_TurnsTheMouseOff)
        {
            FakeControllerBackend       backend;
            Recorder                    recorder;
            std::unique_ptr<TestShell>  shell    = std::make_unique<TestShell>();



            RunModeChange (*shell, backend, PlayerEntryKind::MousePaddle, PlayerMode::Joystick, recorder);

            Assert::IsTrue (shell->GetPointerMode() == InputMappingMode::Off, L"the mouse has let the paddle go");
            Assert::IsTrue (shell->GetService().GetPlayerEntries()[0].kind == PlayerEntryKind::Automatic, L"Player 1 is on Automatic");
            Assert::IsTrue (shell->GetService().GetPlayerEntries()[0].mode == PlayerMode::Joystick,       L"in Joystick mode");

            for (PlayerEntryKind kind : recorder.playerOne)
            {
                Assert::IsTrue (kind != PlayerEntryKind::MousePaddle, L"and the mouse is not picked again on the way");
            }
        }
    };
}
