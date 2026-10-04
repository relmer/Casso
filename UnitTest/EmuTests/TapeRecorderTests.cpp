#include "Pch.h"

#include "Devices/Tape/TapeRecorder.h"
#include "Devices/Tape/WavCodec.h"
#include "FakeDiskFileIo.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecorderTests
//
//  Clock and sample rate are equal, so one bus cycle is one sample.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TapeRecorderTests)
{
public:

    static constexpr double    kClock = 44100.0;
    static constexpr uint32_t  kRate  = 44100;


    static void MakeTape (TapeAudio & tape, size_t length, float fill)
    {
        tape.sampleRate = kRate;
        tape.samples.assign (length, fill);
    }


    TEST_METHOD (TogglesBecomeASquareWaveAtTheirCycles)
    {
        TapeAudio         tape;
        RecordingCapture  capture;



        MakeTape (tape, 100, 0.0f);
        capture.startSample  = 10.0;
        capture.startCycle   = 1000;
        capture.endCycle     = 1050;
        capture.toggleCycles = { 1010, 1020, 1040 };

        TapeRecorder::Splice (tape, capture, kClock);

        Assert::AreEqual (-TapeRecorder::kLevel, tape.samples[10 + 5],  L"low until the first toggle");
        Assert::AreEqual ( TapeRecorder::kLevel, tape.samples[10 + 15], L"high after it");
        Assert::AreEqual (-TapeRecorder::kLevel, tape.samples[10 + 30], L"low after the second");
        Assert::AreEqual ( TapeRecorder::kLevel, tape.samples[10 + 45], L"high after the third");
    }


    TEST_METHOD (SamplesBeforeTheRecordingPointAreUntouched)
    {
        TapeAudio         tape;
        RecordingCapture  capture;



        MakeTape (tape, 100, 0.25f);
        capture.startSample = 40.0;
        capture.startCycle  = 0;
        capture.endCycle    = 20;

        TapeRecorder::Splice (tape, capture, kClock);

        for (size_t i = 0; i < 40; i++)
        {
            Assert::AreEqual (0.25f, tape.samples[i]);
        }

        Assert::AreEqual (0.25f, tape.samples[60], L"and after the recording ends, too");
    }


    TEST_METHOD (RecordingPastTheEndExtendsTheTape)
    {
        TapeAudio         tape;
        RecordingCapture  capture;



        MakeTape (tape, 50, 0.0f);
        capture.startSample = 40.0;
        capture.startCycle  = 0;
        capture.endCycle    = 30;

        TapeRecorder::Splice (tape, capture, kClock);

        Assert::AreEqual (size_t (70), tape.samples.size());
    }


    //  A held output is not recorded: past the longest Apple half-cycle with
    //  no toggle, the tape settles to faint hiss about the center line, as an
    //  AC-coupled recorder's would -- before the first toggle and after the
    //  last. Readers that end a record at the first cycle out of range need
    //  it, CiderPress II among them.
    TEST_METHOD (HeldOutputSettlesToHiss)
    {
        TapeAudio         tape;
        RecordingCapture  capture;
        size_t            settled = (size_t) (TapeRecorder::kSettleSeconds * kRate) + 2;
        bool              crosses = false;



        tape.sampleRate      = kRate;
        capture.startSample  = 0.0;
        capture.startCycle   = 0;
        capture.endCycle     = (uint64_t) (0.1 * kClock);
        capture.toggleCycles = { (uint64_t) (0.01 * kClock), (uint64_t) (0.0105 * kClock) };

        TapeRecorder::Splice (tape, capture, kClock);

        // Before the first toggle, and long after the last: hiss, not a level.
        Assert::IsTrue (std::abs (tape.samples[settled]) <= TapeRecorder::kHissLevel);
        Assert::IsTrue (std::abs (tape.samples.back()) <= TapeRecorder::kHissLevel);

        // Between the toggles: the level itself.
        Assert::AreEqual (TapeRecorder::kLevel, tape.samples[(size_t) (0.0102 * kRate)]);

        // And the hiss does cross the center line.
        for (size_t i = (size_t) (0.05 * kRate); i + 1 < tape.samples.size(); i++)
        {
            crosses = crosses || (tape.samples[i] < 0.0f) != (tape.samples[i + 1] < 0.0f);
        }

        Assert::IsTrue (crosses);
    }


    TEST_METHOD (CommitRewritesTheWavAtomicallyAndDecodesIt)
    {
        FakeDiskFileIo    io;
        TapeAudio         blank;
        std::vector<Byte> bytes;
        RecordingCapture  capture;
        TapeImage         image;
        std::string       error;
        TapeAudio         written;
        HRESULT           hr = S_OK;



        blank.sampleRate = kRate;
        WavCodec::Encode (blank, bytes);
        io.files["C:\\Tapes\\new.wav"] = bytes;

        capture.startCycle   = 0;
        capture.endCycle     = 4410;
        capture.toggleCycles = { 100, 200, 300, 400 };

        hr = TapeRecorder::Commit (io, "C:\\Tapes\\new.wav", capture, kClock, image, error);

        Assert::AreEqual (S_OK, hr);
        Assert::AreEqual (1, io.replaceCount);
        Assert::IsTrue   (image.isWritable);
        Assert::AreEqual (std::string ("C:\\Tapes\\new.wav"), image.path);
        // The four toggles, and one more where the level held after the last
        // settles back to the center line, as on any real recording.
        Assert::AreEqual (size_t (5), image.signal.transitions.size());

        hr = WavCodec::Decode (io.files["C:\\Tapes\\new.wav"], written, error);
        Assert::AreEqual (S_OK, hr);
        Assert::AreEqual (size_t (4410), written.samples.size());
    }


    TEST_METHOD (FailedWriteLeavesTheTapeAsItWas)
    {
        FakeDiskFileIo    io;
        TapeAudio         blank;
        std::vector<Byte> bytes;
        RecordingCapture  capture;
        TapeImage         image;
        std::string       error;
        HRESULT           hr = S_OK;



        blank.sampleRate = kRate;
        WavCodec::Encode (blank, bytes);
        io.files["C:\\Tapes\\new.wav"] = bytes;
        io.failNextReplace = true;

        capture.endCycle = 100;
        hr = TapeRecorder::Commit (io, "C:\\Tapes\\new.wav", capture, kClock, image, error);

        Assert::IsTrue  (FAILED (hr));
        Assert::IsFalse (error.empty());
        Assert::IsTrue  (io.files["C:\\Tapes\\new.wav"] == bytes);
    }
};
