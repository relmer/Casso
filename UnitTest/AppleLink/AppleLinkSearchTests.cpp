#include "Pch.h"

#include "../EhmTestHelper.h"
#include "FakeApple.h"
#include "InMemoryAppleLinkFolder.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "AppleLink/AppleLinkSearch.h"
#include "AppleLink/AppleLinkSession.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkSearchTests
//
//  The COM search over FakeAppleFactory's ports, and a link's ports open
//  only while it is started (FR-083a, FR-097).
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AppleLinkSearchTests)
{
public:

    using Port = FakeAppleFactory::Port;

    static constexpr DWORD  kReplyMs   = 300;
    static constexpr DWORD  kSpeeds[]  = { 9600, 115200 };

    struct Sink : IAppleLinkSessionSink
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
        InMemoryAppleLinkFolder            folder;
        FakeDiskFileIo                     fileIo;
        Sink                               sink;
        std::unique_ptr<AppleLinkSession>  session;

        Rig()
        {
            std::vector<Byte>  disk (NibblizationLayer::kImageByteSize, 0);

            AssertSucceeded (Dos33Skeleton::Write (disk, 254));

            apple.drives[0] = disk;
            apple.drives[1] = disk;
            session         = std::make_unique<AppleLinkSession> (factory, folder, fileIo, sink);
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

        AssertSucceeded (AppleLinkSearch::Find (rig.factory, ComSettings(), kSpeeds, kReplyMs, port));

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

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_NOT_FOUND), AppleLinkSearch::Find (rig.factory, ComSettings(), kSpeeds, kReplyMs, port));
        Assert::IsTrue   (port.empty());
        Assert::IsTrue   (rig.factory.ArePortsClosed());
    }




    TEST_METHOD (ASerialLinkWithNoPort_SearchesAndConnects)
    {
        Rig                rig;
        AppleLinkSettings  settings;

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
        AppleLinkSettings  settings;

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
        AppleLinkSettings  settings;

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

        AssertSucceeded (AppleLinkSearch::Find (rig.factory, ComSettings(), kSpeeds, kReplyMs, port));

        Assert::AreEqual (std::wstring (L"COM4"), port);
        Assert::IsTrue   (rig.factory.comBauds == std::vector<DWORD> { 9600, 115200 }, L"a boot disk's shim starts at the link's speed");
    }




    TEST_METHOD (ASerialLink_OpensAtTheBootstrapSpeed_ThenTheLinks)
    {
        Rig                rig;
        AppleLinkSettings  settings;

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
};