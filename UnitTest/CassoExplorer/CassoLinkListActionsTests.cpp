#include "Pch.h"
#include "ExplorerWindowPartsRig.h"
#include "CassoExplorer/Model/CassoLinkConnectionText.h"
#include "Net/FirewallText.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkListActionsTests
//
//  The buttons and drive links under the list's message for a CassoLink
//  connection, run through the window's parts with no window.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkListActionsTests)
{
public:

    using Rig = ExplorerWindowPartsRig;



    TEST_METHOD (FillListAction_DisconnectedLink_ShowsConnect)
    {
        Rig  rig;



        rig.links->Disconnect ("Test");
        ShowLink (rig, L"Test");

        Assert::AreEqual (std::wstring (L"Connect"), GetShownAction (rig));
        Assert::IsFalse  (rig.parts->linkListActions.IsMessageAboveActions());
    }



    TEST_METHOD (FillListAction_SerialLinkNotReplying_ShowsBootstrap)
    {
        Rig                rig;
        CassoLinkSettings  serial;



        serial.name      = "Serial";
        serial.transport = LinkTransport::Serial;
        rig.prefs.links  = { serial };

        ShowLink (rig, L"Serial");

        Assert::AreEqual (std::wstring (L"Bootstrap the Apple"), GetShownAction (rig));
    }



    TEST_METHOD (FillListAction_LinkThatIsNotSerial_ShowsNoBootstrap)
    {
        Rig  rig;



        ShowLink (rig, L"Test");

        Assert::AreEqual (std::wstring(), GetShownAction (rig));
    }



    TEST_METHOD (FillListAction_NoMessage_ShowsNoButton)
    {
        Rig  rig;



        rig.links->Disconnect ("Test");
        ShowLink (rig, L"Test", false);

        Assert::AreEqual (std::wstring(), GetShownAction (rig));
    }



    TEST_METHOD (FillListAction_FirewallBlocksTheConnection_ShowsAllowOnly)
    {
        Rig  rig;



        ShowLink (rig, L"Test");
        BlockInFirewall (rig, false);
        rig.parts->linkListActions.FillListAction (true);

        Assert::AreEqual (std::wstring (FirewallText::kAllowAction), GetShownAction (rig));
        Assert::IsFalse  (IsShown (rig, FirewallText::kPrivateAction));
        Assert::IsTrue   (rig.parts->linkListActions.IsMessageAboveActions());
    }



    TEST_METHOD (FillListAction_FirewallBlocksOnAPublicNetwork_ShowsMakePrivateBesideAllow)
    {
        Rig  rig;
        RECT allow;
        RECT makePrivate;



        ShowLink (rig, L"Test");
        BlockInFirewall (rig, true);
        rig.parts->linkListActions.FillListAction (true);

        allow       = rig.parts->linkListActions.GetListActionRect();
        makePrivate = rig.parts->linkListActions.GetListActionAltRect();

        Assert::IsTrue (IsShown (rig, FirewallText::kAllowAction));
        Assert::IsTrue (IsShown (rig, FirewallText::kPrivateAction));
        Assert::IsTrue (makePrivate.left > allow.right, L"the private-network button is right of the allow button");
        Assert::AreEqual (allow.top, makePrivate.top);
    }



    TEST_METHOD (FillListAction_FirewallBlocksADisconnectedLink_ShowsConnectInstead)
    {
        Rig  rig;



        rig.links->Disconnect ("Test");
        ShowLink (rig, L"Test");
        BlockInFirewall (rig, true);
        rig.parts->linkListActions.FillListAction (true);

        Assert::AreEqual (std::wstring (L"Connect"), GetShownAction (rig));
        Assert::IsFalse  (IsShown (rig, FirewallText::kPrivateAction));
    }



    TEST_METHOD (FillListAction_LinkNotUsingVirtualDrives_HidesTheDriveLinks)
    {
        Rig                        rig;
        std::vector<std::wstring>  drives;



        ShowLink (rig, L"Test");

        Assert::IsFalse (rig.parts->linkListActions.IsUsingVirtualDrives ("Test", drives));
        Assert::IsTrue  (drives.empty());
        Assert::IsFalse (rig.parts->linkListActions.IsMessageAboveActions());
        Assert::AreEqual ((size_t) 0, GetShownDriveLabels (rig).size());
    }



    TEST_METHOD (GetDriveLinkRect_PutsTheSecondDriveBelowTheFirst)
    {
        Rig   rig;
        RECT  first;
        RECT  second;



        rig.Open();

        first  = rig.parts->linkListActions.GetDriveLinkRect (0);
        second = rig.parts->linkListActions.GetDriveLinkRect (1);

        Assert::AreEqual (first.left,   second.left);
        Assert::AreEqual (first.bottom, second.top);
    }



    TEST_METHOD (GetDriveLabelRect_EndsLeftOfItsDriveLink)
    {
        Rig   rig;
        RECT  link;
        RECT  label;



        rig.Open();

        link  = rig.parts->linkListActions.GetDriveLinkRect (1);
        label = rig.parts->linkListActions.GetDriveLabelRect (1);

        Assert::IsTrue  (label.right < link.left);
        Assert::AreEqual (link.top,    label.top);
        Assert::AreEqual (link.bottom, label.bottom);
    }



    TEST_METHOD (GetListActionRect_UnderTheListMessage_FirewallActionsAreWider)
    {
        Rig  rig;
        RECT plain;
        RECT blocked;



        ShowLink (rig, L"Test");
        plain = rig.parts->linkListActions.GetListActionRect();

        BlockInFirewall (rig, false);
        rig.parts->linkListActions.FillListAction (true);
        blocked = rig.parts->linkListActions.GetListActionRect();

        Assert::AreEqual ((LONG) rig.scaler.ToPx (CassoLinkListActions::kListActionWidthDip), plain.right - plain.left);
        Assert::AreEqual ((LONG) rig.scaler.ToPx (CassoLinkListActions::kListActionWideDip),  blocked.right - blocked.left);
    }



    TEST_METHOD (PollFirewall_WithNoFirewallMonitor_ChangesNothing)
    {
        Rig  rig;



        ShowLink (rig, L"Test");
        rig.parts->linkListActions.PollFirewall();

        Assert::IsTrue (rig.window.messages.empty());
    }



private:

    //  Opens the window and makes the link's root the location, then fills
    //  the list's button as the list does for a location with a message.
    static void  ShowLink (Rig & rig, const std::wstring & name, bool hasMessage = true)
    {
        rig.Open();
        rig.browser.GetNavigator().NavigateToLocation (Location::MakeRoot (TreeModel::MakeLinkId (name)));
        rig.parts->linkListActions.FillListAction (hasMessage);
    }



    //  The firewall blocking the first link, as the tree's entry reports it.
    static void  BlockInFirewall (Rig & rig, bool isPublic)
    {
        std::vector<LinkTreeEntry>  entries = rig.browser.GetTreeModel().GetLinks();



        Assert::IsFalse (entries.empty());

        entries[0].firewall       = L"Windows Firewall blocks the port.";
        entries[0].firewallPublic = isPublic;

        rig.browser.GetTreeModel().SetLinks (entries);
    }



    static bool  IsShown (Rig & rig, const wchar_t * label)
    {
        DxuiPanel  & panel     = rig.window.GetChildPanel();
        bool         isVisible = false;



        for (size_t i = 0; i < panel.GetChildCount(); i++)
        {
            DxuiButton  * button = dynamic_cast<DxuiButton *> (panel.GetChild (i));



            if (button != nullptr && button->IsVisible() && button->GetAccessibleName() == label)
            {
                isVisible = true;
            }
        }

        return isVisible;
    }



    //  The label of the list's button, or empty while it is hidden.
    static std::wstring  GetShownAction (Rig & rig)
    {
        for (const wchar_t * label : { L"Connect", L"Bootstrap the Apple", L"Start server", FirewallText::kAllowAction })
        {
            if (IsShown (rig, label))
            {
                return label;
            }
        }

        return std::wstring();
    }



    //  The visible labels reading "Drive 1:" or "Drive 2:".
    static std::vector<std::wstring>  GetShownDriveLabels (Rig & rig)
    {
        DxuiPanel                & panel   = rig.window.GetChildPanel();
        std::vector<std::wstring>  visible;



        for (size_t i = 0; i < panel.GetChildCount(); i++)
        {
            DxuiLabel  * label = dynamic_cast<DxuiLabel *> (panel.GetChild (i));



            if (label != nullptr && label->IsVisible() && label->GetText().starts_with (L"Drive "))
            {
                visible.push_back (label->GetText());
            }
        }

        return visible;
    }
};
