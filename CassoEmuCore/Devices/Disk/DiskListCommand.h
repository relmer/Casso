#pragma once

#include "Pch.h"

#include "CommandLineOptions.h"
#include "Devices/Disk/DiskCommandResult.h"
#include "Devices/Disk/DiskFileCommands.h"
#include "Devices/Disk/DiskImageSession.h"
#include "Devices/Disk/IDiskCommandContext.h"
#include "Machines/Apple2/Common/IVolume.h"

enum class VolumeKind;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskListCommand
//
//  The disk tool's list command on an image: a catalog in the form the
//  image's file system prints it.
//
////////////////////////////////////////////////////////////////////////////////

class DiskListCommand
{
public:
    DiskListCommand (IDiskCommandContext & context, DiskFileCommands & fileCommands) : m_context (context), m_fileCommands (fileCommands) {}

    void  RunList (const CommandLineOptions & options, DiskCommandResult & result);

private:
    //  One line per catalog entry, in the shape the guest's own listing uses --
    //  with every column the volume records, since ProDOS fills eof= and aux=
    //  whether or not anybody asks and the widest row still fits in 80.
    static constexpr size_t  kNameWidth = 17;

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

    static std::string  FormatDos33Entry   (const FileEntry & entry);
    static std::string  FormatProDosEntry  (const FileEntry & entry, size_t nameWidth = kNameWidth);
    static char         GetDos33TypeLetter (Byte type);

    //  Whether a ProDOS volume carries the PRODOS file its boot block loads.
    static bool  HasProDosSystemFile (const VolumeListing & listing);

    IDiskCommandContext  & m_context;
    DiskFileCommands     & m_fileCommands;
};
