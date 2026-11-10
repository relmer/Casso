#pragma once

#include "Pch.h"

#include "Devices/Disk/CachingBlockStore.h"
#include "Devices/Disk/IBlockStore.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Devices/Disk/Partitions/PartitionAccess.h"
#include "Seams/RawDevice/IRawDevice.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IPhysicalCardHost
//
//  The drives of the PC, and the blocks of one of them.
//
//  A SEAM BECAUSE OPENING A DRIVE RAISES A UAC PROMPT. The tests open a card
//  whose blocks are in memory.
//
////////////////////////////////////////////////////////////////////////////////

class IPhysicalCardHost
{
public:
    virtual ~IPhysicalCardHost() = default;

    //  Every drive, removable or not.
    virtual HRESULT  ListDrives (std::vector<PhysicalDrive> & outDrives) = 0;

    //  Fails with ERROR_CANCELLED when the user declines administrator access.
    //  Destroying `outStore` closes the drive.
    virtual HRESULT  OpenDrive  (int number, HWND owner, PhysicalDrive & outDrive, std::unique_ptr<IBlockStore> & outStore) = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCards
//
//  The CompactFlash and SD cards open in Casso Explorer, each read and
//  written in place through a removable drive of the PC.
//
//  A card's path is `\\.\PhysicalDrive2`, and a partition's is that path, '|'
//  and the partition's step: `\\.\PhysicalDrive2|#3.po`. A card is opened by
//  `Open` only, which is where Windows prompts for administrator access. A
//  path on a card that is not open fails, and never raises the prompt.
//
//  A partition of kMaxPartitionBytes or less is read and written whole. A
//  write is sent to the card after the confirmation callback returns true
//  for the drive, and only the blocks that differ are sent. With no callback
//  every write fails with ERROR_CANCELLED.
//
//  A card's blocks are cached while it is open. Opening it again reads it
//  anew.
//
////////////////////////////////////////////////////////////////////////////////

class PhysicalCards
{
public:
    enum class PathKind
    {
        //  Not a path on a drive.
        None,
        Card,
        Partition,

        //  Anything else on a drive, such as a commit's temporary file.
        Other,
    };

    using ConfirmWrite = std::function<bool (const PhysicalDrive &)>;

    static constexpr uint64_t  kMaxPartitionBytes = 32ull * 1024 * 1024;

    explicit PhysicalCards (IPhysicalCardHost & host) : m_host (host) {}

    static std::string   MakeCardPath (int number);
    static PathKind      ParsePath    (const std::string & path, int & outDrive, int & outPartition);
    static bool          IsCardPath   (const std::string & path);
    static bool          IsCardPath   (const std::wstring & path);
    static bool          IsOnCard     (const std::wstring & path);

    //  "SanDisk Ultra (29.7 GB)".
    static std::wstring  FormatDrive  (const PhysicalDrive & drive);

    void     SetConfirmWrite     (ConfirmWrite confirm) { m_confirmWrite = std::move (confirm); }

    HRESULT  ListRemovableDrives (std::vector<PhysicalDrive> & outDrives);

    //  Fails with E_ACCESSDENIED for a drive that is not removable.
    HRESULT  Open                (int number, HWND owner);
    void     Close               (int number);
    bool     IsOpen              (int number);
    bool     TryGetDrive         (int number, PhysicalDrive & outDrive);

    HRESULT  ListPartitions      (int number, PartitionLayoutKind & outKind, std::vector<PartitionEntry> & outEntries);
    HRESULT  ReadPartition       (int number, int partition, std::vector<Byte> & outBytes);

    //  `bytes` is written over the start of the partition.
    HRESULT  WritePartition      (int number, int partition, std::span<const Byte> bytes);

    //  The partition's length, and a number that changes at each open of the
    //  card and each write to it.
    HRESULT  GetStamp            (int number, int partition, FileStamp & outStamp);

private:
    struct Session
    {
        PhysicalDrive                       drive;
        std::unique_ptr<IBlockStore>        store;
        std::unique_ptr<CachingBlockStore>  cache;
        PartitionLayoutKind                 kind   = PartitionLayoutKind::None;
        std::vector<PartitionEntry>         entries;
        bool                                listed = false;
        int64_t                             stamp  = 0;
    };

    static constexpr std::string_view  kDrivePrefix = "\\\\.\\PhysicalDrive";
    static constexpr size_t            kMaxDigits   = 4;

    HRESULT  FindSession   (int number, Session * & outSession);
    HRESULT  ListSession   (Session & session);
    HRESULT  FindPartition (Session & session, int partition, const PartitionEntry * & outEntry);
    HRESULT  StageWrite    (int number, int partition, std::span<const Byte> bytes, Session * & outSession);

    IPhysicalCardHost       & m_host;
    ConfirmWrite              m_confirmWrite;
    std::mutex                m_mutex;
    std::map<int, Session>    m_sessions;
    int64_t                   m_nextStamp  = 1;
    bool                      m_confirming = false;
};