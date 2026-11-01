#include "Pch.h"

#include "CppUnitTest.h"
#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/Panes/SoundBoardView.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardViewTests
//
//  The Mockingboard view's mutes, tips, reflow and scrolling.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (SoundBoardViewTests)
    {
    public:

        static DiagnosticsSoundBoard MakeBoard (bool hasVoice)
        {
            DiagnosticsSoundBoard  board;



            board.slot = 4;
            board.halves.resize (2);
            board.halves[0].index       = 0;
            board.halves[0].baseAddress = 0xC400;
            board.halves[1].index       = 1;
            board.halves[1].baseAddress = 0xC480;

            if (hasVoice)
            {
                board.voice = DiagnosticsSoundVoice();
            }

            return board;
        }


        static DxuiMouseEvent MakeClick (const RECT & rect)
        {
            DxuiMouseEvent  ev;



            ev.kind        = DxuiMouseEventKind::Down;
            ev.button      = DxuiMouseButton::Left;
            ev.positionDip = { (rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2 };

            return ev;
        }


        //  Tall and wide enough for two columns and no scrollbar.
        static void LayOut (SoundBoardView & view, int width = 1400, int height = 1000)
        {
            view.Layout ({ 0, 0, width, height }, DxuiDpiScaler());
        }


        TEST_METHOD (AClickOnAMuteTogglesItsChannel)
        {
            SoundBoardView         view;
            DiagnosticsSoundBoard  board   = MakeBoard (true);
            int                    channel = -1;
            bool                   isMuted = false;



            board.halves[1].psg.channels[2].isMuted = true;

            view.SetOnMute ([&] (int c, bool m) { channel = c; isMuted = m; });
            LayOut (view);
            view.SetBoard (board);

            Assert::IsTrue  (view.OnMouse (MakeClick (view.GetMuteRect (1))));
            Assert::AreEqual (1, channel);
            Assert::IsTrue  (isMuted, L"not muted before");

            Assert::IsTrue  (view.OnMouse (MakeClick (view.GetMuteRect (5))));
            Assert::AreEqual (5, channel);
            Assert::IsFalse (isMuted, L"muted before");

            Assert::IsTrue  (view.OnMouse (MakeClick (view.GetMuteRect (SoundBoardView::kVoiceMuteChannel))));
            Assert::AreEqual (SoundBoardView::kVoiceMuteChannel, channel);
        }


        TEST_METHOD (ACardWithoutTheSpeechChipHasNoVoiceMute)
        {
            SoundBoardView  view;
            RECT            voice = {};
            RECT            first = {};



            LayOut (view);
            view.SetBoard (MakeBoard (false));
            voice = view.GetMuteRect (SoundBoardView::kVoiceMuteChannel);
            first = view.GetMuteRect (0);

            Assert::IsTrue (IsRectEmpty (&voice) != FALSE);
            Assert::IsTrue (IsRectEmpty (&first) == FALSE);
        }


        TEST_METHOD (AClickElsewhereMutesNothing)
        {
            SoundBoardView  view;
            int             calls = 0;



            view.SetOnMute ([&] (int, bool) { calls++; });
            LayOut (view);
            view.SetBoard (MakeBoard (true));

            Assert::IsFalse (view.OnMouse (MakeClick ({ 300, 900, 300, 900 })));
            Assert::AreEqual (0, calls);
        }


        TEST_METHOD (ATipHasTheRawRegisterValues)
        {
            SoundBoardView         view;
            DiagnosticsSoundBoard  board = MakeBoard (false);
            RECT                   mute  = {};
            std::wstring           tip;



            board.halves[0].psg.channels[0].period = 0x0123;
            board.halves[0].psg.mixer              = 0x3E;
            LayOut (view);
            view.SetBoard (board);
            mute = view.GetMuteRect (0);

            Assert::IsTrue  (view.TryGetTipAt ({ mute.right + 10, mute.top + 2 }, tip));
            Assert::IsTrue  (tip.find (L"$0123") != std::wstring::npos, tip.c_str());
            Assert::IsTrue  (tip.find (L"$3E")   != std::wstring::npos, tip.c_str());

            Assert::IsTrue  (view.TryGetTipAt ({ (mute.left + mute.right) / 2, (mute.top + mute.bottom) / 2 }, tip));
            Assert::AreEqual (std::wstring (L"Mute channel A"), tip);
        }


        //  At two columns the halves are side by side; at one, the second is below.
        TEST_METHOD (TheCardsReflowWithTheWidth)
        {
            SoundBoardView  view;
            RECT            first  = {};
            RECT            second = {};



            view.SetBoard (MakeBoard (false));

            LayOut (view, 1400, 2000);
            first  = view.GetMuteRect (0);
            second = view.GetMuteRect (3);
            Assert::AreEqual (first.top, second.top, L"side by side");
            Assert::IsTrue   (second.left > first.left);

            LayOut (view, 500, 2000);
            first  = view.GetMuteRect (0);
            second = view.GetMuteRect (3);
            Assert::AreEqual (first.left, second.left, L"in one column");
            Assert::IsTrue   (second.top > first.top);
        }


        TEST_METHOD (TheWheelScrollsATallBoardWithinItsContent)
        {
            SoundBoardView  view;
            DxuiMouseEvent  wheel;
            RECT            before = {};
            RECT            after  = {};



            LayOut (view, 500, 300);
            view.SetBoard (MakeBoard (true));
            before = view.GetMuteRect (0);

            wheel.kind        = DxuiMouseEventKind::Wheel;
            wheel.positionDip = { 100, 100 };
            wheel.wheelDelta  = -1.0f;

            Assert::IsTrue (view.OnMouse (wheel));
            after = view.GetMuteRect (0);
            Assert::IsTrue (after.top < before.top, L"moved up");

            wheel.wheelDelta = 1000.0f;
            view.OnMouse (wheel);
            Assert::AreEqual (0, view.GetScrollTop(), L"to the top and no further");

            wheel.wheelDelta = -1000.0f;
            view.OnMouse (wheel);
            Assert::IsTrue (view.GetScrollTop() > 0);
            Assert::IsTrue (view.GetScrollTop() <= view.GetContentHeight());
        }


        //  Every part with data in it, in the three kinds of theme.
        TEST_METHOD (AFullBoardPaintsInEachTheme)
        {
            SoundBoardView         view;
            DiagnosticsSoundBoard  board = MakeBoard (true);
            MockDxuiTextRenderer   text;



            board.sampleRate                            = 44100;
            board.halves[0].busState                    = "Write";
            board.halves[0].psg.channels[0].frequencyHz = 440.0;
            board.halves[0].psg.channels[0].level       = 15;
            board.halves[0].psg.channels[0].isToneOn    = true;
            board.halves[0].psg.channels[0].scope.assign (1024, 0.5f);
            board.halves[0].psg.channels[1].isMuted     = true;
            board.halves[0].psg.envelopeRegister        = 0x0E;
            board.voice->phoneme                        = "IU (you)";
            board.voice->isSounding                     = true;
            board.voice->pitchHz                        = 100.0;
            board.voice->formantsHz                     = { 400.0, 1500.0, 2500.0 };
            board.voice->recent                         = { { "E (meet)", 0.05f }, { "IU (you)", 0.1f } };
            board.voice->scope.assign (1024, 0.25f);

            LayOut (view);
            view.SetBoard (board);

            for (const CassoTheme & theme : { CassoTheme::MakeSkeuomorphic(), CassoTheme::MakeDarkModern(), CassoTheme::MakeRetroTerminal() })
            {
                MockDxuiPainter  painter;

                view.Paint (painter, text, theme);

                Assert::IsFalse (painter.Calls().empty());
            }
        }


        TEST_METHOD (ABoardThatFitsDoesNotScroll)
        {
            SoundBoardView  view;
            DxuiMouseEvent  wheel;



            LayOut (view, 1400, 2000);
            view.SetBoard (MakeBoard (false));

            wheel.kind        = DxuiMouseEventKind::Wheel;
            wheel.positionDip = { 100, 100 };
            wheel.wheelDelta  = -5.0f;
            view.OnMouse (wheel);

            Assert::AreEqual (0, view.GetScrollTop());
        }
    };
}
