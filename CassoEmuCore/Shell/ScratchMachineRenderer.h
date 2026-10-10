#pragma once

#include "Pch.h"

#include "Core/MachineConfig.h"
#include "Debugger/Reverse/IHistoryFrameRenderer.h"
#include "Shell/MachineBuilder.h"

class MachineHost;





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchMachineRenderer
//
//  Draws a snapshot's screen with the emulator's own video modes, on a
//  second machine of its own built from the running machine's configuration
//  with no disks, no audio and no printer: the snapshot is loaded into it
//  for its picture alone (MachineHost::LoadStateForPicture), and the running
//  machine is never touched.
//
//  SetMachine is called on the thread that builds the running machine, each
//  time it does; the scratch machine is built again on the worker before the
//  next picture. Render runs on one worker thread at a time; each picture
//  claims the scratch machine's disks for its thread and releases them when
//  it is done, so the next may be drawn on another thread.
//
////////////////////////////////////////////////////////////////////////////////

class ScratchMachineRenderer : public IHistoryFrameRenderer
{
public:
              ScratchMachineRenderer  ();
              ~ScratchMachineRenderer () override;

    void      SetMachine (const MachineConfig & config, const std::wstring & name);

    HRESULT   Render     (const std::vector<Byte>  & state,
                          std::vector<uint32_t>    & outBgra,
                          int                      & outWidth,
                          int                      & outHeight) override;

private:
    HRESULT   Build      (const MachineConfig & config, const std::wstring & name);

    //  What the next build is made from, and how many times it has changed.
    std::mutex                       m_lock;
    MachineConfig                    m_config;
    std::wstring                     m_name;
    uint64_t                         m_generation      = 0;

    //  The worker's own.
    MachineBuildServices             m_services;
    std::unique_ptr<MachineHost>     m_machine;
    std::unique_ptr<MachineBuilder>  m_builder;
    uint64_t                         m_builtGeneration = 0;
};
