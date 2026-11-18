#pragma once

#include "Pch.h"

#include "CassoLink/CassoLinkHost.h"
#include "Devices/Disk/BlockImage.h"
#include "Devices/Disk/CachingBlockStore.h"
#include "Devices/Disk/IBlockStore.h"

class BlockBuffer;
class CassoLinkSession;
class IVolume;
struct LinkOperation;
enum class VolumeKind;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBlockDevices
//
//  The Apple's ProDOS block devices and SmartPort units over the link, by
//  the block module, as the session's drives from kFirstDrive in the order
//  List found them (FR-191, FR-192). A drive's volume is read through its
//  blocks as an operation touches them, with the blocks cached until the
//  next catalog of its top, and a change writes only the blocks it touched.
//  A whole device is read to an image or written from one by
//  CassoLinkBlockTransfer. The Apple's label has the device and the block.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkBlockDevices
{
public:
    static constexpr int  kFirstDrive = 3;

    explicit CassoLinkBlockDevices (CassoLinkSession & session) : m_session (session) {}
    ~CassoLinkBlockDevices ();

    CassoLinkBlockDevices (const CassoLinkBlockDevices &)             = delete;
    CassoLinkBlockDevices & operator= (const CassoLinkBlockDevices &) = delete;

    //  List block devices: the devices the drives are, from now on.
    HRESULT  List    (LinkOperation & operation);

    //  An operation on a drive from kFirstDrive.
    HRESULT  Execute (LinkOperation & operation);

    //  The devices, by drive from kFirstDrive; any thread.
    std::vector<CassoLinkBlockDevice>  GetDevices () const;

    //  The devices forgotten, as a dropped line makes them unknown.
    void     Clear   ();

    //  A device as the Apple's label has it: S5,U1, and its volume.
    static std::string  DescribeDevice (const CassoLinkBlockDevice & device);

private:
    //  A device's blocks over the link, as the session's thread reads them.
    class LinkStore : public IBlockStore
    {
    public:
        LinkStore (ICassoLinkBlockIo & io, const CassoLinkBlockDevice & device) : m_io (io), m_device (device) {}

        uint32_t  GetBlockCount () const override { return m_device.blocks; }
        HRESULT   ReadBlocks    (uint32_t first, uint32_t count, std::span<Byte> outBytes) override;
        HRESULT   WriteBlocks   (uint32_t first, std::span<const Byte> bytes) override;
        HRESULT   Commit        () override { return S_OK; }

        //  The device's error in words, for the last failure; empty with none.
        const std::string &  GetError () const { return m_error; }

    private:
        ICassoLinkBlockIo           & m_io;
        CassoLinkBlockDevice          m_device;
        std::string                   m_error;
    };

    struct Unit
    {
        CassoLinkBlockDevice                 device;
        std::unique_ptr<LinkStore>           store;
        std::unique_ptr<CachingBlockStore>   cache;
        std::unique_ptr<BlockImage>          image;
        std::unique_ptr<IVolume>             volume;
    };

    using ChangeFn = std::function<HRESULT (const IVolume & volume, BlockBuffer & buffer)>;

    HRESULT  EnsureModule   ();
    HRESULT  FindUnit       (int drive, Unit *& outUnit);
    HRESULT  OpenVolume     (Unit & unit, bool isFresh, VolumeKind & outKind);
    HRESULT  RunChange      (Unit & unit, const ChangeFn & change);
    HRESULT  Transfer       (LinkOperation & operation, Unit & unit, bool isRead);
    void     ReportTransfer (LinkOperation & operation, const Unit & unit, bool isRead, uint32_t done, uint32_t total);
    HRESULT  SendLabel      (const std::string & text);

    CassoLinkSession                     & m_session;
    std::vector<Byte>                      m_module;
    mutable std::mutex                     m_lock;
    std::vector<CassoLinkBlockDevice>      m_devices;      // any thread, under m_lock
    std::vector<std::unique_ptr<Unit>>     m_units;        // the session's thread alone
};
