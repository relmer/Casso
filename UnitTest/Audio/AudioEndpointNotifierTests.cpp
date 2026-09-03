#include "Pch.h"

#include "Audio/AudioEndpointNotifier.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AudioEndpointNotifierTests
//
//  The notifier is the only thing that hears the user change the default
//  output device (GH #137): the endpoint already open stays valid across such
//  a change, so no HRESULT anywhere reports it. What is testable here is the
//  filtering and the one-shot flag, driven by calling the COM entry points
//  the way MMDevice would.
//
//  Every instance is a stack object whose reference count starts at 1 and is
//  never taken to 0, so nothing here can reach the `delete this` in Release.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AudioEndpointNotifierTests)
{
public:

    TEST_METHOD (Defaults_reportsNoPendingChange)
    {
        AudioEndpointNotifier  notifier;
        bool                   moved = false;



        moved = notifier.ConsumeDefaultRenderChange();

        Assert::IsFalse (moved, L"a fresh notifier has heard nothing");
    }


    //  DefaultRenderConsoleMoved_isReportedOnce
    //
    //  The whole point: eRender / eConsole is the pair the host stream opens,
    //  so a move of it is what has to reach the audio object. Reading it also
    //  clears it, or the reopen would be re-armed on every frame that follows.

    TEST_METHOD (DefaultRenderConsoleMoved_isReportedOnce)
    {
        AudioEndpointNotifier  notifier;
        bool                   first    = false;
        bool                   second   = false;



        AssertSucceeded (notifier.OnDefaultDeviceChanged (eRender, eConsole, L"{0.0.0.00000000}.{device}"),
                         L"OnDefaultDeviceChanged must not fail");

        first  = notifier.ConsumeDefaultRenderChange();
        second = notifier.ConsumeDefaultRenderChange();

        Assert::IsTrue  (first,  L"the move must be reported");
        Assert::IsFalse (second, L"and reported only once");
    }


    //  RepeatedNotifications_collapseIntoOneReport
    //
    //  Windows reports the default endpoint moving several times while a
    //  device settles. One flag with one consumer is what keeps that burst
    //  from becoming a burst of teardowns.

    TEST_METHOD (RepeatedNotifications_collapseIntoOneReport)
    {
        AudioEndpointNotifier  notifier;
        int                    i      = 0;
        bool                   first  = false;
        bool                   second = false;



        for (i = 0; i < 5; i++)
        {
            AssertSucceeded (notifier.OnDefaultDeviceChanged (eRender, eConsole, L"{device}"),
                             L"OnDefaultDeviceChanged must not fail");
        }

        first  = notifier.ConsumeDefaultRenderChange();
        second = notifier.ConsumeDefaultRenderChange();

        Assert::IsTrue  (first,  L"five reports, one pending change");
        Assert::IsFalse (second, L"and nothing left behind them");
    }


    //  OtherFlowsAndRoles_reportNothing
    //
    //  A different capture default, or a different role on the render side,
    //  moves nothing we are playing through. Tearing the stream down for one
    //  of those would drop audio for no reason.

    TEST_METHOD (OtherFlowsAndRoles_reportNothing)
    {
        AudioEndpointNotifier  notifier;
        bool                   moved = false;



        AssertSucceeded (notifier.OnDefaultDeviceChanged (eCapture, eConsole,        L"{device}"));
        AssertSucceeded (notifier.OnDefaultDeviceChanged (eRender,  eMultimedia,     L"{device}"));
        AssertSucceeded (notifier.OnDefaultDeviceChanged (eRender,  eCommunications, L"{device}"));

        moved = notifier.ConsumeDefaultRenderChange();

        Assert::IsFalse (moved, L"only eRender / eConsole may arm the reopen");
    }


    //  OtherDeviceEvents_reportNothing
    //
    //  Arrival, removal, state and property changes are required stubs. A
    //  device appearing is not the default endpoint moving; when it does move
    //  the default, OnDefaultDeviceChanged reports that separately.

    TEST_METHOD (OtherDeviceEvents_reportNothing)
    {
        AudioEndpointNotifier  notifier;
        PROPERTYKEY            key   = {};
        bool                   moved = false;



        AssertSucceeded (notifier.OnDeviceAdded (L"{device}"));
        AssertSucceeded (notifier.OnDeviceRemoved (L"{device}"));
        AssertSucceeded (notifier.OnDeviceStateChanged (L"{device}", DEVICE_STATE_ACTIVE));
        AssertSucceeded (notifier.OnPropertyValueChanged (L"{device}", key));

        moved = notifier.ConsumeDefaultRenderChange();

        Assert::IsFalse (moved, L"none of those move the default endpoint");
    }


    //  QueryInterface_answersItsOwnInterfaces
    //
    //  MMDevice queries the object it was handed. Answering the wrong set
    //  means RegisterEndpointNotificationCallback accepts an object it can
    //  never call back, which looks exactly like a working registration.

    TEST_METHOD (QueryInterface_answersItsOwnInterfaces)
    {
        AudioEndpointNotifier  notifier;
        void                 * asUnknown  = nullptr;
        void                 * asClient   = nullptr;
        void                 * asStranger = nullptr;
        HRESULT                hrStranger = S_OK;
        HRESULT                hrNoOut    = S_OK;



        AssertSucceeded (notifier.QueryInterface (__uuidof (IUnknown), &asUnknown));
        Assert::IsNotNull (asUnknown, L"IUnknown must be answered");
        notifier.Release();

        AssertSucceeded (notifier.QueryInterface (__uuidof (IMMNotificationClient), &asClient));
        Assert::IsNotNull (asClient, L"IMMNotificationClient must be answered");
        notifier.Release();

        hrStranger = notifier.QueryInterface (__uuidof (IMMDevice), &asStranger);

        Assert::IsTrue (hrStranger == E_NOINTERFACE, L"an unrelated IID must be refused");
        Assert::IsNull (asStranger, L"a refused QueryInterface must null the out pointer");

        hrNoOut = notifier.QueryInterface (__uuidof (IUnknown), nullptr);

        Assert::IsTrue (hrNoOut == E_POINTER, L"a null out pointer must be refused");
    }


    //  AddRefRelease_countIsBalanced
    //
    //  The count starts at 1 so the creator can attach it to a ComPtr without
    //  taking a second reference. Release returns what remains, which is what
    //  tells the last reference to delete.

    TEST_METHOD (AddRefRelease_countIsBalanced)
    {
        AudioEndpointNotifier  notifier;
        ULONG                  second = 0;
        ULONG                  third  = 0;
        ULONG                  back2  = 0;
        ULONG                  back1  = 0;



        second = notifier.AddRef();
        third  = notifier.AddRef();
        back2  = notifier.Release();
        back1  = notifier.Release();

        Assert::IsTrue (second == 2, L"AddRef must report the second reference");
        Assert::IsTrue (third  == 3, L"AddRef must report the third reference");
        Assert::IsTrue (back2  == 2, L"Release must report two remaining");
        Assert::IsTrue (back1  == 1, L"Release must leave the creator's own");
    }
};
