#include "Pch.h"

#include "Shell/ScratchMachineRenderer.h"
#include "Core/Prng.h"
#include "Core/StateReader.h"
#include "Shell/HeadlessMachineFactory.h"
#include "Shell/MachineHost.h"
#include "Video/MachineFrameRenderer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchMachineRenderer::ScratchMachineRenderer
//
////////////////////////////////////////////////////////////////////////////////

ScratchMachineRenderer::ScratchMachineRenderer() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchMachineRenderer::~ScratchMachineRenderer
//
//  The builder holds the machine, so it goes first.
//
////////////////////////////////////////////////////////////////////////////////

ScratchMachineRenderer::~ScratchMachineRenderer()
{
    m_builder.reset();
    m_machine.reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchMachineRenderer::SetMachine
//
////////////////////////////////////////////////////////////////////////////////

void ScratchMachineRenderer::SetMachine (
    const MachineConfig  & config,
    const std::wstring   & name)
{
    std::lock_guard<std::mutex>  held (m_lock);



    m_config = config;
    m_name   = name;
    m_generation++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchMachineRenderer::Render
//
//  The text flashes in its on phase: a picture is one instant, and the
//  blink is not part of the state.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ScratchMachineRenderer::Render (
    const std::vector<Byte>  & state,
    std::vector<uint32_t>    & outBgra,
    int                      & outWidth,
    int                      & outHeight)
{
    HRESULT        hr         = S_OK;
    MachineConfig  config;
    std::wstring   name;
    uint64_t       generation = 0;
    bool           isBuilt    = false;
    StateReader    reader (state);



    outWidth  = 0;
    outHeight = 0;

    //  The scratch machine's disks belong to the thread drawing with it: a
    //  machine built here is already this thread's, and one built for an
    //  earlier picture was released when that picture was done.
    if (m_machine != nullptr)
    {
        m_machine->ClaimDiskOwnership();
    }

    {
        std::lock_guard<std::mutex>  held (m_lock);

        generation = m_generation;

        if (generation != m_builtGeneration)
        {
            config = m_config;
            name   = m_name;
        }
    }

    CBR (generation != 0);

    if (generation != m_builtGeneration)
    {
        hr = Build (config, name);
        CHR (hr);

        m_builtGeneration = generation;
    }

    isBuilt = m_machine != nullptr && m_builder != nullptr;
    CBRA (isBuilt);

    hr = m_machine->LoadStateForPicture (reader);
    CHR (hr);

    hr = MachineFrameRenderer::Render (*m_machine, *m_builder, true, outBgra);
    CHR (hr);

    outWidth  = MachineFrameRenderer::kWidth;
    outHeight = MachineFrameRenderer::kHeight;

Error:
    if (m_machine != nullptr)
    {
        m_machine->ReleaseDiskOwnership();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchMachineRenderer::Build
//
//  As the headless factory builds a machine: a Prng and a video timing model
//  before the devices. A machine that does not build is dropped, so the
//  next picture tries again rather than draw from half a machine.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ScratchMachineRenderer::Build (
    const MachineConfig  & config,
    const std::wstring   & name)
{
    HRESULT  hr = S_OK;



    m_builder.reset();
    m_machine.reset();

    m_machine = std::make_unique<MachineHost>();
    m_builder = std::make_unique<MachineBuilder> (*m_machine, m_services);

    m_machine->SetPrng               (std::make_unique<Prng> (HeadlessMachineFactory::kDefaultSeed));
    m_machine->SetVideoTiming        (std::make_unique<VideoTiming>());
    m_machine->SetCurrentMachineName (name);
    m_machine->GetConfig() = config;

    hr = m_builder->Build (config);
    CHR (hr);

Error:
    if (FAILED (hr))
    {
        m_builder.reset();
        m_machine.reset();
    }

    return hr;
}
