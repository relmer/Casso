#include "Pch.h"

#include "../EhmTestHelper.h"
#include "FakeApple.h"
#include "InMemoryCassoLinkFolder.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "CassoLink/CassoLinkSearch.h"
#include "CassoLink/CassoLinkSession.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSearchTests
//
//  The COM search over FakeAppleFactory's ports, and a link's ports open
//  only while it is started.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkSearchTests)
{
public:

    using Port = FakeAppleFactory::Port;

    static constexpr DWORD  kReplyMs   = 300;
    static constexpr DWORD  kSpeeds[]  = { 9600, 115200 };

    struct Sink : ICassoLinkSessionSink
    {
        std::mutex               lock;
        std::condition_variable  changed;
        LinkState                state = LinkState::Closed;

        void  OnStateChanged (LinkState next) override
        {
            std::lock_guard<std::mutex>  guard (lock);

            state = next;
            changed.notify_all();
        }

        void  OnProgress (const std::shared_ptr<LinkOperation> &) override {}
        void  OnDone     (const std::shared_ptr<LinkOperation> &) override {}

        bool  WaitForState (LinkState goal, DWORD timeoutMs = 10000)
        {
            std::unique_lock<std::mutex>  guard (lock);

            return changed.wait_for (guard, std::chrono::milliseconds (timeoutMs), [&] () { return state == goal; });
        }
    };

    struct Rig
    {
        FakeApple                          apple;
        FakeAppleFactory                   factory { apple };
        InMemoryCassoLinkFolder            folder;
        FakeDiskFileIo                     fileIo;
        Sink                               sink;
        std::unique_ptr<CassoLinkSession>  session;

        Rig()
        {
            std::vector<Byte>  disk (NibblizationLayer::kImageByteSize, 0);

            AssertSucceeded (Dos33Skeleton::Write (disk, 254));

            apple.drives[0] = disk;
            apple.drives[1] = disk;
            session         = std::make_unique<CassoLinkSession> (factory, folder, fileIo, sink);
        }

        ~Rig()
        {
            session.reset();
        }
    };




    TEST_METHOD (Find_SkipsABusyAndASilentPort_AndStopsAtTheFirstApple)
    {
        Rig           rig;
        std::wstring  port;

        rig.factory.comPorts = { { L"COM1", Port::Busy }, { L"COM2", Port::Silent }, { L"COM3", Port::Apple }, { L"COM4", Port::Apple } };

        AssertSucceeded (CassoLinkSearch::Find (rig.factory, ComSettings(), kSpeeds, kReplyMs, port));

        Assert::AreEqual (std::wstring (L"COM3"), port);
        Assert::IsTrue   (rig.factory.comOpened == std::vector<std::wstring> { L"COM2", L"COM2", L"COM3" }, L"the busy port fails to open; the silent one is opened at both speeds; the search stops at COM3");
        Assert::IsFalse  (rig.factory.comOverlapped.load(), L"each port closed before the next");
        Assert::IsTrue   (rig.factory.ArePortsClosed());
    }




    TEST_METHOD (Find_WithNoAppleOnAnyPort_IsNotFound)
    {
        Rig           rig;
        std::wstring  port;

        rig.factory.comPorts = { { L"COM1", Port::Silent }, { L"COM2", Port::Busy } };

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_NOT_FOUND), CassoLinkSearch::Find (rig.factory, ComSettings(), kSpeeds, kReplyMs, port));
        Assert::IsTrue   (port.empty());
        Assert::IsTrue   (rig.factory.ArePortsClosed());
    }




    TEST_METHOD (ASerialLinkWithNoPort_SearchesAndConnects)
    {
        Rig                rig;
        CassoLinkSettings  settings;

        rig.factory.comPorts = { { L"COM1", Port::Silent }, { L"COM5", Port::Apple } };

        settings.name      = "Test";
        settings.transport = LinkTransport::Serial;

        AssertSucceeded (rig.session->Start (settings));
        Assert::IsTrue  (rig.sink.WaitForState (LinkState::Ready), L"connected on the port found");
        Assert::AreEqual (std::wstring (L"COM5"), rig.factory.comOpened.back());
    }




    TEST_METHOD (NoPortIsOpen_UntilStart_AndNoneAfterStop)
    {
        Rig                rig;
        CassoLinkSettings  settings;

        rig.factory.comPorts = { { L"COM3", Port::Apple } };

        settings.name      = "Test";
        settings.transport = LinkTransport::Serial;
        settings.port      = "COM3";

        Assert::IsTrue (rig.factory.comOpened.empty());

        AssertSucceeded (rig.session->Start (settings));
        Assert::IsTrue  (rig.sink.WaitForState (LinkState::Ready));
        Assert::IsFalse (rig.factory.ArePortsClosed(), L"open while started");

        rig.session->Stop();
        Assert::IsTrue (rig.factory.ArePortsClosed(), L"released on stop");
    }




    TEST_METHOD (AWifiModemLink_ListensOnTheNetwork_OnlyWhileStarted)
    {
        Rig                rig;
        CassoLinkSettings  settings;

        settings.name      = "Test";
        settings.transport = LinkTransport::WifiModem;
        settings.address   = "0.0.0.0:6502";

        AssertSucceeded (rig.session->Start (settings));
        Assert::IsTrue  (rig.sink.WaitForState (LinkState::WaitingForReady));

        Assert::IsTrue  (rig.factory.listened == std::vector<LinkTransport> { LinkTransport::WifiModem });
        Assert::AreEqual ("0.0.0.0", ByteStreamSettings::GetBindAddress (rig.factory.listened[0]));

        rig.session->Stop();
        Assert::IsTrue (rig.factory.ArePortsClosed());
    }



    TEST_METHOD (Find_OpensEachPortAtTheLinksSpeed_WhenTheBootstrapSpeedIsSilent)
    {
        Rig           rig;
        std::wstring  port;

        rig.factory.comPorts  = { { L"COM4", Port::Apple } };
        rig.factory.appleBaud = 115200;

        AssertSucceeded (CassoLinkSearch::Find (rig.factory, ComSettings(), kSpeeds, kReplyMs, port));

        Assert::AreEqual (std::wstring (L"COM4"), port);
        Assert::IsTrue   (rig.factory.comBauds == std::vector<DWORD> { 9600, 115200 }, L"a boot disk's shim starts at the link's speed");
    }




    TEST_METHOD (ASerialLink_OpensAtTheBootstrapSpeed_ThenTheLinks)
    {
        Rig                rig;
        CassoLinkSettings  settings;

        rig.factory.comPorts  = { { L"COM3", Port::Apple } };
        rig.factory.appleBaud = 115200;

        settings.name      = "Test";
        settings.transport = LinkTransport::Serial;
        settings.port      = "COM3";

        rig.session->SetConnectTimeout (2000);
        AssertSucceeded (rig.session->Start (settings));
        Assert::IsTrue  (rig.sink.WaitForState (LinkState::Ready), L"connected at the link's speed");
        Assert::AreEqual ((DWORD) 9600,   rig.factory.comBauds.front());
        Assert::AreEqual ((DWORD) 115200, rig.factory.comBauds.back());
    }




    TEST_METHOD (ASpeedChange_KeepsTheLinkAndItsDrives_ThroughAPortOpenedAgain)
    {
        Rig                rig;
        CassoLinkSettings  settings;

        rig.factory.comPorts     = { { L"COM3", Port::Apple } };
        rig.factory.peerReady    = 1;
        rig.factory.peerLagReads = 3;

        settings.name      = "Test";
        settings.transport = LinkTransport::Serial;
        settings.port      = "COM3";

        AssertSucceeded (rig.session->Start (settings));
        Assert::IsTrue   (rig.sink.WaitForState (LinkState::Ready));
        Assert::IsTrue   (rig.factory.comBauds == std::vector<DWORD> { 9600, 115200 }, L"connected at the bootstrap speed, then opened again at the link's");
        Assert::AreEqual ((int) Byte (0x03), (int) rig.session->GetDriveMap(), L"the drives of the Ready before the port was opened again");

        Assert::IsFalse (rig.sink.WaitForState (LinkState::Disconnected, 1000), L"DSR off on the port just opened is not the Apple gone");
    }




    TEST_METHOD (ABootstrapWithNoPort_UsesThePCsOnlyComPort)
    {
        Rig                             rig;
        CassoLinkSettings               settings;
        std::shared_ptr<LinkOperation>  bootstrap;

        rig.factory.comPorts = { { L"COM3", Port::Apple } };
        rig.apple.startAsShim = true;

        settings.name      = "Test";
        settings.transport = LinkTransport::Serial;

        rig.session->SetBootstrapPause (0, 0);
        AssertSucceeded (rig.session->StartWithBootstrap (settings, bootstrap));
        Assert::IsTrue  (rig.sink.WaitForState (LinkState::Ready), L"connected after the bootstrap");
        Assert::IsTrue  (rig.factory.comOpened.front() == L"COM3", L"the loader went to the only port, with no search");
        Assert::AreEqual (0, rig.apple.shimIdles.load(), L"no Idle prompt, which would cross the new shim's Ready");
    }
};