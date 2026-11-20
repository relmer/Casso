#pragma once

#include "Pch.h"

#include "ApplesoftTokenizer.h"
#include "CommandLineHelp.h"
#include "CommandLineOptions.h"
#include "CommitPlan.h"
#include "DiskCommandResult.h"
#include "DiskHelpPage.h"
#include "DiskImageSession.h"
#include "DiskCreateCommand.h"
#include "DiskFileCommands.h"
#include "DiskSectorCommands.h"
#include "Devices/Disk/BlockImage.h"
#include "IDiskCommandContext.h"
#include "IDiskFileIo.h"
#include "Machines/Apple2/Common/IVolume.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"

//  Forward-declared rather than included: pulling VolumeImage.h in would drag
//  DiskImage.h through this header and into the console project, which does not
//  share the core library's Pch conveniences.
enum class VolumeKind;

//  BlankDiskBuilder.h is kept out for the same reason, and the create and init
//  commands take everything from it by reference, so opaque declarations are all
//  this header needs. Including it built the library fine and broke the console
//  project, which is the failure mode the note above is about.
enum class DiskFormat;
enum class BlankDiskContents;
enum class ProDosBootSystem;
struct BlankDiskSpec;
struct BootPayload;
struct ParsedHostName;





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
//
//  DiskCommandRunner
//
//  Every decision a disk command makes: which command, which filesystem, whether
//  the result is safe to commit, what to say when it is not, and which exit
//  status the whole thing earns.
//
//  It reaches the host only through IDiskFileIo, which is what allows the
//  entire command path to be tested -- the test assembly does not link the
//  console executable, so anything placed there would be unreachable.
//
////////////////////////////////////////////////////////////////////////////////

class IIntentChannel;
class BlockBuffer;
class CardVolumeEdit;


class DiskCommandRunner : private IDiskCommandContext
{
public:


    explicit DiskCommandRunner (IDiskFileIo & fileIo);

    //  Where a stated intent is announced once the image has been written.
    //
    //  OPTIONAL, AND NULL IS THE ORDINARY CASE IN A TEST. Nothing about a disk
    //  command depends on anybody hearing it, so a runner with no channel does
    //  everything a runner with one does, minus the announcement.
    void  SetIntentChannel (IIntentChannel * channel) { m_intentChannel = channel; }

    //  How a command asks the person running it to confirm something, and the
    //  answer it gets.
    //
    //  INJECTED, BECAUSE THE RUNNER REACHES THE HOST THROUGH NOTHING ELSE. A
    //  console read placed here would be unreachable from the tests, which do
    //  not link the executable. NONE SET MEANS NOBODY IS THERE TO ASK, which is
    //  every test and every script, and a removal that needs an answer is
    //  turned down rather than assumed.
    using ConfirmFn = std::function<bool (const std::string & question)>;

    void  SetConfirmAsker (ConfirmFn asker) { m_confirm = std::move (asker); }

    //  Where a new Apple Pascal disk's date comes from: today, unless a test
    //  sets another clock.
    using ClockFn = std::function<SYSTEMTIME()>;

    void  SetClock (ClockFn clock) { m_clock = std::move (clock); }

    //  The thumbnail cache `thumbnail` uses: a test's temporary folder.
    void  SetThumbnailFolder (std::filesystem::path folder) { m_thumbnailFolder = std::move (folder); }

    DiskCommandResult  Run (const CommandLineOptions & options);





































    //  What a command says when a required operand is not there: that
    //  command's usage, and then which parameter is missing.
    //
    //  A MEMBER, because the command it should print usage for is the one this
    //  runner is running, and the deepest caller that notices a missing image
    //  is OpenImage, which is handed a path and nothing else. Threading the
    //  command through every layer to reach it would put it in six signatures
    //  that have no other use for it.
    void  ReportMissingParameter (const std::string & parameter,
                                  DiskCommandResult & result) const override;

    //  Every required operand this command did not get, in the order its
    //  grammar lists them.
    //
    //  ALL OF THEM, NOT THE FIRST ONE NOTICED. Each command used to check its
    //  own operands wherever it happened to need them, so `disk get` with
    //  nothing at all complained that <name> was missing and never mentioned
    //  <image> -- the operand that comes first, and that the reader would have
    //  had to supply before the complaint made sense.
    std::vector<std::string>  FindMissingParameters (const CommandLineOptions & options) const;

    //  The same report for a list of them.
    void  ReportMissingParameters (const std::vector<std::string> & parameters,
                                   DiskCommandResult & result) const;



    //  The same substitution, against the prefix THIS run was asked for.
    //
    //  For diagnostics rather than for the page, and applied to the literal
    //  rather than to the finished sentence: a diagnostic carries file names
    //  and volume labels, and a sweep over the whole thing would rewrite a
    //  name that happened to hold %L.
    std::string         ApplyPrefix        (const std::string & text) const override;


    //  The banner the Help command prints above the page, handed over by whoever
    //  built the runner. Empty means no banner, which is what every caller that
    //  is not the console executable wants.
    void  SetBanner (const std::string & banner);

private:
    void  RunList      (const CommandLineOptions & options, DiskCommandResult & result);
    void  RunBoot      (const CommandLineOptions & options, DiskCommandResult & result);
    void  RunThumbnail (const CommandLineOptions & options, DiskCommandResult & result);

    //  A Pascal or CP/M catalog as the Filer or STAT prints it.
    HRESULT  AppendNativeListing (const CommandLineOptions             & options,
                                  const DiskImageSession::OpenedImage  & opened,
                                  const IVolume                        & volume,
                                  const VolumeListing                  & listing,
                                  DiskCommandResult                    & result);

    //  What a listing could not read, on the error stream, with the status.
    void     ReportListingDamage (const CommandLineOptions             & options,
                                  const DiskImageSession::OpenedImage  & opened,
                                  const VolumeListing                  & listing,
                                  DiskCommandResult                    & result);

    //  One directory's rows, and with `recurse` the rows of everything below
    //  it, each of those written as a full path. The name column is
    //  `nameWidth` wide.
    HRESULT  AppendDirectoryRows (const IVolume      & volume,
                                  VolumeKind           kind,
                                  const std::string  & directory,
                                  bool                 recurse,
                                  size_t               nameWidth,
                                  DiskCommandResult  & result);

    //  The name column's width for a recursive listing of `directory`: its
    //  longest name or path and a gap, if wider than inOutWidth.
    static HRESULT  MeasureNameWidth (const IVolume & volume, const std::string & directory, size_t & inOutWidth);










    //  Whether a booting DOS 3.3 would actually run this file. Its boot command
    //  is RUN, so a greeting RUN does not understand is one in name only --
    //  measured on the stock master, where a binary named as the greeting
    //  leaves the disk booting and the program never running.
    static bool  IsRunnableAsDos33Greeting (const VolumeListing  & listing,
                                            const std::string    & name);

    //  Whether a ProDOS volume carries the PRODOS file its boot block loads.
    static bool  HasProDosSystemFile       (const VolumeListing  & listing);















    //  One line per catalog entry, in the shape the guest's own listing uses --
    //  with every column the volume records, since ProDOS fills eof= and aux=
    //  whether or not anybody asks and the widest row still fits in 80.
    static constexpr size_t  kNameWidth = 17;

    static std::string  FormatDos33Entry  (const FileEntry & entry);
    static std::string  FormatProDosEntry (const FileEntry & entry, size_t nameWidth = kNameWidth);

    static char  GetDos33TypeLetter (Byte type);



    //  The open/commit seam, reachable so the commit-path tests can
    //  drive it directly, which is why it was public on the runner
    //  before it was a class.
public:
    DiskImageSession &  GetSession ()  override { return m_session; }

private:
    static SYSTEMTIME  GetLocalDate ();

    IDiskFileIo  & GetFileIo () override                             { return m_fileIo; }
    bool           CanConfirm() const override                       { return m_confirm != nullptr; }
    bool           Confirm   (const std::string & question) override { return m_confirm (question); }
    SYSTEMTIME     GetToday  () override                             { return m_clock(); }

    //  Whether a command puts bytes into the image it was given.
    static bool  WritesTheImage (CommandLineOptions::DiskOptions::Command command);

    //  Tells any running emulator what this write was meant to do.
    void         AnnounceIntent (const CommandLineOptions & options,
                                 const DiskCommandResult  & result);

    //  Caller-owned; null means nothing is announced, which is every test and
    //  every caller that has no emulator to talk to.
    IIntentChannel    * m_intentChannel = nullptr;

    //  Caller-owned as well; none means no terminal.
    ConfirmFn           m_confirm;
    ClockFn             m_clock = GetLocalDate;

    IDiskFileIo       & m_fileIo;
    DiskImageSession    m_session;
    std::string         m_banner;

    //  The thumbnail cache, or empty for the one under local application data.
    std::filesystem::path  m_thumbnailFolder;

    DiskCreateCommand      m_createCommand  { *this };
    DiskFileCommands       m_fileCommands   { *this };
    DiskSectorCommands     m_sectorCommands { *this };

    //  The command being run and the prefix it was typed with, recorded at the
    //  top of Run so a refusal deep inside can answer in the reader's own
    //  spelling and about the command they actually asked for.
    CommandLineOptions::DiskOptions::Command  m_command    =
        CommandLineOptions::DiskOptions::Command::None;
    char                                      m_flagPrefix = '-';

};
