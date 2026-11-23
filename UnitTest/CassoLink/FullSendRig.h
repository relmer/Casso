#pragma once

#include "Pch.h"

#include "../EmuTests/FakeDiskFileIo.h"
#include "CassoLink/FullSend/FullSendLink.h"
#include "CassoLink/FullSend/FullSendProtection.h"
#include "CassoLink/FullSend/FullSendScreen.h"
#include "CassoLink/FullSend/FullSendSession.h"
#include "FullSendTestDisks.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendRig
//
//  A scripted Apple for FullSendSession: each drive's disk changes at
//  scripted events, and each key in the script ends a pause.
//
////////////////////////////////////////////////////////////////////////////////

namespace FullSendRig
{
    using namespace Microsoft::VisualStudio::CppUnitTestFramework;
    using namespace FullSendTestDisks;

    static constexpr const char * kFolder = "C:\\Box";

    struct ScriptedLink : IFullSendLink
    {
        std::array<Image, 2>                   drives;
        std::array<bool, 2>                    present             = { true, true };
        std::deque<FullSendKey>                keys;
        std::vector<int>                       diskReads;   // the drive of each whole-disk read, in order
        std::vector<bool>                      readsVerified;   // whether verification was on for each whole-disk read
        std::vector<int>                       trackDrives;   // the drive of each track read
        std::vector<FullSendScreen>            screens;
        std::vector<std::string>               reports;
        std::vector<std::pair<int, int>>       badSectors;   // reported on every disk read of badDisk
        Image                                  badDisk;
        int                                    waits               = 0;
        bool                                   begun               = false;
        bool                                   verifying           = false;
        std::vector<SectorMark>                reportMarks;
        int                                    reportVerified      = 0;
        int                                    reportEmpty         = 0;
        bool                                   bootstrapped        = false;
        FullSendKey                            beginKey            = FullSendKey::Space;
        std::vector<std::string>               entryLines;
        std::vector<std::vector<std::string>>  pauseLines;
        std::deque<Byte>                       typed;   // the codes of a line typed after P
        std::vector<std::vector<std::string>>  typedLines;
        bool                                   wholeTracksBad      = false;   // badDisk's tracks are not read whole in a check
        std::vector<int>                       tracksRead;   // the tracks of the whole-disk reads
        std::set<int>                          blankTracks;   // tracks with no sector that reads, as on some protected disks
        std::vector<int>                       captures;   // the drive of each raw capture
        int                                    capturesWithEarlier = 0;   // and how many were built with the capture before
        Image                                  rawWoz              = { 'W', 'O', 'Z', '2', 0xFF, 0x0A, 0x0D, 0x0A };
        Image                                  rawReads;   // a raw capture's reads, empty when they aren't saved
        std::string                            rawMarks;   // what a raw capture marked
        std::vector<std::string>               noted;   // the path of each image written
        std::array<Image, 2>                   rawTrack0;   // each drive's revolution for ProbeRaw
        std::vector<int>                       probes;   // the drive of each ProbeRaw
        std::function<void (const std::string &)>  onAwait;               // with one, each image is booted, and this is the wait for its result
        bool                                  bootTones      = false;
        std::array<bool, 2>                   writeProtected = { false, false };
        bool                                  measuring      = false;
        double                              * clock          = nullptr;   // the status's clock, which a disk read and a raw capture advance
        double                                readSeconds    = 0.0;
        double                                captureSeconds = 0.0;

        void  Advance (double seconds) { if (clock != nullptr) { *clock += seconds; } }
        std::vector<TrackLayout>              layouts;   // each track's, for a read when measuring and for MeasureLayouts
        std::vector<int>                      measures;  // the tracks of each MeasureLayouts

        //  Called at the end of the Nth whole-disk read, counted from 1, and
        //  at the Nth wait for a key.
        std::map<int, std::function<void (ScriptedLink &)>>  afterDiskRead;
        std::map<int, std::function<void (ScriptedLink &)>>  duringDiskRead;
        std::map<int, std::function<void (ScriptedLink &)>>  beforeKey;

        //  Reads of a drive's track return alternating fingerprints this many
        //  times, as a disk not seated.
        std::array<int, 2>                    unseatedReads  = { 0, 0 };
        HRESULT                               failDiskRead   = S_OK;

        bool  HasDrive (int drive) override { return present[(size_t) drive - 1]; }

        HRESULT  ReadTrack (int drive, int track, Image & ioImage, bool & outHasDisk, bool & outWhole) override
        {
            const Image  & disk = drives[(size_t) drive - 1];

            trackDrives.push_back (drive);
            outHasDisk = !disk.empty() && !blankTracks.contains (track);
            outWhole   = outHasDisk && !(disk == badDisk && wholeTracksBad) && unseatedReads[(size_t) drive - 1] == 0;

            if (outHasDisk)
            {
                std::copy_n (disk.begin() + (ptrdiff_t) (track * kTrackBytes), kTrackBytes, ioImage.begin() + (ptrdiff_t) (track * kTrackBytes));

                if (unseatedReads[(size_t) drive - 1] > 0)
                {
                    ioImage[(size_t) track * kTrackBytes + 200] = (Byte) unseatedReads[(size_t) drive - 1]--;
                }
            }

            return S_OK;
        }

        bool  IsVerifying  () override { return verifying; }
        void  ToggleVerify () override { verifying = !verifying; }
        void  SetMeasuring (bool on) override { measuring = on; }

        HRESULT  MeasureLayouts (int drive, int tracks, std::vector<TrackLayout> & outLayouts) override
        {
            UNREFERENCED_PARAMETER (drive);

            measures.push_back (tracks);
            outLayouts.assign (layouts.begin(), layouts.begin() + (ptrdiff_t) std::min ((size_t) tracks, layouts.size()));
            return S_OK;
        }

        HRESULT  ReadDisk (int drive, Image & ioImage, std::span<const int> haveTracks, std::vector<std::pair<int, int>> & outUnreadable, FullSendReadReport & outReport) override
        {
            outReport.marks          = reportMarks;
            outReport.tracksVerified = reportVerified;
            outReport.emptyTracks    = reportEmpty;
            outReport.layouts        = measuring ? layouts : std::vector<TrackLayout>();

            int  read = (int) diskReads.size() + 1;

            diskReads.push_back (drive);
            readsVerified.push_back (verifying);
            Advance (readSeconds);

            if (failDiskRead != S_OK)
            {
                return failDiskRead;
            }

            if (duringDiskRead.contains (read))
            {
                duringDiskRead[read] (*this);
            }

            for (int track = 0; track < 35; track++)
            {
                if (std::find (haveTracks.begin(), haveTracks.end(), track) == haveTracks.end())
                {
                    tracksRead.push_back (track);
                    std::copy_n (drives[(size_t) drive - 1].begin() + (ptrdiff_t) (track * kTrackBytes), kTrackBytes, ioImage.begin() + (ptrdiff_t) (track * kTrackBytes));
                }
            }

            if (ioImage == badDisk)
            {
                outUnreadable = badSectors;
            }

            if (afterDiskRead.contains (read))
            {
                afterDiskRead[read] (*this);
            }

            return S_OK;
        }

        HRESULT  ShowScreen (const FullSendScreen & screen) override
        {
            screens.push_back (screen);
            return S_OK;
        }

        bool     IsBootstrapped() override { return bootstrapped; }

        HRESULT  CaptureRaw (int drive, bool withEarlier, Image & outWoz, Image & outReads, std::string & outMarks) override
        {
            captures.push_back (drive);
            Advance (captureSeconds);
            capturesWithEarlier += withEarlier ? 1 : 0;
            outWoz   = rawWoz;
            outReads = rawReads;
            outMarks = rawMarks;
            return S_OK;
        }

        //  A drive's raw track 0, a revolution that repeats, or empty for no
        //  disk; the same revolution again is the same disk.
        HRESULT  ProbeRaw (int drive, const Image & previous, Image & outRevolution, bool & outSame) override
        {
            probes.push_back (drive);
            outRevolution = rawTrack0[(size_t) drive - 1];
            outSame       = !previous.empty() && previous == outRevolution;
            return S_OK;
        }

        bool     NoteImage (const std::string & path) override { noted.push_back (path); return (bool) onAwait; }

        bool  HasBootTones     () override { return bootTones; }
        bool  IsWriteProtected (int drive) override { return writeProtected[(size_t) drive - 1]; }

        HRESULT  AwaitBoot (const std::string & file) override
        {
            onAwait (file);
            return S_OK;
        }

        HRESULT  WaitForKey (const std::vector<std::string> & lines, FullSendKey & outKey) override
        {
            //  The first wait is the one that begins the session.
            if (!begun)
            {
                begun      = true;
                entryLines = lines;
                outKey     = beginKey;
                return S_OK;
            }

            pauseLines.push_back (lines);
            waits++;

            if (beforeKey.contains (waits))
            {
                beforeKey[waits] (*this);
            }

            if (keys.empty())
            {
                return HRESULT_FROM_WIN32 (ERROR_CANCELLED);
            }

            outKey = keys.front();
            keys.pop_front();
            return S_OK;
        }

        HRESULT  WaitForAnyKey (const std::vector<std::string> & lines, Byte & outCode) override
        {
            typedLines.push_back (lines);

            if (typed.empty())
            {
                return HRESULT_FROM_WIN32 (ERROR_CANCELLED);
            }

            outCode = typed.front();
            typed.pop_front();
            return S_OK;
        }

        void  Report (const std::string & text) override { reports.push_back (text); }
    };

    struct Rig
    {
        Rig() { link.clock = &now; }

        ScriptedLink               link;
        FakeDiskFileIo             io;
        std::string                extension;
        std::string                prefix;
        double                     now        = 0.0;
        FullSendStatus             status     { [this] () { return now; }, [] () { return std::string(); } };
        FullSendProtectionChoices  protection;

        std::unique_ptr<FullSendSession>  Run (HRESULT result = HRESULT_FROM_WIN32 (ERROR_CANCELLED))
        {
            auto  session = std::make_unique<FullSendSession> (link, io, [] () { return std::string ("2026-10-30T12:00:00Z"); }, kFolder, extension, prefix, &status);

            session->SetProtection (protection);

            Assert::AreEqual (result, session->Run());
            Assert::IsTrue (session->GetState() == FullSendSession::State::Ended);

            return session;
        }

        std::vector<std::string>  Files() const
        {
            std::vector<std::string>  files;

            for (const auto & [path, bytes] : io.files)
            {
                files.push_back (std::filesystem::path (path).filename().string());
            }

            std::sort (files.begin(), files.end());

            return files;
        }
    };
}