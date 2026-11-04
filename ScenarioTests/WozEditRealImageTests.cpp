#include "Pch.h"
#include "EhmTestHelper.h"
#include "CassoExplorer/Model/PreviewDecoder.h"
#include "CassoExplorer/Model/WozProperties.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImageSession.h"
#include "Devices/Disk/DiskImage.h"
#include "GuestSession.h"
#include "Machines/Apple2/Common/WozLoader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  WozEditRealImageTests
//
//  Disk properties' edits made in real WOZ images from the repo, an
//  Applesauce WOZ 2 and a WOZ 1, in memory: the edited bytes load, their CRC
//  matches, the edits read back, and every byte before META is as it was but
//  INFO's edited fields and the header CRC.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (WozEditRealImageTests)
{
public:

    static constexpr const char *  kWoz2Path = "Apple2/Demos/The Print Shop Color side A.woz";
    static constexpr const char *  kWoz1Path = "Apple2/Demos/Karateka.woz";
    static constexpr size_t        kInfoAt   = WozLoader::kHeaderSize + 8;   // INFO's payload, after its id and size

    static WozLoader::Edits MakeEdits (const WozLoader::Description & woz)
    {
        WozLoader::Edits  edits;

        edits.writeProtected     = !woz.writeProtected;
        edits.bootSectorFormat   = woz.bootSectorFormat;
        edits.compatibleHardware = woz.compatibleHardware;
        edits.requiredRamK       = woz.requiredRamK;
        edits.meta               = woz.meta;

        for (WozLoader::MetaField & field : edits.meta)
        {
            field.value = (field.key == "title") ? std::string ("Edited title") : field.value;
        }

        return edits;
    }

    //  Where META's chunk starts, from a walk of the chunks.
    static size_t FindMeta (const std::vector<Byte> & bytes)
    {
        size_t  at = WozLoader::kHeaderSize;

        while (at + 8 <= bytes.size() && memcmp (bytes.data() + at, "META", 4) != 0)
        {
            at += 8 + (bytes[at + 4] | (bytes[at + 5] << 8) | (bytes[at + 6] << 16) | ((size_t) bytes[at + 7] << 24));
        }

        return at;
    }

    static std::wstring GetTitle (const WozLoader::Description & woz)
    {
        auto  found = std::find_if (woz.meta.begin(), woz.meta.end(), [] (const WozLoader::MetaField & field) { return field.key == "title"; });

        return (found != woz.meta.end()) ? TextEncoding::Utf8ToWide (found->value) : std::wstring();
    }


    TEST_METHOD (Woz2_Edits_LoadAndReadBack_WithTheTracksUntouched)
    {
        std::vector<Byte>       before = GuestSession::RequireRepoImage (kWoz2Path);
        std::vector<Byte>       after  = before;
        WozLoader::Description  woz;
        WozLoader::Description  edited;
        DiskImage               image;
        size_t                  meta   = 0;

        WozLoader::Describe (before, woz);
        Assert::IsTrue (woz.isWoz && woz.hasBootSectorFormat, L"the image has a version 2 INFO");
        Assert::IsFalse (woz.meta.empty(), L"and a META to edit");

        meta = FindMeta (before);

        AssertSucceeded (WozLoader::ApplyEdits (after, MakeEdits (woz)));
        WozLoader::Describe (after, edited);

        Assert::AreEqual (!woz.writeProtected, edited.writeProtected);
        Assert::AreEqual (std::wstring (L"Edited title"), GetTitle (edited));
        Assert::AreEqual (woz.meta.size(), edited.meta.size());

        for (size_t at = 0; at < meta; at++)
        {
            bool  changed = (at >= 8 && at < WozLoader::kHeaderSize) || at == kInfoAt + WozLoader::kInfoOffsetWriteProtected;

            if (!changed)
            {
                Assert::AreEqual (before[at], after[at], std::format (L"byte {} before META", at).c_str());
            }
        }

        AssertSucceeded (WozLoader::Load (after, image));
        Assert::IsFalse (image.IsDamaged(), L"the CRC matches the edited bytes");
    }


    TEST_METHOD (Woz1_TakesMetaAndWriteProtection_ButNotTheVersion2Fields)
    {
        std::vector<Byte>       bytes = GuestSession::RequireRepoImage (kWoz1Path);
        std::vector<Byte>       kept  = bytes;
        WozLoader::Description  woz;
        WozLoader::Description  edited;
        WozLoader::Edits        edits;
        DiskImage               image;
        HRESULT                 hr    = S_OK;

        WozLoader::Describe (bytes, woz);
        Assert::IsTrue  (woz.isWoz);
        Assert::IsFalse (woz.hasBootSectorFormat, L"the image has a version 1 INFO");

        edits                    = MakeEdits (woz);
        edits.compatibleHardware = 1;

        hr = WozLoader::ApplyEdits (bytes, edits);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED), hr);
        Assert::IsTrue   (kept == bytes, L"a refused edit changes nothing");

        edits.compatibleHardware = 0;

        AssertSucceeded (WozLoader::ApplyEdits (bytes, edits));
        WozLoader::Describe (bytes, edited);

        Assert::AreEqual (std::wstring (L"Edited title"), GetTitle (edited));
        AssertSucceeded  (WozLoader::Load (bytes, image));
        Assert::IsFalse  (image.IsDamaged());
    }

    TEST_METHOD (Woz2_Details_HaveBothHeadings_AndAValuePerLine)
    {
        std::vector<Byte>                                   bytes = GuestSession::RequireRepoImage (kWoz2Path);
        std::vector<std::pair<std::wstring, std::wstring>>  details;
        auto                                                is    = [&details] (size_t at, const wchar_t * label, const wchar_t * value)
        {
            return at < details.size() && details[at].first == label && details[at].second == value;
        };
        size_t                                              developer = 0;

        Assert::IsTrue (PreviewDecoder::ParseDetails (DiskImageSession::DescribeWozChunks (bytes), details));

        Assert::IsTrue (std::any_of (details.begin(), details.end(), [] (const auto & d) { return d.first == L"WOZ info"     && d.second.empty(); }));
        Assert::IsTrue (std::any_of (details.begin(), details.end(), [] (const auto & d) { return d.first == L"WOZ metadata" && d.second.empty(); }));

        developer = (size_t) (std::find_if (details.begin(), details.end(), [] (const auto & d) { return d.first == L"Developer"; }) - details.begin());

        Assert::IsTrue (is (developer,     L"Developer", L"David Balsam"), L"the first of the values on the label's line");
        Assert::IsTrue (is (developer + 1, L"",          L"Martin Kahn"),  L"and the next on a line of its own");
    }

    TEST_METHOD (HardHatMack_MetaAsTheEditorSavesIt_KeepsEveryValue)
    {
        std::vector<Byte>                                    bytes = GuestSession::RequireRepoImage ("Apple2/Demos/HardHatMack.woz");
        WozLoader::Description                               woz;
        WozLoader::Description                               saved;
        WozLoader::Edits                                     edits;
        std::vector<std::pair<std::string, std::wstring>>    typed;
        std::string                                          text;

        WozLoader::Describe (bytes, woz);
        Assert::IsFalse (woz.meta.empty());

        //  As the editor fills its fields: a list's values a line each.
        for (const WozLoader::MetaField & field : woz.meta)
        {
            std::wstring  value = TextEncoding::Utf8ToWide (field.value);

            std::replace (value.begin(), value.end(), L'|', L'\n');
            typed.emplace_back (field.key, value);
        }

        edits.writeProtected     = woz.writeProtected;
        edits.bootSectorFormat   = woz.bootSectorFormat;
        edits.compatibleHardware = woz.compatibleHardware;
        edits.requiredRamK       = woz.requiredRamK;
        edits.meta               = WozProperties::MakeMeta (typed);

        AssertSucceeded (WozLoader::ApplyEdits (bytes, edits));
        WozLoader::Describe (bytes, saved);

        for (const WozLoader::MetaField & field : woz.meta)
        {
            auto  found = std::find_if (saved.meta.begin(), saved.meta.end(), [&field] (const WozLoader::MetaField & f) { return f.key == field.key; });

            Assert::IsTrue   (found != saved.meta.end(), TextEncoding::Utf8ToWide (field.key).c_str());
            Assert::AreEqual (field.value, found->value, TextEncoding::Utf8ToWide (field.key).c_str());
        }

        //  Its empty standard keys are written empty, as the image had them.
        text.assign (bytes.begin(), bytes.end());
        Assert::IsTrue (text.find ("subtitle\t\n") != std::string::npos);
    }
};
