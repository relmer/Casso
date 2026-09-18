#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiKeyMapTests
//
//  A named table from key chord to an application's command id, and the window
//  path that consults it.
//
//  A MAP IS DATA, SO SCHEMES ARE DATA. An application with more than one
//  keyboard scheme builds one map per scheme and installs the chosen one; the
//  window never knows which scheme it has.
//
//  THE MAP COMES LAST. The focused control sees a key first, so a text box
//  keeps its keys; only a key nothing claimed is looked up.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiKeyMapTests
{
    static constexpr int  kRun      = 101;
    static constexpr int  kStepOver = 102;
    static constexpr int  kStepOut  = 103;



    static const DxuiKeyChord  s_kVisualStudio[] =
    {
        { VK_F5,  false, false, false, kRun      },
        { VK_F10, false, false, false, kStepOver },
        { VK_F11, false, false, true,  kStepOut  },
    };

    static const DxuiKeyChord  s_kOther[] =
    {
        { VK_SPACE, true, false, false, kStepOver },
    };





    ////////////////////////////////////////////////////////////////////////////
    //
    //  RecordingWindow
    //
    //  A window that records the commands its map hands it, and claims the
    //  keys a focused control would have claimed.
    //
    ////////////////////////////////////////////////////////////////////////////

    class RecordingWindow : public DxuiWindow
    {
    public:
        std::vector<int>  commands;
        WPARAM            claimed = 0;

        bool  Press (WPARAM vk, bool ctrl = false, bool alt = false, bool shift = false)
        {
            DxuiKeyEvent  ev;



            ev.kind  = DxuiKeyEventKind::Down;
            ev.vk    = vk;
            ev.ctrl  = ctrl;
            ev.alt   = alt;
            ev.shift = shift;

            return OnKey (ev) || RouteMappedKey (ev);
        }

        bool  OnKey (const DxuiKeyEvent & ev) override
        {
            return claimed != 0 && ev.vk == claimed;
        }

    protected:
        bool  OnMappedCommand (int commandId) override
        {
            commands.push_back (commandId);
            return true;
        }
    };





    ////////////////////////////////////////////////////////////////////////////
    //
    //  LookupTests
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (LookupTests)
    {
    public:

        TEST_METHOD (AChordTranslatesToItsCommand)
        {
            DxuiKeyMap  map (L"Visual Studio", s_kVisualStudio);
            int         id = 0;



            Assert::IsTrue   (map.TryTranslate (VK_F10, false, false, false, id));
            Assert::AreEqual (kStepOver, id);
            Assert::AreEqual (std::wstring (L"Visual Studio"), map.GetName());
        }


        TEST_METHOD (ModifiersMustMatchExactly)
        {
            DxuiKeyMap  map (L"Visual Studio", s_kVisualStudio);
            int         id = 0;



            Assert::IsTrue  (map.TryTranslate (VK_F11, false, false, true,  id), L"Shift+F11");
            Assert::IsFalse (map.TryTranslate (VK_F11, false, false, false, id), L"F11 alone is not Shift+F11");
            Assert::IsFalse (map.TryTranslate (VK_F5,  true,  false, false, id), L"Ctrl+F5 is not F5");
        }


        TEST_METHOD (AnUnmappedKeyTranslatesToNothing)
        {
            DxuiKeyMap  map (L"Visual Studio", s_kVisualStudio);
            DxuiKeyMap  empty;
            int         id = 7;



            Assert::IsFalse  (map.TryTranslate ('Q', false, false, false, id));
            Assert::IsFalse  (empty.TryTranslate (VK_F5, false, false, false, id));
            Assert::AreEqual (7, id, L"a miss leaves the out value alone");
        }


        TEST_METHOD (TheChordTextNamesTheKeysForAMenu)
        {
            DxuiKeyMap  vs    (L"Visual Studio", s_kVisualStudio);
            DxuiKeyMap  other (L"Other",         s_kOther);



            Assert::AreEqual (std::wstring (L"F10"),        vs.GetChordText (kStepOver));
            Assert::AreEqual (std::wstring (L"Shift+F11"),  vs.GetChordText (kStepOut));
            Assert::AreEqual (std::wstring (L"Ctrl+Space"), other.GetChordText (kStepOver));
            Assert::AreEqual (std::wstring(),               other.GetChordText (kRun), L"no chord, no text");
        }
    };





    ////////////////////////////////////////////////////////////////////////////
    //
    //  WindowTests
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (WindowTests)
    {
    public:

        TEST_METHOD (AnUnclaimedKeyReachesTheMap)
        {
            DxuiKeyMap       map (L"Visual Studio", s_kVisualStudio);
            RecordingWindow  window;



            window.SetKeyMap (&map);

            Assert::IsTrue   (window.Press (VK_F5));
            Assert::AreEqual ((size_t) 1, window.commands.size());
            Assert::AreEqual (kRun, window.commands[0]);
        }


        TEST_METHOD (AClaimedKeyNeverReachesTheMap)
        {
            DxuiKeyMap       map (L"Visual Studio", s_kVisualStudio);
            RecordingWindow  window;



            window.SetKeyMap (&map);
            window.claimed = VK_F5;

            Assert::IsTrue (window.Press (VK_F5));
            Assert::IsTrue (window.commands.empty(), L"the focused control kept it");
        }


        TEST_METHOD (MapsSwapAtRunTime)
        {
            DxuiKeyMap       vs    (L"Visual Studio", s_kVisualStudio);
            DxuiKeyMap       other (L"Other",         s_kOther);
            RecordingWindow  window;



            window.SetKeyMap (&vs);
            Assert::IsTrue  (window.Press (VK_F10));
            Assert::IsFalse (window.Press (VK_SPACE, true));

            window.SetKeyMap (&other);
            Assert::IsFalse (window.Press (VK_F10),        L"the old scheme's key is gone");
            Assert::IsTrue  (window.Press (VK_SPACE, true));

            Assert::AreEqual ((size_t) 2, window.commands.size());
            Assert::IsTrue   (window.GetKeyMap() == &other);
        }


        TEST_METHOD (NoMapRoutesNothing)
        {
            RecordingWindow  window;



            Assert::IsFalse (window.Press (VK_F5));
            Assert::IsTrue  (window.commands.empty());
        }
    };
}
