#include "Pch.h"

#include "Ui/DiskInspector/Components/InspectorMenus.h"
#include "Ui/DiskInspector/Components/InspectorSession.h"
#include "Ui/DiskInspector/Components/PlatterCorners.h"
#include "Ui/DiskInspector/Components/TrackColumn.h"
#include "Ui/DiskInspector/PlatterView.h"
#include "FakeInspectorHost.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorComponentTests
//
//  The disk inspector's components, each created on a fake window (FR-162):
//  the view selector's views and order (FR-160), the overlay chips
//  (FR-031, FR-163) and the Go menu's rows (FR-158, FR-164).
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (InspectorComponentTests)
{
public:

    static std::wstring DescribeViews (const vector<TrackColumn::View> & views)
    {
        std::wstring  text;



        for (const TrackColumn::View & view : views)
        {
            text += std::wstring (view.label) + L";";
        }

        return text;
    }



    static InspectorCommands::Command MakeCommand (LPCWSTR label)
    {
        InspectorCommands::Command  command = std::make_shared<DxuiCommand>();



        command->label = label;
        return command;
    }



    TEST_METHOD (TheViewSelectorListsThePresentViewsInOrder)
    {
        InspectorSession   session;
        FakeInspectorHost  host;
        TrackColumn        column (session, host);



        column.Create();

        Assert::AreEqual (std::wstring (L"Files;Sectors;Tracks;Image;Nibbles;"), DescribeViews (column.GetShownViews()),
                          L"no issues, no flux tracks, no comparison and no hits");
        Assert::AreEqual (std::wstring (L"Issues;Files;Sectors;Tracks;Image;Nibbles;Flux;Differences;Find results;"), DescribeViews (TrackColumn::GetAllViews()));
    }



    TEST_METHOD (AViewThatIsNotPresentGivesWayToSectors)
    {
        InspectorSession   session;
        FakeInspectorHost  host;
        TrackColumn        column (session, host);



        column.Create();
        session.context.hasDisk = true;
        session.isDiskGroup     = true;
        session.diskTab         = kTabFindings;

        column.Update();

        Assert::AreEqual (static_cast<size_t> (1), host.trackTabs.size(), L"Issues is not present without issues");
        Assert::AreEqual (static_cast<int> (kTabSectorData), host.trackTabs.back());
    }



    TEST_METHOD (TheScansChipTurnsTheOverlayOnAndIsSaved)
    {
        InspectorSession    session;
        FakeInspectorHost   host;
        PlatterView       * platter = host.panel.CreateChild<PlatterView> (session.context);
        PlatterCorners      corners (session, host, *platter);
        DiskInspectorPrefs  prefs;



        corners.Create();
        corners.SetOverlay (PlatterOverlay::Scans, true);

        Assert::IsTrue  (session.context.isScansOverlay);
        Assert::IsTrue  (corners.IsOverlayOn (PlatterOverlay::Scans));
        Assert::IsFalse (corners.IsOverlayOn (PlatterOverlay::Reads), L"each overlay is its own");

        corners.GetPrefs (prefs);
        Assert::IsTrue (prefs.isScans);

        prefs.isScans = false;
        corners.ApplyPrefs (prefs);
        Assert::IsFalse (session.context.isScansOverlay);
    }



    TEST_METHOD (BackAndForwardHeadTheGoMenu)
    {
        InspectorSession   session;
        FakeInspectorHost  host;
        InspectorCommands  commands;
        int                back    = -1;
        int                forward = -1;
        int                goTo    = -1;



        commands.back    = MakeCommand (L"&Back");
        commands.forward = MakeCommand (L"For&ward");
        commands.goTo    = MakeCommand (L"&Go to...");

        InspectorMenus  menus (session, host, commands);

        menus.Create();

        for (const DxuiMenuBarItem & menu : menus.GetMenuBar().GetItems())
        {
            for (int row = 0; row < static_cast<int> (menu.submenu.size()); row++)
            {
                const DxuiPopupMenuItem &  item = menu.submenu[static_cast<size_t> (row)];

                back    = (item.command == commands.back)    ? row : back;
                forward = (item.command == commands.forward) ? row : forward;
                goTo    = (item.command == commands.goTo)    ? row : goTo;
            }
        }

        Assert::AreEqual (0, back);
        Assert::AreEqual (1, forward);
        Assert::AreEqual (3, goTo, L"after a separator");
    }
};
