#include "Pch.h"

#include "Shell/WindowCommandManager.h"
#include "Ui/Chrome/MainMenu.h"
#include "resource.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ChromeCommandRoutingTests
//
//  Enforces the FR-026 / SC-006 menu-command parity guarantee: every
//  IDM_* command exposed in Casso/resource.h must have a corresponding
//  entry in EmulatorCommands::GetMenuEntries(). The test iterates the
//  published id set so a future PR that adds a new menu item without
//  registering it in the command table fails CI loudly.
//
//  See specs/007-ui-overhaul/menu-command-parity.md for the rendered
//  table (generated from `EmulatorCommands::EmitParityMarkdown`).
//
////////////////////////////////////////////////////////////////////////////////






TEST_CLASS (ChromeCommandRoutingTests)
{
public:

    // The authoritative menu-command id set. Mirrors the IDM_*
    // identifiers in Casso/resource.h that the nav layer wires up.
    // When you add a new menu item, add it here AND to the
    // EmulatorCommands menu table — this test will fail until both are
    // in sync, which is exactly the point.
    static constexpr WORD kKnownMenuCommandIds[] =
    {
        IDM_FILE_EXIT,

        IDM_EDIT_COPY_TEXT,
        IDM_EDIT_COPY_SCREENSHOT,
        IDM_EDIT_PASTE,

        IDM_MACHINE_RESET,
        IDM_MACHINE_POWERCYCLE,


        IDM_MACHINE_PAUSE,
        IDM_MACHINE_STEP,

        IDM_DISK_INSERT1,
        IDM_DISK_EJECT1,
        IDM_DISK_INSERT2,
        IDM_DISK_EJECT2,

        IDM_VIEW_FULLSCREEN,
        IDM_VIEW_RESET_SIZE,
        IDM_VIEW_DISK2_DEBUG,

        IDM_HELP_KEYMAP,
        IDM_HELP_ABOUT,
    };

    // Every IDM_* in resource.h, however it is dispatched: WM_COMMAND to
    // WindowCommandManager::OnCommand, the CPU thread's command queue, or a
    // notification posted back to the UI thread. The two dispatchers switch on
    // the same integer separately, so a duplicate value compiles cleanly and
    // misroutes later. Add a new id here when you add it to the header.
    struct IdmRow
    {
        const wchar_t *  name;
        WORD             id;
    };

    static constexpr IdmRow  kAllIdmIds[] =
    {
        { L"IDM_FILE_OPEN",                 IDM_FILE_OPEN                 },
        { L"IDM_FILE_RECENT",               IDM_FILE_RECENT               },
        { L"IDM_FILE_EXIT",                 IDM_FILE_EXIT                 },
        { L"IDM_EDIT_COPY_TEXT",            IDM_EDIT_COPY_TEXT            },
        { L"IDM_EDIT_COPY_SCREENSHOT",      IDM_EDIT_COPY_SCREENSHOT      },
        { L"IDM_EDIT_PASTE",                IDM_EDIT_PASTE                },
        { L"IDM_MACHINE_RESET",             IDM_MACHINE_RESET             },
        { L"IDM_MACHINE_POWERCYCLE",        IDM_MACHINE_POWERCYCLE        },
        { L"IDM_MACHINE_PAUSE",             IDM_MACHINE_PAUSE             },
        { L"IDM_MACHINE_STEP",              IDM_MACHINE_STEP              },
        { L"IDM_MACHINE_SPEED_1X",          IDM_MACHINE_SPEED_1X          },
        { L"IDM_MACHINE_SPEED_2X",          IDM_MACHINE_SPEED_2X          },
        { L"IDM_MACHINE_SPEED_MAX",         IDM_MACHINE_SPEED_MAX         },
        { L"IDM_MACHINE_INFO",              IDM_MACHINE_INFO              },
        { L"IDM_MACHINE_ARROWS_JOYSTICK",   IDM_MACHINE_ARROWS_JOYSTICK   },
        { L"IDM_MACHINE_ARROWS_PADDLE",     IDM_MACHINE_ARROWS_PADDLE     },
        { L"IDM_DISK_INSERT1",              IDM_DISK_INSERT1              },
        { L"IDM_DISK_INSERT2",              IDM_DISK_INSERT2              },
        { L"IDM_DISK_EJECT1",               IDM_DISK_EJECT1               },
        { L"IDM_DISK_EJECT2",               IDM_DISK_EJECT2               },
        { L"IDM_DISK_WRITEMODE_BUFFER",     IDM_DISK_WRITEMODE_BUFFER     },
        { L"IDM_DISK_WRITEMODE_COW",        IDM_DISK_WRITEMODE_COW        },
        { L"IDM_DISK_WRITEPROTECT1",        IDM_DISK_WRITEPROTECT1        },
        { L"IDM_DISK_WRITEPROTECT2",        IDM_DISK_WRITEPROTECT2        },
        { L"IDM_DISK_WP1",                  IDM_DISK_WP1                  },
        { L"IDM_DISK_WP2",                  IDM_DISK_WP2                  },
        { L"IDM_DISK_SALVAGE1",             IDM_DISK_SALVAGE1             },
        { L"IDM_DISK_SALVAGE2",             IDM_DISK_SALVAGE2             },
        { L"IDM_DISK_RESOLVE_CHANGE",       IDM_DISK_RESOLVE_CHANGE       },
        { L"IDM_VIEW_COLOR",                IDM_VIEW_COLOR                },
        { L"IDM_VIEW_GREEN",                IDM_VIEW_GREEN                },
        { L"IDM_VIEW_AMBER",                IDM_VIEW_AMBER                },
        { L"IDM_VIEW_WHITE",                IDM_VIEW_WHITE                },
        { L"IDM_VIEW_FULLSCREEN",           IDM_VIEW_FULLSCREEN           },
        { L"IDM_VIEW_CRT_SHADER",           IDM_VIEW_CRT_SHADER           },
        { L"IDM_VIEW_RESET_SIZE",           IDM_VIEW_RESET_SIZE           },
        { L"IDM_VIEW_INPUT_DEBUG",          IDM_VIEW_INPUT_DEBUG          },
        { L"IDM_VIEW_DISK2_DEBUG",          IDM_VIEW_DISK2_DEBUG          },
        { L"IDM_VIEW_SETTINGS",             IDM_VIEW_SETTINGS             },
        { L"IDM_AUDIO_DRIVE_ENABLE",        IDM_AUDIO_DRIVE_ENABLE        },
        { L"IDM_AUDIO_DRIVE_DISABLE",       IDM_AUDIO_DRIVE_DISABLE       },
        { L"IDM_AUDIO_DRIVE_MECHANISM",     IDM_AUDIO_DRIVE_MECHANISM     },
        { L"IDM_AUDIO_DRIVE_VOLUMES",       IDM_AUDIO_DRIVE_VOLUMES       },
        { L"IDM_AUDIO_DRIVE_PAN",           IDM_AUDIO_DRIVE_PAN           },
        { L"IDM_AUDIO_DRIVE_TEST",          IDM_AUDIO_DRIVE_TEST          },
        { L"IDM_DRIVE_EXTERNAL_CONNECT",    IDM_DRIVE_EXTERNAL_CONNECT    },
        { L"IDM_DRIVE_EXTERNAL_DISCONNECT", IDM_DRIVE_EXTERNAL_DISCONNECT },
        { L"IDM_MOUSE_CONNECT",             IDM_MOUSE_CONNECT             },
        { L"IDM_MOUSE_DISCONNECT",          IDM_MOUSE_DISCONNECT          },
        { L"IDM_PRINTER_DISCARD",           IDM_PRINTER_DISCARD           },
        { L"IDM_PRINTER_COPY",              IDM_PRINTER_COPY              },
        { L"IDM_PRINTER_PREVIEW",           IDM_PRINTER_PREVIEW           },
        { L"IDM_PRINTER_PRINT",             IDM_PRINTER_PRINT             },
        { L"IDM_PRINTER_SAVEAS",            IDM_PRINTER_SAVEAS            },
        { L"IDM_PRINTER_MODERN_SENT",       IDM_PRINTER_MODERN_SENT       },
        { L"IDM_PRINTER_MODERN_FAILED",     IDM_PRINTER_MODERN_FAILED     },
        { L"IDM_VIEW_DRIVE_STRIP",          IDM_VIEW_DRIVE_STRIP          },
        { L"IDM_VIEW_FRAME_RATE",           IDM_VIEW_FRAME_RATE           },
        { L"IDM_VIEW_SCENE_VIEW",           IDM_VIEW_SCENE_VIEW           },
        { L"IDM_HELP_KEYMAP",               IDM_HELP_KEYMAP               },
        { L"IDM_HELP_ABOUT",                IDM_HELP_ABOUT                },
    };

    TEST_METHOD (Every_IDM_Value_Is_Unique)
    {
        std::unordered_map<WORD, const wchar_t *>  seen;



        for (const IdmRow & row : kAllIdmIds)
        {
            wchar_t  msg[160]    = {};
            auto     [it, isNew] = seen.emplace (row.id, row.name);

            swprintf_s (msg, L"%s and %s share the value %u", it->second, row.name, (unsigned) row.id);
            Assert::IsTrue (isNew, msg);
        }
    }


    TEST_METHOD (Every_Command_Table_Id_Is_In_The_IDM_List)
    {
        // Fails when a menu row's id is missing from kAllIdmIds, so the
        // uniqueness check covers every id in the command table.
        std::unordered_set<WORD>  listed;
        size_t                    checked = 0;



        for (const IdmRow & row : kAllIdmIds)
        {
            listed.insert (row.id);
        }

        for (const EmulatorMenuEntry & e : EmulatorCommands::GetMenuEntries())
        {
            wchar_t  msg[128] = {};

            if (EmulatorCommands::IsSeparator (e))
            {
                continue;
            }

            swprintf_s (msg, L"Command table id %u is missing from kAllIdmIds", (unsigned) e.commandId);
            Assert::IsTrue (listed.count (e.commandId) == 1, msg);
            checked++;
        }

        Assert::IsTrue (checked > 0, L"The command table has no entries to check");
    }


    TEST_METHOD (Every_Command_Table_Id_Routes_To_A_Handler)
    {
        // Menu and toolbar picks go through WindowCommandManager::OnCommand,
        // which drops an id that falls outside every range it checks.
        size_t  checked = 0;



        for (const EmulatorMenuEntry & e : EmulatorCommands::GetMenuEntries())
        {
            wchar_t             msg[128] = {};
            WindowCommandRoute  route    = WindowCommandRoute::None;

            if (EmulatorCommands::IsSeparator (e))
            {
                continue;
            }

            route = WindowCommandManager::GetCommandRoute (e.commandId);

            swprintf_s (msg, L"Command table id %u reaches no OnCommand handler", (unsigned) e.commandId);
            Assert::IsTrue (route != WindowCommandRoute::None, msg);
            checked++;
        }

        Assert::IsTrue (checked > 0, L"The command table has no entries to check");
    }


    TEST_METHOD (Salvage_Menu_Picks_Route_To_The_Disk_Handler)
    {
        Assert::IsTrue (WindowCommandManager::GetCommandRoute (IDM_DISK_SALVAGE1) == WindowCommandRoute::Disk);
        Assert::IsTrue (WindowCommandManager::GetCommandRoute (IDM_DISK_SALVAGE2) == WindowCommandRoute::Disk);
    }


    TEST_METHOD (Every_Known_IDM_Has_MainMenu_Entry)
    {
        std::unordered_set<WORD>  registered;

        for (const EmulatorMenuEntry & e : EmulatorCommands::GetMenuEntries())
        {
            registered.insert (e.commandId);
        }

        for (WORD id : kKnownMenuCommandIds)
        {
            wchar_t  msg[128] = {};
            swprintf_s (msg, L"IDM_ command 0x%04X missing from the command parity table", id);
            Assert::IsTrue (registered.count (id) == 1, msg);
        }
    }


    TEST_METHOD (MainMenu_Entries_Have_Unique_Command_Ids)
    {
        std::unordered_set<WORD>  seen;

        for (const EmulatorMenuEntry & e : EmulatorCommands::GetMenuEntries())
        {
            wchar_t  msg[128] = {};

            if (EmulatorCommands::IsSeparator (e))
            {
                continue;
            }

            swprintf_s (msg, L"Duplicate command id 0x%04X in the command table", e.commandId);
            Assert::IsTrue (seen.insert (e.commandId).second, msg);
        }
    }


    TEST_METHOD (MainMenu_Entries_Have_NonEmpty_Labels_And_Known_Menu)
    {
        for (const EmulatorMenuEntry & e : EmulatorCommands::GetMenuEntries())
        {
            const wchar_t * name = nullptr;

            if (EmulatorCommands::IsSeparator (e))
            {
                continue;
            }

            Assert::IsNotNull (e.label,
                               L"Menu entry must have a non-null label");
            Assert::IsTrue   (e.label[0] != L'\0',
                               L"Menu entry label must be non-empty");

            // GetMenuName returns "?" for unknown enumerators — bare
            // pointer compare against the known menus is enough.
            name = EmulatorCommands::GetMenuName (e.menu);
            Assert::IsTrue (name[0] != L'?',
                            L"Menu entry uses an unknown MainMenuId enumerator");
        }
    }


    TEST_METHOD (Dispatch_Is_NoOp_When_No_Callback_Installed)
    {
        // Default-constructed MainMenu has no dispatch — calling
        // Dispatch must not crash.
        MainMenu  nl;
        nl.Dispatch (IDM_FILE_EXIT);
    }


    TEST_METHOD (EmitParityMarkdown_Includes_Every_Command)
    {
        std::string  md = EmulatorCommands::EmitParityMarkdown();

        for (const EmulatorMenuEntry & e : EmulatorCommands::GetMenuEntries())
        {
            char     needle[32] = {};
            wchar_t  msg[160]   = {};

            if (EmulatorCommands::IsSeparator (e))
            {
                continue;
            }

            snprintf (needle, sizeof (needle), "| %u |", (unsigned) e.commandId);

            swprintf_s (msg, L"Markdown missing decimal command id for 0x%04X", e.commandId);
            Assert::IsTrue (md.find (needle) != std::string::npos, msg);
        }
    }


    TEST_METHOD (Toolbar_Entries_Share_The_Menu_Commands)
    {
        // A command on both surfaces is one declaration: the toolbar's
        // Settings entry is the menu's Settings row, with the short label
        // the strip draws and the mnemonic label the menu draws.
        EmulatorCommands     cmds;
        const DxuiCommand *  settings = cmds.Find (IDM_VIEW_SETTINGS);

        Assert::IsNotNull (settings);
        Assert::AreEqual  (L"Settings",       settings->GetShortText().c_str());
        Assert::AreEqual  (L"Se&ttings...",   settings->GetLabelText().c_str());
        Assert::IsNotNull (settings->glyph);
        Assert::IsNotNull (cmds.Find (EmulatorCommands::kIdTheme));
        Assert::IsNotNull (cmds.Find (EmulatorCommands::kIdInput));
    }
};
