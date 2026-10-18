#include "Pch.h"

#include "Devices/Tape/TapeSignalDecoder.h"
#include "TapeTestEncoder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TapeSignalDecoderTests
//
//  A 1 kHz tone is 1,000 half-cycles of 500 us; at 44.1 kHz each lasts 22.05
//  samples. The comparator trips at a fixed fraction of the peak, so every
//  edge is delayed by the same amount and the spacing between edges is what
//  these tests hold to.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TapeSignalDecoderTests)
{
public:

    static constexpr size_t  kHalfCycles       = 1000;
    static constexpr double  kHalfUs           = 500.0;
    static constexpr double  kHalfSamples      = 44100.0 * kHalfUs / 1000000.0;
    static constexpr double  kSpacingTolerance = 0.1;
    static constexpr size_t  kSettleEdges      = 20;


    static void MakeTone (TapeEncodeOptions & options, TapeSignal & signal)
    {
        std::vector<double>  halves (kHalfCycles, kHalfUs);
        TapeAudio            audio;



        TapeTestEncoder::Render (halves, options, audio);
        TapeSignalDecoder::Decode (audio, signal);
    }


    static void CheckSpacing (const TapeSignal & signal, double tolerance)
    {
        Assert::IsTrue (signal.transitions.size() > kSettleEdges * 2);

        for (size_t i = kSettleEdges; i + 1 < signal.transitions.size(); i++)
        {
            double  spacing = signal.transitions[i + 1] - signal.transitions[i];

            Assert::AreEqual (kHalfSamples, spacing, tolerance);
        }
    }


    TEST_METHOD (CleanToneGivesOneEdgePerHalfCycle)
    {
        TapeEncodeOptions  options;
        TapeSignal         signal;



        MakeTone (options, signal);

        Assert::AreEqual (kHalfCycles, signal.transitions.size());
        CheckSpacing (signal, kSpacingTolerance);
    }


    TEST_METHOD (DcOffsetAddsAndDropsNothing)
    {
        constexpr double  kOffsets[] = { 0.4, -0.4 };



        for (double offset : kOffsets)
        {
            TapeEncodeOptions  options;
            TapeSignal         signal;



            options.amplitude = 0.5;
            options.dcOffset  = offset;
            MakeTone (options, signal);

            Assert::AreEqual (kHalfCycles, signal.transitions.size());
            CheckSpacing (signal, kSpacingTolerance);
        }
    }


    TEST_METHOD (RisingGainDecodes)
    {
        TapeEncodeOptions  options;
        TapeSignal         signal;



        options.amplitude = 0.05;
        options.gainEnd   = 20.0;
        MakeTone (options, signal);

        Assert::AreEqual (kHalfCycles, signal.transitions.size());
        CheckSpacing (signal, kSpacingTolerance);
    }


    TEST_METHOD (NoiseDoesNotChatter)
    {
        constexpr double   kNoisyTolerance = kHalfSamples * 0.25;
        TapeEncodeOptions  options;
        TapeSignal         signal;



        // No silent tails: hiss with nothing to ride on chatters on any
        // comparator, real ones included, and is not what this measures.
        options.noiseSnrDb  = 15.0;
        options.tailSeconds = 0.0;
        MakeTone (options, signal);

        Assert::AreEqual (kHalfCycles, signal.transitions.size());
        CheckSpacing (signal, kNoisyTolerance);
    }


    TEST_METHOD (SilenceGivesNoTransitions)
    {
        TapeAudio   audio;
        TapeSignal  signal;



        audio.sampleRate = 44100;
        audio.samples.assign (44100, 0.0f);
        TapeSignalDecoder::Decode (audio, signal);

        Assert::IsTrue   (signal.transitions.empty());
        Assert::AreEqual (uint64_t (44100), signal.lengthSamples);
    }


    TEST_METHOD (LowHissGivesNoTransitions)
    {
        TapeEncodeOptions    options;
        TapeAudio            audio;
        TapeSignal           signal;
        std::vector<double>  none;



        options.amplitude  = 0.005;
        options.noiseSnrDb = 0.001;
        TapeTestEncoder::Render (none, options, audio);
        TapeSignalDecoder::Decode (audio, signal);

        Assert::IsFalse (audio.samples.empty());
        Assert::IsTrue  (signal.transitions.empty());
    }


    TEST_METHOD (InvertedPolarityGivesSameEdgesOppositeLevel)
    {
        constexpr double   kEdgeTolerance = 0.01;
        TapeEncodeOptions  normal;
        TapeEncodeOptions  inverted;
        TapeSignal         normalSignal;
        TapeSignal         invertedSignal;



        inverted.isInverted = true;
        MakeTone (normal,   normalSignal);
        MakeTone (inverted, invertedSignal);

        Assert::AreEqual (normalSignal.transitions.size(), invertedSignal.transitions.size());
        Assert::AreNotEqual (normalSignal.initialLevel, invertedSignal.initialLevel);

        for (size_t i = 0; i < normalSignal.transitions.size(); i++)
        {
            Assert::AreEqual (normalSignal.transitions[i], invertedSignal.transitions[i], kEdgeTolerance);
        }
    }


    TEST_METHOD (EncodedRecordGivesOneEdgePerHalfCycle)
    {
        std::vector<Byte>    data = { 0x00, 0xFF, 0xA5, 0x5A };
        std::vector<double>  halves;
        TapeEncodeOptions    options;
        TapeAudio            audio;
        TapeSignal           signal;



        TapeTestEncoder::AppendRecord (halves, data, options);
        TapeTestEncoder::Render (halves, options, audio);
        TapeSignalDecoder::Decode (audio, signal);

        Assert::AreEqual (halves.size(), signal.transitions.size());
    }
};
