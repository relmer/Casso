#include "Pch.h"

#include "Audio/PrinterAudioSource.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterAudioSourceTests
//
//  The printer audio source is pure logic once its grains are injected (the
//  Media Foundation decode is bypassed via SetGrainsForTest / SetFullBuffer
//  ForTest, constitution Principle II). Drive it with synthetic reveal state,
//  read back the mixed PCM + the play flags.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
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
}




TEST_CLASS (PrinterAudioSourceTests)
{
public:

    TEST_METHOD (Defaults_CenteredSilentIdle)
    {
        PrinterAudioSource  src;

        Assert::AreEqual (IDriveAudioSource::kCenterPan, src.PanLeft (),  0.0001f);
        Assert::AreEqual (IDriveAudioSource::kCenterPan, src.PanRight (), 0.0001f);
        Assert::IsFalse  (src.IsPrinting ());
        Assert::IsFalse  (src.IsFeedPlaying ());

        // No grains loaded: even with the head advancing, output is silent.
        src.PublishReveal (1000, 0);
        vector<float>  out (32, 0.0f);
        src.GeneratePCM (out.data (), (uint32_t) out.size ());
        Assert::AreEqual (0.0f, Peak (out), 0.0f);
    }



    TEST_METHOD (SliceGrains_FromFullBuffer)
    {
        PrinterAudioSource  src;

        // 20 s at 1 kHz. Print loop = 8.4 s -> end (11600); feed = 3.7..3.9 (200).
        src.SetFullBufferForTest (Const (20000, 0.25f), 1000);

        Assert::AreEqual ((size_t) 11600, src.PrintLoopSamples ());
        Assert::AreEqual ((size_t) 200,   src.FeedSamples ());
    }



    TEST_METHOD (CarriageLoop_PlaysWhileHeadAdvances)
    {
        PrinterAudioSource  src;
        src.SetGrainsForTest (Const (64, 0.5f), Const (8, 0.9f), 1000);
        src.SetVolume (1.0f);

        src.PublishReveal (1000, 0);   // head moved forward
        vector<float>  out (16, 0.0f);
        src.GeneratePCM (out.data (), (uint32_t) out.size ());

        Assert::IsTrue   (src.IsPrinting ());
        Assert::AreEqual (0.5f, Peak (out), 0.0001f);   // loop grain at unity volume
    }



    TEST_METHOD (CarriageLoop_StopsShortlyAfterHeadStops)
    {
        PrinterAudioSource  src;
        src.SetGrainsForTest (Const (64, 0.5f), Const (8, 0.9f), 1000);
        src.SetVolume (1.0f);

        // hold = 0.05 s * 1000 = 50 samples. One 100-sample frame consumes it.
        src.PublishReveal (1000, 0);
        vector<float>  first (100, 0.0f);
        src.GeneratePCM (first.data (), (uint32_t) first.size ());
        Assert::IsTrue (Peak (first) > 0.0f);           // still playing this frame

        // Same reveal (head stopped): the hold has drained, so now silent.
        vector<float>  second (16, 0.0f);
        src.GeneratePCM (second.data (), (uint32_t) second.size ());
        Assert::IsFalse  (src.IsPrinting ());
        Assert::AreEqual (0.0f, Peak (second), 0.0f);
    }



    TEST_METHOD (LineFeed_FiresOnColumnWrap)
    {
        PrinterAudioSource  src;
        src.SetGrainsForTest (Const (64, 0.0f), Const (8, 0.9f), 1000);   // silent loop isolates the feed
        src.SetVolume (1.0f);

        // Establish the previous column near the right margin.
        src.PublishReveal (2000, 1200);
        vector<float>  warm (8, 0.0f);
        src.GeneratePCM (warm.data (), (uint32_t) warm.size ());
        Assert::IsFalse (src.IsFeedPlaying ());

        // Column wraps back to the margin: a new line began -> clack fires.
        src.PublishReveal (3000, 10);
        vector<float>  out (8, 0.0f);
        src.GeneratePCM (out.data (), (uint32_t) out.size ());
        Assert::IsTrue   (src.IsFeedPlaying ());
        Assert::AreEqual (0.9f, Peak (out), 0.0001f);
    }



    TEST_METHOD (LineFeed_NotFiredOnForwardSweep)
    {
        PrinterAudioSource  src;
        src.SetGrainsForTest (Const (64, 0.0f), Const (8, 0.9f), 1000);
        src.SetVolume (1.0f);

        src.PublishReveal (2000, 100);
        vector<float>  a (8, 0.0f);
        src.GeneratePCM (a.data (), (uint32_t) a.size ());

        src.PublishReveal (2100, 300);   // column advancing, no wrap
        vector<float>  b (8, 0.0f);
        src.GeneratePCM (b.data (), (uint32_t) b.size ());
        Assert::IsFalse (src.IsFeedPlaying ());
    }



    TEST_METHOD (Muted_IsSilent)
    {
        PrinterAudioSource  src;
        src.SetGrainsForTest (Const (64, 0.5f), Const (8, 0.9f), 1000);
        src.SetVolume (1.0f);
        src.SetMuted (true);

        src.PublishReveal (1000, 0);
        vector<float>  out (16, 0.0f);
        src.GeneratePCM (out.data (), (uint32_t) out.size ());
        Assert::AreEqual (0.0f, Peak (out), 0.0f);
    }



    TEST_METHOD (Volume_ScalesOutput)
    {
        PrinterAudioSource  src;
        src.SetGrainsForTest (Const (64, 1.0f), Const (8, 0.0f), 1000);

        src.SetVolume (0.25f);
        src.PublishReveal (1000, 0);
        vector<float>  out (16, 0.0f);
        src.GeneratePCM (out.data (), (uint32_t) out.size ());
        Assert::AreEqual (0.25f, Peak (out), 0.0001f);
    }



    TEST_METHOD (SetPan_StoresCoefficients)
    {
        PrinterAudioSource  src;
        src.SetPan (0.3f, 0.9f);
        Assert::AreEqual (0.3f, src.PanLeft (),  0.0001f);
        Assert::AreEqual (0.9f, src.PanRight (), 0.0001f);
    }
};
