#include "Pch.h"

#include "Seams/RawDevice/Win32PhysicalCardHost.h"
#include "Seams/RawDevice/RawDeviceClient.h"
#include "Seams/RawDevice/Win32PipeStream.h"
#include "Seams/RawDevice/Win32RawDevice.h"
#include "Seams/RawDevice/Win32StorageCardProxy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProxyProcess
//
//  The running storage card proxy: the pipe to it, the connection on the
//  pipe, and the process's handle. Destroying it closes the pipe, and the
//  proxy then exits.
//
////////////////////////////////////////////////////////////////////////////////

class ProxyProcess
{
public:
    ProxyProcess() = default;
    ~ProxyProcess();

    ProxyProcess (const ProxyProcess &) = delete;
    ProxyProcess & operator= (const ProxyProcess &) = delete;

    HRESULT                Start          (HWND owner) { return Win32StorageCardProxy::Launch (owner, m_stream, m_process); }
    bool                   IsRunning      () const;
    RawDeviceConnection  & GetConnection  ()           { return m_connection; }

private:
    Win32PipeStream      m_stream;
    RawDeviceConnection  m_connection { m_stream };
    HANDLE               m_process    = nullptr;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ProxyDriveStore
//
//  One card open through the proxy. The blocks of a commit that fails are
//  dropped, so that a later commit sends only its blocks.
//
////////////////////////////////////////////////////////////////////////////////

class ProxyDriveStore : public IBlockStore
{
public:
    explicit ProxyDriveStore (std::shared_ptr<ProxyProcess> proxy) : m_proxy (std::move (proxy)), m_client (m_proxy->GetConnection()) {}

    HRESULT   Connect       (int number, PhysicalDrive & outDrive) { return m_client.Connect (number, outDrive); }

    uint32_t  GetBlockCount() const override                                                    { return m_client.GetBlockCount(); }
    HRESULT   ReadBlocks    (uint32_t first, uint32_t count, std::span<Byte> outBytes) override { return m_client.ReadBlocks (first, count, outBytes); }
    HRESULT   WriteBlocks   (uint32_t first, std::span<const Byte> bytes) override              { return m_client.WriteBlocks (first, bytes); }
    HRESULT   Commit        () override;

private:
    std::shared_ptr<ProxyProcess>  m_proxy;
    RawDeviceClient                m_client;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ProxyProcess::~ProxyProcess
//
////////////////////////////////////////////////////////////////////////////////

ProxyProcess::~ProxyProcess()
{
    m_stream.Close();

    if (m_process != nullptr)
    {
        CloseHandle (m_process);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProxyProcess::IsRunning
//
////////////////////////////////////////////////////////////////////////////////

bool ProxyProcess::IsRunning() const
{
    return m_process != nullptr && WaitForSingleObject (m_process, 0) == WAIT_TIMEOUT;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProxyDriveStore::Commit
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProxyDriveStore::Commit()
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
    HRESULT                           hr        = S_OK;
    std::unique_ptr<ProxyDriveStore>  store;



    outStore.reset();

    hr = StartProxy (owner);
    CHR (hr);

    store = std::make_unique<ProxyDriveStore> (m_proxy);

    hr = store->Connect (number, outDrive);
    CHR (hr);

    outStore = std::move (store);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PhysicalCardHost::HasUnmountedPartition
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PhysicalCardHost::HasUnmountedPartition (int number, bool & outHasUnmounted)
{
    return Win32RawDevice::HasUnmountedPartition (number, outHasUnmounted);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PhysicalCardHost::ListVolumes
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PhysicalCardHost::ListVolumes (int number, std::vector<WindowsVolume> & outVolumes)
{
    return Win32RawDevice::ListVolumes (number, outVolumes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PhysicalCardHost::InitializeDrive
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PhysicalCardHost::InitializeDrive (int number, HWND owner)
{
    HRESULT  hr = S_OK;



    hr = StartProxy (owner);
    CHR (hr);

    hr = m_proxy->GetConnection().Initialize (number);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PhysicalCardHost::StartProxy
//
//  The proxy is started when none is running, which raises the UAC prompt.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PhysicalCardHost::StartProxy (HWND owner)
{
    HRESULT                        hr        = S_OK;
    bool                           isRunning = m_proxy != nullptr && m_proxy->IsRunning();
    std::shared_ptr<ProxyProcess>  proxy;



    BAIL_OUT_IF (isRunning, S_OK);

    m_proxy.reset();

    proxy = std::make_shared<ProxyProcess>();

    hr = proxy->Start (owner);
    CHR (hr);

    m_proxy = std::move (proxy);

Error:
    return hr;
}