#pragma once

#include "Pch.h"

#include "Controllers/ControlMapping.h"
#include "Controllers/GamePortInputMixer.h"
#include "Controllers/MappingEvaluator.h"
#include "Seams/IControllerBackend.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerInputService
//
//  What the controller thread does on every wake: read the selected
//  controller, turn its state into a game-port contribution, and submit that
//  to the mixer. It also answers how long the thread should wait for the next
//  wake, which is what keeps an idle Casso idle.
//
//  Nothing here touches a device or a machine directly: the backend reads
//  controllers and the mixer writes the machine, so every rule in this class
//  is exercised in tests with a scripted backend and a recording sink.
//
//  A read that fails is a disconnect, never a controller resting at center:
//  reporting rest for a controller that is gone would leave a game believing
//  the stick is centered and the buttons up, which is indistinguishable from
//  a healthy controller doing nothing.
//
////////////////////////////////////////////////////////////////////////////////

class ControllerInputService : public IControllerBackendEvents
{
public:

    // The measured XInput report interval on this machine (research R13).
    static constexpr DWORD  kPollPeriodMs = 8;

    struct Snapshot
    {
        std::vector<ControllerDeviceInfo>  devices;
        std::optional<ControllerUnitKey>   selection;
        ControllerSample                   lastSample;
        bool                               isSelectedConnected = false;
    };

    ControllerInputService (IControllerBackend & backend, GamePortInputMixer & mixer);

    void  OnDevicesChanged () override;

    void  SetActive        (bool isActive);
    void  SetSelection     (const std::optional<ControllerUnitKey> & selection);
    void  SetDeadzone      (float deadzone);

    // Reads the selected controller once and returns what the thread should
    // wait on before reading again.
    ControllerWaitSources  Tick ();

    Snapshot  GetSnapshot () const;

private:

    void  RefreshDevices           ();
    void  ReleaseContribution      ();
    void  EnsureMappingForSelection ();
    bool  IsSelectedDevicePresent  () const;

    // Both assume m_mutex is already held.
    const ControllerDeviceInfo *  FindSelectedDeviceLocked      () const;
    void                          EnsureMappingForSelectionLocked ();

    IControllerBackend                 & m_backend;
    GamePortInputMixer                 & m_mixer;
    MappingEvaluator                     m_evaluator;
    ControlMapping                       m_mapping;
    std::vector<ControllerDeviceInfo>    m_devices;
    std::optional<ControllerUnitKey>     m_selection;
    ControllerSample                     m_lastSample;
    float                                m_deadzone            = 0.0f;
    bool                                 m_isActive            = true;
    bool                                 m_isSelectedConnected = false;
    bool                                 m_hasContribution     = false;
    std::atomic<bool>                    m_devicesDirty        {true};
    mutable std::mutex                   m_mutex;
};
