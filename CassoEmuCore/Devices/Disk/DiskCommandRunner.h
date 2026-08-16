#pragma once

#include "Pch.h"

#include "CommandLineOptions.h"
#include "IDiskFileIo.h"
#include "IVolume.h"
#include "VolumeImage.h"





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

    std::string   output;                 // stdout, text
    std::string   diagnostics;            // stderr, always
    vector<Byte>  payload;                // stdout, binary
    bool          hasPayload  = false;
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
    explicit DiskCommandRunner (IDiskFileIo & fileIo);

    DiskCommandResult  Run (const CommandLineOptions & options);

    //  Exit-status vocabulary, named so call sites do not spell the numbers.
    static constexpr int  kClean          = 0;
    static constexpr int  kWithComplaints = 1;
    static constexpr int  kNoOutput       = 2;

private:
    //  Loads the image and identifies its filesystem, or explains why not.
    HRESULT  OpenVolume (const std::string   & imagePath,
                         vector<Byte>        & outSectors,
                         VolumeKind          & outKind,
                         SectorDecodeReport  & outReport,
                         DiskCommandResult   & result);

    void  RunList (const CommandLineOptions & options, DiskCommandResult & result);
    void  RunGet  (const CommandLineOptions & options, DiskCommandResult & result);

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
};
