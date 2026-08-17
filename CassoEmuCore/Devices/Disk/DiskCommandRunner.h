#pragma once

#include "Pch.h"

#include "CommandLineOptions.h"
#include "CommitPlan.h"
#include "IDiskFileIo.h"
#include "IVolume.h"
#include "SectorDecodeReport.h"

//  Forward-declared rather than included: pulling VolumeImage.h in would drag
//  DiskImage.h through this header and into the console project, which does not
//  share the core library's Pch conveniences.
enum class VolumeKind;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandResult
//
//  Everything one disk command produced, separated by where it goes. The runner
//  decides all of it; the executable only delivers it.
//
//  Payload is kept apart from output text because the two need different
//  treatment at the platform edge -- text wants the host's line-ending
//  translation and binary must not have it.
//
////////////////////////////////////////////////////////////////////////////////

struct DiskCommandResult
{
    //  0 clean, 1 succeeded with complaints, 2 produced no output. The same
    //  meanings the assembler and run subcommands already assign.
    int           exitStatus  = 0;

    std::string        output;   // stdout, text
    std::string        diagnostics;   // stderr, always
    std::vector<Byte>  payload;   // stdout, binary
    bool               hasPayload  = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunner
//
//  Every decision a disk command makes: which verb, which filesystem, whether
//  the result is safe to commit, what to say when it is not, and which exit
//  status the whole thing earns.
//
//  It reaches the host only through IDiskFileIo, which is what allows the
//  entire command path to be tested -- the test assembly does not link the
//  console executable, so anything placed there would be unreachable.
//
////////////////////////////////////////////////////////////////////////////////

class DiskCommandRunner
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
    };

    explicit DiskCommandRunner (IDiskFileIo & fileIo);

    DiskCommandResult  Run (const CommandLineOptions & options);

    //  Loads the image, identifies its filesystem and records its stamp, or
    //  explains why not.
    HRESULT  OpenImage (const std::string  & imagePath,
                        OpenedImage        & outOpened,
                        DiskCommandResult  & result);

    //  Puts a computed image over the target, or refuses and leaves the target
    //  byte-for-byte as it was.
    HRESULT  CommitImage (const OpenedImage        & opened,
                          const std::vector<Byte>  & newImageBytes,
                          DiskCommandResult        & result);

    //  Exit-status vocabulary, named so call sites do not spell the numbers.
    static constexpr int  kClean          = 0;
    static constexpr int  kWithComplaints = 1;
    static constexpr int  kNoOutput       = 2;

    //
    //  What the help says about the in-use probe.
    //
    //  It lives here, beside the code that performs the probe, so the claim and
    //  the capability cannot drift apart -- and so a test can read it, since the
    //  console executable is not linked by the test assembly.
    //
    //  The second sentence is the load-bearing one. A probe that catches other
    //  tools and cannot catch this emulator must not be described in a way that
    //  lets a reader conclude a clean probe means their mounted disk is safe.
    //
    static constexpr const char *  kInUseHelpText =
        "in-use check: a write is refused when another program holds the image open.\n"
        "         It cannot tell whether the image is mounted here -- a mounted image is\n"
        "         not held open -- so a mounted disk is neither detected nor protected.";

private:
    void  RunList   (const CommandLineOptions & options, DiskCommandResult & result);
    void  RunGet    (const CommandLineOptions & options, DiskCommandResult & result);
    void  RunPut    (const CommandLineOptions & options, DiskCommandResult & result);
    void  RunDelete (const CommandLineOptions & options, DiskCommandResult & result);
    void  RunBoot   (const CommandLineOptions & options, DiskCommandResult & result);

    //  Renders an edited sector buffer back into the container it came from and
    //  puts it where the old one was. One helper because the two orderings it
    //  imposes -- render first, then commit -- are what keep a refused write
    //  from ever reaching the target.
    HRESULT  SaveAndCommit (const OpenedImage        & opened,
                            const std::vector<Byte>  & editedSectors,
                            DiskCommandResult        & result);

    //  Turns a volume layer's refusal into something a user can act on.
    //
    //  THIS IS WHAT KEEPS A PLATFORM CODE OUT OF THE OUTPUT. The volume layer
    //  answers in Win32 codes because they carry the right meanings without
    //  asserting on user input, but a number is not a reason -- the person
    //  reading it wants to know that their file is locked, not that something
    //  returned 0x80070005.
    static std::string  DescribeVolumeRefusal (HRESULT hr);

    //  The same job for the atomic replace, which is where write protection
    //  enforced by the HOST file's read-only attribute finally surfaces -- the
    //  volume layer never sees it, because nothing about the image's contents
    //  says the file may not be written.
    static std::string  DescribeReplaceFailure (HRESULT hr);

    //  Whether a booting DOS 3.3 would actually run this file. Its boot command
    //  is RUN, so a greeting RUN does not understand is one in name only --
    //  measured on the stock master, where a binary named as the greeting
    //  leaves the disk booting and the program never running.
    static bool  IsRunnableAsDos33Greeting (const VolumeListing  & listing,
                                            const std::string    & name);

    //  Which type byte a placement uses: what the caller named, or the sensible
    //  one for the conversion they asked for.
    static HRESULT  ResolveFileType (const CommandLineOptions & options,
                                     VolumeKind                 kind,
                                     Byte                     & outType,
                                     DiskCommandResult        & result);

    //  The bytes a host file becomes on the disk, with whatever conversion was
    //  asked for already applied. Refuses a conversion this build cannot do.
    static HRESULT  BuildPutPayload (const CommandLineOptions  & options,
                                     VolumeKind                  kind,
                                     const std::vector<Byte>   & hostBytes,
                                     FilePayload               & outPayload,
                                     DiskCommandResult         & result);

    //  What the file is called on the disk: --as when given, otherwise the host
    //  file's own last component, which is the name the caller already chose.
    static std::string  OnDiskNameFor (const CommandLineOptions & options);

    //  Names the image and the reason, sets the no-output status, and returns
    //  nothing -- so a refusal path cannot report one without the other.
    static void  RefuseCommit (const std::string  & imagePath,
                               const std::string  & reason,
                               DiskCommandResult  & result);

    //  Applies the requested character conversion, or refuses one this build
    //  cannot perform. A parsed-then-ignored flag is worse than an absent one.
    static HRESULT  ApplyEncoding (const CommandLineOptions & options,
                                   FilePayload              & payload,
                                   DiskCommandResult        & result);

    //  One line per catalog entry, in the shape the guest's own listing uses.
    static std::string  FormatDos33Entry  (const FileEntry & entry);
    static std::string  FormatProDosEntry (const FileEntry & entry, bool longForm);

    static char  Dos33TypeLetter (Byte type);

    //  Names the image, the file, and the reason -- in that order, because a
    //  script's user reads the first line and needs to know which disk.
    static std::string  Failure (const std::string & imagePath,
                                 const std::string & fileName,
                                 const std::string & reason);

    IDiskFileIo &  m_fileIo;

    //  What separates this runner's temporaries from any other invocation's.
    //  Taken once, at construction, so every commit this runner performs is
    //  distinguishable from every commit anybody else performs.
    uint64_t       m_invocationTag = CommitPlan::NextInvocationTag();
};
