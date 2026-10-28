#include "Pch.h"

#include "Ui/Settings/SerialAttachmentView.h"
#include "../SerialTests/FakeHostSerialPorts.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace SerialAttachmentViewTestNs
{
    static constexpr int  kComPortChoice = 4;     // on a card: None, Loopback, Output file, Input file, COM port
    static constexpr int  kTcpChoice     = 5;
    static constexpr int  kModemChoice   = 6;
    static constexpr int  kListenChoice  = 1;




    ////////////////////////////////////////////////////////////////////////////
    //
    //  ViewRig
    //
    //  A view on a panel, laid out, and a list of every change it reports.
    //
    ////////////////////////////////////////////////////////////////////////////

    class ViewRig
    {
    public:
        explicit ViewRig (SerialAttachmentView::Kind kind = SerialAttachmentView::Kind::Card)
            : view (kind, L"Serial connector:")
        {
            view.AdoptInto (panel);
            view.SetSerialPorts (&ports);
            view.SetOnChange ([this] (const SerialAttachmentChoice & choice) { reported.push_back (choice); });
            Lay();
        }

        void  Lay()
        {
            constexpr int  kLabelWidth    = 200;
            constexpr int  kControlsWidth = 260;
            DxuiDpiScaler  scaler;



            view.Layout (0, 0, kLabelWidth, kControlsWidth, { 28, 8, 14, 36 }, scaler);
        }

        static void  Pick (DxuiComboBox & combo, int index)
        {
            combo.Open();

            while (combo.GetHighlightIndex() < index)
            {
                combo.HandleKey (VK_DOWN);
            }

            while (combo.GetHighlightIndex() > index)
            {
                combo.HandleKey (VK_UP);
            }

            combo.HandleKey (VK_RETURN);
        }

        static void  Type (DxuiTextInput & input, const std::wstring & text)
        {
            input.SetFocused (true);
            input.SetText    (L"");

            for (wchar_t ch : text)
            {
                input.OnChar (ch);
            }
        }

        DxuiPanel                            panel;
        FakeHostSerialPorts                  ports;
        SerialAttachmentView                 view;
        std::vector<SerialAttachmentChoice>  reported;
    };




    ////////////////////////////////////////////////////////////////////////////
    //
    //  SerialAttachmentViewTests
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SerialAttachmentViewTests)
    {
    public:
        TEST_METHOD (ACardListsNoneFirstThenEveryAttachment)
        {
            ViewRig  rig;



            Assert::IsTrue (std::vector<std::string> { "", kpszSerialLoopback, kpszSerialFileOut, kpszSerialFileIn, kpszSerialComPort, kpszSerialTcp, kpszSerialModem }
                            == rig.view.GetDevices());
        }


        //  A //c port's list starts with None, then the loopback plug, as a card's does.
        TEST_METHOD (ABuiltInPortListsNoneThenTheLoopbackPlug)
        {
            ViewRig  rig (SerialAttachmentView::Kind::BuiltInPort);



            Assert::IsTrue (std::vector<std::string> { "", kpszSerialLoopback, kpszSerialFileOut, kpszSerialFileIn, kpszSerialComPort, kpszSerialTcp, kpszSerialModem }
                            == rig.view.GetDevices(), L"the card's list");

            rig.view.SetChoice ({ "", "", {} });
            Assert::AreEqual (0, rig.view.GetAttachDropdown().GetSelectedIndex(), L"an empty device is None");

            rig.view.SetChoice ({ kpszSerialLoopback, "", {} });
            rig.Lay();
            Assert::AreEqual (1, rig.view.GetAttachDropdown().GetSelectedIndex(), L"the loopback plug");
            Assert::IsFalse (rig.view.GetNoteText().empty(), L"with its note");
        }


        TEST_METHOD (PickingTcpReportsItAndShowsItsOptions)
        {
            ViewRig  rig;



            ViewRig::Pick (rig.view.GetAttachDropdown(), kTcpChoice);
            rig.Lay();

            Assert::AreEqual<size_t> (1, rig.reported.size());
            Assert::AreEqual (std::string (kpszSerialTcp), rig.reported.back().device);
            Assert::IsTrue  (rig.view.GetHostInput().IsVisible());
            Assert::IsTrue  (rig.view.GetPortInput().IsVisible());
            Assert::IsTrue  (rig.view.GetTelnetCheckbox().IsVisible());
            Assert::IsFalse (rig.view.GetComPortDropdown().IsVisible());
            Assert::IsFalse (rig.view.GetListenPortInput().IsVisible());
            Assert::IsFalse (rig.view.GetTelnetCheckbox().IsChecked(), L"TCP passes bytes as they are by default");
        }


        TEST_METHOD (ListeningHidesTheHost)
        {
            ViewRig  rig;



            ViewRig::Pick (rig.view.GetAttachDropdown(), kTcpChoice);
            ViewRig::Pick (rig.view.GetConnectDropdown(), kListenChoice);
            rig.Lay();

            Assert::IsTrue  (rig.reported.back().options.listen);
            Assert::IsFalse (rig.view.GetHostInput().IsVisible());
            Assert::IsTrue  (rig.view.GetPortInput().IsVisible());
        }


        TEST_METHOD (HostAndPortAreReportedAsTyped)
        {
            ViewRig  rig;



            ViewRig::Pick (rig.view.GetAttachDropdown(), kTcpChoice);
            ViewRig::Type (rig.view.GetHostInput(), L"peer.example.org");
            ViewRig::Type (rig.view.GetPortInput(), L"2323");

            Assert::AreEqual (std::string ("peer.example.org"), rig.reported.back().options.host);
            Assert::AreEqual (2323, rig.reported.back().options.tcpPort);
        }


        TEST_METHOD (AnInvalidPortIsNotTakenAndShowsTheRule)
        {
            ViewRig  rig;



            ViewRig::Pick (rig.view.GetAttachDropdown(), kTcpChoice);
            ViewRig::Type (rig.view.GetPortInput(), L"70000");

            Assert::AreEqual (7000, rig.reported.back().options.tcpPort, L"the last port that parsed, typed a character before");
            Assert::IsTrue (rig.view.IsErrorShown());
            Assert::AreEqual (std::wstring (L"Invalid port number\nA port number is from 1 to 65535."), rig.view.GetErrorText());

            ViewRig::Type (rig.view.GetPortInput(), L"23");
            Assert::AreEqual (23, rig.reported.back().options.tcpPort);
            Assert::IsFalse (rig.view.IsErrorShown());
        }


        TEST_METHOD (AnEmptyListenPortIsNone)
        {
            ViewRig  rig;



            ViewRig::Pick (rig.view.GetAttachDropdown(), kModemChoice);
            rig.Lay();
            Assert::IsTrue (rig.view.GetListenPortInput().IsVisible());
            Assert::IsTrue (rig.view.GetTelnetCheckbox().IsChecked(), L"the modem uses Telnet by default");

            ViewRig::Type (rig.view.GetListenPortInput(), L"6400");
            Assert::AreEqual (6400, rig.reported.back().options.modemListenPort);

            rig.view.GetListenPortInput().OnKey (VK_END);

            for (int i = 0; i < 4; i++)
            {
                rig.view.GetListenPortInput().OnKey (VK_BACK);
            }

            Assert::AreEqual (0, rig.reported.back().options.modemListenPort);
        }


        TEST_METHOD (TheComPortListIsReadEachTimeItOpens)
        {
            ViewRig  rig;



            rig.ports.AddPort ("COM3");
            ViewRig::Pick (rig.view.GetAttachDropdown(), kComPortChoice);

            rig.ports.AddPort ("COM5");
            ViewRig::Pick (rig.view.GetComPortDropdown(), 1);

            Assert::AreEqual (std::string ("COM5"), rig.reported.back().options.comPort, L"a port added after the page opened is listed");
        }


        TEST_METHOD (AStagedPortThatIsGoneStaysListed)
        {
            ViewRig                 rig;
            SerialAttachmentChoice  choice;



            choice.device          = kpszSerialComPort;
            choice.options.comPort = "COM9";
            rig.ports.AddPort ("COM3");
            rig.view.SetChoice (choice);

            Assert::AreEqual (1, rig.view.GetComPortDropdown().GetSelectedIndex(), L"COM9 after the ports present");
        }


        TEST_METHOD (TelnetIsReportedWhenTurnedOff)
        {
            ViewRig  rig;



            ViewRig::Pick (rig.view.GetAttachDropdown(), kModemChoice);
            rig.Lay();
            rig.view.GetTelnetCheckbox().SetFocused (true);
            rig.view.GetTelnetCheckbox().OnKey (VK_SPACE);

            Assert::IsTrue  (rig.reported.back().options.telnet.has_value());
            Assert::IsFalse (*rig.reported.back().options.telnet);
        }


        TEST_METHOD (BrowseSetsTheFile)
        {
            ViewRig  rig;
            bool     wasOutput = false;



            rig.view.SetOnBrowse ([&wasOutput] (bool forOutput, std::string & inOutPath)
            {
                wasOutput = forOutput;
                inOutPath = "C:\\log.txt";
                return true;
            });

            ViewRig::Pick (rig.view.GetAttachDropdown(), 2);
            rig.Lay();
            Assert::IsTrue (rig.view.GetBrowseButton().IsVisible());

            rig.view.GetBrowseButton().Click();

            Assert::IsTrue (wasOutput, L"the output file");
            Assert::AreEqual (std::string ("C:\\log.txt"), rig.reported.back().path);
        }


        TEST_METHOD (LeavingAFileAttachmentDropsThePath)
        {
            ViewRig  rig;



            rig.view.SetChoice ({ kpszSerialFileOut, "C:\\log.txt", {} });
            ViewRig::Pick (rig.view.GetAttachDropdown(), kTcpChoice);

            Assert::AreEqual (std::string(), rig.reported.back().path);
        }


        TEST_METHOD (PortsParse)
        {
            int  port = -1;



            Assert::IsTrue  (SerialAttachmentView::TryParsePort (L"1", false, port));
            Assert::IsTrue  (SerialAttachmentView::TryParsePort (L"65535", false, port));
            Assert::IsFalse (SerialAttachmentView::TryParsePort (L"0", false, port));
            Assert::IsFalse (SerialAttachmentView::TryParsePort (L"65536", false, port));
            Assert::IsFalse (SerialAttachmentView::TryParsePort (L"12a", false, port));
            Assert::IsFalse (SerialAttachmentView::TryParsePort (L"", false, port));
            Assert::IsTrue  (SerialAttachmentView::TryParsePort (L"", true, port));
            Assert::AreEqual (0, port);
        }
    };
}
