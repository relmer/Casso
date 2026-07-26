#include "Pch.h"

#include "Audio/PrinterAudioSource.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterAudioSourceTests
//
//  The printer audio source is pure logic once its grains are injected (the
//  Media Foundation decode is bypassed via the SetLoopForTest / SetLineFeedForTest
//  / SetPageFeedForTest / SetTearForTest seams, constitution Principle II). Drive
//  it with synthetic reveal state + user actions, read back the mixed PCM and the
//  play flags. Distinct grain amplitudes let a test identify which grain played.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (PrinterAudioSourceTests)
{
    using Quality = PrinterAudioSource::Quality;

    static vector<float>  Const (size_t n, float v)
    {
        return vector<float> (n, v);
    }


    // Peak absolute sample in a mixed buffer.
    static float  Peak (const vector<float> & buf)
    {
        float  peak = 0.0f;
        for (float s : buf) { peak = (std::max) (peak, std::fabs (s)); }
        return peak;
    }


    // Advance one frame and return the peak of that frame.
    static float  Frame (PrinterAudioSource & src, uint32_t n)
    {
        vector<float>  out (n, 0.0f);
        src.GeneratePCM (out.data(), n);
        return Peak (out);
    }

public:

    TEST_METHOD (Defaults_CenteredSilentIdleDraftQuality)
    {
        PrinterAudioSource  src;

        Assert::AreEqual (IDriveAudioSource::kCenterPan, src.PanLeft(),  0.0001f);
        Assert::AreEqual (IDriveAudioSource::kCenterPan, src.PanRight(), 0.0001f);
        Assert::IsFalse  (src.IsPrinting());
        Assert::IsFalse  (src.IsFeedPlaying());
        Assert::IsFalse  (src.IsActionPlaying());

        // Default loop matches the head, which is paced at real draft speed.
        Assert::IsTrue (Quality::Draft == src.CurrentQuality());

        // No grains loaded: even with the head advancing, output is silent.
        src.PublishReveal (1000, 0);
        Assert::AreEqual (0.0f, Frame (src, 32), 0.0f);
    }



    TEST_METHOD (CarriageLoop_PlaysWhileHeadAdvances)
    {
        PrinterAudioSource  src;
        src.SetLoopForTest (Quality::Draft, Const (64, 0.5f), 1000);
        src.SetVolume (1.0f);

        src.PublishReveal (1000, 0);   // head moved forward

        Assert::AreEqual (0.5f, Frame (src, 16), 0.0001f);   // loop grain at unity volume
        Assert::IsTrue   (src.IsPrinting());
    }



    TEST_METHOD (CarriageLoop_SilentWhenFeedingWithoutInk)
    {
        PrinterAudioSource  src;
        src.SetLoopForTest (Quality::Draft, Const (64, 0.5f), 1000);
        src.SetVolume (1.0f);

        // Head advancing but NOT laying ink (a form feed / blank line feed):
        // the carriage buzz stays silent even though the reveal progresses.
        src.PublishReveal (1000, 0, false /* inkActive */);
        Assert::AreEqual (0.0f, Frame (src, 16), 0.0f);
        Assert::IsFalse  (src.IsPrinting());

        // Progress at a further position WITH ink -> the buzz arms.
        src.PublishReveal (2000, 0, true /* inkActive */);
        Assert::AreEqual (0.5f, Frame (src, 16), 0.0001f);
        Assert::IsTrue   (src.IsPrinting());
    }



    TEST_METHOD (CarriageLoop_TracksSelectedQuality)
    {
        PrinterAudioSource  src;
        src.SetLoopForTest (Quality::Draft, Const (64, 0.4f), 1000);
        src.SetLoopForTest (Quality::NLQ,   Const (64, 0.9f), 1000);
        src.SetVolume (1.0f);

        // Default draft loop.
        src.PublishReveal (1000, 0);
        Assert::AreEqual (0.4f, Frame (src, 16), 0.0001f);

        // Switch to NLQ: the NLQ loop now plays. (Progress stays inside the
        // same pin band -- crossing a band mid-hold would trigger the
        // line-articulation gap, which is its own test.)
        src.SetQuality (Quality::NLQ);
        src.PublishReveal (1200, 0);
        Assert::AreEqual (0.9f, Frame (src, 16), 0.0001f);
    }



    TEST_METHOD (CarriageLoop_StopsShortlyAfterHeadStops)
    {
        PrinterAudioSource  src;
        src.SetLoopForTest (Quality::Draft, Const (64, 0.5f), 1000);
        src.SetVolume (1.0f);

        // hold = kPrintHoldSec (0.05 s) * 1000 = 50 samples: a short RELEASE, not
        // a sustain -- long enough to bridge one ink=false wrap frame, short
        // enough that a blank stretch mid-pass goes silent (edge-triggered gate).
        src.PublishReveal (1000, 0);
        Assert::IsTrue (Frame (src, 40) > 0.0f);    // still inside the hold

        // Head stopped (same reveal, no re-arm): drain the rest of the hold. Once
        // more than 50 samples have elapsed with no advance, the buzz is silent.
        Frame (src, 60);                             // 40 + 60 = 100 > 50 -> drained
        Assert::AreEqual (0.0f, Frame (src, 16), 0.0f);
        Assert::IsFalse  (src.IsPrinting());
    }



    TEST_METHOD (CarriageLoop_BridgesLineFeedInkGap)
    {
        PrinterAudioSource  src;
        src.SetLoopForTest (Quality::Draft, Const (64, 0.5f), 1000);
        src.SetVolume (1.0f);

        // Print a line (ink) to arm the buzz.
        src.PublishReveal (1000, 1200, true /* inkActive */);
        Assert::AreEqual (0.5f, Frame (src, 16), 0.0001f);

        // A mid-pass ink=false frame (frame quantization while sweeping the SAME
        // band): the buzz must stay alive under it (it does not re-arm, but the
        // 50-sample hold outlasts this 16-sample frame) so one line's buzz is
        // not chopped by sampling. BETWEEN lines the band-step articulation gap
        // deliberately cuts it -- see LineBoundary_ArticulatesEachPass.
        src.PublishReveal (1100, 10, false /* inkActive */);
        Assert::AreEqual (0.5f, Frame (src, 16), 0.0001f);
        Assert::IsTrue   (src.IsPrinting());

        // Next inked line re-arms as usual.
        src.PublishReveal (1200, 20, true /* inkActive */);
        Assert::AreEqual (0.5f, Frame (src, 16), 0.0001f);
    }



    TEST_METHOD (BorderOnlyLine_GoesSilentOverTheBlankMiddle)
    {
        PrinterAudioSource  src;
        src.SetLoopForTest (Quality::Draft, Const (64, 0.5f), 1000);
        src.SetVolume (1.0f);

        // A sign line that is just a thin border at each margin: strike, a long
        // blank middle, strike. The head keeps MOVING the whole way, but the gate
        // is edge-triggered on ink -- so the buzz must release over the blank
        // middle (~0.3 s of sweep, far past the 50-sample hold), not smear across
        // it sounding like a full line of print.
        src.PublishReveal (1040, 40, true /* left border strike */);
        Assert::IsTrue (Frame (src, 16) > 0.0f);

        src.PublishReveal (1400, 400, false /* crossing the blank middle */);
        Frame (src, 100);                            // 16 + 100 > 50 -> hold drained
        src.PublishReveal (1800, 800, false /* still blank, still moving */);
        Assert::AreEqual (0.0f, Frame (src, 16), 0.0f);
        Assert::IsFalse  (src.IsPrinting());

        // Right border: ink returns under the head -> the buzz re-arms.
        src.PublishReveal (2240, 1240, true /* right border strike */);
        Assert::AreEqual (0.5f, Frame (src, 16), 0.0001f);
        Assert::IsTrue   (src.IsPrinting());
    }



    TEST_METHOD (LineBoundary_ArticulatesEachPassAsItsOwnBurst)
    {
        PrinterAudioSource  src;
        src.SetLoopForTest (Quality::Draft, Const (64, 0.5f), 1000);
        src.SetVolume (1.0f);

        // Line 1: an inked sweep in the first pin band arms the buzz.
        src.PublishReveal (1000, 1000, true);
        Assert::IsTrue (Frame (src, 16) > 0.0f);

        // The pass completes and the reveal steps into the next band while the
        // hold is still live. The real machine stops printing for the line feed
        // between passes, so the buzz must CUT for the articulation gap -- this
        // is what makes a 19-line catalog sound like 19 bursts, not one zip.
        src.PublishReveal (1280 + 40, 40, true);
        Assert::AreEqual (0.0f, Frame (src, 16), 0.0f);   // inside the 35-sample gap
        Assert::IsFalse  (src.IsPrinting());

        // ...and re-arm BY ITSELF once the gap expires (the new band is inked),
        // even with no further publish -- a coarse frame must not eat line 2.
        Frame (src, 40);                                  // 16 + 40 > 35: gap drained
        Assert::AreEqual (0.5f, Frame (src, 16), 0.0001f);
        Assert::IsTrue   (src.IsPrinting());
    }



    TEST_METHOD (FormFeed_CutsCarriageBuzz)
    {
        PrinterAudioSource  src;
        src.SetLoopForTest     (Quality::Draft, Const (64, 0.5f), 1000);
        src.SetPageFeedForTest (0, Const (8, 0.9f));
        src.SetVolume (1.0f);

        // Arm the buzz, then request a form feed. The action latch cuts the
        // carriage hold so the page feed plays clean under its own grain
        // rather than trailing the buzz across the advance.
        src.PublishReveal (1000, 1200, true /* inkActive */);
        Assert::IsTrue (Frame (src, 16) > 0.0f);

        src.PlayFormFeed (0.10f /* short */);
        Assert::AreEqual (0.9f, Frame (src, 8), 0.0001f);   // the page-feed grain, not the 0.5 buzz
        Assert::IsFalse  (src.IsPrinting());                // carriage hold was cut
    }



    TEST_METHOD (LineFeed_FiresOnColumnWrap)
    {
        PrinterAudioSource  src;
        src.SetLoopForTest    (Quality::Draft, Const (64, 0.0f), 1000);   // silent loop isolates the feed
        src.SetLineFeedForTest (0, Const (8, 0.9f));
        src.SetVolume (1.0f);

        // Establish the previous column near the right margin.
        src.PublishReveal (2000, 1200);
        Frame (src, 8);
        Assert::IsFalse (src.IsFeedPlaying());

        // Column wraps back to the margin: a new line began -> clack fires.
        src.PublishReveal (3000, 10);
        Assert::AreEqual (0.9f, Frame (src, 8), 0.0001f);
        Assert::IsTrue   (src.IsFeedPlaying());
    }



    TEST_METHOD (LineFeed_RotatesThroughVariants)
    {
        PrinterAudioSource  src;
        src.SetLoopForTest     (Quality::Draft, Const (64, 0.0f), 1000);
        src.SetLineFeedForTest (0, Const (4, 0.2f));
        src.SetLineFeedForTest (1, Const (4, 0.5f));
        src.SetLineFeedForTest (2, Const (4, 0.8f));
        src.SetVolume (1.0f);

        float  peaks[3] = { 0.0f, 0.0f, 0.0f };

        for (int line = 0; line < 3; line++)
        {
            // Prime the right margin, then wrap. Space the wraps past the
            // line-feed throttle (0.09 s * 1000 = 90 samples) with a wide frame.
            src.PublishReveal (1000 + line * 10, 1200);
            Frame (src, 200);
            src.PublishReveal (2000 + line * 10, 10);
            peaks[line] = Frame (src, 4);   // exactly the 4-sample grain
        }

        // Each successive wrap plays the next recorded variant.
        Assert::AreEqual (0.2f, peaks[0], 0.0001f);
        Assert::AreEqual (0.5f, peaks[1], 0.0001f);
        Assert::AreEqual (0.8f, peaks[2], 0.0001f);
    }



    TEST_METHOD (LineFeed_NotFiredOnForwardSweep)
    {
        PrinterAudioSource  src;
        src.SetLoopForTest     (Quality::Draft, Const (64, 0.0f), 1000);
        src.SetLineFeedForTest (0, Const (8, 0.9f));
        src.SetVolume (1.0f);

        src.PublishReveal (2000, 100);
        Frame (src, 8);

        src.PublishReveal (2100, 300);   // column advancing, no wrap
        Frame (src, 8);
        Assert::IsFalse (src.IsFeedPlaying());
    }



    TEST_METHOD (FormFeed_PicksGrainByUnusedFraction)
    {
        auto  peakFor = [] (float unused, float shortAmp, float medAmp, float longAmp) -> float
        {
            PrinterAudioSource  src;
            src.SetPageFeedForTest (0, Const (8, shortAmp));
            src.SetPageFeedForTest (1, Const (8, medAmp));
            src.SetPageFeedForTest (2, Const (8, longAmp));
            src.SetVolume (1.0f);

            src.PlayFormFeed (unused);
            return Frame (src, 8);
        };

        // Less of the page unused -> shorter feed grain, and vice versa.
        Assert::AreEqual (0.3f, peakFor (0.10f, 0.3f, 0.6f, 0.9f), 0.0001f);   // short
        Assert::AreEqual (0.6f, peakFor (0.50f, 0.3f, 0.6f, 0.9f), 0.0001f);   // medium
        Assert::AreEqual (0.9f, peakFor (0.90f, 0.3f, 0.6f, 0.9f), 0.0001f);   // long
    }



    TEST_METHOD (TearOff_PlaysARandomTear)
    {
        PrinterAudioSource  src;
        for (int i = 0; i < PrinterAudioSource::kNumTears; i++)
        {
            src.SetTearForTest (i, Const (8, 0.7f));
        }
        src.SetVolume (1.0f);

        Assert::IsFalse (src.IsActionPlaying());
        src.PlayTearOff();
        Assert::AreEqual (0.7f, Frame (src, 8), 0.0001f);
    }



    TEST_METHOD (Muted_IsSilent)
    {
        PrinterAudioSource  src;
        src.SetLoopForTest (Quality::Draft, Const (64, 0.5f), 1000);
        src.SetVolume (1.0f);
        src.SetMuted (true);

        src.PublishReveal (1000, 0);
        Assert::AreEqual (0.0f, Frame (src, 16), 0.0f);
    }



    TEST_METHOD (Volume_ScalesOutput)
    {
        PrinterAudioSource  src;
        src.SetLoopForTest (Quality::Draft, Const (64, 1.0f), 1000);

        src.SetVolume (0.25f);
        src.PublishReveal (1000, 0);
        Assert::AreEqual (0.25f, Frame (src, 16), 0.0001f);
    }



    TEST_METHOD (SetPan_StoresCoefficients)
    {
        PrinterAudioSource  src;
        src.SetPan (0.3f, 0.9f);
        Assert::AreEqual (0.3f, src.PanLeft(),  0.0001f);
        Assert::AreEqual (0.9f, src.PanRight(), 0.0001f);
    }
};
