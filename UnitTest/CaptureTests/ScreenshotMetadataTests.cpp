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
        f.hasScenePose       = true;
        f.orbitYawRad        = 0.2181662f;      // 12.5 degrees
        f.orbitPitchRad      = -0.1396263f;     // -8.0 degrees
        f.zoom               = 1.0f;
        f.panX               = 0.0f;
        f.panY               = 0.0f;
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

        TEST_METHOD (SceneEmitsEveryEntryInContractOrder)
        {
            vector<MetadataEntry>   e = ScreenshotMetadata::Compose (MakeFacts (ScreenshotMode::Scene));
            const char *            expected[] = {
                "Software", "Source", "Creation Time", "Casso capture", "Casso monitor",
                "Casso scene yaw", "Casso scene pitch", "Casso scene zoom",
                "Casso scene pan X", "Casso scene pan Y",
                "Casso CRT brightness", "Casso CRT contrast", "Casso CRT gamma",
                "Casso CRT scanlines", "Casso CRT bloom strength", "Casso CRT bloom radius",
                "Casso CRT bleed", "Casso CRT persistence" };
            size_t                  i = 0;

            Assert::AreEqual (std::size (expected), e.size());

            for (i = 0; i < e.size(); i++)
            {
                Assert::AreEqual (string (expected[i]), e[i].keyword);
            }
        }


        // One value per keyword, so adding a parameter later is a new entry --
        // which the contract permits -- rather than a changed grammar inside
        // an existing one, which it does not cover.
        TEST_METHOD (EveryValueIsASingleFieldNotABlob)
        {
            vector<MetadataEntry>   e = ScreenshotMetadata::Compose (MakeFacts (ScreenshotMode::Scene));
            size_t                  i = 0;

            for (i = 0; i < e.size(); i++)
            {
                if (e[i].keyword.rfind ("Casso scene", 0) == 0
                 || e[i].keyword.rfind ("Casso CRT", 0) == 0)
                {
                    Assert::IsTrue (e[i].value.find (' ') == string::npos,
                                    ToWide (e[i].keyword + " = " + e[i].value).c_str());
                    Assert::IsTrue (e[i].value.find ('/') == string::npos);
                }
            }
        }


        TEST_METHOD (CrtEmitsThirteenWithoutTheScenePose)
        {
            vector<MetadataEntry>   e = ScreenshotMetadata::Compose (MakeFacts (ScreenshotMode::Crt));

            Assert::AreEqual ((size_t) 13, e.size());
            Assert::IsFalse (Has (e, "Casso scene yaw"),
                L"A picture capture has no scene, so a pose would describe nothing");
            Assert::IsTrue (Has (e, "Casso CRT brightness"));
            Assert::IsTrue (Has (e, "Casso CRT persistence"));
        }


        TEST_METHOD (RawEmitsFiveWithoutPoseOrCrt)
        {
            vector<MetadataEntry>   e = ScreenshotMetadata::Compose (MakeFacts (ScreenshotMode::Raw));

            Assert::AreEqual ((size_t) 5, e.size());
            Assert::IsFalse (Has (e, "Casso scene yaw"));
            Assert::IsFalse (Has (e, "Casso CRT brightness"),
                L"No CRT parameters were applied, so claiming any would be false");
        }


        TEST_METHOD (TheCaptureTokenMatchesTheMode)
        {
            Assert::AreEqual (string ("scene"),
                ValueOf (ScreenshotMetadata::Compose (MakeFacts (ScreenshotMode::Scene)), "Casso capture"));
            Assert::AreEqual (string ("crt"),
                ValueOf (ScreenshotMetadata::Compose (MakeFacts (ScreenshotMode::Crt)), "Casso capture"));
            Assert::AreEqual (string ("raw"),
                ValueOf (ScreenshotMetadata::Compose (MakeFacts (ScreenshotMode::Raw)), "Casso capture"));
        }


        // The monitor key is emitted for raw too: the tube did not touch those
        // pixels but the color mode did, since the tint is applied in the
        // framebuffer.
        TEST_METHOD (EveryModeCarriesTheMonitorKey)
        {
            Assert::AreEqual (string ("AppleMonitorII/GreenMono"),
                ValueOf (ScreenshotMetadata::Compose (MakeFacts (ScreenshotMode::Raw)), "Casso monitor"));
        }


        // The desk scene may not have composed yet. A blank value is worse
        // than an absent entry -- it asserts a pose of nothing.
        TEST_METHOD (AnUncomposedPoseIsSkippedRatherThanEmittedAsZeros)
        {
            ScreenshotFacts   f = MakeFacts (ScreenshotMode::Scene);
            f.hasScenePose = false;

            vector<MetadataEntry>   e = ScreenshotMetadata::Compose (f);

            Assert::AreEqual ((size_t) 13, e.size());
            Assert::IsFalse (Has (e, "Casso scene yaw"));
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


        TEST_METHOD (PoseValuesAreDegreesAtTheReadoutPrecision)
        {
            vector<MetadataEntry>   e = ScreenshotMetadata::Compose (MakeFacts (ScreenshotMode::Scene));

            Assert::AreEqual (string ("12.5"),  ValueOf (e, "Casso scene yaw"));
            Assert::AreEqual (string ("-8.0"),  ValueOf (e, "Casso scene pitch"));
            Assert::AreEqual (string ("1.00"),  ValueOf (e, "Casso scene zoom"));
            Assert::AreEqual (string ("0.000"), ValueOf (e, "Casso scene pan X"));
        }


        TEST_METHOD (CrtValuesAreFixedWidthSoTwoCapturesMatchByteForByte)
        {
            ScreenshotFacts   f = MakeFacts (ScreenshotMode::Crt);
            f.crt.brightness        = 1.0f;
            f.crt.contrast          = 1.05f;
            f.crt.scanlineIntensity = 0.35f;
            f.crt.bloomStrength     = 0.5f;

            vector<MetadataEntry>   e = ScreenshotMetadata::Compose (f);

            Assert::AreEqual (string ("1.00"), ValueOf (e, "Casso CRT brightness"));
            Assert::AreEqual (string ("1.05"), ValueOf (e, "Casso CRT contrast"));
            Assert::AreEqual (string ("0.35"), ValueOf (e, "Casso CRT scanlines"));
            Assert::AreEqual (string ("0.50"), ValueOf (e, "Casso CRT bloom strength"));
        }


        // The readout keeps its one-line form; the file does not use it.
        TEST_METHOD (TheReadoutFormatIsStillAvailableForTheOnScreenPose)
        {
            Assert::AreEqual (string ("yaw 12.5  pitch -8.0  zoom 1.00  pan 0.000 0.000"),
                              ScreenshotMetadata::FormatScenePose (0.2181662f, -0.1396263f,
                                                                   1.0f, 0.0f, 0.0f));
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
