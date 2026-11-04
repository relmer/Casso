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



        static DxuiKeyEvent MakeKey (WPARAM vk, bool shift = false)
        {
            DxuiKeyEvent  ev;



            ev.kind  = DxuiKeyEventKind::Down;
            ev.vk    = vk;
            ev.shift = shift;
            return ev;
        }


        //  Each mute is a button in the Tab order: Tab enters at the speech
        //  chip's, moves through the channels and out of the view after the
        //  last, Shift+Tab moves back, and Space presses the one focused.
        TEST_METHOD (TheMutesAreTabStopsThatSpacePresses)
        {
            SoundBoardView         view;
            DiagnosticsSoundBoard  board   = MakeBoard (true);
            int                    channel = -1;
            bool                   isMuted = false;



            view.SetOnMute ([&] (int c, bool m) { channel = c; isMuted = m; });
            LayOut (view);
            view.SetBoard (board);

            Assert::AreEqual (-1, view.GetFocusedMute(), L"a mute has the focus while the view doesn't");

            view.OnFocusChanged (true);
            view.OnFocusEntered (true);
            Assert::AreEqual (SoundBoardView::kVoiceMuteChannel, view.GetFocusedMute(), L"Tab enters at the speech chip's mute");

            Assert::IsTrue   (view.OnKey (MakeKey (VK_TAB)));
            Assert::AreEqual (0, view.GetFocusedMute(), L"then channel A of AY #1");

            Assert::IsTrue   (view.OnKey (MakeKey (VK_SPACE)));
            Assert::AreEqual (0, channel);
            Assert::IsTrue   (isMuted, L"Space mutes the focused channel");

            for (int c = 1; c < 6; c++)
            {
                Assert::IsTrue (view.OnKey (MakeKey (VK_TAB)));
            }

            Assert::AreEqual (5,    view.GetFocusedMute(), L"channel C of AY #2 last");
            Assert::IsFalse  (view.OnKey (MakeKey (VK_TAB)), L"Tab after the last moves out of the view");
            Assert::IsTrue   (view.OnKey (MakeKey (VK_TAB, true)));
            Assert::AreEqual (4,    view.GetFocusedMute(), L"Shift+Tab moves back");

            view.OnFocusEntered (false);
            Assert::AreEqual (5, view.GetFocusedMute(), L"Shift+Tab enters at the last");
            Assert::IsTrue   (view.GetAccessibleName().find (L"channel C of AY #2") != std::wstring::npos, view.GetAccessibleName().c_str());
        }


        //  A mute is drawn as a button, with an edge around it and the focus
        //  ring while the keyboard has it, so it doesn't read as a label.
        TEST_METHOD (AMuteIsDrawnAsAButton)
        {
            SoundBoardView         view;
            MockDxuiPainter        painter;
            MockDxuiTextRenderer   text;
            CassoTheme             theme   = CassoTheme::MakeSkeuomorphic();
            RECT                   mute    = {};
            auto                   edges   = [&] (const RECT & at)
            {
                int  count = 0;

                for (const RecordedPaintCall & call : painter.Calls())
                {
                    bool  isAround = call.x <= (float) at.left && call.y <= (float) at.top &&
                                     call.x + call.width >= (float) at.right && call.y + call.height >= (float) at.bottom &&
                                     call.width <= (float) (at.right - at.left) + 12.0f;

                    count += (call.kind == RecordedPaintKind::OutlineRoundedRect && isAround) ? 1 : 0;
                }

                return count;
            };



            LayOut (view);
            view.SetBoard (MakeBoard (true));
            mute = view.GetMuteRect (2);

            view.Paint (painter, text, theme);
            Assert::AreEqual (1, edges (mute), L"an edge around the mute");

            view.OnFocusChanged (true);
            view.OnFocusEntered (true);

            for (int c = 0; c < 3; c++)
            {
                (void) view.OnKey (MakeKey (VK_RIGHT));
            }

            Assert::AreEqual (2, view.GetFocusedMute());

            painter.Reset();
            view.Paint (painter, text, theme);
            Assert::AreEqual (2, edges (mute), L"and the focus ring around that");
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


        //  The phoneme strip scrolls back through the history a few
        //  phonemes a notch, to the oldest the chip has and back to the
        //  newest, and no further either way.
        TEST_METHOD (ThePhonemeStripScrollsBackThroughTheHistory)
        {
            SoundBoardView         view;
            DiagnosticsSoundBoard  board = MakeBoard (true);



            for (int i = 0; i < 10; i++)
            {
                board.voice->recent.push_back ({ "E (meet)", 0.05f });
            }

            LayOut (view);
            view.SetBoard (board);

            view.ScrollPhonemes (SoundBoardView::kPhonemesPerNotch);
            Assert::AreEqual ((size_t) SoundBoardView::kPhonemesPerNotch, view.GetPhonemesBack());

            view.ScrollPhonemes (1000);
            Assert::AreEqual ((size_t) 9, view.GetPhonemesBack(), L"as far back as the oldest");

            view.ScrollPhonemes (-1000);
            Assert::AreEqual ((size_t) 0, view.GetPhonemesBack(), L"the newest again");
        }


        //  The oldest phonemes are at the strip's left, so a tilt of the
        //  wheel to the left goes back to them and one to the right comes
        //  forward again.
        TEST_METHOD (TheHorizontalWheelScrollsThePhonemeStrip)
        {
            SoundBoardView         view;
            DiagnosticsSoundBoard  board = MakeBoard (true);
            DxuiMouseEvent         wheel;
            bool                   found = false;



            for (int i = 0; i < 10; i++)
            {
                board.voice->recent.push_back ({ "E (meet)", 0.05f });
            }

            LayOut (view, 500, 2000);
            view.SetBoard (board);

            wheel.kind = DxuiMouseEventKind::Wheel;

            for (int y = 0; y < 2000 && !found; y += 2)
            {
                wheel.positionDip = POINT { 250, y };
                found             = view.IsOverPhonemeStrip (wheel.positionDip);
            }

            Assert::IsTrue (found, L"no strip on the board");

            wheel.wheelHorizontal = true;
            wheel.wheelDelta      = -1.0f;
            Assert::IsTrue   (view.OnMouse (wheel));
            Assert::AreEqual ((size_t) SoundBoardView::kPhonemesPerNotch, view.GetPhonemesBack(), L"the tilt to the left");

            wheel.wheelDelta = 1.0f;
            Assert::IsTrue   (view.OnMouse (wheel));
            Assert::AreEqual ((size_t) 0, view.GetPhonemesBack(), L"the tilt to the right");
        }


        //  A pause falls between words, so on the strip and in the row of
        //  labels under it a pause is a line rather than a block or a label.
        TEST_METHOD (APauseIsALineBetweenTheWords)
        {
            SoundBoardView         view;
            DiagnosticsSoundBoard  words     = MakeBoard (true);
            DiagnosticsSoundBoard  letters   = MakeBoard (true);
            MockDxuiTextRenderer   text;
            MockDxuiPainter        withPause;
            MockDxuiPainter        without;
            CassoTheme             theme     = CassoTheme::MakeDarkModern();
            auto                   count     = [] (const MockDxuiPainter & painter, RecordedPaintKind kind)
            {
                return std::ranges::count_if (painter.Calls(), [kind] (const RecordedPaintCall & call)
                {
                    return call.kind == kind && (kind != RecordedPaintKind::DrawLine || call.width < 1.0f);
                });
            };



            words.voice->recent   = { { "E (meet)", 0.1f }, { "PA (pause)", 0.1f, true }, { "E (meet)", 0.1f } };
            letters.voice->recent = { { "E (meet)", 0.1f }, { "E (meet)", 0.1f },         { "E (meet)", 0.1f } };

            LayOut (view);
            view.SetBoard (words);
            view.Paint (withPause, text, theme);

            view.SetBoard (letters);
            view.Paint (without, text, theme);

            Assert::AreEqual (count (without, RecordedPaintKind::DrawLine) + 2, count (withPause, RecordedPaintKind::DrawLine), L"a line on the strip and one in the labels");
            Assert::AreEqual (count (without, RecordedPaintKind::FillRoundedRect) - 1, count (withPause, RecordedPaintKind::FillRoundedRect), L"the pause has a block");
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


        TEST_METHOD (TheEnvelopesR13ValueIsBesideItsPicture)
        {
            SoundBoardView         view;
            DiagnosticsSoundBoard  board   = MakeBoard (false);
            MockDxuiPainter        painter;
            MockDxuiTextRenderer   text;
            CassoTheme             theme   = CassoTheme::MakeSkeuomorphic();
            bool                   isFound = false;



            board.halves[0].psg.envelopeRegister = 0x0E;
            LayOut (view);
            view.SetBoard (board);
            view.Paint (painter, text, theme);

            for (const RecordedTextCall & call : text.Calls())
            {
                isFound = isFound || call.text == L"$0E (14)";
            }

            Assert::IsTrue (isFound);
        }


        //  After a paint, each part has a separate tip: an R13 bit and what it
        //  does now, and a register and what its value means.
        TEST_METHOD (EachPaintedPartHasATipOfItsOwn)
        {
            SoundBoardView         view;
            DiagnosticsSoundBoard  board   = MakeBoard (false);
            MockDxuiPainter        painter;
            MockDxuiTextRenderer   text;
            CassoTheme             theme   = CassoTheme::MakeSkeuomorphic();
            std::wstring           tip;
            std::set<std::wstring> tips;
            RECT                   bounds  = {};



            board.halves[0].psg.envelopeRegister = 0x0E;
            board.halves[0].psg.registers[7]     = 0x38;
            LayOut (view);
            view.SetBoard (board);
            view.Paint (painter, text, theme);
            bounds = view.GetBounds();

            for (int y = bounds.top; y < bounds.bottom; y += 4)
            {
                for (int x = bounds.left; x < bounds.right; x += 4)
                {
                    if (view.TryGetTipAt ({ x, y }, tip))
                    {
                        tips.insert (tip);
                    }
                }
            }

            Assert::IsTrue (std::ranges::any_of (tips, [] (const std::wstring & each) { return each.starts_with (L"R13's bit 1, alternate"); }), L"the ALT bit");
            Assert::IsTrue (std::ranges::any_of (tips, [] (const std::wstring & each) { return each.starts_with (L"The mixer:") && each.ends_with (L"Tone is on for A, B, C and noise for none."); }), L"the mixer's register");
            Assert::IsTrue (std::ranges::any_of (tips, [] (const std::wstring & each) { return each.starts_with (L"Auxiliary control"); }), L"the 6522's ACR");
            Assert::IsTrue (std::ranges::any_of (tips, [] (const std::wstring & each) { return each.starts_with (L"Channel A's output"); }), L"channel A's scope");
        }



        //  A tip is one paragraph, and it doesn't begin with the label drawn
        //  beside its value: CONT's tip isn't "CONT, continue", and Writes'
        //  isn't "Writes: 12".
        TEST_METHOD (NoTipBeginsWithItsLabelOrBreaksALine)
        {
            static constexpr const wchar_t * kLabels[] =
            {
                L"CONT", L"ATT", L"ALT", L"HOLD", L"Ramp", L"R13", L"Noise", L"Tone",
                L"Bus", L"Writes", L"Last", L"ORA", L"ORB", L"DDRA", L"DDRB", L"ACR", L"PCR", L"IFR", L"IER",
                L"T1", L"Latch", L"T2", L"Rate", L"Run", L"Fired", L"T1 IRQ", L"IRQ",
                L"Mode", L"Duration", L"Frame", L"Pitch", L"Formants", L"Articulation", L"Amplitude", L"Filter",
                L"Power", L"Sounding", L"Request", L"Recent phonemes",
            };
            SoundBoardView          view;
            DiagnosticsSoundBoard   board   = MakeBoard (true);
            MockDxuiPainter         painter;
            MockDxuiTextRenderer    text;
            CassoTheme              theme   = CassoTheme::MakeSkeuomorphic();
            std::wstring            tip;
            std::set<std::wstring>  tips;
            RECT                    bounds  = {};



            LayOut (view);
            view.SetBoard (board);
            view.Paint (painter, text, theme);
            bounds = view.GetBounds();

            for (int y = bounds.top; y < bounds.bottom; y += 4)
            {
                for (int x = bounds.left; x < bounds.right; x += 4)
                {
                    if (view.TryGetTipAt ({ x, y }, tip))
                    {
                        tips.insert (tip);
                    }
                }
            }

            Assert::IsTrue (tips.size() > 40, L"the parts' tips are found");

            for (const std::wstring & each : tips)
            {
                Assert::IsTrue (each.find (L'\n') == std::wstring::npos, each.c_str());

                for (const wchar_t * label : kLabels)
                {
                    Assert::IsFalse (each.starts_with (std::format (L"{}:", label)) || each.starts_with (std::format (L"{},", label)), each.c_str());
                }
            }
        }


        //  The dot is where the envelope is along its picture's ramps: on a
        //  repeating triangle, the second ramp falls, so a high level there is
        //  near the ramp's start. An envelope stopped at its end moves on
        //  along its flat line at the same pace, a ramp for each 16 steps,
        //  and stays at the right once there.
        TEST_METHOD (TheEnvelopesDotFollowsItAlongTheRamps)
        {
            DiagnosticsSoundPsg  psg;



            psg.envelopeRegister = 0x0E;    // CONT, ATT, ALT: a triangle rising first
            psg.envelopeRamps    = 0;
            psg.envelopeLevel    = 15;
            Assert::AreEqual (0.25f, SoundBoardView::GetEnvelopeMarkX (psg), 0.001f, L"the top of the first ramp");

            psg.envelopeRamps = 1;
            psg.envelopeLevel = 15;
            Assert::AreEqual (0.25f, SoundBoardView::GetEnvelopeMarkX (psg), 0.001f, L"the start of the falling second ramp");

            psg.envelopeLevel = 0;
            Assert::AreEqual (0.50f, SoundBoardView::GetEnvelopeMarkX (psg), 0.001f, L"the bottom of the second ramp");

            psg.envelopeRamps = 5;
            Assert::AreEqual (0.50f, SoundBoardView::GetEnvelopeMarkX (psg), 0.001f, L"ramps after the fourth wrap around the picture");

            psg.envelopeRegister  = 0x0D;   // CONT, ATT, HOLD: rises once, then stays at 15
            psg.envelopeRamps     = 1;
            psg.envelopeLevel     = 15;
            psg.isEnvelopeHeld    = true;
            psg.envelopeHeldSteps = 0;
            Assert::AreEqual (0.25f, SoundBoardView::GetEnvelopeMarkX (psg), 0.001f, L"where the level stopped, at the end of its ramp");

            psg.envelopeHeldSteps = 16;
            Assert::AreEqual (0.50f, SoundBoardView::GetEnvelopeMarkX (psg), 0.001f, L"a ramp's time later, along the flat line");

            psg.envelopeHeldSteps = 64;
            Assert::AreEqual (1.0f, SoundBoardView::GetEnvelopeMarkX (psg), 0.001f, L"at the right, and no further");
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
