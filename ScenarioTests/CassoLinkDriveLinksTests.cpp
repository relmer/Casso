#include "Pch.h"

#include "CassoExplorer/ExplorerWindowPartsRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDriveLinksScenarioTests
//
//  The virtual drives' image links under the list's message: a serial link
//  whose fake Apple booted the CassoLink virtual drives disk sends a virtual
//  drive request, the session replies on its thread, and the list then has
//  a label and an image link for each drive.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkDriveLinksScenarioTests)
{
public:

    using Rig  = ExplorerWindowPartsRig;
    using Port = FakeAppleFactory::Port;

    static constexpr auto  kServeWait = std::chrono::seconds (10);



    TEST_METHOD (FillListAction_LinkUsingItsVirtualDrives_ShowsEachDrivesImage)
    {
        Rig                 rig;
        CassoLinkSettings   serial;
        CassoLinkSession  * session = nullptr;
        bool                served  = false;
        auto                until   = std::chrono::steady_clock::now() + kServeWait;



        serial.name                 = "Serial";
        serial.transport            = LinkTransport::Serial;
        serial.port                 = "COM2";
        serial.virtualDrives.drive1 = L"C:\\Link\\One.dsk";
        serial.virtualDrives.drive2 = L"C:\\Link\\Two.dsk";

        rig.factory.comPorts = { { L"COM2", Port::Booted } };
        rig.prefs.links      = { serial };
        rig.links->SetLinks (rig.prefs.links);

        AssertSucceeded (rig.links->Start ("Serial"));

        while (!served && std::chrono::steady_clock::now() < until)
        {
            session = rig.links->GetSession ("Serial");
            served  = session != nullptr && session->IsServingVirtualDrives();

            std::this_thread::sleep_for (std::chrono::milliseconds (20));
        }

        Assert::IsTrue (served, L"The session replied to the virtual drive request");

        rig.Open();
        rig.browser.GetNavigator().NavigateToLocation (Location::MakeRoot (TreeModel::MakeLinkId (L"Serial")));
        rig.parts->linkListActions.FillListAction (true);

        Assert::IsTrue (GetShownTexts<DxuiLabel>  (rig) == std::vector<std::wstring> { L"Drive 1:", L"Drive 2:" });
        Assert::IsTrue (GetShownTexts<DxuiButton> (rig) == std::vector<std::wstring> { L"One.dsk", L"Two.dsk" });
        Assert::IsTrue (rig.parts->linkListActions.IsMessageAboveActions());
    }



private:

    //  The visible labels reading "Drive N:", or the visible buttons' accessible text
    //  ending in .dsk: the drives' image links.
    template <typename Control>
    static std::vector<std::wstring>  GetShownTexts (Rig & rig)
    {
        DxuiPanel                & panel = rig.window.GetChildPanel();
        std::vector<std::wstring>  shown;



        for (size_t i = 0; i < panel.GetChildCount(); i++)
        {
            Control       * control = dynamic_cast<Control *> (panel.GetChild (i));
            std::wstring    text;



            if (control == nullptr || !control->IsVisible())
            {
                continue;
            }

            if constexpr (std::is_same_v<Control, DxuiLabel>)
            {
                text = control->GetText();
            }
            else
            {
                text = control->GetAccessibleName();
            }

            if (text.starts_with (L"Drive ") || text.ends_with (L".dsk"))
            {
                shown.push_back (text);
            }
        }

        return shown;
    }
};
