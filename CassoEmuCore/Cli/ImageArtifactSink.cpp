#include "Pch.h"

#include "ImageArtifactSink.h"

#include "Devices/Disk/AssembledFilePlacement.h"
#include "Devices/Disk/Dos33Volume.h"
#include "Devices/Disk/FilePath.h"
#include "Devices/Disk/ProDosVolume.h"
#include "Devices/Disk/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ImageArtifactSink::GetDiagnostics
//
////////////////////////////////////////////////////////////////////////////////

const std::string & ImageArtifactSink::GetDiagnostics() const
{
    return m_diagnostics;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageArtifactSink::WriteListing
//
//  Delegated to the host sink, unredirected.
//
//  The listing describes the assembly rather than any one of its outputs, and
//  it is read from the host while the program under test runs from the image.
//  Sending it into the volume would put it where the reader is not and take
//  space from the disk they are building.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ImageArtifactSink::WriteListing (const AssemblyResult & result,
                                         const CommandLineOptions & options,
                                         const std::vector<DialectReportLine> & reports)
{
    return m_hostArtifacts.WriteListing (result, options, reports);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageArtifactSink::ComposeOutputs
//
//  Every output onto the buffer the one before it returned.
//
//  THE COMPOSITION IS WHAT MAKES THE WRITE ALL-OR-NOTHING. The volume layer
//  never mutates in place: each write either produces a complete new buffer or
//  produces none. Feeding each result into the next and committing only the
//  last means a failure anywhere abandons every buffer and leaves the image
//  untouched, without any step having to remember to undo itself.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ImageArtifactSink::ComposeOutputs (const AssemblyResult          & result,
                                           const CommandLineOptions      & options,
                                           DiskImageSession::OpenedImage & opened,
                                           std::vector<Byte>             & outSectors)
{
    HRESULT      hr     = S_OK;
    bool         named  = !options.onDiskName.empty();
    std::string  onDisk;
    std::string  typeErr;
    FilePayload  payload;
    FilePath     path;



    outSectors = opened.sectors;

    for (const SavePoint & span : result.savePoints)
    {
        std::vector<Byte>  edited;

        //  The command line beats the source, and the source beats the default,
        //  which is the precedence the tool already applies to the object's
        //  name. Each output carries its own, so a source that produces several
        //  names them individually.
        onDisk = named          ? options.onDiskName
               : span.name.empty() ? options.outputFile
                                   : span.name;

        hr = AssembledFilePlacement::BuildPayload (span, opened.kind, options.imageTypeName, payload, typeErr);
        CHRF (hr, m_diagnostics += typeErr + "\n");

        path = FilePath::Parse (onDisk);

        {
            Dos33Volume   dos (outSectors);
            ProDosVolume  pro (outSectors);
            IVolume     & volume = (opened.kind == VolumeKind::Dos33)
                                 ? static_cast<IVolume &> (dos)
                                 : static_cast<IVolume &> (pro);

            hr = volume.Write (path, payload, edited);
        }

        CHRF (hr, m_diagnostics += DiskCommandResult::Failure (options.imagePath, onDisk,
                                       "could not be written to the volume") + "\n");

        outSectors = edited;
    }

    hr = ApplyStartupProgram (options, opened, onDisk, outSectors);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageArtifactSink::ApplyStartupProgram
//
//  Makes the object the program the volume runs when it boots.
//
//  INSIDE THE COMPOSITION, before the single commit, so a disk that cannot
//  actually run the file is refused with the image still untouched rather than
//  written and then complained about.
//
//  WHETHER DOS 3.3 WOULD RUN IT IS THE VOLUME LAYER'S RULE, not a second copy
//  of it here. The command that sets a startup program separately applies the
//  same one, and two routes to one idea with a rule each is how they come to
//  accept different things.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ImageArtifactSink::ApplyStartupProgram (const CommandLineOptions            & options,
                                                const DiskImageSession::OpenedImage & opened,
                                                const std::string                   & onDisk,
                                                std::vector<Byte>                   & inOutSectors)
{
    HRESULT            hr       = S_OK;
    HRESULT            listHr   = S_OK;
    bool               wanted   = options.setStartupProgram;
    bool               runnable = true;
    FilePath           path;
    VolumeListing      listing;
    std::vector<Byte>  edited;



    BAIL_OUT_IF (!wanted, S_OK);

    path = FilePath::Parse (onDisk);

    {
        Dos33Volume   dos (inOutSectors);
        ProDosVolume  pro (inOutSectors);
        IVolume     & volume = (opened.kind == VolumeKind::Dos33)
                             ? static_cast<IVolume &> (dos)
                             : static_cast<IVolume &> (pro);

        hr = volume.SetStartupProgram (path, edited);

        if (SUCCEEDED (hr) && opened.kind == VolumeKind::Dos33)
        {
            listHr = volume.Enumerate (listing);
            IGNORE_RETURN_VALUE (listHr, S_OK);

            runnable = Dos33Volume::IsRunnableAsGreeting (listing, onDisk);
        }
    }

    CHRF (hr, m_diagnostics += DiskCommandResult::Failure (options.imagePath, onDisk,
                                   "cannot be made the startup program") + "\n");

    if (!runnable)
    {
        m_diagnostics += DiskCommandResult::Failure (options.imagePath, onDisk,
                             "would not run at boot: a booting DOS 3.3 RUNs its greeting, "
                             "so a binary named as one leaves the disk booting and the program never running") + "\n";
    }

    CBR (runnable);

    inOutSectors = edited;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageArtifactSink::WriteBinary
//
//  The assembly's objects, onto the volume.
//
//  A MISSING IMAGE IS REFUSED and names the command that makes one. The
//  assembler does not create disks: doing so would mean choosing a container,
//  a filesystem, a volume name and whether it boots, all of which the
//  disk-creation command already owns. Two routes to those decisions is two
//  sets of rules for one job.
//
//  The freshness flag the session carries for a file that does not exist yet is
//  deliberately NOT set here. It exists for the command that creates disks, and
//  setting it would turn "there is no such image" into a new one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ImageArtifactSink::WriteBinary (const AssemblyResult & result,
                                        const CommandLineOptions & options)
{
    HRESULT                        hr       = S_OK;
    bool                           hasBytes = !result.savePoints.empty();
    DiskCommandResult              opening;
    DiskImageSession::OpenedImage  opened;
    std::vector<Byte>              sectors;



    if (!hasBytes)
    {
        m_diagnostics += DiskCommandResult::Failure (options.imagePath, "",
                             "has nothing to receive: the assembly produced no bytes") + "\n";
    }

    CBR (hasBytes);

    hr = m_session.OpenImage (options.imagePath, opened, opening);
    CHRF (hr, m_diagnostics += opening.diagnostics);

    hr = ComposeOutputs (result, options, opened, sectors);
    CHR (hr);

    hr = m_session.SaveAndCommit (opened, sectors, opening);
    CHRF (hr, m_diagnostics += opening.diagnostics);

Error:
    return hr;
}
