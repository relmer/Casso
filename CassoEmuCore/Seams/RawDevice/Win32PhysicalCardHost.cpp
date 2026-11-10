#include "Pch.h"

#include "Seams/RawDevice/Win32PhysicalCardHost.h"
#include "Seams/RawDevice/RawDeviceClient.h"
#include "Seams/RawDevice/Win32PipeStream.h"
#include "Seams/RawDevice/Win32RawDevice.h"
#include "Seams/RawDevice/Win32RawDeviceHelper.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ElevatedDriveStore
//
//  A drive open in the elevated process: the pipe to it, the client on the
//  pipe, and the process's handle.
//
////////////////////////////////////////////////////////////////////////////////

class ElevatedDriveStore : public IBlockStore
{
public:
    ElevatedDriveStore() = default;
    ~ElevatedDriveStore() override;

    ElevatedDriveStore (const ElevatedDriveStore &) = delete;
    ElevatedDriveStore & operator= (const ElevatedDriveStore &) = delete;

    HRESULT   Open          (int number, HWND owner, PhysicalDrive & outDrive);

    uint32_t  GetBlockCount() const override { return m_client.GetBlockCount(); }
    HRESULT   ReadBlocks    (uint32_t first, uint32_t count, std::span<Byte> outBytes) override { return m_client.ReadBlocks (first, count, outBytes); }
    HRESULT   WriteBlocks   (uint32_t first, std::span<const Byte> bytes) override              { return m_client.WriteBlocks (first, bytes); }
    HRESULT   Commit        () override;

private:
    Win32PipeStream  m_stream;
    RawDeviceClient  m_client  { m_stream };
    HANDLE           m_process = nullptr;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ElevatedDriveStore::~ElevatedDriveStore
//
////////////////////////////////////////////////////////////////////////////////

ElevatedDriveStore::~ElevatedDriveStore()
{
    m_stream.Close();

    if (m_process != nullptr)
    {
        CloseHandle (m_process);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ElevatedDriveStore::Open
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ElevatedDriveStore::Open (int number, HWND owner, PhysicalDrive & outDrive)
{
    HRESULT  hr = S_OK;



    hr = Win32RawDeviceHelper::Launch (owner, number, m_stream, m_process);
    CHR (hr);

    hr = m_client.Connect (outDrive);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ElevatedDriveStore::Commit
//
//  The blocks of a commit that fails are dropped, so that a later commit
//  sends only its blocks.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ElevatedDriveStore::Commit()
{
    HRESULT  hr = m_client.Commit();



    if (FAILED (hr))
    {
        m_client.Discard();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PhysicalCardHost::ListDrives
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PhysicalCardHost::ListDrives (std::vector<PhysicalDrive> & outDrives)
{
    Win32RawDevice  device;



    return device.ListDrives (outDrives);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PhysicalCardHost::OpenDrive
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PhysicalCardHost::OpenDrive (int number, HWND owner, PhysicalDrive & outDrive, std::unique_ptr<IBlockStore> & outStore)
{
    HRESULT                              hr    = S_OK;
    std::unique_ptr<ElevatedDriveStore>  store = std::make_unique<ElevatedDriveStore>();



    outStore.reset();

    hr = store->Open (number, owner, outDrive);
    CHR (hr);

    outStore = std::move (store);

Error:
    return hr;
}