#include "Pch.h"

#include "CassoLink/Session/CassoLinkBlockDevices.h"
#include "CassoLink/Session/CassoLinkBlockTransfer.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/CassoLinkSession.h"
#include "Devices/Disk/BlockBuffer.h"
#include "Machines/Apple2/Common/HfsVolume.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"





static constexpr size_t  s_kBlock            = CassoLinkCodec::kBlockSize;
static constexpr size_t  s_kVolumeHeaderAt   = 2 * s_kBlock + 4;
static constexpr Byte    s_kVolumeHeaderType = 0xF0;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBlockDevices::~CassoLinkBlockDevices
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkBlockDevices::~CassoLinkBlockDevices() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBlockDevices::List
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkBlockDevices::List (LinkOperation & operation)
{
    HRESULT                            hr = S_OK;
    std::vector<CassoLinkBlockDevice>  devices;



    UNREFERENCED_PARAMETER (operation);

    hr = EnsureModule();
    CHR (hr);

    hr = m_session.m_host->ListBlockDevices (devices);
    CHR (hr);

    m_units.clear();

    for (const CassoLinkBlockDevice & device : devices)
    {
        std::unique_ptr<Unit>  unit = std::make_unique<Unit>();

        unit->device = device;
        m_units.push_back (std::move (unit));
    }

    {
        std::lock_guard<std::mutex>  lock (m_lock);

        m_devices = std::move (devices);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBlockDevices::Execute
//
//  A change is made into a buffer over the volume, and the blocks it touched
//  are written; a catalog of the top opens the volume again, so a disk
//  changed on the Apple is read as it is now.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkBlockDevices::Execute (LinkOperation & operation)
{
    HRESULT     hr   = S_OK;
    Unit      * unit = nullptr;
    VolumeKind  kind = VolumeKind::Unknown;



    hr = FindUnit (operation.drive, unit);
    CHR (hr);

    hr = EnsureModule();
    CHR (hr);

    if (operation.kind == LinkOperationKind::ReadDisk || operation.kind == LinkOperationKind::WriteDisk)
    {
        hr = Transfer (operation, *unit, operation.kind == LinkOperationKind::ReadDisk);
        CHR (hr);
        BAIL_OUT_IF (true, S_OK);
    }

    hr = SendLabel (std::format ("{} {}", operation.kind == LinkOperationKind::Enumerate ? "CATALOG" : "FILES", DescribeDevice (unit->device)));
    CHR (hr);

    if (unit->volume == nullptr || (operation.kind == LinkOperationKind::Enumerate && operation.path.IsEmpty()))
    {
        hr = OpenVolume (*unit, true, kind);
        CHR (hr);
    }

    switch (operation.kind)
    {
        case LinkOperationKind::Enumerate:
            hr                   = unit->volume->EnumerateDirectory (operation.path, operation.listing);
            operation.volumeKind = kind;
            break;

        case LinkOperationKind::ReadFile:
            hr = unit->volume->Read (operation.path, operation.file);
            break;

        case LinkOperationKind::WriteFile:
            hr = RunChange (*unit, [&operation] (const IVolume & volume, BlockBuffer & buffer) { return volume.Write (operation.path, operation.payload, buffer); });
            break;

        case LinkOperationKind::Delete:
            hr = RunChange (*unit, [&operation] (const IVolume & volume, BlockBuffer & buffer)
            {
                DeleteOutcome  outcome;

                return volume.Delete (operation.path, buffer, outcome);
            });
            break;

        case LinkOperationKind::Rename:
            hr = RunChange (*unit, [&operation] (const IVolume & volume, BlockBuffer & buffer) { return volume.Rename (operation.path, operation.newName, buffer); });
            break;

        case LinkOperationKind::MakeDirectory:
            hr = RunChange (*unit, [&operation] (const IVolume & volume, BlockBuffer & buffer) { return volume.CreateDirectory (operation.path, buffer); });
            break;

        case LinkOperationKind::RemoveDirectory:
            hr = RunChange (*unit, [&operation] (const IVolume & volume, BlockBuffer & buffer)
            {
                DeleteOutcome  outcome;

                return volume.RemoveDirectory (operation.path, true, buffer, outcome);
            });
            break;

        default:
            hr = HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED);
            break;
    }

    if (FAILED (hr) && unit->store != nullptr && !unit->store->GetError().empty())
    {
        operation.message = unit->store->GetError();
    }

    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBlockDevices::GetDevices
//
////////////////////////////////////////////////////////////////////////////////

std::vector<CassoLinkBlockDevice> CassoLinkBlockDevices::GetDevices() const
{
    std::lock_guard<std::mutex>  lock (m_lock);



    return m_devices;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBlockDevices::Clear
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkBlockDevices::Clear()
{
    std::lock_guard<std::mutex>  lock (m_lock);



    m_devices.clear();
    m_units.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBlockDevices::DescribeDevice
//
//  "S5,U1", or an Apple ///'s device by its SOS name, as ".D1".
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkBlockDevices::DescribeDevice (const CassoLinkBlockDevice & device)
{
    std::string  text = device.device.empty() ? std::format ("S{},U{}", device.slot, device.unit) : device.device;



    return device.volume.empty() ? text : text + " (/" + device.volume + ")";
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBlockDevices::EnsureModule
//
//  The block module is loaded again whenever the Apple has another, as a
//  disk operation's buffers overwrite it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkBlockDevices::EnsureModule()
{
    HRESULT  hr = S_OK;



    if (m_module.empty())
    {
        hr = m_session.m_host->BuildBlockModule (m_module);
        CHR (hr);
    }

    hr = m_session.m_host->EnsureModule (m_module);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBlockDevices::FindUnit
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkBlockDevices::FindUnit (int drive, Unit *& outUnit)
{
    HRESULT  hr    = S_OK;
    size_t   index = (size_t) (drive - kFirstDrive);
    bool     found = drive >= kFirstDrive && index < m_units.size();



    outUnit = nullptr;
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_INVALID_DRIVE));

    outUnit = m_units[index].get();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBlockDevices::OpenVolume
//
//  HFS first, as BlockVolumeOpener has it: a ProDOS volume header's storage
//  type byte is part of an HFS volume's creation date. A fresh open reads
//  the device again; any other opens the volume over the cached blocks, as
//  after a change, which a volume made before it doesn't see. A 140K
//  device's blocks are read in DOS order, as the volume layer reads a 140K
//  volume in that order.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkBlockDevices::OpenVolume (Unit & unit, bool isFresh, VolumeKind & outKind)
{
    HRESULT  hr       = S_OK;
    bool     isHfs    = false;
    bool     isProDos = false;
    size_t   header   = 0;



    outKind = VolumeKind::Unknown;

    unit.volume.reset();

    unit.image.reset();

    if (isFresh || unit.cache == nullptr)
    {
        unit.order.reset();
        unit.cache.reset();

        unit.store = std::make_unique<LinkStore> (*m_session.m_host, unit.device);
        unit.cache = std::make_unique<CachingBlockStore> (*unit.store);
        unit.view  = unit.cache.get();

        if (DosOrderBlockStore::IsDosOrderSize (unit.device.blocks))
        {
            unit.order = std::make_unique<DosOrderBlockStore> (*unit.cache);
            unit.view  = unit.order.get();
        }
    }

    unit.image = std::make_unique<BlockImage> (*unit.view);

    header   = ProDosSkeleton::GetBlockByteOffset (unit.image->size(), (int) (s_kVolumeHeaderAt / s_kBlock), s_kVolumeHeaderAt % s_kBlock);
    isHfs    = HfsVolume::IsFound (*unit.image);
    isProDos = !isHfs && unit.device.blocks > s_kVolumeHeaderAt / s_kBlock && ((*unit.image)[header] & 0xF0) == s_kVolumeHeaderType;

    if (!unit.store->GetError().empty())
    {
        hr = HRESULT_FROM_WIN32 (ERROR_READ_FAULT);
        CHR (hr);
    }

    if (isHfs)
    {
        unit.volume = std::make_unique<HfsVolume> (*unit.image);
        outKind     = VolumeKind::Hfs;
    }
    else if (isProDos)
    {
        unit.volume = std::make_unique<ProDosVolume> (*unit.image);
        outKind     = VolumeKind::ProDos;
    }

    CBREx (unit.volume != nullptr, HRESULT_FROM_WIN32 (ERROR_UNRECOGNIZED_VOLUME));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBlockDevices::RunChange
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkBlockDevices::RunChange (Unit & unit, const ChangeFn & change)
{
    HRESULT      hr     = S_OK;
    BlockBuffer  buffer (*unit.image);
    VolumeKind   kind   = VolumeKind::Unknown;



    hr = change (*unit.volume, buffer);
    CHR (hr);

    for (const auto & [block, bytes] : buffer.GetTouchedBlocks())
    {
        hr = unit.view->WriteBlocks (block, bytes);
        CHR (hr);
    }

    hr = unit.view->Commit();
    CHR (hr);

    hr = OpenVolume (unit, false, kind);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBlockDevices::Transfer
//
//  The Apple's label has the last block done, after each run of blocks; the
//  volume is opened again by the next operation, as a write changes it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkBlockDevices::Transfer (LinkOperation & operation, Unit & unit, bool isRead)
{
    HRESULT                             hr      = S_OK;
    CassoLinkBlockTransfer::Result      result;
    CassoLinkBlockTransfer::ProgressFn  report;
    CassoLinkBlockTransfer::CancelFn    cancel;
    bool                                verify  = isRead ? operation.verifyReads : operation.verifyWrites;



    report = [this, &operation, &unit, isRead] (uint32_t done, uint32_t total) { ReportTransfer (operation, unit, isRead, done, total); };
    cancel = [this] () { return m_session.m_canceled.load(); };

    unit.volume.reset();

    hr = isRead ? CassoLinkBlockTransfer::ReadDevice  (*m_session.m_host, m_session.m_fileIo, unit.device, operation.imagePath, verify, operation.resume, report, cancel, result)
                : CassoLinkBlockTransfer::WriteDevice (*m_session.m_host, m_session.m_fileIo, unit.device, operation.imagePath, verify, operation.resume, report, cancel, result);

    operation.message = result.message;

    if (SUCCEEDED (hr) && isRead)
    {
        m_session.m_sink.OnImageWritten (operation.imagePath, std::string());
    }

    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBlockDevices::ReportTransfer
//
//  The Apple's label has the last block done, and the operation its share.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkBlockDevices::ReportTransfer (LinkOperation & operation, const Unit & unit, bool isRead, uint32_t done, uint32_t total)
{
    HRESULT  hr = SendLabel (std::format ("{} S{},U{} BLOCK {} OF {}", isRead ? "READ" : "WRITE", unit.device.slot, unit.device.unit, done, total));



    IGNORE_RETURN_VALUE (hr, S_OK);

    m_session.Report (operation, total > 0 ? (int) ((uint64_t) done * 100 / total) : 100);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBlockDevices::SendLabel
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkBlockDevices::SendLabel (const std::string & text)
{
    return m_session.m_host->SendLabel (text.substr (0, CassoLinkCodec::kLabelLength));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBlockDevices::LinkStore::ReadBlocks
//
//  The blocks in requests of up to kMaxBlocksARequest; a device error fails
//  the read, with its words saved for the operation's message.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkBlockDevices::LinkStore::ReadBlocks (uint32_t first, uint32_t count, std::span<Byte> outBytes)
{
    HRESULT            hr     = S_OK;
    std::vector<Byte>  data;
    CassoLinkStatus    status;
    uint32_t           done   = 0;
    bool               fits   = outBytes.size() >= (size_t) count * s_kBlock;



    CBRAEx (fits, E_INVALIDARG);

    while (done < count)
    {
        Byte  run = (Byte) std::min<uint32_t> ((uint32_t) CassoLinkHost::kMaxBlocksARequest, count - done);

        hr = m_io.ReadBlocks (m_device, first + done, run, false, data, status);
        CHR (hr);

        if (status.error != CassoLinkHost::kErrorNone || status.done != run)
        {
            m_error = CassoLinkHost::DescribeBlockError (status.error, first + done + status.done);
            hr      = HRESULT_FROM_WIN32 (ERROR_READ_FAULT);
            CHR (hr);
        }

        std::copy (data.begin(), data.end(), outBytes.begin() + (ptrdiff_t) ((size_t) done * s_kBlock));
        done += run;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBlockDevices::LinkStore::WriteBlocks
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkBlockDevices::LinkStore::WriteBlocks (uint32_t first, std::span<const Byte> bytes)
{
    HRESULT          hr     = S_OK;
    CassoLinkStatus  status;
    size_t           blocks = bytes.size() / s_kBlock;
    size_t           done   = 0;



    while (done < blocks)
    {
        size_t  run = std::min (CassoLinkHost::kMaxBlocksARequest, blocks - done);

        hr = m_io.WriteBlocks (m_device, first + (uint32_t) done, bytes.subspan (done * s_kBlock, run * s_kBlock), true, status);
        CHR (hr);

        if (status.error != CassoLinkHost::kErrorNone || status.done != run)
        {
            m_error = CassoLinkHost::DescribeBlockError (status.error, first + (uint32_t) done + status.done);
            hr      = HRESULT_FROM_WIN32 (ERROR_WRITE_FAULT);
            CHR (hr);
        }

        done += run;
    }

Error:
    return hr;
}
