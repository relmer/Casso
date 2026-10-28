#include "Pch.h"
#include "EmuTests/EventCacheRebuild.h"
#include "EmuTests/InlineWorkQueue.h"
#include "EmuTests/ReverseSessionRig.h"
#include "Debugger/Events/DebugEventHistory.h"
#include "Debugger/Events/MachineEventFeed.h"
#include "Debugger/Reverse/ReverseController.h"
#include "Devices/Acia6551.h"
#include "Machines/Apple2/Common/IDisk2EventSink.h"
#include "Shell/DebugEventLogs.h"
#include "Shell/ScratchEventReplayer.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebugEventLogsTests
{
    static constexpr size_t  kSliceCycles = 17030;



    //  Runs the rig's loop for count slices, the host feeding it input at the
    //  start of each.
    static void  Run (TestMachine & machine, size_t firstSlice, size_t count)
    {
        size_t  slice = 0;
        size_t  spent = 0;



        for (slice = firstSlice; slice < firstSlice + count; slice++)
        {
            ReverseSessionRig::Feed (machine, slice);

            for (spent = 0; spent < kSliceCycles; )
            {
                spent += machine.StepOne();
            }
        }
    }


    //  The rows of a delta that came from the cache: not the journal's.
    static size_t  CountCached (const std::vector<InputLogRow> & rows)
    {
        return std::ranges::count_if (rows, [] (const InputLogRow & row)
        {
            return row.kind == DebugLogRowKind::Gap || (row.kind == DebugLogRowKind::Event && row.event.category == InputEventCategory::Guest);
        });
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PanelRecorder
    //
    //  The Disk II events on the Panel slot, which gets the same calls as the
    //  History slot, each stored with the machine's position and cycle.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class PanelRecorder : public IDisk2EventSink
    {
    public:
        struct Seen
        {
            uint64_t  position = 0;
            uint64_t  cycle    = 0;
        };

        explicit PanelRecorder (MachineHost & machine) : m_machine (machine) {}

        std::vector<Seen>  seen;

        void OnMotorCommandOn() override                    { Add(); }
        void OnMotorEngaged() override                      { Add(); }
        void OnMotorCommandOff() override                   { Add(); }
        void OnMotorDisengaged() override                   { Add(); }
        void OnHeadStep        (int, int) override          { Add(); }
        void OnHeadBump        (int) override               { Add(); }
        void OnAddressMark     (int, int, int) override     { Add(); }
        void OnDataMarkRead    (int, int, int, std::span<const Byte>) override { Add(); }
        void OnDataMarkWrite   (int, int, int, std::span<const Byte>) override { Add(); }
        void OnReadStall       (int) override               { Add(); }
        void OnDriveSelect     (int) override               { Add(); }
        void OnDiskInserted    (int) override               { Add(); }
        void OnDiskEjected     (int) override               { Add(); }

    private:
        void  Add()
        {
            seen.push_back ({ m_machine.GetPosition(), m_machine.GetCpu()->GetTotalCycles() });
        }

        MachineHost  & m_machine;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebugEventLogsTests
    //
    //  The Disk II event log and the input log on the rig's session: the
    //  caches have the entries a rebuild from history returns, take nothing
    //  while the machine replays its own history or while recording is
    //  paused, drop a future a change in the past discarded, and send an
    //  open pane each row once, and older rows rebuilt on request.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebugEventLogsTests)
    {
    public:

        struct Session
        {
            TestMachine        machine    { "Apple2e" };
            ReverseController  controller { machine };
            InlineWorkQueue    queue;
            DebugEventLogs     logs       { machine };

            Session()
            {
                HRESULT  hr = S_OK;



                ReverseSessionRig::Prepare (machine);

                hr = controller.Start (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames));
                AssertSucceeded (hr, L"Start");

                logs.SetWorkQueue (&queue);
            }

            ~Session()
            {
                logs.Stop();
            }

            void  WaitForKeyframes()
            {
                HRESULT  hr = controller.GetKeyframes().WaitForPending();



                AssertSucceeded (hr, L"WaitForPending");
            }
        };


        template <typename T>
        static void  AssertCacheIsRebuilt (Session & session, const DebugEventCache<T> & cache, std::unique_ptr<IDebugEventProbe<T>> probe, size_t & outEvents)
        {
            AssertCacheIsRebuilt<T> (session.machine, session.controller, cache, std::move (probe), outEvents);
        }


        template <typename T>
        static void  AssertCacheIsRebuilt (TestMachine & machine, ReverseController & controller, const DebugEventCache<T> & cache, std::unique_ptr<IDebugEventProbe<T>> probe, size_t & outEvents)
        {
            EventCacheRebuild::AssertMatches<T> (machine, controller, cache, std::move (probe), outEvents);
        }


        TEST_METHOD (TheCachesHoldWhatARebuildGives)
        {
            Session  session;
            size_t   disk    = 0;
            size_t   input   = 0;
            size_t   video   = 0;
            size_t   sound   = 0;



            session.logs.Start (&session.controller);
            Run (session.machine, 0, 300);
            session.WaitForKeyframes();

            AssertCacheIsRebuilt<Disk2Event> (session, session.logs.GetDiskCache(),  std::make_unique<DiskEventProbe>(),       disk);
            AssertCacheIsRebuilt<InputEvent> (session, session.logs.GetInputCache(), std::make_unique<InputEventProbe>(),      input);
            AssertCacheIsRebuilt<IoEvent>    (session, session.logs.GetVideoCache(), std::make_unique<IoEventProbe> (IoLogKind::Video), video);
            AssertCacheIsRebuilt<IoEvent>    (session, session.logs.GetSoundCache(), std::make_unique<IoEventProbe> (IoLogKind::Sound), sound);

            Assert::IsTrue   (disk > 0,  L"the loop ran the disk");
            Assert::IsTrue   (input > 0, L"and polled the keyboard and paddle");
            Assert::IsTrue   (sound > 0, L"and wrote the Mockingboard");
            Assert::AreEqual ((size_t) 0, video, L"and left the video mode alone");
        }


        //  A //c sends a byte through its first port's loopback plug and reads
        //  it back, again and again: the serial log's cache has the entries a
        //  rebuild from history makes, bytes sent and received and the
        //  settings and lines among them.
        TEST_METHOD (TheSerialCacheHasWhatARebuildGives)
        {
            static constexpr Byte  kProgram[] =
            {
                0xA9, 0x0B,             // LDA #$0B     DTR on, no interrupts
                0x8D, 0x9A, 0xC0,       // STA $C09A
                0xA9, 0x1E,             // LDA #$1E     9600 baud, 8 bits, 1 stop bit
                0x8D, 0x9B, 0xC0,       // STA $C09B
                0xA2, 0x00,             // LDX #$00
                0x8E, 0x98, 0xC0,       // STX $C098    loop: send X
                0xAD, 0x98, 0xC0,       // LDA $C098    and read what came back
                0xE8,                   // INX
                0xA0, 0x00,             // LDY #$00
                0x88,                   // DEY          a character time and more
                0xD0, 0xFD,             // BNE -3
                0x4C, 0x0C, 0x08,       // JMP loop
            };

            TestMachine        machine    ("Apple2c");
            ReverseController  controller (machine);
            InlineWorkQueue    queue;
            DebugEventLogs     logs       (machine);
            HRESULT            hr         = S_OK;
            size_t             serial     = 0;
            size_t             i          = 0;
            size_t             spent      = 0;



            for (i = 0; i < std::size (kProgram); i++)
            {
                machine.GetMemoryBus().WriteByte ((Word) (0x0800 + i), kProgram[i]);
            }

            machine.GetCpu()->SetPC (0x0800);

            hr = controller.Start (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames));
            AssertSucceeded (hr, L"Start");

            logs.SetWorkQueue (&queue);
            logs.Start (&controller);

            for (spent = 0; spent < 100 * kSliceCycles; )
            {
                spent += machine.StepOne();
            }

            hr = controller.GetKeyframes().WaitForPending();
            AssertSucceeded (hr, L"WaitForPending");

            AssertCacheIsRebuilt<IoEvent> (machine, controller, logs.GetSerialCache(), std::make_unique<IoEventProbe> (IoLogKind::Serial), serial);

            Assert::IsTrue (serial > 100, L"the port sent and received bytes");
            Assert::IsTrue (std::ranges::any_of (Entries (logs.GetSerialCache()), [] (const DebugEventEntry<IoEvent> & e) { return e.event.kind == IoEventKind::SerialReceived; }), L"received bytes are listed");
            Assert::IsTrue (std::ranges::any_of (Entries (logs.GetSerialCache()), [] (const DebugEventEntry<IoEvent> & e) { return e.event.kind == IoEventKind::SerialSettings; }), L"so are the settings");

            logs.Stop();
        }


        static std::vector<DebugEventEntry<IoEvent>>  Entries (const DebugEventCache<IoEvent> & cache)
        {
            std::vector<DebugEventEntry<IoEvent>>  entries;



            cache.CopySpan (0, UINT64_MAX, entries);
            return entries;
        }


        //  A machine made live at a point in history goes on with that point's
        //  active drive: a keyframe's state is loaded with no drive select,
        //  so the feed reads the drive again from the controller.
        TEST_METHOD (GoingLiveInThePastTakesThatPointsDrive)
        {
            Session                                     session;
            ReverseResult                               result;
            std::vector<DebugEventEntry<Disk2Event>>    entries;
            HRESULT                                     hr      = S_OK;
            uint64_t                                    past    = 0;
            uint64_t                                    live    = 0;
            Disk2Controller                           * disk    = session.machine.GetRefs().diskController;



            Assert::IsNotNull (disk, L"a Disk II");

            session.logs.Start (&session.controller);
            Run (session.machine, 0, 30);
            session.WaitForKeyframes();

            Assert::AreEqual (0, disk->GetActiveDrive(), L"drive 1 while the loop runs");

            past = session.machine.GetPosition();
            Run (session.machine, 30, 30);

            (void) session.machine.GetMemoryBus().ReadByte (0xC0EB);
            Assert::AreEqual (1, disk->GetActiveDrive(), L"drive 2 selected live");

            hr = session.controller.SeekToPosition (past, result);
            AssertSucceeded (hr, L"SeekToPosition");

            Assert::AreEqual (0, disk->GetActiveDrive(), L"drive 1 again in the past");

            hr = session.controller.OnMachineChanged();
            AssertSucceeded (hr, L"OnMachineChanged");

            live = session.machine.GetPosition();
            (void) session.machine.GetMemoryBus().ReadByte (0xC0E9);

            session.logs.GetDiskCache().CopySpan (live, UINT64_MAX, entries);

            Assert::IsFalse  (entries.empty(), L"the motor command is logged");
            Assert::AreEqual (0, (int) entries.back().event.drive, L"on drive 1");
        }


        //  A listing of the sound log includes entries from before the cache,
        //  rebuilt from history on the spot; a clear hides the entries before it.
        TEST_METHOD (ASoundLogListingIsRebuiltFromHistoryBeforeTheCache)
        {
            Session       session;
            IoLogEntries  listing;
            uint64_t      start   = 0;
            uint64_t      last    = 0;
            size_t        older   = 0;
            HRESULT       hr      = S_OK;



            Run (session.machine, 0, 200);

            start = session.machine.GetPosition();
            session.logs.Start (&session.controller);

            Run (session.machine, 200, 30);
            session.WaitForKeyframes();

            hr = session.logs.GetIoEntries (IoLogKind::Sound, SIZE_MAX, listing);
            AssertSucceeded (hr, L"GetIoEntries");

            for (const DebugEventEntry<IoEvent> & entry : listing.entries)
            {
                Assert::IsTrue (entry.position >= last, L"oldest first");

                last   = entry.position;
                older += (entry.kind == DebugEventEntryKind::Event && entry.position < start) ? 1 : 0;
            }

            Assert::IsTrue (older > 0, L"entries from before the cache began, rebuilt");
            Assert::IsTrue (listing.entries.size() > older, L"and the cache's");

            session.logs.ClearIoLog (IoLogKind::Sound);
            Run (session.machine, 230, 2);

            hr = session.logs.GetIoEntries (IoLogKind::Sound, SIZE_MAX, listing);
            AssertSucceeded (hr, L"GetIoEntries after the clear");

            Assert::IsTrue (listing.isComplete && listing.wasCleared, L"every entry since the clear");
            Assert::IsTrue (!listing.entries.empty() && listing.entries.size() < older, L"only the newest two slices'");
        }


        //  A stretch of history that fails to replay, here because the scratch
        //  machine does not build, is an Unavailable entry in the listing,
        //  and the listing succeeds with the cache's entries.
        TEST_METHOD (AStretchThatFailsToReplayIsListedAsUnavailable)
        {
            Session       session;
            IoLogEntries  listing;
            HRESULT       hr      = S_OK;



            Run (session.machine, 0, 200);

            //  Start copies the running machine's configuration to the
            //  scratch machine; a system ROM that is not there makes it fail
            //  to build.
            session.machine.GetConfig().systemRom.resolvedPath = "C:\\No such folder\\No such ROM.rom";
            session.logs.Start (&session.controller);

            Run (session.machine, 200, 30);
            session.WaitForKeyframes();

            hr = session.logs.GetIoEntries (IoLogKind::Sound, SIZE_MAX, listing);
            AssertSucceeded (hr, L"GetIoEntries");

            Assert::IsTrue (std::ranges::any_of (listing.entries, [] (const DebugEventEntry<IoEvent> & entry) { return entry.kind == DebugEventEntryKind::Unavailable; }),
                            L"the stretch that failed is an Unavailable entry");
            Assert::IsTrue (std::ranges::any_of (listing.entries, [] (const DebugEventEntry<IoEvent> & entry) { return entry.kind == DebugEventEntryKind::Event; }),
                            L"with the cache's entries after it");
        }


        //  A rewind behind a clear lowers the clear to where the machine went
        //  back to, so the entries made from there are listed.
        TEST_METHOD (ARewindBehindAClearListsWhatRunsFromThere)
        {
            Session        session;
            IoLogEntries   listing;
            ReverseResult  result;
            uint64_t       rewound  = 0;
            uint64_t       cleared  = 0;
            HRESULT        hr       = S_OK;
            bool           isListed = false;



            session.logs.Start (&session.controller);
            Run (session.machine, 0, 10);

            rewound = session.machine.GetPosition();
            Run (session.machine, 10, 20);

            cleared = session.machine.GetPosition();
            session.logs.ClearIoLog (IoLogKind::Sound);
            Run (session.machine, 30, 2);
            session.WaitForKeyframes();

            hr = session.controller.SeekToPosition (rewound, result);
            AssertSucceeded (hr, L"SeekToPosition");

            hr = session.logs.GetIoEntries (IoLogKind::Sound, SIZE_MAX, listing);
            AssertSucceeded (hr, L"GetIoEntries");

            isListed = std::ranges::any_of (listing.entries, [rewound, cleared] (const DebugEventEntry<IoEvent> & entry)
            {
                return entry.kind == DebugEventEntryKind::Event && entry.position >= rewound && entry.position < cleared;
            });

            Assert::IsTrue (isListed, L"entries from the rewound position on, before the old clear");
        }


        TEST_METHOD (NothingIsCachedWhileTheMachineReplaysItsHistory)
        {
            Session        session;
            uint64_t       diskEnd  = 0;
            uint64_t       inputEnd = 0;
            int            i        = 0;
            HRESULT        hr       = S_OK;
            ReverseResult  result;



            session.logs.Start (&session.controller);
            Run (session.machine, 0, 120);
            session.WaitForKeyframes();

            diskEnd  = session.logs.GetDiskCache().GetEndSerial();
            inputEnd = session.logs.GetInputCache().GetEndSerial();

            for (i = 0; i < 3; i++)
            {
                hr = session.controller.StepBack (result);
                AssertSucceeded (hr, L"StepBack");
            }

            hr = session.controller.SeekToPosition (session.machine.GetPosition() - 5000, result);
            AssertSucceeded (hr, L"SeekToPosition");

            Assert::IsTrue   (session.controller.IsInHistory());
            Assert::AreEqual (diskEnd,  session.logs.GetDiskCache().GetEndSerial(),  L"no disk event cached behind live");
            Assert::AreEqual (inputEnd, session.logs.GetInputCache().GetEndSerial(), L"no input event cached behind live");
        }


        //  A change made in the past cuts history at the machine's position:
        //  the cache drops its entries from there, and the next delta drops
        //  an open pane's rows from there.
        TEST_METHOD (ACutDropsTheDiscardedFutureAndTellsThePane)
        {
            Session                              session;
            const DebugEventCache<InputEvent>  & cache   = session.logs.GetInputCache();
            uint64_t                             cut     = 0;
            HRESULT                              hr      = S_OK;
            ReverseResult                        result;
            InputLogDelta                        delta;



            session.logs.Start (&session.controller);
            session.logs.SetOpen (DebugLogKind::Input, true);
            Run (session.machine, 0, 200);
            session.WaitForKeyframes();

            session.logs.TakeDelta (delta);
            Assert::IsTrue (delta.isReset,        L"the pane's first delta");
            Assert::IsTrue (cache.GetCount() > 4, L"the loop polled the keyboard and paddle");

            cut = cache.GetAt (cache.GetCount() / 2).position;

            hr = session.controller.SeekToPosition (cut, result);
            AssertSucceeded (hr, L"SeekToPosition");

            hr = session.controller.OnMachineChanged();
            AssertSucceeded (hr, L"OnMachineChanged");

            Assert::IsTrue (cache.GetAt (cache.GetCount() - 1).position < cut, L"nothing cached from the cut on");

            session.logs.TakeDelta (delta);
            Assert::AreEqual (cut, delta.cutFrom, L"the pane drops its rows from the cut on");
        }


        //  At Maximum speed recording pauses: nothing is cached until it
        //  resumes, where a gap entry is.
        TEST_METHOD (AGapIsMarkedWhereRecordingResumes)
        {
            Session                              session;
            const DebugEventCache<InputEvent>  & cache   = session.logs.GetInputCache();
            uint64_t                             paused  = 0;
            uint64_t                             resumed = 0;
            size_t                               gaps    = 0;
            size_t                               i       = 0;
            HRESULT                              hr      = S_OK;



            session.logs.Start (&session.controller);
            Run (session.machine, 0, 60);

            hr = session.controller.SetUserMaximumSpeed (true);
            AssertSucceeded (hr, L"SetUserMaximumSpeed");

            paused = session.machine.GetPosition();
            Run (session.machine, 60, 60);
            resumed = session.machine.GetPosition();

            hr = session.controller.SetUserMaximumSpeed (false);
            AssertSucceeded (hr, L"SetUserMaximumSpeed");

            Run (session.machine, 120, 60);


            for (i = 0; i < cache.GetCount(); i++)
            {
                const DebugEventEntry<InputEvent>  & entry = cache.GetAt (i);

                if (entry.kind == DebugEventEntryKind::Gap)
                {
                    Assert::AreEqual (resumed, entry.position, L"the gap is where recording resumed");
                    gaps++;
                }
                else
                {
                    Assert::IsFalse (entry.position >= paused && entry.position < resumed, L"no event from the gap");
                }
            }

            Assert::AreEqual ((size_t) 1, gaps);
        }


        //  A byte a serial port takes from a host attachment is host input, a
        //  row of the input log with the port and the byte.
        TEST_METHOD (ABytePushedToASerialPortIsAHostRow)
        {
            static constexpr Byte  kByte      = 'A';
            TestMachine            machine    ("Apple2c");
            ReverseController      controller (machine);
            DebugEventLogs         logs       (machine);
            InputLogDelta          delta;
            const InputLogRow    * row        = nullptr;
            HRESULT                hr         = S_OK;



            hr = controller.Start (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames));
            AssertSucceeded (hr, L"Start");

            logs.Start   (&controller);
            logs.SetOpen (DebugLogKind::Input, true);

            machine.GetRefs().serialChips[1]->EnqueueReceivedBytes (&kByte, 1);
            machine.RunCycles (kSliceCycles);

            logs.TakeDelta (delta);
            logs.Stop();

            for (const InputLogRow & each : delta.newer)
            {
                row = (each.kind == DebugLogRowKind::Event && each.event.type == InputEventType::HostSerialByte) ? &each : row;
            }

            Assert::IsNotNull (row, L"the byte is a row");
            Assert::IsTrue    (row->event.category == InputEventCategory::Host, L"of host input");
            Assert::AreEqual<Byte>     (kByte, row->event.payload.io.value, L"with the byte");
            Assert::AreEqual<uint16_t> (1,     row->event.payload.io.address, L"and the port's index");
        }


        //  A pane gets everything when it opens, then each new row once.
        TEST_METHOD (AnOpenPaneGetsEachRowOnce)
        {
            Session                              session;
            const DebugEventCache<InputEvent>  & cache   = session.logs.GetInputCache();
            InputLogDelta                        first;
            InputLogDelta                        second;
            InputLogDelta                        third;



            session.logs.Start (&session.controller);
            Run (session.machine, 0, 30);

            session.logs.SetOpen (DebugLogKind::Input, true);
            session.logs.TakeDelta (first);

            Run (session.machine, 30, 30);
            session.logs.TakeDelta (second);
            session.logs.TakeDelta (third);

            Assert::IsTrue   (first.isReset);
            Assert::IsFalse  (second.isReset);
            Assert::IsFalse  (first.newer.empty() || second.newer.empty(), L"rows in both");
            Assert::IsTrue   (first.newer.back().position <= second.newer.front().position, L"the second follows the first");
            Assert::AreEqual (cache.GetCount(), CountCached (first.newer) + CountCached (second.newer), L"every cached entry once");
            Assert::IsTrue   (third.IsEmpty(), L"nothing new");
        }


        //  The sound log's pane (FR-198) gets its rows as the other logs' panes
        //  do. SOUNDLOG CLEAR starts it again from the machine's position: it
        //  drops its rows, gets only what comes after, is at the log's start,
        //  and a request for older rows rebuilds nothing from before the clear.
        TEST_METHOD (AClearedSoundPaneStartsAgainFromTheClear)
        {
            Session     session;
            IoLogDelta  first;
            IoLogDelta  second;
            IoLogDelta  third;
            uint64_t    cleared = 0;



            session.logs.Start (&session.controller);
            Run (session.machine, 0, 30);

            session.logs.SetOpen (DebugLogKind::Sound, true);
            Assert::IsTrue  (session.logs.TakeDelta (DebugLogKind::Sound, first));
            Assert::IsTrue  (first.isReset);
            Assert::IsFalse (first.newer.empty(), L"the sound the machine made");

            cleared = session.machine.GetPosition();
            session.logs.ClearIoLog (IoLogKind::Sound);
            Run (session.machine, 30, 2);

            Assert::IsTrue  (session.logs.TakeDelta (DebugLogKind::Sound, second));
            Assert::IsTrue  (second.isReset, L"the pane drops what it had");
            Assert::IsFalse (second.newer.empty(), L"and gets the sound since");
            Assert::IsTrue  (std::ranges::all_of (second.newer, [cleared] (const IoLogRow & row) { return row.position >= cleared; }), L"nothing from before the clear");
            Assert::IsTrue  (second.isAtStart, L"the log starts at the clear");

            session.logs.RequestEarlier (DebugLogKind::Sound);
            (void) session.logs.TakeDelta (DebugLogKind::Sound, third);

            Assert::IsTrue  (third.older.empty(), L"no rows rebuilt from before the clear");
            Assert::IsFalse (third.isRebuilding);
        }


        //  The video and sound logs are only TakeDelta's I/O kinds.
        TEST_METHOD (TheIoDeltaIsOnlyForTheVideoAndSoundLogs)
        {
            Session     session;
            IoLogDelta  delta;



            session.logs.Start (&session.controller);
            session.logs.SetOpen (DebugLogKind::Disk, true);

            Assert::IsFalse (session.logs.TakeDelta (DebugLogKind::Disk, delta));
            Assert::IsTrue  (delta.IsEmpty());
        }


        //  After a request for the rows before its oldest, the pane gets the
        //  rows of history rebuilt back from there, the events the machine
        //  reported then.
        TEST_METHOD (EarlierRowsAreRebuiltFromHistory)
        {
            Session                            session;
            PanelRecorder                      recorder (session.machine);
            uint64_t                           start    = 0;
            size_t                             tries    = 0;
            std::vector<DiskLogRow>            older;
            std::vector<PanelRecorder::Seen>   reported;
            DiskLogDelta                       delta;
            size_t                             i        = 0;



            session.machine.AttachObservers ({ &recorder, nullptr }, ObserverSlot::Panel);

            Run (session.machine, 0, 200);

            start = session.machine.GetPosition();
            session.logs.Start (&session.controller);
            session.logs.SetOpen (DebugLogKind::Disk, true);

            Run (session.machine, 200, 30);
            session.WaitForKeyframes();

            session.logs.TakeDelta (delta);
            Assert::AreEqual (start, delta.heldFrom, L"the cache starts where the log started");
            Assert::IsFalse  (delta.isAtStart,       L"history starts earlier");

            session.logs.RequestEarlier (DebugLogKind::Disk);

            for (tries = 0; tries < 10 && older.empty(); tries++)
            {
                session.logs.TakeDelta (delta);
                session.queue.WaitAll();
                session.logs.Pump();

                older = delta.older;
            }

            session.logs.TakeDelta (delta);
            older.insert (older.end(), delta.older.begin(), delta.older.end());

            session.machine.AttachObservers ({}, ObserverSlot::Panel);

            Assert::IsFalse (older.empty(), L"rows were rebuilt");

            std::ranges::copy_if (recorder.seen, std::back_inserter (reported), [&delta, start] (const PanelRecorder::Seen & seen)
            {
                return seen.position >= delta.heldFrom && seen.position < start;
            });

            std::erase_if (older, [] (const DiskLogRow & row) { return row.kind != DebugLogRowKind::Event; });

            Assert::AreEqual (reported.size(), older.size(), L"the events the machine reported then");

            for (i = 0; i < older.size(); i++)
            {
                Assert::AreEqual (reported[i].position, older[i].position);
                Assert::AreEqual (reported[i].cycle,    older[i].cycle);
            }
        }


        //  The sound log's pane gets rows rebuilt from history as the disk
        //  log's does, and the rebuild finishes.
        TEST_METHOD (EarlierSoundRowsAreRebuiltFromHistory)
        {
            Session     session;
            IoLogDelta  delta;
            size_t      tries    = 0;
            size_t      older    = 0;



            Run (session.machine, 0, 200);

            session.logs.Start (&session.controller);
            session.logs.SetOpen (DebugLogKind::Sound, true);

            Run (session.machine, 200, 30);
            session.WaitForKeyframes();

            session.logs.TakeDelta (DebugLogKind::Sound, delta);
            Assert::IsFalse (delta.isAtStart, L"history starts earlier");

            session.logs.RequestEarlier (DebugLogKind::Sound);

            for (tries = 0; tries < 10 && (older == 0 || delta.isRebuilding); tries++)
            {
                session.logs.TakeDelta (DebugLogKind::Sound, delta);
                session.queue.WaitAll();
                session.logs.Pump();

                older += delta.older.size();
            }

            Assert::IsTrue  (older > 0, L"rows were rebuilt");
            Assert::IsFalse (delta.isRebuilding, L"and the rebuild finished");
        }
    };
}
