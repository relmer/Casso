#include "Pch.h"

#include "CppUnitTest.h"
#include "Ui/Debugger/Panes/SoundBoardLayout.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SoundBoardLayoutTests
//
//  The Mockingboard view's arithmetic.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (SoundBoardLayoutTests)
    {
    public:

        TEST_METHOD (ANoteHasItsNameOctaveAndCents)
        {
            Assert::AreEqual (std::wstring (L"A4"),      SoundBoardLayout::GetNoteText (440.0));
            Assert::AreEqual (std::wstring (L"C4"),      SoundBoardLayout::GetNoteText (261.6256));
            Assert::AreEqual (std::wstring (L"A#4"),     SoundBoardLayout::GetNoteText (466.1638));
            Assert::AreEqual (std::wstring (L"A4 +20"),  SoundBoardLayout::GetNoteText (445.1));
            Assert::AreEqual (std::wstring (L"A4 -20"),  SoundBoardLayout::GetNoteText (434.95));
            Assert::AreEqual (std::wstring (L""),        SoundBoardLayout::GetNoteText (0.0));
            Assert::AreEqual (std::wstring (L""),        SoundBoardLayout::GetNoteText (100000.0), L"above the MIDI range");
        }


        TEST_METHOD (AFrequencyIsInHertzOrKilohertz)
        {
            Assert::AreEqual (std::wstring (L"440.0 Hz"), SoundBoardLayout::GetFrequencyText (440.0));
            Assert::AreEqual (std::wstring (L"1.25 kHz"), SoundBoardLayout::GetFrequencyText (1250.0));
        }


        static void AssertEnvelope (Byte shape, std::vector<std::pair<float, float>> expected)
        {
            std::vector<SoundBoardLayout::Point>  points = SoundBoardLayout::GetEnvelopePoints (shape);
            size_t                                i      = 0;



            Assert::AreEqual (expected.size(), points.size());

            for (i = 0; i < expected.size(); i++)
            {
                Assert::AreEqual (expected[i].first,  points[i].x, 0.001f);
                Assert::AreEqual (expected[i].second, points[i].y, 0.001f);
            }
        }


        //  R13's values, from the datasheet's table of the sixteen.
        TEST_METHOD (TheEnvelopePictureFollowsR13)
        {
            AssertEnvelope (0x00, { { 0.0f, 1.0f }, { 0.25f, 0.0f }, { 0.25f, 0.0f }, { 1.0f, 0.0f } });    // fall, then silent
            AssertEnvelope (0x04, { { 0.0f, 0.0f }, { 0.25f, 1.0f }, { 0.25f, 0.0f }, { 1.0f, 0.0f } });    // rise, then silent
            AssertEnvelope (0x09, { { 0.0f, 1.0f }, { 0.25f, 0.0f }, { 0.25f, 0.0f }, { 1.0f, 0.0f } });    // fall, hold low
            AssertEnvelope (0x0B, { { 0.0f, 1.0f }, { 0.25f, 0.0f }, { 0.25f, 1.0f }, { 1.0f, 1.0f } });    // fall, hold high
            AssertEnvelope (0x0D, { { 0.0f, 0.0f }, { 0.25f, 1.0f }, { 0.25f, 1.0f }, { 1.0f, 1.0f } });    // rise, hold high
            AssertEnvelope (0x0F, { { 0.0f, 0.0f }, { 0.25f, 1.0f }, { 0.25f, 0.0f }, { 1.0f, 0.0f } });    // rise, hold low
            AssertEnvelope (0x0E, { { 0.0f, 0.0f }, { 0.25f, 1.0f }, { 0.5f, 0.0f }, { 0.75f, 1.0f }, { 1.0f, 0.0f } });      // triangle
            AssertEnvelope (0x08, { { 0.0f, 1.0f }, { 0.25f, 0.0f },
                                    { 0.25f, 1.0f }, { 0.5f, 0.0f },
                                    { 0.5f, 1.0f },  { 0.75f, 0.0f },
                                    { 0.75f, 1.0f }, { 1.0f, 0.0f } });                                     // falling saw
        }


        TEST_METHOD (TheScopeWindowIsAboutFourCycles)
        {
            constexpr uint32_t  kRate = 48000;



            Assert::AreEqual ((size_t) 400, SoundBoardLayout::GetScopeWindow (480.0, kRate));
            Assert::AreEqual (SoundBoardLayout::kScopeWindowMax, SoundBoardLayout::GetScopeWindow (50.0, kRate), L"a low note");
            Assert::AreEqual (SoundBoardLayout::kScopeWindowMin, SoundBoardLayout::GetScopeWindow (20000.0, kRate), L"a high note");
            Assert::AreEqual (SoundBoardLayout::kScopeWindowMax, SoundBoardLayout::GetScopeWindow (0.0, kRate));
        }


        TEST_METHOD (TheTriggerIsTheFirstRiseWithAFullWindow)
        {
            std::vector<float>  rising = { 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f };
            std::vector<float>  flat (8, 0.5f);



            Assert::AreEqual ((size_t) 3, SoundBoardLayout::FindTrigger (rising, 4));
            Assert::AreEqual ((size_t) 2, SoundBoardLayout::FindTrigger (rising, 6), L"none in time: the last window");
            Assert::AreEqual ((size_t) 4, SoundBoardLayout::FindTrigger (flat, 4));
            Assert::AreEqual ((size_t) 0, SoundBoardLayout::FindTrigger ({}, 4));
        }


        TEST_METHOD (AScopeColumnHasItsSamplesRange)
        {
            std::vector<float>  samples = { 0.0f, 1.0f, 0.5f, 0.5f };
            auto                columns = SoundBoardLayout::GetScopeColumns (samples, 0, 4, 2, 0.0f, 1.0f);



            Assert::AreEqual ((size_t) 2, columns.size());
            Assert::AreEqual (0.0f, columns[0].low);
            Assert::AreEqual (1.0f, columns[0].high);
            Assert::AreEqual (0.5f, columns[1].low);
            Assert::AreEqual (0.5f, columns[1].high);
            Assert::IsTrue   (SoundBoardLayout::GetScopeColumns (samples, 9, 4, 2, 0.0f, 1.0f).empty(), L"past the samples");
        }


        TEST_METHOD (TheFormantAxisIsLogarithmic)
        {
            double  middle = std::sqrt (SoundBoardLayout::kFormantMinHz * SoundBoardLayout::kFormantMaxHz);



            Assert::AreEqual (0.0f, SoundBoardLayout::GetFormantX (SoundBoardLayout::kFormantMinHz), 0.001f);
            Assert::AreEqual (0.5f, SoundBoardLayout::GetFormantX (middle), 0.001f);
            Assert::AreEqual (1.0f, SoundBoardLayout::GetFormantX (SoundBoardLayout::kFormantMaxHz), 0.001f);
        }


        TEST_METHOD (TheFormantCurvePeaksAtEachFormant)
        {
            constexpr int  kColumns = 201;
            auto           curve    = SoundBoardLayout::GetFormantCurve ({ 500.0, 1500.0, 3000.0 }, kColumns);
            auto           silent   = SoundBoardLayout::GetFormantCurve ({ 0.0, 0.0, 0.0 }, kColumns);
            int            atF1     = (int) std::lround (SoundBoardLayout::GetFormantX (500.0) * (kColumns - 1));
            int            atF2     = (int) std::lround (SoundBoardLayout::GetFormantX (1500.0) * (kColumns - 1));



            Assert::AreEqual (1.0f,  curve[(size_t) atF1], 0.01f);
            Assert::AreEqual (0.78f, curve[(size_t) atF2], 0.01f);
            Assert::IsTrue   (curve[(size_t) atF2] > curve[(size_t) (atF2 + 8)], L"it falls away from the peak");
            Assert::AreEqual (0.0f, *std::max_element (silent.begin(), silent.end()));
        }


        //  A strip of 300: a second is 360 wide, so 0.25 s is 90 and 0.05 s is 18.
        TEST_METHOD (ThePhonemeStripIsNewestAtTheRightInProportion)
        {
            auto  placed = SoundBoardLayout::PlaceStrip ({ 0.05f, 0.25f }, { 10.0f, 10.0f }, 300.0f, 2.0f);



            Assert::AreEqual ((size_t) 2, placed.size());
            Assert::AreEqual ((size_t) 1, placed[1].index);
            Assert::AreEqual (90.0f,  placed[1].width, 0.01f);
            Assert::AreEqual (210.0f, placed[1].x,     0.01f);
            Assert::AreEqual (18.0f,  placed[0].width, 0.01f);
            Assert::AreEqual (190.0f, placed[0].x,     0.01f);
        }


        TEST_METHOD (ThePhonemeStripKeepsToItsLeastWidthAndDropsTheOldest)
        {
            auto  placed = SoundBoardLayout::PlaceStrip ({ 0.0f, 0.0f, 0.0f }, { 40.0f, 40.0f, 40.0f }, 90.0f, 2.0f);



            Assert::AreEqual ((size_t) 2, placed.size(), L"the third does not fit");
            Assert::AreEqual ((size_t) 1, placed[0].index);
            Assert::AreEqual ((size_t) 2, placed[1].index);
            Assert::AreEqual (40.0f, placed[1].width, 0.01f);
        }


        TEST_METHOD (TheColumnsFollowTheWidth)
        {
            Assert::AreEqual (1, SoundBoardLayout::GetColumnCount (300,  460, 10), L"always one");
            Assert::AreEqual (1, SoundBoardLayout::GetColumnCount (900,  460, 10));
            Assert::AreEqual (2, SoundBoardLayout::GetColumnCount (930,  460, 10));
            Assert::AreEqual (3, SoundBoardLayout::GetColumnCount (9000, 460, 10), L"at most three");
        }


        TEST_METHOD (ACardGoesInTheShortestColumn)
        {
            int   height = 0;
            auto  places = SoundBoardLayout::PlaceCards ({ 100, 50, 30 }, 2, 210, 10, height);



            Assert::AreEqual ((size_t) 3, places.size());
            Assert::AreEqual (0L,   places[0].left);
            Assert::AreEqual (110L, places[1].left);
            Assert::AreEqual (110L, places[2].left, L"under the shorter one");
            Assert::AreEqual (60L,  places[2].top);
            Assert::AreEqual (100,  height);
        }
    };
}
