#pragma once

#include "Pch.h"

#include "CassoLink/Host/CassoLinkHostTypes.h"

class CassoLinkHost;
struct CassoLinkPacket;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkAdtProServer
//
//  Answering ADTPro's own client: its requests read and reported, the
//  shared folder's directory, CD, size, get and put, nibble puts, and the
//  virtual drive's images, blocks and batch names.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkAdtProServer
{
public:
    explicit CassoLinkAdtProServer (CassoLinkHost & host);

    static constexpr size_t  kBlockSize          = 512;
    static constexpr size_t  kDirectoryPage      = 1024;
    static constexpr size_t  kNamesPerPage       = 18;
    static constexpr size_t  kSectorsPerTrack    = 16;

    //  Virtual-drive request kinds.
    static constexpr Byte    kDriveRead        = 0x01;
    static constexpr Byte    kDriveWrite2      = 0x04;
    static constexpr Byte    kDriveRead3Drive2 = 0x05;

    void     SetVirtualDrive (int drive, const std::string & name);

    HRESULT  ServeOne (DWORD timeoutMs);

    //  The ProDOS date and time for Read3 from a local time.
    static std::array<Byte, 4>  GetProDosDateTime (const SYSTEMTIME & time);

    //  A Directory page: the folder's path and a page of its files, as
    //  ADTPro's client prints it. The last byte is 1 when another page
    //  follows.
    static std::vector<Byte>    BuildDirectoryPage (const std::string & path, const std::vector<std::string> & names, size_t page);

    //  Whether a name matches a DIR request's filespec: * for any run of
    //  characters and ? for any one, without regard to case; an empty
    //  filespec matches every name.
    static bool                 MatchesFilespec    (const std::string & name, const std::string & filespec);

    //  One revolution of a nibble track from a capture of two: from the first
    //  address field, as long as the track turns, padded with sync bytes to
    //  kNibbleTrackSize.
    static std::vector<Byte>    ToNibbleTrack      (std::span<const Byte> capture);

    //  An image file as 512-byte blocks, and back. A .dsk or .do of 143,360
    //  bytes is in DOS 3.3 sector order, as ADTPro's server reads and writes
    //  one, so each block's two sectors are gathered from where DOS 3.3 puts
    //  them; any other image is in block order already.
    static bool                 IsDosOrder         (const std::string & name, size_t size);
    static void                 ToBlockOrder       (const std::string & name, const std::vector<Byte> & file, std::vector<Byte> & outBlocks);
    static void                 ToFileOrder        (const std::string & name, const std::vector<Byte> & blocks, std::vector<Byte> & outFile);
    HRESULT  SendHome        ();

    HRESULT  ServePacket       (const CassoLinkPacket & packet);
    HRESULT  ServeChangeDir    (const CassoLinkPacket & packet);
    HRESULT  ServeDirectory    (const CassoLinkPacket & packet);
    HRESULT  ServeSize         (const CassoLinkPacket & packet);
    HRESULT  ServeGet          (const CassoLinkPacket & packet);
    HRESULT  ServePut          (const CassoLinkPacket & packet, bool batch);
    HRESULT  ServeNibblePut    (const CassoLinkPacket & packet, bool batch);
    std::string  ResolveName   (const std::vector<Byte> & payload, size_t & outEnd) const;
    HRESULT  ServeVirtualDrive ();
    void     ReportRequest     (const CassoLinkPacket & packet, HRESULT result);

    HRESULT  ReceiveImage      (std::vector<Byte> & image, bool untilSummary = true);
    HRESULT  LoadVirtualDrive  (int drive);
    HRESULT  WriteDriveBlock   (size_t drive, size_t block, std::span<const Byte> data);
    HRESULT  ReadImage         (const std::string & name, std::vector<Byte> & outFile);
    std::string  GetBatchName  (const std::string & prefix, const std::string & extension);
    static std::string  AddImageExtension (const std::string & name, size_t bytes);

    static std::string  ReadName (const std::vector<Byte> & payload, size_t & outEnd);

    //  The DOS 3.3 sector for each of a track's 16 sectors in block order:
    //  block n of a track is sectors 2n and 2n + 1.
    static constexpr size_t  s_kBlockSectorToDos[16] = { 0, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 15 };

private:
    friend class CassoLinkHost;

    CassoLinkHost  & m_host;

    std::vector<Byte>             m_buffer;
    std::array<std::string, 2>    m_driveNames = { "Virtual.po", "Virtual2.po" };
    std::array<std::vector<Byte>, 2>  m_driveImages;
    std::array<bool, 2>               m_driveLoaded        = {};
    DWORD                             m_replyTimeoutMs     = CassoLinkHostTypes::kReplyTimeoutMs;
    DWORD                             m_operationTimeoutMs = CassoLinkHostTypes::kOperationTimeoutMs;
    std::atomic<bool>                 m_adtProClient       { false };
    CassoLinkHostTypes::Clients                           m_clients            = CassoLinkHostTypes::Clients::Both;
    std::atomic<HRESULT>              m_lastResult { S_OK };
    CassoLinkHostTypes::ActivityFn  m_activity;
    std::vector<std::string>        m_listing;   // the last DIR's names, which ADTPro's file chooser picks from by page and row
};
