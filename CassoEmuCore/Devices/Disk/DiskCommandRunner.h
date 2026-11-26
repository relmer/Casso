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
#include "DiskListCommand.h"
#include "DiskSectorCommands.h"
#include "Devices/Disk/BlockImage.h"
#include "IDiskCommandContext.h"
#include "IDiskFileIo.h"
#include "Machines/Apple2/Common/IVolume.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"

class IIntentChannel;





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

    //  The banner the Help command prints above the page, handed over by whoever
    //  built the runner. Empty means no banner, which is what every caller that
    //  is not the console executable wants.
    void  SetBanner (const std::string & banner);

    //  Every required operand this command did not get, in the order its
    //  grammar lists them.
    //
    //  ALL OF THEM, NOT THE FIRST ONE NOTICED. Each command used to check its
    //  own operands wherever it happened to need them, so `disk get` with
    //  nothing at all complained that <name> was missing and never mentioned
    //  <image> -- the operand that comes first, and that the reader would have
    //  had to supply before the complaint made sense.
    std::vector<std::string>  FindMissingParameters (const CommandLineOptions & options) const;

    //  The open/commit seam, reachable so the commit-path tests can
    //  drive it directly, which is why it was public on the runner
    //  before it was a class.
    DiskImageSession &  GetSession ()  override { return m_session; }

private:
    void  RunBoot      (const CommandLineOptions & options, DiskCommandResult & result);
    void  RunThumbnail (const CommandLineOptions & options, DiskCommandResult & result);

    //  Whether a booting DOS 3.3 would actually run this file. Its boot command
    //  is RUN, so a greeting RUN does not understand is one in name only --
    //  measured on the stock master, where a binary named as the greeting
    //  leaves the disk booting and the program never running.
    static bool  IsRunnableAsDos33Greeting (const VolumeListing  & listing,
                                            const std::string    & name);

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

    //  The same report for a list of them.
    void  ReportMissingParameters (const std::vector<std::string> & parameters,
                                   DiskCommandResult & result) const;

    //  The same substitution, against the prefix THIS run was asked for.
    //
    //  For diagnostics rather than for the page, and applied to the literal
    //  rather than to the finished sentence: a diagnostic carries file names
    //  and volume labels, and a sweep over the whole thing would rewrite a
    //  name that happened to hold %L.
    std::string  ApplyPrefix (const std::string & text) const override;

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
    DiskListCommand        m_listCommand    { *this, m_fileCommands };
    DiskSectorCommands     m_sectorCommands { *this };

    //  The command being run and the prefix it was typed with, recorded at the
    //  top of Run so a refusal deep inside can answer in the reader's own
    //  spelling and about the command they actually asked for.
    CommandLineOptions::DiskOptions::Command  m_command    =
        CommandLineOptions::DiskOptions::Command::None;
    char                                      m_flagPrefix = '-';
};
