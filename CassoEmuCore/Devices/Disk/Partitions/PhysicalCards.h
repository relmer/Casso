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

    //  Read without administrator access: true for a drive with no partition
    //  table, or with a partition Windows hasn't mounted with a file system.
    virtual HRESULT  HasUnmountedPartition (int number, bool & outHasUnmounted) = 0;
    virtual HRESULT  ListVolumes           (int number, std::vector<WindowsVolume> & outVolumes) = 0;

    //  The drive a volume such as `I:\` is on, read without administrator
    //  access. Fails for a volume on more than one drive.
    virtual HRESULT  FindVolumeDrive       (const std::wstring & root, int & outNumber) = 0;

    //  The drive erased and given CFFA's layout, through the broker. Each
    //  step of the write is reported to `progress`, on the calling thread.
    virtual HRESULT  InitializeDrive       (int number, HWND owner, uint64_t layout, const WriteProgress & progress) = 0;

    //  FAT32 written over the first partition of the drive's MBR without
    //  administrator access. `outDone` is false, and nothing written, for a
    //  drive with no such partition.
    virtual HRESULT  FormatFirstPartition  (int number, const WriteProgress & progress, bool & outDone) = 0;

    //  True while the broker runs, so a drive opens without the UAC prompt.
    virtual bool     IsBrokerRunning       () = 0;

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

    using ConfirmWrite     = std::function<bool (const PhysicalDrive &)>;
    using ConfirmElevation = std::function<bool (const PhysicalDrive &)>;

    struct StorageCard
    {
        PhysicalDrive  drive;

        //  A card with only volumes Windows mounts has no storage card node:
        //  its volumes are drives under This PC.
        bool           hasUnmounted = false;
    };

    static constexpr uint64_t  kMaxPartitionBytes = 32ull * 1024 * 1024;

    explicit PhysicalCards (IPhysicalCardHost & host) : m_host (host) {}

    static std::string   MakeCardPath (int number);
    static PathKind      ParsePath    (const std::string & path, int & outDrive, int & outPartition);
    static bool          IsCardPath   (const std::string & path);
    static bool          IsCardPath   (const std::wstring & path);
    static bool          IsOnCard     (const std::wstring & path);

    //  "SanDisk Ultra (29.7 GB)".
    static std::wstring  FormatDrive  (const PhysicalDrive & drive);

    //  "Storage card (SanDisk Ultra, 29.7 GB)", or with `letters`,
    //  "Storage card (I:, SanDisk Ultra, 29.7 GB)".
    static std::wstring  FormatCardLabel (const PhysicalDrive & drive, const std::wstring & letters = {});

    void     SetConfirmWrite     (ConfirmWrite confirm) { m_confirmWrite = std::move (confirm); }

    //  Called before an open or an initialize that starts the broker, which
    //  raises the UAC prompt, on the calling thread. False cancels with
    //  ERROR_CANCELLED.
    void     SetConfirmElevation (ConfirmElevation confirm) { m_confirmElevation = std::move (confirm); }

    //  While set, writes are sent without the confirmation callback, so one
    //  command that writes several times prompts once.
    void     SetConfirmed        (bool confirmed)       { m_confirmed = confirmed; }

    HRESULT  ListRemovableDrives (std::vector<PhysicalDrive> & outDrives);
    HRESULT  ListStorageCards    (std::vector<StorageCard> & outCards);
    HRESULT  ListVolumes         (int number, std::vector<WindowsVolume> & outVolumes) { return m_host.ListVolumes (number, outVolumes); }

    //  The storage card a drive letter's volume is on. False for a volume on
    //  any other drive.
    bool     TryGetVolumeCard    (const std::wstring & root, int & outNumber);

    //  The card closed, erased and given CFFA's layout: up to 13 empty ProDOS
    //  volumes of 32 MB. The only write that is not confirmed by the
    //  callback, as the caller confirms it with the volumes erased.
    //
    //  Runs on any thread. While it runs the card is busy: `TryGetWork`
    //  returns its progress, and opening it fails with ERROR_BUSY. The first
    //  call to `progress`, with 0 and 0, comes once the card is busy.
    HRESULT  Initialize          (int number, HWND owner, uint64_t layout, const WriteProgress & progress = {});

    //  True while the card is being initialized, with the bytes written so
    //  far and the bytes to write. `outTotal` is 0 before the first report.
    bool     TryGetWork          (int number, uint64_t & outDone, uint64_t & outTotal);

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

    struct Work
    {
        uint64_t  done  = 0;
        uint64_t  total = 0;
    };

    static constexpr std::string_view  kDrivePrefix = "\\\\.\\PhysicalDrive";
    static constexpr size_t            kMaxDigits   = 4;
    static constexpr uint64_t          kGigabyte    = 1024ull * 1024 * 1024;

    static std::string  FormatDriveSize (const PhysicalDrive & drive);

    HRESULT  FindSession   (int number, Session * & outSession);
    HRESULT  CheckNotBusy  (int number);
    void     SetWork       (int number, uint64_t done, uint64_t total);
    void     EndWork       (int number);
    HRESULT  ListSession   (Session & session);
    HRESULT  FindPartition (Session & session, int partition, const PartitionEntry * & outEntry);
    HRESULT  StageWrite    (int number, int partition, std::span<const Byte> bytes, Session * & outSession);
    HRESULT  CheckElevation (const PhysicalDrive & drive);

    IPhysicalCardHost       & m_host;
    ConfirmWrite              m_confirmWrite;
    ConfirmElevation          m_confirmElevation;
    std::mutex                m_mutex;
    std::map<int, Session>    m_sessions;
    std::mutex                m_workMutex;
    std::map<int, Work>       m_work;
    int64_t                   m_nextStamp  = 1;
    bool                      m_confirming = false;
    std::atomic<bool>         m_confirmed  = false;
};