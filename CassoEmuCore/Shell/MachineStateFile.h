#pragma once

#include "Pch.h"

#include "Core/IMachineState.h"
#include "Devices/Disk/IDiskImage.h"

class MachineHost;





////////////////////////////////////////////////////////////////////////////////
//
//  SavedDisk
//
//  One disk a machine state file carries: the bay it was in, the path of the
//  image file it was mounted from, and the whole image as the machine held
//  it, writes not yet saved to the file included.
//
////////////////////////////////////////////////////////////////////////////////

struct SavedDisk
{
    int                slot   = 0;
    int                drive  = 0;
    DiskFormat         format = DiskFormat::Dsk;
    std::string        path;
    std::vector<Byte>  image;
};





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateContents
//
//  A machine state file, read: which machine and ROM set saved it, the disks
//  in it, the machine's own state blob (MachineHost::SaveState), and from
//  version 2 the time it was saved (a UTC FILETIME as one 64-bit value) and
//  a PNG thumbnail of the screen. A version-1 state has a zero time and no
//  thumbnail.
//
////////////////////////////////////////////////////////////////////////////////

struct MachineStateContents
{
    std::wstring            machineName;
    uint64_t                romIdentity = 0;
    std::vector<SavedDisk>  disks;
    std::vector<Byte>       machineState;
    uint64_t                savedAtUtc  = 0;
    std::vector<Byte>       thumbnailPng;
};





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateError
//
//  Why a state could not be saved or loaded: a short label and the rule
//  behind it, as complete sentences.
//
////////////////////////////////////////////////////////////////////////////////

struct MachineStateError
{
    std::string  label;
    std::string  detail;
};





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateFile
//
//  The whole machine as a file the user keeps: pause, save, and resume later,
//  in this session or another. Everything here works on bytes; the caller
//  reads and writes the file.
//
//  The file is a fixed header and a payload:
//
//    magic           8 bytes   "CASSOSTA"
//    format version  2 bytes
//    reserved        2 bytes   zero
//    payload size    8 bytes
//    payload hash    8 bytes   StateHash over the payload as stored
//    payload         a StateWriter section 'SFIL': the machine name, the ROM
//                    identity, each mounted disk (bay, format, path, image
//                    bytes), the machine state blob, and from version 2 the
//                    time it was saved and a PNG thumbnail of the screen
//
//  From version 3 the payload is stored compressed: its size before
//  compression (8 bytes), then the section packed with the Windows
//  Compression API's MSZIP algorithm (deflate) in buffer mode.
//
//  All values are little-endian. The size and hash are checked before
//  anything is read, so a damaged or cut-short file fails to load before the
//  machine is touched.
//
////////////////////////////////////////////////////////////////////////////////

class MachineStateFile
{
public:
    static constexpr const wchar_t *  kExtension      = L"cassostate";
    static constexpr uint16_t         kFormatVersion  = 3;
    static constexpr size_t           kHeaderSize     = 28;
    static constexpr uint32_t         kPayloadTag     = IMachineState::MakeTag ('S', 'F', 'I', 'L');
    static constexpr uint16_t         kPayloadVersion = 2;

    static HRESULT  Build (MachineHost & machine, std::vector<Byte> & outBytes, MachineStateError & outError);
    static HRESULT  Build (MachineHost & machine, const std::vector<Byte> & thumbnailPng, std::vector<Byte> & outBytes, MachineStateError & outError);
    static HRESULT  Parse (const std::vector<Byte> & bytes, MachineStateContents & outContents, MachineStateError & outError);
    static HRESULT  Check (const MachineHost & machine, const MachineStateContents & contents, MachineStateError & outError);
    static HRESULT  Apply (MachineHost & machine, const MachineStateContents & contents, MachineStateError & outError);

    //  An error from its label and detail.
    static MachineStateError  MakeError (const char * label, const char * detail);

    //  A file name for a save of machineName taken at time.
    static std::wstring  MakeDefaultFileName (const std::wstring & machineName, const SYSTEMTIME & time);

private:
    static constexpr size_t    kMagicSize                  = 8;
    static constexpr uint16_t  kFirstVersionWithThumbnail  = 2;
    static constexpr uint16_t  kFirstCompressedVersion     = 3;
    static constexpr size_t    kMaxUnpackedPayloadBytes    = 512ull * 1024 * 1024;
    static constexpr DWORD     kCompressionAlgorithm       = COMPRESS_ALGORITHM_MSZIP;

    static void      WriteHeader        (const std::vector<Byte> & payload, std::vector<Byte> & outBytes);
    static HRESULT   CheckHeader        (const std::vector<Byte> & bytes, uint16_t & outVersion, MachineStateError & outError);
    static HRESULT   PackPayload        (const std::vector<Byte> & section, std::vector<Byte> & outPayload);
    static HRESULT   UnpackPayload      (const Byte * data, size_t size, std::vector<Byte> & outSection);
    static HRESULT   ReadPayload        (const Byte * data, size_t size, MachineStateContents & outContents);
    static HRESULT   CheckDisks         (const MachineStateContents & contents, MachineStateError & outError);
    static HRESULT   RestoreDisks       (MachineHost & machine, const MachineStateContents & contents);
    static HRESULT   RollBack           (MachineHost & machine, const std::vector<Byte> & before);
    static uint64_t  ReadLittleEndian   (const Byte * data, size_t byteCount);
    static void      AppendLittleEndian (uint64_t value, size_t byteCount, std::vector<Byte> & outBytes);
};
