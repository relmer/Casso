#pragma once

#include "ArtifactWriter.h"
#include "Devices/Disk/DiskImageSession.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ImageArtifactSink
//
//  The sink that writes an assembly's objects into a disk image.
//
//  A SECOND IMPLEMENTATION OF AN EXISTING SEAM, not a new one. Where an
//  assembly's output goes was already a decision behind ArtifactSink, and the
//  mode already takes one by pointer; this is the arm that writes onto a
//  volume instead of onto the host.
//
//  ONLY THE OBJECT COMES HERE. The listing, the symbol table and the debug
//  file stay on the host, because that is where host tools and any later
//  debugger read them while the program under test runs from the image. The
//  listing call therefore delegates rather than being redirected.
//
//  NOTHING REACHES THE TARGET UNTIL EVERY OUTPUT HAS BEEN COMPOSED. Each write
//  is handed the buffer the previous one returned and only the last is
//  committed, so an assembly that fails partway leaves the image byte for byte
//  as it was. That guarantee is structural rather than remembered: the volume
//  layer computes a whole new buffer or none, so there is no half-written state
//  for a failure to leave behind.
//
//  It reaches the host only through the file-I/O seam, for the reason the disk
//  command runner does: the test assembly does not link the console
//  executable, so anything placed there would be unreachable.
//
////////////////////////////////////////////////////////////////////////////////

class ImageArtifactSink : public ArtifactSink
{
public:

    explicit ImageArtifactSink (IDiskFileIo & fileIo)
        : m_session (fileIo)
    {
    }

    HRESULT  WriteBinary  (const AssemblyResult & result,
                           const CommandLineOptions & options) override;

    HRESULT  WriteListing (const AssemblyResult & result,
                           const CommandLineOptions & options,
                           const std::vector<DialectReportLine> & reports) override;

    //  What went wrong, in the words a reader sees. Empty when nothing did.
    //
    //  CARRIED RATHER THAN PRINTED, for the reason the command line's refusals
    //  are: a library has no business owning a console, and a message written
    //  straight to one is a message no test can read.
    const std::string &  GetDiagnostics() const;

private:

    //  Every output onto the buffer the previous one returned, so the image is
    //  written once or not at all.
    HRESULT  ComposeOutputs (const AssemblyResult      & result,
                             const CommandLineOptions  & options,
                             DiskImageSession::OpenedImage & opened,
                             std::vector<Byte>         & outSectors);

    DiskImageSession  m_session;
    FileArtifactSink  m_hostArtifacts;
    std::string       m_diagnostics;
};
