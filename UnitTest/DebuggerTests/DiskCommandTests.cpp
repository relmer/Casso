#include "Pch.h"

#include "Devices/Disk/DiskImageStore.h"
#include "EntryPointRig.h"

#include "CppUnitTest.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DiskCommandTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DiskConsoleRig
    //
    //  The emulator's debugger over a //e with a Disk II card in slot 6, as the
    //  emulator runs one: the window's console and the channel over the same
    //  session. Disks are mounted into the machine's disk store, which is where
    //  the emulator's disk manager puts them.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class DiskConsoleRig
    {
    public:
        TestMachine            machine;
        CpuManager             cpuManager;
        InMemoryPipeTransport  transport;
        InMemoryFileSystem     files;
        DebuggerController     controller;
        DebuggerViewState      view;


        DiskConsoleRig() :
            machine    (std::string ("Apple2e"), TestMachine::Slots::DiskOnly),
            controller (machine, Paused (cpuManager), transport, files, nullptr, 1)
        {
            HRESULT  hr = controller.Open();



            Assert::AreEqual (S_OK, hr);
        }


        static CpuManager & Paused (CpuManager & cpu)
        {
            cpu.SetPaused (true);
            return cpu;
        }


        void Mount (int drive, const std::string & path)
        {
            static constexpr size_t  kDskBytes = 143360;
            std::vector<Byte>        blank (kDskBytes, 0);
            HRESULT                  hr     = machine.GetDiskStore().MountFromBytes (kSlot, drive, path, DiskFormat::Dsk, blank);



            Assert::AreEqual (S_OK, hr, L"the disk mounted");
        }


        void SwitchTo (CommandMode mode)
        {
            std::string  line = std::string (CommandModeHelp::GetMarker (controller.GetSession().GetMode())) + "MODE " + CommandModeNames::GetUpperName (mode);



            (void) Type (line);
            Assert::IsTrue (controller.GetSession().GetMode() == mode);
        }


        Reply Type (const std::string & line)
        {
            return view.ExecuteWindowLine (controller.GetSession(), line, controller.GetSession().GetMode());
        }


        Reply Request (const std::string & line, std::optional<CommandMode> mode = std::nullopt)
        {
            return controller.RunLine (line, mode, std::nullopt);
        }


        static constexpr int  kSlot = 6;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DiskCommandTests
    //
    //  DISK lists the slot 6 drives and what is in them; DISK SLOT gives the
    //  slot. The words that would change a drive are reported as not available
    //  here, and leave it as it was.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DiskCommandTests)
    {
    public:

        TEST_METHOD (InTheWindow_ListsWhatTheEmulatorMounted)
        {
            DiskConsoleRig  rig;
            size_t          checked = 0;



            rig.Mount (0, "C:\\disks\\game.dsk");

            for (const char * line : { "DISK", "disk", "DISK INFO", "disk info", "DISK INFO more words" })
            {
                ReplyCheck::IsOk (rig.Type (line), "Slot 6, drive 1: C:\\disks\\game.dsk\nSlot 6, drive 2: empty", line);
                ++checked;
            }

            rig.Mount (1, "D:\\two.dsk");
            ReplyCheck::IsOk (rig.Type ("DISK"), "Slot 6, drive 1: C:\\disks\\game.dsk\nSlot 6, drive 2: D:\\two.dsk", "both drives");
            Assert::AreEqual ((size_t) 5, checked);
        }


        TEST_METHOD (OnTheChannel_TheSame)
        {
            DiskConsoleRig  rig;



            rig.Mount (1, "C:\\b.dsk");
            ReplyCheck::IsOk (rig.Request ("DISK"),                          "Slot 6, drive 1: empty\nSlot 6, drive 2: C:\\b.dsk", "DISK");
            ReplyCheck::IsOk (rig.Request ("/DISK", CommandMode::Monitor),   "Slot 6, drive 1: empty\nSlot 6, drive 2: C:\\b.dsk", "/DISK");
            ReplyCheck::IsOk (rig.Request ("disk",  CommandMode::GSSquared), "Slot 6, drive 1: empty\nSlot 6, drive 2: C:\\b.dsk", "disk");
            ReplyCheck::IsOk (rig.Request ("!disk", CommandMode::WinDbg),    "Slot 6, drive 1: empty\nSlot 6, drive 2: C:\\b.dsk", "!disk");
        }


        //  --disk1 and --disk2 go through the file system into the store DISK
        //  reads.
        TEST_METHOD (InBatch_ListsTheDisksGiven)
        {
            BatchScriptRig  rig;



            rig.files.WriteAllText (L"C:\\Work\\one.dsk", std::string (143360, '\0'));
            rig.files.WriteAllText (L"C:\\Work\\two.dsk", std::string (143360, '\0'));
            rig.options.disk1    = "C:\\Work\\one.dsk";
            rig.options.disk2    = "C:\\Work\\two.dsk";
            rig.options.commands = { "DISK", "DISK SLOT" };

            Assert::AreEqual (0, rig.Run (""));
            Assert::AreEqual (std::string (">DISK\nSlot 6, drive 1: C:\\Work\\one.dsk\nSlot 6, drive 2: C:\\Work\\two.dsk\n>DISK SLOT\nDisk slot: 6\n"),
                              rig.result.output);
        }


        TEST_METHOD (Slot_InEveryDialect)
        {
            const std::pair<CommandMode, const char *>  forms[] =
            {
                { CommandMode::AppleWin,  "DISK SLOT"    },
                { CommandMode::AppleWin,  "disk slot"    },
                { CommandMode::AppleWin,  "DISK SLOT 5"  },
                { CommandMode::Casso,     "DISK SLOT"    },
                { CommandMode::Monitor,   "/DISK SLOT"   },
                { CommandMode::GSSquared, "disk slot"    },
                { CommandMode::WinDbg,    "!disk slot"   },
            };
            size_t  checked = 0;



            for (const auto & [mode, line] : forms)
            {
                DiskConsoleRig  rig;



                rig.SwitchTo (mode);
                ReplyCheck::IsOk (rig.Type (line), "Disk slot: 6", line);
                ++checked;
            }

            Assert::AreEqual (std::size (forms), checked);
        }


        TEST_METHOD (Info_InEveryDialect)
        {
            const std::pair<CommandMode, const char *>  forms[] =
            {
                { CommandMode::Casso,     "DISK"       },
                { CommandMode::Monitor,   "/DISK"      },
                { CommandMode::Monitor,   "/disk info" },
                { CommandMode::GSSquared, "disk"       },
                { CommandMode::GSSquared, "DISK INFO"  },
                { CommandMode::WinDbg,    "!disk"      },
                { CommandMode::WinDbg,    "!DISK INFO" },
            };
            size_t  checked = 0;



            for (const auto & [mode, line] : forms)
            {
                DiskConsoleRig  rig;



                rig.Mount (0, "C:\\a.dsk");
                rig.SwitchTo (mode);
                ReplyCheck::IsOk (rig.Type (line), "Slot 6, drive 1: C:\\a.dsk\nSlot 6, drive 2: empty", line);
                ++checked;
            }

            Assert::AreEqual (std::size (forms), checked);
        }


        //  Not available, in any case, with the verb upper-cased in the reply;
        //  the drive keeps its disk. Batch counts not available as success.
        TEST_METHOD (ChangingADrive_IsNotAvailable_AndChangesNothing)
        {
            const std::pair<const char *, const char *>  forms[] =
            {
                { "DISK EJECT",        "DISK EJECT is not available in this session."   },
                { "DISK EJECT 1",      "DISK EJECT is not available in this session."   },
                { "disk eject",        "DISK EJECT is not available in this session."   },
                { "DISK INSERT C:\\f", "DISK INSERT is not available in this session."  },
                { "disk insert 1 f",   "DISK INSERT is not available in this session."  },
                { "DISK PROTECT 1",    "DISK PROTECT is not available in this session." },
                { "Disk Protect",      "DISK PROTECT is not available in this session." },
            };
            DiskConsoleRig  rig;
            size_t          checked = 0;



            rig.Mount (0, "C:\\disks\\game.dsk");

            for (const auto & [line, detail] : forms)
            {
                BatchScriptRig  batch;



                ReplyCheck::IsError (rig.Type (line), CommandStatus::NotAvailable, "command not available", detail, line);
                ReplyCheck::IsOk    (rig.Type ("DISK"), "Slot 6, drive 1: C:\\disks\\game.dsk\nSlot 6, drive 2: empty", std::string ("after ") + line);
                Assert::IsTrue      (rig.machine.GetDiskStore().IsMounted (DiskConsoleRig::kSlot, 0), ReplyCheck::Widen (line).c_str());

                batch.options.commands = { line };
                Assert::AreEqual (0, batch.Run (""), ReplyCheck::Widen (line).c_str());
                ++checked;
            }

            Assert::AreEqual (std::size (forms), checked);
        }


        TEST_METHOD (ChangingADrive_InMonitorMode_CarriesErr)
        {
            DiskConsoleRig  rig;



            rig.SwitchTo (CommandMode::Monitor);
            ReplyCheck::IsMonitorError (rig.Type ("/DISK EJECT"), CommandStatus::NotAvailable, "command not available",
                                        "DISK EJECT is not available in this session.", "/DISK EJECT");
        }


        TEST_METHOD (AnyOtherWord_IsAnError)
        {
            DiskConsoleRig  rig;
            BatchScriptRig  batch;



            for (const char * line : { "DISK FROB", "DISK 1", "disk list" })
            {
                ReplyCheck::IsError (rig.Type (line), CommandStatus::Error, "invalid arguments", "DISK takes INFO, SLOT, EJECT, INSERT or PROTECT.", line);
            }

            batch.options.commands = { "DISK FROB" };
            Assert::AreEqual (1, batch.Run (""));
        }
    };
}
