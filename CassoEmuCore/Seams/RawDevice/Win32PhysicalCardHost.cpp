#include "Pch.h"

#include "Seams/RawDevice/Win32PhysicalCardHost.h"
#include "Seams/RawDevice/RawDeviceClient.h"
#include "Seams/RawDevice/Win32PipeStream.h"
#include "Seams/RawDevice/Win32RawDevice.h"
#include "Seams/RawDevice/Win32StorageCardBroker.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BrokerProcess
//
//  The running storage card broker: the pipe to it, the connection on the
//  pipe, and the process's handle. Destroying it closes the pipe, and the
//  broker then exits.
//
////////////////////////////////////////////////////////////////////////////////

class BrokerProcess
{
public:
    BrokerProcess() = default;
    ~BrokerProcess();

    BrokerProcess (const BrokerProcess &) = delete;
    BrokerProcess & operator= (const BrokerProcess &) = delete;

    HRESULT                Start          (HWND owner) { return Win32StorageCardBroker::Launch (owner, m_stream, m_process); }
    bool                   IsRunning      () const;
    RawDeviceConnection  & GetConnection  ()           { return m_connection; }

private:
    Win32PipeStream      m_stream;
    RawDeviceConnection  m_connection { m_stream };
    HANDLE               m_process    = nullptr;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BrokerDriveStore
//
//  One card open through the broker. The blocks of a commit that fails are
//  dropped, so that a later commit sends only its blocks.
//
////////////////////////////////////////////////////////////////////////////////

class BrokerDriveStore : public IBlockStore
{
public:
    explicit BrokerDriveStore (std::shared_ptr<BrokerProcess> broker) : m_broker (std::move (broker)), m_client (m_broker->GetConnection()) {}

    HRESULT   Connect       (int number, PhysicalDrive & outDrive) { return m_client.Connect (number, outDrive); }

    uint32_t  GetBlockCount() const override                                                    { return m_client.GetBlockCount(); }
    HRESULT   ReadBlocks    (uint32_t first, uint32_t count, std::span<Byte> outBytes) override { return m_client.ReadBlocks (first, count, outBytes); }
    HRESULT   WriteBlocks   (uint32_t first, std::span<const Byte> bytes) override              { return m_client.WriteBlocks (first, bytes); }
    HRESULT   Commit        () override;

private:
    std::shared_ptr<BrokerProcess>  m_broker;
    RawDeviceClient                 m_client;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BrokerProcess::~BrokerProcess
//
////////////////////////////////////////////////////////////////////////////////

BrokerProcess::~BrokerProcess()
{
    m_stream.Close();

    if (m_process != nullptr)
    {
        CloseHandle (m_process);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrokerProcess::IsRunning
//
////////////////////////////////////////////////////////////////////////////////

bool BrokerProcess::IsRunning() const
{
    return m_process != nullptr && WaitForSingleObject (m_process, 0) == WAIT_TIMEOUT;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrokerDriveStore::Commit
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BrokerDriveStore::Commit()
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
    HRESULT                            hr    = S_OK;
    std::unique_ptr<BrokerDriveStore>  store;



    outStore.reset();

    hr = StartBroker (owner);
    CHR (hr);

    store = std::make_unique<BrokerDriveStore> (m_broker);

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

HRESULT Win32PhysicalCardHost::InitializeDrive (int number, HWND owner, uint64_t layout)
{
    HRESULT  hr = S_OK;



    hr = StartBroker (owner);
    CHR (hr);

    hr = m_broker->GetConnection().Initialize (number, layout);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PhysicalCardHost::StartBroker
//
//  The broker is started when none is running, which raises the UAC prompt.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PhysicalCardHost::StartBroker (HWND owner)
{
    HRESULT                         hr        = S_OK;
    bool                            isRunning = m_broker != nullptr && m_broker->IsRunning();
    std::shared_ptr<BrokerProcess>  broker;



    BAIL_OUT_IF (isRunning, S_OK);

    m_broker.reset();

    broker = std::make_shared<BrokerProcess>();

    hr = broker->Start (owner);
    CHR (hr);

    m_broker = std::move (broker);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PhysicalCardHost::FindVolumeDrive
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PhysicalCardHost::FindVolumeDrive (const std::wstring & root, int & outNumber)
{
    return Win32RawDevice::FindVolumeDrive (root, outNumber);
}