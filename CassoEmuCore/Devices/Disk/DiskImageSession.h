#pragma once

#include "Pch.h"

#include "CommitPlan.h"
#include "DiskCommandResult.h"
#include "IDiskFileIo.h"
#include "SectorDecodeReport.h"

enum class VolumeKind;




////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageSession
//
//  ONE IMAGE, OPENED AND COMMITTED AS A TRANSACTION. Reading records the
//  stamp a later commit re-verifies, the commit writes beside and replaces
//  atomically, and a refusal leaves the target byte-for-byte as it was.
//  This was the runner's open/commit half; it was already public there
//  for the commit-path tests, and naming it a class states what that had
//  already decided.
//
//  It reaches the host only through IDiskFileIo, for the same reason the
//  runner does: the test assembly does not link the console executable.
//
////////////////////////////////////////////////////////////////////////////////

class DiskImageSession
{
public:
    //
    //  One image, read and identified, together with what a later commit over
    //  it will need.
    //
    //  The stamp is here rather than beside the commit because it has to be
    //  taken at READ time to mean anything -- a stamp read just before writing
    //  would agree with itself and prove nothing. Keeping the two in one object
    //  makes it awkward to write a commit that forgot to record one.
    //
    struct OpenedImage
    {
        std::string         imagePath;

        //  The container's own bytes, kept because rendering an edited volume
        //  back needs them: a bit-stream image preserves every track the edit
        //  did not touch verbatim, and it can only do that from the file it
        //  came from. Re-reading here instead would let the two halves of the
        //  write disagree about what the image held.
        std::vector<Byte>   fileBytes;

        std::vector<Byte>   sectors;
        VolumeKind          kind          = VolumeKind {};
        SectorDecodeReport  report;
        FileStamp           stamp;

        //  False when the platform could not tell us the size and time. A
        //  commit refuses rather than proceeding without the check, because a
        //  guarantee quietly not applied is indistinguishable from one applied.
        bool                stampRecorded = false;

        //
        //  Set when the target does not exist yet, which is `create` and
        //  nothing else.
        //
        //  THE FRESHNESS CHECK IS THE WRONG QUESTION FOR A FILE THAT IS NOT
        //  THERE. It asks whether the image changed between being read and
        //  being written, and a file nobody has read cannot have: with no
        //  stamp to compare, the commit refused every new disk with "could
        //  not be checked for changes". Everything else about the write is
        //  unchanged, the in-use probe included, so a name another program
        //  is already holding is still refused.
        bool                isNew         = false;
    };

    explicit DiskImageSession (IDiskFileIo & fileIo)
        : m_fileIo (fileIo)
    {
    }

    //  Loads the image, identifies its filesystem and records its stamp, or
    //  explains why not.
    //  `requireFilesystem` is what every command but `stamp` wants: a disk with
    //  no catalog is nothing they can act on, so identifying none is a
    //  refusal. `stamp` writes to a track and a sector and asks nothing of
    //  the filesystem, so for it an unrecognized disk is an ordinary one.
    HRESULT  OpenImage (const std::string  & imagePath,
                        OpenedImage        & outOpened,
                        DiskCommandResult  & result,
                        bool                 requireFilesystem = true);

    HRESULT  CommitImage (const OpenedImage        & opened,
                          const std::vector<Byte>  & newImageBytes,
                          DiskCommandResult        & result);

    //
    //  What a commit says when another program is holding the image.
    //
    //  It lives here, beside the code that performs the probe, so the claim and
    //  the capability cannot drift apart -- and so a test can read it, since the
    //  console executable is not linked by the test assembly.
    //
    //  IT IS A DIAGNOSTIC RATHER THAN HELP TEXT, which is the whole change. The
    //  help used to carry a paragraph describing the probe; a user meets the
    //  probe by having a write refused, and the refusal names the image and
    //  says what to do about it. Documenting an error message in the usage text
    //  is documenting the wrong surface.
    //
    static constexpr const char *  kInUseRefusalText =
        "is open in another program. Close it and try again";

    //  The sentence a listing gives when neither filesystem is there. Named so
    //  a test asserts on the wording a user reads rather than a paraphrase.
    static constexpr const char *  kNoFilesystemText =
        "does not have a DOS or ProDOS file system";

    //  Renders an edited sector buffer back into the container it came from and
    //  puts it where the old one was. One helper because the two orderings it
    //  imposes -- render first, then commit -- are what keep a refused write
    //  from ever reaching the target.
    HRESULT  SaveAndCommit (const OpenedImage        & opened,
                            const std::vector<Byte>  & editedSectors,
                            DiskCommandResult        & result);

    //
    //  Everything an image still says about itself once neither filesystem
    //  recognizes it.
    //
    //  A FUNCTIONAL, BOOTABLE DISK IS THE COMMON CASE HERE, not an exotic one.
    //  Most commercial Apple II software never used a filesystem at all: it
    //  booted its own loader off track 0 and read its data by track and sector.
    //  Answering only "no filesystem" describes twelve of fourteen real disks
    //  as though nothing could be learned from them, when the image is holding
    //  its title, its publisher, how much of its surface is formatted and
    //  whether it boots.
    //
    static std::string  DescribeUnrecognizedImage (const OpenedImage & opened);

    //  What the container itself records: for WOZ, the INFO and META chunks and
    //  how much of the surface the track map claims.
    static std::string  DescribeWozChunks (const std::vector<Byte> & fileBytes);

    //  What the decoded surface shows: geometry, how the tracks read, and
    //  whether the first sector a boot reads carries anything.
    static std::string  DescribeSurface (const OpenedImage & opened);

    //  One `  label   value` line, or nothing at all when the value is empty --
    //  so a caller can offer every field it knows about without also deciding
    //  which ones this image answered.
    static std::string  DetailLine (const char * label, const std::string & value);

    //  Names the image and the reason, sets the no-output status, and returns
    //  nothing -- so a refusal path cannot report one without the other.
    static void  RefuseCommit (const std::string  & imagePath,
                               const std::string  & reason,
                               DiskCommandResult  & result);

    //  The same job for the atomic replace, which is where write protection
    //  enforced by the HOST file's read-only attribute finally surfaces -- the
    //  volume layer never sees it, because nothing about the image's contents
    //  says the file may not be written.
    static std::string  DescribeReplaceFailure (HRESULT hr);

private:
    IDiskFileIo &  m_fileIo;

    //  What separates this session's temporaries from any other
    //  invocation's, taken once at construction.
    uint64_t       m_invocationTag = CommitPlan::NextInvocationTag();
};
