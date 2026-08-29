#pragma once

#include "Pch.h"

#include "DiskImage.h"
#include "MountDiagnosis.h"
#include "NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore
//
//  Mount/Eject coordinator. Owns the DiskImage instances mounted across
//  every (slot, drive) pair. Routes Mount() requests by file extension:
//      .dsk → NibblizationLayer (DOS 3.3 sector order)
//      .do  → NibblizationLayer (DOS 3.3 sector order)
//      .po  → NibblizationLayer (ProDOS sector order)
//      .woz → WozLoader (native bit-stream)
//
//  Auto-flush invariants (FR-025):
//      Eject(slot, drive) — flush dirty image, then release.
//      FlushAll()         — flush every dirty mount; called on machine
//                           switch and on emulator exit / PowerCycle.
//      SoftReset()        — keep mounts; flush dirty (Phase 4 contract).
//      PowerCycle()       — unmount everything (auto-flush each).
//
//  Test hooks: SetFlushSink redirects serialized bytes to an in-memory
//  capture buffer instead of the host filesystem, and SetImageReader does
//  the same for reads, so the IFixtureProvider isolation contract is
//  preserved even for a read-modify-write.
//
//  There is no way to force a flush past the dirty and write-protect gates.
//  The one caller that wanted that was changing a WOZ's write-protect flag,
//  which lives inside the file; it now goes through SetImageWriteProtect,
//  which patches the single byte instead of rebuilding the image around it.
//
//  Flush-error reporting: a flush that was supposed to persist a dirty
//  image but couldn't (serialize failure, or the file/sink write failed)
//  used to vanish -- every caller drops FlushEntry's HRESULT (Eject and
//  PowerCycle are void; the shell/SoftReset IGNORE_RETURN_VALUE it), so a
//  user's writes could be silently lost. FlushEntry now surfaces the loss
//  itself through the shared EHM notifier (CHRN/CBRN -> EhmNotifyUser),
//  which routes to whatever handler the host registered (a MessageBox in
//  the GUI, stderr headless), so the report reaches the user regardless of
//  what the caller does with the return.
//
////////////////////////////////////////////////////////////////////////////////

//
//  What salvaging a bay would produce, and whether it is worth offering.
//  Read-only: producing one writes nothing and touches nothing.
//
//      isOffered      the disk is damaged AND has ordinary 16-sector
//                     structure. Both halves matter. A disk that is not
//                     damaged is not write-protected, so there is nothing to
//                     escape and a lossy copy would only lose data; a
//                     copy-protected disk has no standard sectors to salvage,
//                     and rebuilding it from sectors would destroy the very
//                     tracks that make it work.
//      totalSectors   sectors on the tracks this disk actually has -- 320 on a
//                     20-track disk, not a flat 560, or the count is a lie.
//      suggestedPath  "<name>.salvaged.woz" beside the original.
//
struct SalvageAssessment
{
    bool              isOffered     = false;
    int               totalSectors  = 0;
    DenibblizeReport  report;
    string            suggestedPath;
};





class DiskImageStore
{
public:
    using FlushSink   = std::function<HRESULT (const string &, const vector<Byte> &)>;
    using ImageReader = std::function<HRESULT (const string &, vector<Byte> &)>;

    static constexpr int   kSlotCount  = 8;
    static constexpr int   kDriveCount = 2;

    DiskImageStore ();

    HRESULT       Mount             (int slot, int drive, const string & path);
    HRESULT       MountFromBytes    (int slot, int drive, const string & virtualPath,
                                     DiskFormat fmt, const vector<Byte> & bytes);

    //  The same two mounts, reporting WHY a refusal happened as well as that
    //  it did. A caller that has a user to answer to wants these; one that
    //  only needs to know whether it worked keeps the shorter forms above.
    //  The diagnosis is written on success too, as None, so a caller cannot
    //  read a stale reason off a mount that worked.
    HRESULT       Mount             (int slot, int drive, const string & path,
                                     MountDiagnosis & outDiagnosis);
    HRESULT       MountFromBytes    (int slot, int drive, const string & virtualPath,
                                     DiskFormat fmt, const vector<Byte> & bytes,
                                     MountDiagnosis & outDiagnosis);
    void          Eject             (int slot, int drive);
    HRESULT       Flush             (int slot, int drive);
    HRESULT       FlushAll          ();

    //  Sets a mounted WOZ's write-protect flag in its backing file by patching
    //  the single byte that carries it -- read the file, set INFO's flag byte,
    //  recompute the header CRC, write it back atomically -- rather than
    //  re-serializing the image.
    //
    //  This replaced a ForceFlush that pushed the flag through the full
    //  rebuild-from-model writer. That writer is correct for a flush, where
    //  the track model IS the newer truth, and wrong for this, where the file
    //  is: one menu click relaid out an entire image to carry one bit, and on
    //  a preservation dump that was pure loss. Patching one byte cannot lose
    //  what it never reads, which is a guarantee by construction rather than
    //  one that depends on the writer retaining every field correctly.
    //
    //  Flushes pending guest writes first, so the patch lands on a file that
    //  already holds them. That ordering used to be the caller's to get right.
    HRESULT       SetImageWriteProtect (int slot, int drive, bool writeProtected);

    //  Whether salvage is worth offering for a bay, and what it would cost.
    //  Writes nothing -- this exists so the user sees the counts BEFORE
    //  committing to a lossy copy, rather than learning them afterwards.
    HRESULT       AssessSalvage (int slot, int drive, SalvageAssessment & out);

    //  The verdict reached at mount. Free to call: no decoding, no allocation.
    //  Use this to decide whether to OFFER salvage; use AssessSalvage when the
    //  user has asked for it and the counts have to be current.
    bool          IsSalvageOffered (int slot, int drive) const;

    //  Writes the salvaged copy to `path`. The ORIGINAL IS NEVER TOUCHED --
    //  that is the whole shape of this feature: the damaged file keeps its
    //  damage, detectably, and the user gets a separate disk they can work on.
    //
    //  The copy keeps the source's META (what the disk *is* -- title,
    //  publisher, provenance) but is stamped with Casso as creator, because
    //  Casso did write this particular file and claiming otherwise would put
    //  a preservation tool's name on a lossy reconstruction.
    HRESULT       SalvageToFile (int slot, int drive, const string & path,
                                 DenibblizeReport & report);
    void          SoftReset         ();
    void          PowerCycle        ();

    DiskImage *   GetImage          (int slot, int drive);
    bool          IsMounted         (int slot, int drive) const;
    const string &GetSourcePath     (int slot, int drive) const;

    //  One mounted image's backing path with its bay, for consumers that need
    //  the full mounted set (the create dialog's mounted-target refusal).
    struct MountedSource
    {
        string  path;
        int     slot  = 0;
        int     drive = 0;
    };

    std::vector<MountedSource>  MountedSourcePaths() const;

    void          SetFlushSink      (FlushSink sink) { m_flushSink = std::move (sink); }

    //  Read counterpart to SetFlushSink: redirects every backing-file read
    //  (Mount, and the write-protect patch's read-modify-write) to the
    //  caller's buffer, so a test can pair the two and exercise a genuine
    //  read-modify-write cycle without a real file.
    void          SetImageReader    (ImageReader reader) { m_imageReader = std::move (reader); }

    static HRESULT  DetectFormatByExtension (const string & path, DiskFormat & outFmt);

    //  Whether `path`'s extension names a container this build can actually
    //  mount. Answered BY DetectFormatByExtension rather than by a second list
    //  of extensions, which is the point: the interface offering a file and
    //  the loader accepting it must not be able to disagree. They did, over
    //  `.nib` -- the drop filter said yes, the loader said no, and the mount
    //  failed silently.
    //
    //  The wide overload exists because the drag-and-drop and picker filters
    //  work in wchar_t, and narrowing at each call site is how the two answers
    //  drift apart again.
    static bool     IsMountableImageExtension (const string  & path);
    static bool     IsMountableImageExtension (const wstring & path);

    //  Where a recovery image goes for the Nth attempt, when an image cannot be
    //  written back to its own format without loss. Public because the naming
    //  and never-overwrite policy are part of the observable contract, not an
    //  implementation detail -- and pure, so both are testable without a disk.
    static string   MakeRecoveryPath        (const string & imagePath, int attempt);

    //  Replaces `path` with `bytes` without ever leaving the target in a
    //  partially written state: the bytes land in a sibling temporary file
    //  that is verified in full, then renamed over the target. A failure at
    //  any step leaves the ORIGINAL file exactly as it was and reports it,
    //  which is what a flush needs -- opening the target directly truncates
    //  it before the first byte is written, so a write that then fails
    //  (a full volume, a disconnected share) destroys the only copy.
    //
    //  Public because it is the unit of behavior worth testing on its own:
    //  the failure paths are filesystem states, not emulator states.
    static HRESULT  WriteFileAtomically (const string & path, const vector<Byte> & bytes);

    //  Reads a whole file into `bytes`. The read counterpart of the write
    //  above, and public for the same reason.
    static HRESULT  ReadFileBytes (const string & path, vector<Byte> & bytes);

    //  Why a mount failed, in the user's terms: the file named, then the
    //  diagnosis worded as a sentence about it. Pure -- no filesystem access
    //  -- and public because the wording is the observable half of a failed
    //  mount, and the only half worth testing on its own.
    //
    //  It takes the diagnosis rather than re-deriving one from the path. The
    //  path can only ever answer "is this an extension we read", which left
    //  every other refusal sharing one sentence that named none of them.
    static wstring  FormatMountFailureMessage (const string & path,
                                               const MountDiagnosis & diagnosis);

    //  Why a load of these bytes was refused, from what the bytes and the
    //  format alone can settle. Static and pure, so a test can ask it about a
    //  buffer without mounting anything.
    static MountDiagnosis  ClassifyLoadFailure (DiskFormat fmt, const vector<Byte> & bytes);
private:
    struct Entry
    {
        unique_ptr<DiskImage>  image;
        string                 path;
        DiskFormat             format  = DiskFormat::Dsk;
        bool                   mounted = false;

        //  Whether salvage applies to this image, decided once at mount.
        //  Answering it needs a full decode for a damaged disk, which is far
        //  too much work to repeat every time a menu is drawn.
        bool                   salvageOffered = false;
    };

    // Every public accessor takes a caller-supplied slot/drive pair, so each
    // one range-checks before At() indexes the fixed array.
    static bool   IsValidBay        (int slot, int drive);

    Entry &       At                (int slot, int drive);
    const Entry & At                (int slot, int drive) const;
    HRESULT       FlushEntry        (Entry & entry);

    // Routes through m_imageReader when a test has installed one, so the
    // read and write seams stay symmetric.
    HRESULT       ReadImageFile     (const string & path, vector<Byte> & bytes) const;

    // Recover what can be recovered. Decode only -- this is all the counts
    // need, and it is the half AssessSalvage can afford to run.
    HRESULT       DecodeForSalvage (Entry & entry, vector<Byte> & outSectors,
                                    DenibblizeReport & report);

    // Decode, then rebuild the result as a WOZ. Only the write path needs the
    // rebuild, so only the write path pays for it.
    HRESULT       BuildSalvagedImage (Entry & entry, vector<Byte> & outBytes,
                                      DenibblizeReport & report);

    // Builds the user-facing "could not save" message from the mount path;
    // handed to CHRN/CBRN in FlushEntry on a genuine persist failure. When a
    // recovery image was written, its path is named so the user can retrieve
    // the session rather than only being told what was lost.
    //  recoveryPath is optional: the write-protect path has no recovery copy
    //  to name, and the message says something useful either way.
    static wstring FormatFlushLossMessage (const string & path,
                                           const string & recoveryPath = string());

    // Preserves an image that could not be serialized to its own format,
    // beside the original and losslessly. Leaves the original untouched.
    HRESULT        TryWriteRecoveryImage  (Entry & entry, string & outPath);

    // Enough room to step past collisions without ever spinning.
    static constexpr int  kMaxRecoveryNameAttempts = 64;

    // Why a damaged image will not be written to. A checksum mismatch
    // write-protects the image for the session, so the file is never
    // rewritten and the damage it carries stays detectable.
    static wstring FormatDamagedImageMessage (const string & path);

    // Why a salvaged copy could not be written. Names the copy, and says the
    // original is untouched -- which is the thing the user will worry about.
    static wstring FormatSalvageFailedMessage (const string & path);

    Entry        m_entries[kSlotCount][kDriveCount];
    FlushSink    m_flushSink;
    ImageReader  m_imageReader;
    string       m_emptyPath;
};
