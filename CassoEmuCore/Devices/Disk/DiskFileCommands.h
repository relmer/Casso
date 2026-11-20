#pragma once

#include "Pch.h"

#include "CommandLineOptions.h"
#include "Devices/Disk/BlockImage.h"
#include "Devices/Disk/DiskCommandResult.h"
#include "Devices/Disk/FilePath.h"
#include "Devices/Disk/IDiskCommandContext.h"
#include "Machines/Apple2/Common/IVolume.h"

enum class VolumeKind;
class BlockBuffer;
class CardVolumeEdit;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFileCommands
//
//  The disk tool's commands on the files and folders in a volume: get, put,
//  delete, krunch, test, mkdir and rmdir.
//
////////////////////////////////////////////////////////////////////////////////

class DiskFileCommands
{
public:
    explicit DiskFileCommands (IDiskCommandContext & context) : m_context (context) {}

    void  RunGet       (const CommandLineOptions & options, DiskCommandResult & result);
    void  RunPut       (const CommandLineOptions & options, DiskCommandResult & result);
    void  RunDelete    (const CommandLineOptions & options, DiskCommandResult & result);
    void  RunKrunch    (const CommandLineOptions & options, DiskCommandResult & result);
    void  RunMkdir     (const CommandLineOptions & options, DiskCommandResult & result);
    void  RunRmdir     (const CommandLineOptions & options, DiskCommandResult & result);
    void  RunTest      (const CommandLineOptions & options, DiskCommandResult & result);

    //  A typed path as the volume reads it. A path with no leading slash is
    //  read from the volume directory and comes back unchanged; one with a
    //  leading slash names the volume first, and is turned down when that name
    //  is not this image's. A DOS 3.3 name is one name, slashes included.
    HRESULT  ResolveVolumePath (const CommandLineOptions             & options,
                                VolumeKind                             kind,
                                const BlockImage                     & sectors,
                                const std::string                    & typed,
                                std::string                          & outPath,
                                DiskCommandResult                    & result);

    //  The path `get`, `delete` and `boot` act on, and the name used in their
    //  messages: the name as typed, or with --index the entry at that position.
    HRESULT  ResolveEntryPath (const CommandLineOptions             & options,
                               VolumeKind                             kind,
                               const BlockImage                     & sectors,
                               FilePath                             & outPath,
                               std::string                          & outName,
                               DiskCommandResult                    & result);

private:
    //  The resource fork, Finder information and, with no --type or --load,
    //  the ProDOS type from `._NAME` beside a file being put, as get writes
    //  them and Casso Explorer reads them.
    void     ReadAppleDouble  (const CommandLineOptions & options, FilePayload & inOutPayload);
    HRESULT  WriteAppleDouble (const std::string & hostFile, const FilePayload & payload);

    //  Puts --user on a CP/M name. An error on any other file system.
    HRESULT  ApplyUser (const CommandLineOptions & options, VolumeKind kind, std::string & inOutName, DiskCommandResult & result);

    //  The blocks a change to a card partition's volume touched written to
    //  the card, or the reason the commit failed set in the result.
    static HRESULT  CommitCard (const std::string & imagePath, CardVolumeEdit & card, const BlockBuffer & edited, DiskCommandResult & result);

    //  Whether mkdir and rmdir change the file system.
    static bool  HasDirectories (VolumeKind kind);

    IDiskCommandContext  & m_context;
};
