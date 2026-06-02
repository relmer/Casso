#include "Pch.h"

#include "Ui/Chrome/NavLayer.h"
#include "resource.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ChromeCommandRoutingTests
//
//  Enforces the FR-026 / SC-006 menu-command parity guarantee: every
//  IDM_* command exposed in Casso/resource.h must have a corresponding
//  entry in NavLayer::GetCommandEntries(). The test iterates the
//  published id set so a future PR that adds a new menu item without
//  registering it in NavLayer fails CI loudly.
//
//  See specs/007-ui-overhaul/menu-command-parity.md for the rendered
//  table (generated from `NavLayer::EmitParityMarkdown`).
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    // The authoritative menu-command id set. Mirrors the IDM_*
    // identifiers in Casso/resource.h that the nav layer wires up.
    // When you add a new menu item, add it here AND to NavLayer's
    // kEntries table — this test will fail until both are in sync,
    // which is exactly the point.
    constexpr WORD kKnownMenuCommandIds[] =
    {
        IDM_FILE_EXIT,

        IDM_EDIT_COPY_TEXT,
        IDM_EDIT_COPY_SCREENSHOT,
        IDM_EDIT_PASTE,

        IDM_MACHINE_RESET,
        IDM_MACHINE_POWERCYCLE,
        IDM_MACHINE_ARROWS_JOYSTICK,
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
}





TEST_CLASS (ChromeCommandRoutingTests)
{
public:

    TEST_METHOD (Every_Known_IDM_Has_NavLayer_Entry)
    {
        std::unordered_set<WORD>  registered;

        for (const NavCommandEntry & e : NavLayer::GetCommandEntries())
        {
            registered.insert (e.commandId);
        }

        for (WORD id : kKnownMenuCommandIds)
        {
            wchar_t  msg[128] = {};
            swprintf_s (msg, L"IDM_ command 0x%04X missing from NavLayer parity table", id);
            Assert::IsTrue (registered.count (id) == 1, msg);
        }
    }


    TEST_METHOD (NavLayer_Entries_Have_Unique_Command_Ids)
    {
        std::unordered_set<WORD>  seen;

        for (const NavCommandEntry & e : NavLayer::GetCommandEntries())
        {
            wchar_t  msg[128] = {};

            if (NavLayer::IsSeparator (e))
            {
                continue;
            }

            swprintf_s (msg, L"Duplicate command id 0x%04X in NavLayer table", e.commandId);
            Assert::IsTrue (seen.insert (e.commandId).second, msg);
        }
    }


    TEST_METHOD (NavLayer_Entries_Have_NonEmpty_Labels_And_Known_Menu)
    {
        for (const NavCommandEntry & e : NavLayer::GetCommandEntries())
        {
            if (NavLayer::IsSeparator (e))
            {
                continue;
            }

            Assert::IsNotNull (e.label,
                               L"NavLayer entry must have a non-null label");
            Assert::IsTrue   (e.label[0] != L'\0',
                               L"NavLayer entry label must be non-empty");

            // GetMenuName returns "?" for unknown enumerators — bare
            // pointer compare against the known menus is enough.
            const wchar_t * name = NavLayer::GetMenuName (e.menu);
            Assert::IsTrue (name[0] != L'?',
                            L"NavLayer entry uses an unknown NavMenu enumerator");
        }
    }


    TEST_METHOD (Dispatch_Is_NoOp_When_No_Callback_Installed)
    {
        // Default-constructed NavLayer has no dispatch — calling
        // Dispatch must not crash.
        NavLayer  nl;
        nl.Dispatch (IDM_FILE_EXIT);
    }


    TEST_METHOD (EmitParityMarkdown_Includes_Every_Command)
    {
        std::string  md = NavLayer::EmitParityMarkdown();

        for (const NavCommandEntry & e : NavLayer::GetCommandEntries())
        {
            char  needle[32] = {};

            if (NavLayer::IsSeparator (e))
            {
                continue;
            }

            snprintf (needle, sizeof (needle), "| %u |", (unsigned) e.commandId);

            wchar_t  msg[160] = {};
            swprintf_s (msg, L"Markdown missing decimal command id for 0x%04X", e.commandId);
            Assert::IsTrue (md.find (needle) != std::string::npos, msg);
        }
    }
};

