#include "Pch.h"

#include "../Casso/Ui/Dialog/StartupDownloadDialog.h"



using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace StartupDownloadSetTests
{
    static StartupAssetEntry MakeEntry (StartupAssetKind kind, const wchar_t * display)
    {
        StartupAssetEntry  entry;
        entry.kind        = kind;
        entry.groupLabel  = L"Group";
        entry.displayName = display;
        entry.kindLabel   = (kind == StartupAssetKind::Rom)        ? L"ROM"
                          : (kind == StartupAssetKind::DriveAudio) ? L"Drive audio"
                                                                   : L"Boot disk";
        entry.source      = L"Test source";
        return entry;
    }



    TEST_CLASS (StartupDownloadSetTests)
    {
    public:

        TEST_METHOD (None_Missing_SetEmpty_AndDoesNotRequireRoms)
        {
            StartupDownloadSet  set;

            Assert::IsTrue  (set.Empty(),        L"empty set should report Empty()");
            Assert::IsFalse (set.RequiresRoms(), L"empty set never requires ROMs");
        }


        TEST_METHOD (RomsOnly_RequiresRoms_NotEmpty)
        {
            StartupDownloadSet  set;
            set.entries.push_back (MakeEntry (StartupAssetKind::Rom, L"Apple //e ROM"));
            set.entries.push_back (MakeEntry (StartupAssetKind::Rom, L"Disk II ROM"));

            Assert::IsFalse (set.Empty(),       L"ROM-only set is not empty");
            Assert::IsTrue  (set.RequiresRoms(), L"any ROM forces RequiresRoms");
            Assert::AreEqual ((size_t) 2, set.entries.size());
        }


        TEST_METHOD (AudioOnly_DoesNotRequireRoms)
        {
            StartupDownloadSet  set;
            set.entries.push_back (MakeEntry (StartupAssetKind::DriveAudio, L"Alps WAVs"));
            set.entries.push_back (MakeEntry (StartupAssetKind::DriveAudio, L"Shugart WAVs"));

            Assert::IsFalse (set.Empty());
            Assert::IsFalse (set.RequiresRoms(), L"audio-only set must not force ROMs");
        }


        TEST_METHOD (BothMissing_RequiresRoms)
        {
            StartupDownloadSet  set;
            set.entries.push_back (MakeEntry (StartupAssetKind::DriveAudio, L"Alps WAVs"));
            set.entries.push_back (MakeEntry (StartupAssetKind::Rom,        L"Apple //e ROM"));

            Assert::IsFalse (set.Empty());
            Assert::IsTrue  (set.RequiresRoms(), L"any ROM (even after audio) forces RequiresRoms");
        }


        TEST_METHOD (BootDiskOnly_DoesNotRequireRoms)
        {
            StartupDownloadSet  set;
            set.entries.push_back (MakeEntry (StartupAssetKind::BootDisk, L"DOS 3.3 master"));

            Assert::IsFalse (set.Empty());
            Assert::IsFalse (set.RequiresRoms(), L"boot disks don't trigger required-ROMs button policy");
        }


        TEST_METHOD (InsertionOrder_PreservedAcrossKinds)
        {
            StartupDownloadSet  set;
            set.entries.push_back (MakeEntry (StartupAssetKind::Rom,        L"first"));
            set.entries.push_back (MakeEntry (StartupAssetKind::DriveAudio, L"second"));
            set.entries.push_back (MakeEntry (StartupAssetKind::Rom,        L"third"));
            set.entries.push_back (MakeEntry (StartupAssetKind::BootDisk,   L"fourth"));

            Assert::AreEqual ((size_t) 4, set.entries.size());
            Assert::AreEqual (std::wstring (L"first"),  set.entries[0].displayName);
            Assert::AreEqual (std::wstring (L"second"), set.entries[1].displayName);
            Assert::AreEqual (std::wstring (L"third"),  set.entries[2].displayName);
            Assert::AreEqual (std::wstring (L"fourth"), set.entries[3].displayName);
        }


        TEST_METHOD (EntryDefaults_AreSelectableAndSelected)
        {
            StartupAssetEntry  entry;

            Assert::IsTrue (entry.selectable, L"new entries default to user-toggleable");
            Assert::IsTrue (entry.selected,   L"new entries default to selected");
            Assert::AreEqual ((std::uint64_t) 0, entry.expectedBytes);
            Assert::IsTrue (entry.destPaths.empty());
            Assert::IsTrue (entry.groupLabel.empty());
        }
    };
}
