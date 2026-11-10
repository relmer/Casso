#include "Pch.h"

#include "../EmuTests/FakeDiskFileIo.h"
#include "CassoExplorer/InspectorWindowSet.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Ui/DiskInspector/IDiskInspectorHost.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWindowSetTests
//
//  Casso Explorer's inspector windows (FR-073 to FR-075, FR-078, FR-080):
//  one window per image, a second open bringing it to the front, a window
//  for each of several images, a changed file read again once its changes
//  settle, a file the window saved itself not read again, the note for a
//  file that is gone, a window's state saved as it closes, every window
//  closed with the app, and the theme the windows get. "Compare disk images"
//  (FR-117): one window for the two images, and an open window of the first
//  image used for the comparison.
//
////////////////////////////////////////////////////////////////////////////////





//  What the set did to one window.
struct FakeInspectorWindowLog
{
    int                   created      = 0;
    int                   fronted      = 0;
    int                   walkthroughs = 0;
    int                   shown        = 0;
    int                   frames       = 0;
    int                   reloads      = 0;
    int                   goneNotes    = 0;
    vector<std::wstring>  compared;
    const DxuiTheme     * theme        = nullptr;
    IDiskInspectorHost  * host         = nullptr;
};


class FakeInspectorWindow : public InspectorWindowSet::Window
{
public:
    explicit FakeInspectorWindow (FakeInspectorWindowLog & log) : m_log (log) {}

    HRESULT  Create (HINSTANCE, HWND, const DxuiTheme * theme, IDiskInspectorHost * host) override { m_log.created++; m_log.theme = theme; m_log.host = host; return S_OK; }
    void     Front() override { m_log.fronted++; }
    void     ApplyPrefs (const DiskInspectorPrefs &) override {}
    void     GetPrefs (DiskInspectorPrefs & inOut) const override { inOut.x = kSavedX; }
    void     OpenWalkthrough() override { m_log.walkthroughs++; }
    HRESULT  ShowDisk() override { m_log.shown++; return S_OK; }
    HRESULT  RenderFrame() override { m_log.frames++; return S_OK; }
    void     ReloadDisk() override { m_log.reloads++; }
    void     NoteFileGone() override { m_log.goneNotes++; }
    void     CompareWith (const std::wstring & pathB) override { m_log.compared.push_back (pathB); }
    void     ChangeTheme (const DxuiTheme * theme) override { m_log.theme = theme; }
    HWND     GetHwnd() const override { return nullptr; }

    static constexpr int  kSavedX = 321;

private:
    FakeInspectorWindowLog  & m_log;
};


class FakeImageWatcher : public IImageWatcher
{
public:
    bool  Watch   (const std::string & directory, Callback callback) override { watched.push_back (directory); m_callback = std::move (callback); return true; }
    void  Unwatch (const std::string & directory) override                    { unwatched.push_back (directory); }

    vector<std::string>  watched;
    vector<std::string>  unwatched;

private:
    Callback  m_callback;
};


TEST_CLASS (InspectorWindowSetTests)
{
public:

    static constexpr const wchar_t *  kMist    = L"C:\\Disks\\mist.dsk";
    static constexpr const wchar_t *  kBandits = L"C:\\Disks\\Bandits.woz";
    static constexpr const wchar_t *  kGame    = L"C:\\Games\\Game.nib";



    //  A set whose windows log to `logs`, in the order they were made, on a
    //  clock the test sets and with the files in `missing` gone.
    struct Rig
    {
        FakeImageWatcher                                  watcher;
        DiskInspectorPrefs                                prefs;
        vector<std::unique_ptr<FakeInspectorWindowLog>>   logs;
        ULONGLONG                                         nowMs = 1000;
        std::set<std::wstring>                            missing;
        FakeDiskFileIo                                    fileIo;
        std::unique_ptr<InspectorWindowSet>               set;
        DxuiDarkTheme                                     dark;

        Rig()
        {
            InspectorWindowSet::Seams  seams;

            seams.makeWindow = [this] ()
            {
                logs.push_back (std::make_unique<FakeInspectorWindowLog>());
                return std::unique_ptr<InspectorWindowSet::Window> (std::make_unique<FakeInspectorWindow> (*logs.back()));
            };
            seams.fileExists = [this] (const std::wstring & path) { return missing.count (path) == 0; };
            seams.nowMs      = [this] () { return nowMs; };
            seams.fileIo     = &fileIo;

            set = std::make_unique<InspectorWindowSet> (watcher, prefs, std::move (seams));
        }

        HRESULT  Open (const wchar_t * path) { return set->Open (nullptr, nullptr, path, dark); }
    };



    TEST_METHOD (OpeningAnImageAgainFrontsItsWindow)
    {
        Rig  rig;



        Assert::AreEqual (S_OK, rig.Open (kMist));
        Assert::AreEqual (S_OK, rig.Open (L"c:\\disks\\MIST.DSK"));

        Assert::AreEqual ((size_t) 1, rig.set->GetCount());
        Assert::AreEqual ((size_t) 1, rig.logs.size());
        Assert::AreEqual (1, rig.logs[0]->shown);
        Assert::AreEqual (1, rig.logs[0]->fronted, L"the second open fronts the window, matched without regard to case");
    }



    TEST_METHOD (ComparingTwoImagesOpensOneWindowOnTheFirst)
    {
        Rig  rig;



        Assert::AreEqual (S_OK, rig.set->Compare (nullptr, nullptr, kBandits, kMist, rig.dark));

        Assert::AreEqual ((size_t) 1, rig.set->GetCount(), L"B has no window");
        Assert::AreEqual (1, rig.logs[0]->shown);
        Assert::AreEqual ((size_t) 1, rig.logs[0]->compared.size());
        Assert::AreEqual (std::wstring (kMist), rig.logs[0]->compared[0]);
    }



    TEST_METHOD (ComparingFromAnOpenImageUsesItsWindow)
    {
        Rig  rig;



        Assert::AreEqual (S_OK, rig.Open (kMist));
        Assert::AreEqual (S_OK, rig.Open (kGame));
        Assert::AreEqual (S_OK, rig.set->Compare (nullptr, nullptr, kMist, kGame, rig.dark));

        Assert::AreEqual ((size_t) 2, rig.set->GetCount(), L"no window is added");
        Assert::AreEqual (1, rig.logs[0]->fronted);
        Assert::AreEqual ((size_t) 1, rig.logs[0]->compared.size());
        Assert::AreEqual (std::wstring (kGame), rig.logs[0]->compared[0]);
        Assert::IsTrue   (rig.logs[1]->compared.empty(), L"B's window is not changed");
    }



    TEST_METHOD (SeveralImagesOpenAWindowEach)
    {
        Rig  rig;



        Assert::AreEqual (S_OK, rig.Open (kMist));
        Assert::AreEqual (S_OK, rig.Open (kBandits));
        Assert::AreEqual (S_OK, rig.Open (kGame));

        Assert::AreEqual ((size_t) 3, rig.set->GetCount());
        Assert::AreEqual (0, rig.logs[0]->fronted);
    }



    TEST_METHOD (AFolderIsWatchedOnceWhileAnyOfItsFilesHasAWindow)
    {
        Rig  rig;



        Assert::AreEqual (S_OK, rig.Open (kMist));
        Assert::AreEqual (S_OK, rig.Open (kBandits));
        Assert::AreEqual ((size_t) 1, rig.watcher.watched.size());

        rig.logs[0]->host->OnInspectorClosed();
        rig.set->Tick();
        Assert::AreEqual ((size_t) 0, rig.watcher.unwatched.size(), L"Bandits.woz has a window open");

        rig.logs[1]->host->OnInspectorClosed();
        rig.set->Tick();
        Assert::AreEqual ((size_t) 1, rig.watcher.unwatched.size());
    }



    TEST_METHOD (AChangedFileIsReadAgainOnceItsChangesSettle)
    {
        Rig  rig;



        Assert::AreEqual (S_OK, rig.Open (kMist));

        rig.set->NoteChange ("C:\\Disks\\mist.dsk");
        rig.set->Tick();
        rig.nowMs += InspectorWindowSet::kSettleMs / 2;
        rig.set->NoteChange ("C:\\Disks\\MIST.DSK");
        rig.set->Tick();
        rig.nowMs += InspectorWindowSet::kSettleMs / 2;
        rig.set->Tick();
        Assert::AreEqual (0, rig.logs[0]->reloads, L"each change restarts the wait");

        rig.nowMs += InspectorWindowSet::kSettleMs;
        rig.set->Tick();
        rig.set->Tick();
        Assert::AreEqual (1, rig.logs[0]->reloads, L"a burst of changes is read once");
        Assert::AreEqual (0, rig.logs[0]->goneNotes);
    }



    TEST_METHOD (AFileAsTheWindowReadItIsNotReadAgain)
    {
        Rig          rig;
        std::string  path = "C:\\Disks\\mist.dsk";



        rig.fileIo.files[path]  = vector<Byte> (NibblizationLayer::kImageByteSize, 0x11);
        rig.fileIo.stamps[path] = FileStamp { NibblizationLayer::kImageByteSize, 1 };

        Assert::AreEqual (S_OK, rig.Open (kMist));
        (void) rig.logs[0]->host->PostInspectorRequest ({ InspectorRequestKind::CopyDisk, 0, 0, {}, -1 });

        rig.set->NoteChange (path);
        rig.set->Tick();
        rig.nowMs += InspectorWindowSet::kSettleMs;
        rig.set->Tick();
        Assert::AreEqual (0, rig.logs[0]->reloads, L"the file has the size and time of the window's read");

        rig.fileIo.stamps[path].modifiedUnix = 7;
        rig.set->NoteChange (path);
        rig.set->Tick();
        rig.nowMs += InspectorWindowSet::kSettleMs;
        rig.set->Tick();
        Assert::AreEqual (1, rig.logs[0]->reloads, L"a change from outside the window is read");
    }



    TEST_METHOD (AChangeToAnotherFileInTheFolderIsNotRead)
    {
        Rig  rig;



        Assert::AreEqual (S_OK, rig.Open (kMist));

        rig.set->NoteChange ("C:\\Disks\\mist.dsk.tmp");
        rig.set->Tick();
        rig.nowMs += InspectorWindowSet::kSettleMs * 2;
        rig.set->Tick();

        Assert::AreEqual (0, rig.logs[0]->reloads);
    }



    TEST_METHOD (AFileThatIsGoneKeepsItsAnalysisWithANote)
    {
        Rig  rig;



        Assert::AreEqual (S_OK, rig.Open (kMist));

        rig.missing.insert (kMist);
        rig.set->NoteChange ("C:\\Disks\\mist.dsk");
        rig.set->Tick();
        rig.nowMs += InspectorWindowSet::kSettleMs;
        rig.set->Tick();

        Assert::AreEqual (1, rig.logs[0]->goneNotes);
        Assert::AreEqual (0, rig.logs[0]->reloads);
        Assert::AreEqual ((size_t) 1, rig.set->GetCount(), L"the window stays open");
    }



    TEST_METHOD (AClosedWindowSavesItsStateAndIsReleased)
    {
        Rig  rig;



        Assert::AreEqual (S_OK, rig.Open (kMist));

        rig.logs[0]->host->OnInspectorClosed();
        Assert::AreEqual (FakeInspectorWindow::kSavedX, rig.prefs.x);

        rig.set->Tick();
        Assert::AreEqual ((size_t) 0, rig.set->GetCount());

        Assert::AreEqual (S_OK, rig.Open (kMist));
        Assert::AreEqual ((size_t) 2, rig.logs.size(), L"a closed window is opened again, not fronted");
    }



    TEST_METHOD (EveryWindowClosesWithTheApp)
    {
        Rig  rig;



        Assert::AreEqual (S_OK, rig.Open (kMist));
        Assert::AreEqual (S_OK, rig.Open (kGame));

        rig.set->CloseAll();

        Assert::AreEqual ((size_t) 0, rig.set->GetCount());
        Assert::AreEqual (FakeInspectorWindow::kSavedX, rig.prefs.x);
        Assert::AreEqual ((size_t) 2, rig.watcher.unwatched.size());
    }



    TEST_METHOD (ExplorerSavesNoneOfTheDriveState)
    {
        DiskInspectorPrefs  prefs;
        JsonValue           json;
        bool                value = false;
        int                 drive = 0;



        prefs.w            = 1200;
        prefs.isFollowHead = true;
        prefs.isOpen       = true;
        prefs.lastDrive    = 1;
        json               = prefs.ToJson (false);

        Assert::IsFalse (json.HasBool ("followHead", value));
        Assert::IsFalse (json.HasBool ("open", value), L"Explorer's windows do not reopen at launch (FR-075)");
        Assert::IsFalse (json.HasInt ("lastDrive", drive));
        Assert::IsTrue  (prefs.ToJson (true).HasBool ("open", value));
    }



    TEST_METHOD (TheWalkthroughOpensTheFirstTimeOnly)
    {
        Rig  rig;



        Assert::AreEqual (S_OK, rig.Open (kMist));
        Assert::AreEqual (S_OK, rig.Open (kGame));

        Assert::AreEqual (1, rig.logs[0]->walkthroughs);
        Assert::AreEqual (0, rig.logs[1]->walkthroughs);
        Assert::IsTrue (rig.prefs.isWalkthroughSeen);
    }



    TEST_METHOD (TheWindowsHaveTheSetsOwnCopyOfTheTheme)
    {
        Rig             rig;
        DxuiLightTheme  light;



        Assert::AreEqual (S_OK, rig.Open (kMist));
        Assert::IsTrue (rig.logs[0]->theme != &rig.dark, L"an Explorer window's theme closes with it");
        Assert::AreEqual (rig.dark.Background(), rig.logs[0]->theme->Background());

        rig.set->SetTheme (light);
        Assert::IsTrue (rig.logs[0]->theme != &light);
        Assert::AreEqual (light.Background(), rig.logs[0]->theme->Background());
    }



    TEST_METHOD (OnlyTheFormatsTheInspectorReadsCanBeInspected)
    {
        for (const wchar_t * path : { L"a.dsk", L"a.DO", L"a.po", L"a.nib", L"a.nb2", L"a.woz" })
        {
            Assert::IsTrue (InspectorWindowSet::CanInspect (path), path);
        }

        for (const wchar_t * path : { L"a.2mg", L"a.hdv", L"a.zip", L"a", L"dsk" })
        {
            Assert::IsFalse (InspectorWindowSet::CanInspect (path), path);
        }
    }
};
