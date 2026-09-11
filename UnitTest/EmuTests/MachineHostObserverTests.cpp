#include "Pch.h"

#include "Core/MemoryBus.h"
#include "Devices/IInputEventSink.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Machines/Apple2/Common/IDisk2EventSink.h"
#include "Shell/MachineHost.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RecordingInputSink
//
//  Stands in for the Input debug panel. Counts what it was told.
//
////////////////////////////////////////////////////////////////////////////////

class RecordingInputSink : public IInputEventSink
{
public:

    void OnHostKeyDown (Byte ascii) override  { m_keyDowns += 1; m_lastAscii = ascii; }
    void OnHostKeyUp   (Byte ascii) override  { UNREFERENCED_PARAMETER (ascii); m_keyUps += 1; }

    void OnHostAutoRepeat (Byte ascii) override
    {
        UNREFERENCED_PARAMETER (ascii);
        m_autoRepeats += 1;
    }

    void OnButtonRead (Word address, Byte value) override
    {
        UNREFERENCED_PARAMETER (address);
        UNREFERENCED_PARAMETER (value);
    }

    void OnPaddleTrigger (Word address) override
    {
        UNREFERENCED_PARAMETER (address);
    }

    void OnPaddleRead (Word address, Byte value) override
    {
        UNREFERENCED_PARAMETER (address);
        UNREFERENCED_PARAMETER (value);
    }

    void OnKbdDataRead (Word address, Byte value, bool strobeSet) override
    {
        UNREFERENCED_PARAMETER (address);
        UNREFERENCED_PARAMETER (value);
        UNREFERENCED_PARAMETER (strobeSet);
        m_dataReads += 1;
    }

    void OnKbdStrobe (Word address, Byte value, bool cleared) override
    {
        UNREFERENCED_PARAMETER (address);
        UNREFERENCED_PARAMETER (value);
        UNREFERENCED_PARAMETER (cleared);
        m_strobes += 1;
    }

    int   GetKeyDowns  () const { return m_keyDowns; }
    int   GetAutoRepeats() const { return m_autoRepeats; }
    int   GetKeyUps    () const { return m_keyUps; }
    int   GetDataReads() const { return m_dataReads; }
    int   GetStrobes   () const { return m_strobes; }
    Byte  GetLastAscii() const { return m_lastAscii; }

private:

    int   m_keyDowns    = 0;
    int   m_autoRepeats = 0;
    int   m_keyUps      = 0;
    int   m_dataReads   = 0;
    int   m_strobes     = 0;
    Byte  m_lastAscii   = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHostObserverTests
//
//  Who is watching the machine, and whether they are still watching after
//  the machine is rebuilt underneath them.
//
//  The second half is the shipped fix for Spec-006 bug 15 that has had no
//  test guarding it: a machine switch replaces every device, and an open
//  debug panel that is not re-pointed at the new ones goes silent with no
//  visible sign that anything is wrong.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (MachineHostObserverTests)
{
public:

    TEST_METHOD (AnAttachedSinkSeesTheKeysTheGuestSees)
    {
        MachineHost         host;
        RecordingInputSink  panel;

        BuildKeyboard (host);
        host.AttachObservers ({ nullptr, &panel });

        host.GetRefs().keyboard->BeginKeyRepeat ('A');

        Assert::AreEqual (1, panel.GetKeyDowns(), L"the sink is told about the key");
        Assert::AreEqual<Byte> ('A', panel.GetLastAscii(), L"and about which one");
    }


    TEST_METHOD (DetachingStopsTheEvents)
    {
        MachineHost         host;
        RecordingInputSink  panel;

        BuildKeyboard (host);
        host.AttachObservers ({ nullptr, &panel });
        host.GetRefs().keyboard->BeginKeyRepeat ('A');

        //  Closing the panel.
        host.AttachObservers ({});
        host.GetRefs().keyboard->BeginKeyRepeat ('B');

        Assert::AreEqual (1, panel.GetKeyDowns(),
            L"a closed panel must stop receiving, not merely stop being read -- "
            L"the sink is about to be destroyed");
    }


    TEST_METHOD (RebuildingTheMachineNeedsTheObserversAttachedAgain)
    {
        MachineHost         host;
        RecordingInputSink  panel;

        BuildKeyboard (host);
        host.AttachObservers ({ nullptr, &panel });
        host.GetRefs().keyboard->BeginKeyRepeat ('A');

        //  What a machine switch does: every device is destroyed and built
        //  again, and the refs that named them are cleared.
        RebuildKeyboard (host);

        host.GetRefs().keyboard->BeginKeyRepeat ('B');

        Assert::AreEqual (1, panel.GetKeyDowns(),
            L"the new keyboard starts unwatched -- this is the silence the "
            L"re-attach exists to prevent");

        host.AttachObservers ({ nullptr, &panel });
        host.GetRefs().keyboard->BeginKeyRepeat ('C');

        Assert::AreEqual (2, panel.GetKeyDowns(),
            L"and re-attaching puts the same panel back on the new machine");
    }


    TEST_METHOD (AnInputObserverDoesNotNeedADiskObserver)
    {
        MachineHost         host;
        RecordingInputSink  panel;

        BuildKeyboard (host);

        //  The Input panel is useful on its own. Attaching used to return
        //  early when no disk panel was open, so someone running only this
        //  one saw it go quiet after a machine switch and stay quiet.
        host.AttachObservers ({ nullptr, &panel });
        host.GetRefs().keyboard->BeginKeyRepeat ('A');

        Assert::AreEqual (1, panel.GetKeyDowns(),
            L"the input sink attaches whether or not anything watches the disk");
    }


    TEST_METHOD (AMachineWithNoDevicesIsAttachableWithoutIncident)
    {
        MachineHost         host;
        RecordingInputSink  panel;

        //  Panels can be open across the window in which a machine has been
        //  torn down and not yet rebuilt.
        host.AttachObservers ({ nullptr, &panel });
        host.AttachObservers ({});
    }


private:

    static void BuildKeyboard (MachineHost & host)
    {
        auto  keyboard = std::make_unique<AppleKeyboard>();

        host.GetMemoryBus().AddDevice (keyboard.get());
        host.GetRefs().keyboard = keyboard.get();
        host.GetOwnedDevices().push_back (std::move (keyboard));
    }


    //  A machine switch in miniature: drop what the machine owned, clear the
    //  pointers that named it, build again.
    static void RebuildKeyboard (MachineHost & host)
    {
        host.GetRefs() = {};
        host.GetMemoryBus().Reset();
        host.GetOwnedDevices().clear();

        BuildKeyboard (host);
    }
};
