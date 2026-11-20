#pragma once

#include "Pch.h"

#include "CommandLineOptions.h"
#include "Devices/Disk/DiskCommandResult.h"
#include "Machines/Apple2/Common/IVolume.h"

enum class VolumeKind;
struct ParsedHostName;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFilePayload
//
//  The bytes, type and name a file from the host has on a disk, and the
//  bytes a file from a disk has on the host, with the text and BASIC
//  conversions put and get apply.
//
////////////////////////////////////////////////////////////////////////////////

class DiskFilePayload
{
public:
    //  ProDOS's type for an Integer BASIC program.
    static constexpr Byte  kProDosIntegerType = 0xFA;

    //  The `._NAME` file a data fork's resource fork is written to.
    static std::string  GetAppleDoublePath (const std::string & hostFile);

    //  What a file's own bytes say it is, or 0 when they say nothing.
    //
    //  ONLY CONSULTED WHEN NOBODY NAMED A TYPE. A guess that overrode --type
    //  would be a tool arguing with its operator, and one that fired on a
    //  binary would file it where the guest cannot run it. Anything not
    //  positively recognized stays a binary, which is what a build loop
    //  produces and what the default has always been.
    static Byte     DetectFileType  (const std::vector<Byte> & bytes, VolumeKind kind);

    static HRESULT  ResolvePascalKind  (const CommandLineOptions & options, Byte & outType, DiskCommandResult & result);
    static bool     ParseCpmAttributes (const std::string & text, bool & outReadOnly, bool & outSystem);

    //  Which type byte a placement uses: what the caller named, or the sensible
    //  one for the conversion they asked for.
    static HRESULT  ResolveFileType (const CommandLineOptions & options,
                                     VolumeKind                 kind,
                                     const std::vector<Byte>  & hostBytes,
                                     Byte                     & outType,
                                     DiskCommandResult        & result);

    //  What the file is called on the disk: --as when given, otherwise the host
    //  file's own last component, which is the name the caller already chose.
    static std::string  GetOnDiskName (const CommandLineOptions & options);

    //  With no --as, a host name in one of Casso Explorer's forms sets the
    //  entry's name and, with no --text or --basic, its conversion. False for any
    //  other host name, and the options are then left as they were.
    static bool     ApplyHostFileName (CommandLineOptions & inOutOptions, ParsedHostName & outParsed);

    //  For a get with no --text or --basic, the one an output name in Casso
    //  Explorer's text or BASIC listing form has, as a copy out writes it.
    static void     ApplyOutputEncoding (CommandLineOptions & inOutOptions);

    //  For a get whose output file has a conversion's suffix, such as
    //  SOURCE.Merlin.txt: replace the payload's bytes with that conversion.
    //  An error for a file in another format.
    static HRESULT  ApplyHostFileConversion (const CommandLineOptions  & options,
                                             VolumeKind                  kind,
                                             const std::string         & name,
                                             FilePayload               & inOutPayload,
                                             DiskCommandResult         & result);

    //  Set the payload's type and aux type from that form's name.
    static HRESULT  ApplyHostFileType (const ParsedHostName & parsed, VolumeKind kind, FilePayload & inOutPayload);

    //  The bytes a file from the host becomes on the disk, with whatever conversion was
    //  asked for already applied. Refuses a conversion this build cannot do.
    static HRESULT  BuildPutPayload (const CommandLineOptions  & options,
                                     VolumeKind                  kind,
                                     const std::vector<Byte>   & hostBytes,
                                     FilePayload               & outPayload,
                                     DiskCommandResult         & result);

    //  Applies the requested character conversion, or refuses one this build
    //  cannot perform. A parsed-then-ignored flag is worse than an absent one.
    static HRESULT  ApplyEncoding (const CommandLineOptions & options,
                                   FilePayload              & payload,
                                   DiskCommandResult        & result);

    //  Text with an LF line end made CR LF.
    static void     ToWindowsLineEndings (std::vector<Byte> & inOutBytes);
};
