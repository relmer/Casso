#include "Pch.h"

#include "Capture/ScreenshotMetadata.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenshotMetadataTests
//
//  What a screenshot says about itself, per mode.
//
//  The entry set is a PUBLISHED CONTRACT -- files written under it leave the
//  machine and get attached to issues, so keywords cannot be renamed or
//  repurposed later. That makes the exact set, and its order, worth pinning
//  rather than sampling.
//
//  The exclusions get their own tests because they are the half that cannot be
//  noticed by looking at a file: an entry that should not be there looks
//  exactly like one that should, until someone reads a user's account name out
//  of a screenshot attached to a public issue.
//
////////////////////////////////////////////////////////////////////////////////

namespace ScreenshotMetadataTests
{
    static SYSTEMTIME FixedTime()
    {
        SYSTEMTIME   st = {};
        st.wYear      = 2026;
        st.wMonth     = 9;
        st.wDay       = 5;
        st.wDayOfWeek = 6;          // Saturday
        st.wHour      = 14;
        st.wMinute    = 32;
        st.wSecond    = 7;
        return st;
    }


    static ScreenshotFacts MakeFacts (ScreenshotMode mode)
    {
        ScreenshotFacts   f;
        f.mode               = mode;
        f.versionString      = "Casso 1.22.0";
        f.machineDisplayName = "Apple //e";
        f.when               = FixedTime();
        f.utcOffsetMinutes   = -420;                       // -0700
        f.monitorKey         = "AppleMonitorII/GreenMono";
        f.scenePose          = "yaw 12.5  pitch -8.0  zoom 1.00  pan 0.000 0.000";
        return f;
    }


    static bool Has (const vector<MetadataEntry> & entries, const char * keyword)
    {
        size_t   i = 0;

        for (i = 0; i < entries.size(); i++)
        {
            if (entries[i].keyword == keyword)
            {
                return true;
            }
        }

        return false;
    }


    static string ValueOf (const vector<MetadataEntry> & entries, const char * keyword)
    {
        size_t   i = 0;

        for (i = 0; i < entries.size(); i++)
        {
            if (entries[i].keyword == keyword)
            {
                return entries[i].value;
            }
        }

        return string();
    }




    TEST_CLASS (ScreenshotMetadataTests)
    {
    public:

        //
        //  The per-mode entry set
        //

        TEST_METHOD (SceneEmitsAllSevenEntriesInContractOrder)
        {
            vector<MetadataEntry>   e = ScreenshotMetadata::Compose (MakeFacts (ScreenshotMode::Scene));

            Assert::AreEqual ((size_t) 7, e.size());
            Assert::AreEqual (string ("Software"),         e[0].keyword);
            Assert::AreEqual (string ("Source"),           e[1].keyword);
            Assert::AreEqual (string ("Creation Time"),    e[2].keyword);
            Assert::AreEqual (string ("Casso Capture"),    e[3].keyword);
            Assert::AreEqual (string ("Casso Monitor"),    e[4].keyword);
            Assert::AreEqual (string ("Casso Scene Pose"), e[5].keyword);
            Assert::AreEqual (string ("Casso CRT"),        e[6].keyword);
        }


        TEST_METHOD (CrtEmitsSixWithoutTheScenePose)
        {
            vector<MetadataEntry>   e = ScreenshotMetadata::Compose (MakeFacts (ScreenshotMode::Crt));

            Assert::AreEqual ((size_t) 6, e.size());
            Assert::IsFalse (Has (e, "Casso Scene Pose"),
                L"A picture capture has no scene, so a pose would describe nothing");
            Assert::IsTrue (Has (e, "Casso CRT"));
        }


        TEST_METHOD (RawEmitsFiveWithoutPoseOrCrt)
        {
            vector<MetadataEntry>   e = ScreenshotMetadata::Compose (MakeFacts (ScreenshotMode::Raw));

            Assert::AreEqual ((size_t) 5, e.size());
            Assert::IsFalse (Has (e, "Casso Scene Pose"));
            Assert::IsFalse (Has (e, "Casso CRT"),
                L"No CRT parameters were applied, so claiming any would be false");
        }


        TEST_METHOD (TheCaptureTokenMatchesTheMode)
        {
            Assert::AreEqual (string ("scene"),
                ValueOf (ScreenshotMetadata::Compose (MakeFacts (ScreenshotMode::Scene)), "Casso Capture"));
            Assert::AreEqual (string ("crt"),
                ValueOf (ScreenshotMetadata::Compose (MakeFacts (ScreenshotMode::Crt)), "Casso Capture"));
            Assert::AreEqual (string ("raw"),
                ValueOf (ScreenshotMetadata::Compose (MakeFacts (ScreenshotMode::Raw)), "Casso Capture"));
        }


        // The monitor key is emitted for raw too: the tube did not touch those
        // pixels but the color mode did, since the tint is applied in the
        // framebuffer.
        TEST_METHOD (EveryModeCarriesTheMonitorKey)
        {
            Assert::AreEqual (string ("AppleMonitorII/GreenMono"),
                ValueOf (ScreenshotMetadata::Compose (MakeFacts (ScreenshotMode::Raw)), "Casso Monitor"));
        }


        // The desk scene may not have composed yet. A blank value is worse
        // than an absent entry -- it asserts a pose of nothing.
        TEST_METHOD (AnEmptyPoseIsSkippedRatherThanEmitted)
        {
            ScreenshotFacts   f = MakeFacts (ScreenshotMode::Scene);
            f.scenePose = "";

            vector<MetadataEntry>   e = ScreenshotMetadata::Compose (f);

            Assert::AreEqual ((size_t) 6, e.size());
            Assert::IsFalse (Has (e, "Casso Scene Pose"));
        }


        //
        //  Value formats
        //

        TEST_METHOD (CreationTimeIsRfc1123WithANegativeOffset)
        {
            Assert::AreEqual (string ("Sat, 05 Sep 2026 14:32:07 -0700"),
                              ScreenshotMetadata::FormatCreationTime (FixedTime(), -420));
        }


        TEST_METHOD (CreationTimeHandlesAPositiveOffset)
        {
            Assert::AreEqual (string ("Sat, 05 Sep 2026 14:32:07 +0200"),
                              ScreenshotMetadata::FormatCreationTime (FixedTime(), 120));
        }


        TEST_METHOD (CreationTimeHandlesUtc)
        {
            Assert::AreEqual (string ("Sat, 05 Sep 2026 14:32:07 +0000"),
                              ScreenshotMetadata::FormatCreationTime (FixedTime(), 0));
        }


        // The sign belongs to the hour while the minutes stay positive. A
        // half-hour zone west of Greenwich is where a bare division gets this
        // wrong and produces something like "-0-30".
        TEST_METHOD (CreationTimeKeepsMinutesPositiveInAHalfHourZone)
        {
            Assert::AreEqual (string ("Sat, 05 Sep 2026 14:32:07 -0330"),
                              ScreenshotMetadata::FormatCreationTime (FixedTime(), -210));
            Assert::AreEqual (string ("Sat, 05 Sep 2026 14:32:07 +0545"),
                              ScreenshotMetadata::FormatCreationTime (FixedTime(), 345));
        }


        TEST_METHOD (CrtParamsAreFixedWidthSoTwoCapturesMatchByteForByte)
        {
            CaptureCrtParams   crt;
            crt.brightness        = 1.0f;
            crt.contrast          = 1.05f;
            crt.gamma             = 1.0f;
            crt.scanlineIntensity = 0.35f;
            crt.bloomStrength     = 0.5f;
            crt.bloomRadius       = 1.0f;
            crt.colorBleedWidth   = 0.0f;
            crt.persistence       = 0.2f;

            Assert::AreEqual (string ("brightness 1.00  contrast 1.05  gamma 1.00  scanlines 0.35"
                                      "  bloom 0.50/1.00  bleed 0.00  persistence 0.20"),
                              ScreenshotMetadata::FormatCrtParams (crt));
        }


        //
        //  What must NEVER appear
        //

        TEST_METHOD (NoValueLeaksAPathOrADriveLetter)
        {
            ScreenshotMode   modes[] = { ScreenshotMode::Scene, ScreenshotMode::Crt, ScreenshotMode::Raw };
            size_t           m       = 0;
            size_t           i       = 0;

            for (m = 0; m < std::size (modes); m++)
            {
                vector<MetadataEntry>   e = ScreenshotMetadata::Compose (MakeFacts (modes[m]));

                for (i = 0; i < e.size(); i++)
                {
                    Assert::IsTrue (e[i].value.find ('\\') == string::npos, L"backslash");
                    Assert::IsTrue (e[i].value.find (":\\") == string::npos, L"drive letter");
                    Assert::IsTrue (e[i].value.find ("C:") == string::npos, L"drive letter");
                }
            }
        }


        // The image format's own header records the dimensions. A second copy
        // in text is a second thing to disagree with the first.
        TEST_METHOD (NoEntryDuplicatesTheImageDimensions)
        {
            vector<MetadataEntry>   e = ScreenshotMetadata::Compose (MakeFacts (ScreenshotMode::Raw));
            size_t                  i = 0;

            for (i = 0; i < e.size(); i++)
            {
                Assert::IsTrue (e[i].value.find ("560x384") == string::npos);
                Assert::IsTrue (e[i].keyword.find ("Width") == string::npos);
                Assert::IsTrue (e[i].keyword.find ("Height") == string::npos);
            }
        }


        TEST_METHOD (EveryKeywordSatisfiesThePngRules)
        {
            ScreenshotMode   modes[] = { ScreenshotMode::Scene, ScreenshotMode::Crt, ScreenshotMode::Raw };
            size_t           m       = 0;
            size_t           i       = 0;

            for (m = 0; m < std::size (modes); m++)
            {
                vector<MetadataEntry>   e = ScreenshotMetadata::Compose (MakeFacts (modes[m]));

                for (i = 0; i < e.size(); i++)
                {
                    Assert::IsTrue (PngMetadata::IsValidKeyword (e[i].keyword),
                                    ToWide (e[i].keyword).c_str());
                }
            }
        }


        TEST_METHOD (NoValueIsEmpty)
        {
            vector<MetadataEntry>   e = ScreenshotMetadata::Compose (MakeFacts (ScreenshotMode::Scene));
            size_t                  i = 0;

            for (i = 0; i < e.size(); i++)
            {
                Assert::IsFalse (e[i].value.empty(), ToWide (e[i].keyword).c_str());
            }
        }


    private:
        static wstring ToWide (const string & s)
        {
            return wstring (s.begin(), s.end());
        }
    };
}
