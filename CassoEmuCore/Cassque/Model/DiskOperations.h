#pragma once

#include "Pch.h"

#include "CommandLineOptions.h"
#include "Devices/Disk/DiskCommandResult.h"
#include "Devices/Disk/DiskImageSession.h"
#include "Machines/Apple2/Common/VolumeTypes.h"


class IDiskFileIo;
class IIntentChannel;
enum class VolumeKind;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskOperations
//
//  Every verb the command-line disk tool offers, as calls a window can make.
//
//  BYTE IDENTITY BY CONSTRUCTION. Each verb the command line has is answered by
//  building the same options the parser would and calling the same runner, so
//  a put from the window and a put from a script produce one image because
//  they are one code path. The verbs the command line does not have -- a raw
//  read for a preview, a raw write for a copy between two disks, a rename --
//  go through the session and the volume the runner itself uses.
//
//  Every result carries the runner's own diagnostics, so the dialog a user
//  sees quotes the sentence a script's log would show.
//
////////////////////////////////////////////////////////////////////////////////

class DiskOperations
{
public:
    using Encoding  = CommandLineOptions::DiskOptions::Encoding;
    using Numbering = CommandLineOptions::DiskOptions::Numbering;
    using Command   = CommandLineOptions::DiskOptions::Command;

    //  What one operation produced: whether anything was done, and the text
    //  the runner wrote about it.
    struct Result
    {
        HRESULT            hr         = S_OK;
        int                exitStatus = DiskCommandResult::kClean;
        std::string        message;       // diagnostics, warnings included
        std::string        output;        // the runner's stdout text
        std::vector<Byte>  payload;       // a get's bytes when no file was written

        bool  Succeeded () const { return SUCCEEDED (hr); }
    };

    //  What a new disk is asked to be, in the command line's own words.
    struct NewDiskRequest
    {
        std::string  containerType;    // dsk, do, po, woz, nib, nb2; empty takes the extension
        std::string  formatName;       // dos33, prodos, none
        std::string  volumeName;       // a DOS number or a ProDOS name; empty takes the default
        bool         bootable     = false;
        std::string  bootableFrom;     // an operating-system image, when not the stock master
    };

    explicit DiskOperations (IDiskFileIo & fileIo);

    void  SetIntentChannel (IIntentChannel * channel) { m_intentChannel = channel; }

    //  Reads through the volume layer.
    Result  List (const std::string & imagePath, VolumeListing & outListing, VolumeKind & outKind);
    Result  Read (const std::string & imagePath, const std::string & name, FilePayload & outPayload);

    //  Command-line verbs, through the runner.
    Result  Get      (const std::string & imagePath, const std::string & name, Encoding encoding, const std::string & hostPath);
    Result  Put      (const std::string & imagePath, const std::string & hostPath, const std::string & name,
                      const std::string & typeName, bool hasLoadAddress, Word loadAddress, Encoding encoding);
    Result  Delete   (const std::string & imagePath, const std::string & name);
    Result  Boot     (const std::string & imagePath, const std::string & name);
    Result  Create   (const std::string & imagePath, const NewDiskRequest & request);
    Result  Init     (const std::string & imagePath, const NewDiskRequest & request);

    Result  SectorRead  (const std::string & imagePath, Numbering numbering, int track, int sector, int count, const std::string & hostPath);
    Result  SectorWrite (const std::string & imagePath, const std::string & hostPath, Numbering numbering, int track, int sector);
    Result  BlockRead   (const std::string & imagePath, int block, int count, const std::string & hostPath);
    Result  BlockWrite  (const std::string & imagePath, const std::string & hostPath, int block);

    //  Writes through the volume layer, for what the command line cannot say.
    Result  WritePayload (const std::string & imagePath, const std::string & name, const FilePayload & payload);
    Result  Rename       (const std::string & imagePath, const std::string & from, const std::string & to);

    //  The options a command-line invocation of this verb would parse to.
    static CommandLineOptions  MakeOptions (Command command, const std::string & imagePath);

    //  A volume-layer refusal in the words a user reads.
    static std::string  DescribeVolumeRefusal (HRESULT hr);

private:
    Result  RunCommand (const CommandLineOptions & options);
    Result  OpenVolume (const std::string & imagePath, DiskImageSession::OpenedImage & outOpened);
    Result  CommitEdit (const std::string & imagePath, const DiskImageSession::OpenedImage & opened,
                        const std::vector<Byte> & edited);

    static void  FillNewDisk (const NewDiskRequest & request, CommandLineOptions & inOutOptions);

    IDiskFileIo     & m_fileIo;
    IIntentChannel  * m_intentChannel = nullptr;
};
