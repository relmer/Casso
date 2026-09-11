#include "Pch.h"

#include "Cassque/Model/DiskOperations.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskOperations::DiskOperations
//
////////////////////////////////////////////////////////////////////////////////

DiskOperations::DiskOperations (IDiskFileIo & fileIo)
    : m_fileIo (fileIo)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskOperations::MakeOptions
//
////////////////////////////////////////////////////////////////////////////////

CommandLineOptions DiskOperations::MakeOptions (Command command, const std::string & imagePath)
{
    CommandLineOptions  options;



    options.subcommand     = CommandLineOptions::Subcommand::Disk;
    options.disk.command   = command;
    options.disk.imagePath = imagePath;

    return options;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskOperations::DescribeVolumeRefusal
//
//  The volume layer answers in Win32 codes; a dialog wants a sentence.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskOperations::DescribeVolumeRefusal (HRESULT hr)
{
    if (hr == HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND))          { return "is not on this volume"; }
    if (hr == HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS))             { return "is already the name of another file on this volume"; }
    if (hr == HRESULT_FROM_WIN32 (ERROR_INVALID_NAME))            { return "is not a name this file system can store"; }
    if (hr == HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED))           { return "is locked"; }
    if (hr == HRESULT_FROM_WIN32 (ERROR_DISK_FULL))               { return "does not fit: the volume has no room for it"; }
    if (hr == HRESULT_FROM_WIN32 (ERROR_FILE_TOO_LARGE))          { return "is larger than this file system can record"; }
    if (hr == HRESULT_FROM_WIN32 (ERROR_DIRECTORY_NOT_SUPPORTED)) { return "is a directory, which this operation does not handle"; }
    if (hr == HRESULT_FROM_WIN32 (ERROR_INVALID_PARAMETER))       { return "needs a load address"; }
    if (hr == HRESULT_FROM_WIN32 (ERROR_HANDLE_EOF))              { return "has a damaged sector chain"; }

    return std::format ("could not be processed (0x{:08X})", (unsigned) hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskOperations::RunCommand
//
//  One runner per call, so the staleness stamp is taken fresh each time and
//  a window that keeps an image open for hours never commits over a change
//  it did not see.
//
////////////////////////////////////////////////////////////////////////////////

DiskOperations::Result DiskOperations::RunCommand (const CommandLineOptions & options)
{
    DiskCommandRunner  runner (m_fileIo);
    DiskCommandResult  command;
    Result             result;



    runner.SetIntentChannel (m_intentChannel);

    command = runner.Run (options);

    result.exitStatus = command.exitStatus;
    result.message    = command.diagnostics;
    result.output     = command.output;
    result.hr         = (command.exitStatus == DiskCommandResult::kNoOutput) ? E_FAIL : S_OK;

    if (command.hasPayload)
    {
        result.payload = std::move (command.payload);
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskOperations::OpenVolume
//
////////////////////////////////////////////////////////////////////////////////

DiskOperations::Result DiskOperations::OpenVolume (const std::string & imagePath, DiskImageSession::OpenedImage & outOpened)
{
    DiskImageSession   session (m_fileIo);
    DiskCommandResult  command;
    Result             result;



    result.hr         = session.OpenImage (imagePath, outOpened, command);
    result.exitStatus = command.exitStatus;
    result.message    = command.diagnostics;

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskOperations::CommitEdit
//
//  Renders the edited sectors back into the container and replaces the file,
//  through the same session call every runner verb commits with.
//
////////////////////////////////////////////////////////////////////////////////

DiskOperations::Result DiskOperations::CommitEdit (
    const std::string                    & imagePath,
    const DiskImageSession::OpenedImage  & opened,
    const std::vector<Byte>              & edited)
{
    DiskImageSession   session (m_fileIo);
    DiskCommandResult  command;
    Result             result;



    UNREFERENCED_PARAMETER (imagePath);

    result.hr         = session.SaveAndCommit (opened, edited, command);
    result.exitStatus = command.exitStatus;
    result.message    = command.diagnostics;

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskOperations::List
//
////////////////////////////////////////////////////////////////////////////////

DiskOperations::Result DiskOperations::List (const std::string & imagePath, VolumeListing & outListing, VolumeKind & outKind)
{
    DiskImageSession::OpenedImage  opened;
    Result                         result = OpenVolume (imagePath, opened);
    HRESULT                        hr     = S_OK;



    outListing = VolumeListing();
    outKind    = VolumeKind::Unknown;

    if (result.Succeeded())
    {
        Dos33Volume   dos (opened.sectors);
        ProDosVolume  pro (opened.sectors);
        IVolume     & volume = (opened.kind == VolumeKind::Dos33)
                             ? static_cast<IVolume &> (dos)
                             : static_cast<IVolume &> (pro);

        outKind = opened.kind;
        hr      = volume.Enumerate (outListing);

        if (FAILED (hr))
        {
            result.hr      = hr;
            result.message = DiskCommandResult::FormatFailure (imagePath, "", "catalog could not be read");
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskOperations::Read
//
////////////////////////////////////////////////////////////////////////////////

DiskOperations::Result DiskOperations::Read (const std::string & imagePath, const std::string & name, FilePayload & outPayload)
{
    DiskImageSession::OpenedImage  opened;
    Result                         result = OpenVolume (imagePath, opened);
    HRESULT                        hr     = S_OK;



    outPayload = FilePayload();

    if (result.Succeeded())
    {
        Dos33Volume   dos (opened.sectors);
        ProDosVolume  pro (opened.sectors);
        IVolume     & volume = (opened.kind == VolumeKind::Dos33)
                             ? static_cast<IVolume &> (dos)
                             : static_cast<IVolume &> (pro);

        hr = volume.Read (FilePath::Parse (name), outPayload);

        if (FAILED (hr))
        {
            result.hr      = hr;
            result.message = DiskCommandResult::FormatFailure (imagePath, name, DescribeVolumeRefusal (hr));
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskOperations::Get
//
////////////////////////////////////////////////////////////////////////////////

DiskOperations::Result DiskOperations::Get (
    const std::string  & imagePath,
    const std::string  & name,
    Encoding             encoding,
    const std::string  & hostPath)
{
    CommandLineOptions  options = MakeOptions (Command::Get, imagePath);



    options.disk.path     = name;
    options.disk.encoding = encoding;
    options.disk.hostFile = hostPath;

    return RunCommand (options);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskOperations::Put
//
////////////////////////////////////////////////////////////////////////////////

DiskOperations::Result DiskOperations::Put (
    const std::string  & imagePath,
    const std::string  & hostPath,
    const std::string  & name,
    const std::string  & typeName,
    bool                 hasLoadAddress,
    Word                 loadAddress,
    Encoding             encoding)
{
    CommandLineOptions  options = MakeOptions (Command::Put, imagePath);



    options.disk.hostFile       = hostPath;
    options.disk.path           = name;
    options.disk.typeName       = typeName;
    options.disk.hasLoadAddress = hasLoadAddress;
    options.disk.loadAddress    = loadAddress;
    options.disk.encoding       = encoding;

    return RunCommand (options);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskOperations::Delete
//
////////////////////////////////////////////////////////////////////////////////

DiskOperations::Result DiskOperations::Delete (const std::string & imagePath, const std::string & name)
{
    CommandLineOptions  options = MakeOptions (Command::Delete, imagePath);



    options.disk.path = name;

    return RunCommand (options);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskOperations::Boot
//
////////////////////////////////////////////////////////////////////////////////

DiskOperations::Result DiskOperations::Boot (const std::string & imagePath, const std::string & name)
{
    CommandLineOptions  options = MakeOptions (Command::Boot, imagePath);



    options.disk.path = name;

    return RunCommand (options);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskOperations::FillNewDisk
//
////////////////////////////////////////////////////////////////////////////////

void DiskOperations::FillNewDisk (const NewDiskRequest & request, CommandLineOptions & inOutOptions)
{
    inOutOptions.disk.containerType = request.containerType;
    inOutOptions.disk.formatName    = request.formatName;
    inOutOptions.disk.volumeName    = request.volumeName;
    inOutOptions.disk.bootable      = request.bootable;
    inOutOptions.disk.bootableFrom  = request.bootableFrom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskOperations::Create
//
////////////////////////////////////////////////////////////////////////////////

DiskOperations::Result DiskOperations::Create (const std::string & imagePath, const NewDiskRequest & request)
{
    CommandLineOptions  options = MakeOptions (Command::Create, imagePath);



    FillNewDisk (request, options);

    return RunCommand (options);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskOperations::Init
//
////////////////////////////////////////////////////////////////////////////////

DiskOperations::Result DiskOperations::Init (const std::string & imagePath, const NewDiskRequest & request)
{
    CommandLineOptions  options = MakeOptions (Command::Init, imagePath);



    FillNewDisk (request, options);

    //  init takes the container as it finds it.
    options.disk.containerType.clear();

    return RunCommand (options);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskOperations::SectorRead
//
////////////////////////////////////////////////////////////////////////////////

DiskOperations::Result DiskOperations::SectorRead (
    const std::string  & imagePath,
    Numbering            numbering,
    int                  track,
    int                  sector,
    int                  count,
    const std::string  & hostPath)
{
    CommandLineOptions  options = MakeOptions (Command::SectorRead, imagePath);



    options.disk.numbering = numbering;
    options.disk.track     = track;
    options.disk.sector    = sector;
    options.disk.count     = count;
    options.disk.hostFile  = hostPath;

    return RunCommand (options);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskOperations::SectorWrite
//
////////////////////////////////////////////////////////////////////////////////

DiskOperations::Result DiskOperations::SectorWrite (
    const std::string  & imagePath,
    const std::string  & hostPath,
    Numbering            numbering,
    int                  track,
    int                  sector)
{
    CommandLineOptions  options = MakeOptions (Command::SectorWrite, imagePath);



    options.disk.hostFile  = hostPath;
    options.disk.numbering = numbering;
    options.disk.track     = track;
    options.disk.sector    = sector;

    return RunCommand (options);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskOperations::BlockRead
//
////////////////////////////////////////////////////////////////////////////////

DiskOperations::Result DiskOperations::BlockRead (
    const std::string  & imagePath,
    int                  block,
    int                  count,
    const std::string  & hostPath)
{
    CommandLineOptions  options = MakeOptions (Command::BlockRead, imagePath);



    options.disk.block    = block;
    options.disk.count    = count;
    options.disk.hostFile = hostPath;

    return RunCommand (options);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskOperations::BlockWrite
//
////////////////////////////////////////////////////////////////////////////////

DiskOperations::Result DiskOperations::BlockWrite (
    const std::string  & imagePath,
    const std::string  & hostPath,
    int                  block)
{
    CommandLineOptions  options = MakeOptions (Command::BlockWrite, imagePath);



    options.disk.hostFile = hostPath;
    options.disk.block    = block;

    return RunCommand (options);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskOperations::WritePayload
//
//  A file's bytes, type and address laid down as they came from another
//  volume, with no conversion. The payload's own encoding is left as the
//  reader set it, which is verbatim, so a copy between two disks is exact.
//
////////////////////////////////////////////////////////////////////////////////

DiskOperations::Result DiskOperations::WritePayload (
    const std::string  & imagePath,
    const std::string  & name,
    const FilePayload  & payload)
{
    DiskImageSession::OpenedImage  opened;
    Result                         result = OpenVolume (imagePath, opened);
    HRESULT                        hr     = S_OK;
    std::vector<Byte>              edited;



    if (result.Succeeded())
    {
        Dos33Volume   dos (opened.sectors);
        ProDosVolume  pro (opened.sectors);
        IVolume     & volume = (opened.kind == VolumeKind::Dos33)
                             ? static_cast<IVolume &> (dos)
                             : static_cast<IVolume &> (pro);

        hr = volume.Write (FilePath::Parse (name), payload, edited);

        if (FAILED (hr))
        {
            result.hr      = hr;
            result.message = DiskCommandResult::FormatFailure (imagePath, name, DescribeVolumeRefusal (hr));
        }
    }

    if (result.Succeeded())
    {
        result = CommitEdit (imagePath, opened, edited);
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskOperations::Rename
//
////////////////////////////////////////////////////////////////////////////////

DiskOperations::Result DiskOperations::Rename (
    const std::string  & imagePath,
    const std::string  & from,
    const std::string  & to)
{
    DiskImageSession::OpenedImage  opened;
    Result                         result = OpenVolume (imagePath, opened);
    HRESULT                        hr     = S_OK;
    std::vector<Byte>              edited;



    if (result.Succeeded())
    {
        Dos33Volume   dos (opened.sectors);
        ProDosVolume  pro (opened.sectors);
        IVolume     & volume = (opened.kind == VolumeKind::Dos33)
                             ? static_cast<IVolume &> (dos)
                             : static_cast<IVolume &> (pro);

        hr = volume.Rename (FilePath::Parse (from), to, edited);

        if (FAILED (hr))
        {
            result.hr      = hr;
            result.message = DiskCommandResult::FormatFailure (imagePath, to, DescribeVolumeRefusal (hr));
        }
    }

    if (result.Succeeded())
    {
        result = CommitEdit (imagePath, opened, edited);
    }

    return result;
}
