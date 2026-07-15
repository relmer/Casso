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

namespace
{
    typedef PrinterAudioSource::Quality  Quality;

    vector<float>  Const (size_t n, float v)
    {
        return vector<float> (n, v);
    }


    // Peak absolute sample in a mixed buffer.
    float  Peak (const vector<float> & buf)
    {
        float  peak = 0.0f;
        for (float s : buf) { peak = (std::max) (peak, std::fabs (s)); }
        return peak;
    }


    // Advance one frame and return the peak of that frame.
    float  Frame (PrinterAudioSource & src, uint32_t n)
    {
        vector<float>  out (n, 0.0f);
        src.GeneratePCM (out.data (), n);
        return Peak (out);
    }
}




TEST_CLASS (PrinterAudioSourceTests)
{
public:

    TEST_METHOD (Defaults_CenteredSilentIdleDraftQuality)
    {
        PrinterAudioSource  src;

        Assert::AreEqual (IDriveAudioSource::kCenterPan, src.PanLeft (),  0.0001f);
        Assert::AreEqual (IDriveAudioSource::kCenterPan, src.PanRight (), 0.0001f);
        Assert::IsFalse  (src.IsPrinting ());
        Assert::IsFalse  (src.IsFeedPlaying ());
        Assert::IsFalse  (src.IsActionPlaying ());

        // Default loop matches the head, which is paced at real draft speed.
        Assert::IsTrue (Quality::Draft == src.CurrentQuality ());

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
        Assert::IsTrue   (src.IsPrinting ());
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
        Assert::IsFalse  (src.IsPrinting ());

        // Progress at a further position WITH ink -> the buzz arms.
        src.PublishReveal (2000, 0, true /* inkActive */);
        Assert::AreEqual (0.5f, Frame (src, 16), 0.0001f);
        Assert::IsTrue   (src.IsPrinting ());
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

        // Switch to NLQ: the NLQ loop now plays.
        src.SetQuality (Quality::NLQ);
        src.PublishReveal (2000, 0);
        Assert::AreEqual (0.9f, Frame (src, 16), 0.0001f);
    }



    TEST_METHOD (CarriageLoop_StopsShortlyAfterHeadStops)
    {
        PrinterAudioSource  src;
        src.SetLoopForTest (Quality::Draft, Const (64, 0.5f), 1000);
        src.SetVolume (1.0f);

        // hold = 0.05 s * 1000 = 50 samples. One 100-sample frame consumes it.
        src.PublishReveal (1000, 0);
        Assert::IsTrue (Frame (src, 100) > 0.0f);   // still playing this frame

        // Same reveal (head stopped): the hold has drained, so now silent.
        Assert::AreEqual (0.0f, Frame (src, 16), 0.0f);
        Assert::IsFalse  (src.IsPrinting ());
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
        Assert::IsFalse (src.IsFeedPlaying ());

        // Column wraps back to the margin: a new line began -> clack fires.
        src.PublishReveal (3000, 10);
        Assert::AreEqual (0.9f, Frame (src, 8), 0.0001f);
        Assert::IsTrue   (src.IsFeedPlaying ());
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
        Assert::IsFalse (src.IsFeedPlaying ());
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

        Assert::IsFalse (src.IsActionPlaying ());
        src.PlayTearOff ();
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
        Assert::AreEqual (0.3f, src.PanLeft (),  0.0001f);
        Assert::AreEqual (0.9f, src.PanRight (), 0.0001f);
    }
};
