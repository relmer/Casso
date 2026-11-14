#pragma once

#include "Pch.h"

#include "Devices/Floppy35/IDiskDriveBus.h"

class SmartPortBus;





////////////////////////////////////////////////////////////////////////////////
//
//  BitLine
//
//  One direction of the SmartPort bus's data: the transitions one IWM writes,
//  as times, for another IWM to read a cell at a time. A 1 written in a cell
//  is a transition at the cell's end, the time the writer's IWM has the bit,
//  so a reader never reads past a transition the writer hasn't made yet. A
//  cell read is 1 when a transition falls in it. Transitions older than the
//  cell read are dropped, since nothing was reading the line when they went
//  by.
//
////////////////////////////////////////////////////////////////////////////////

class BitLine
{
public:
    void  Write (Byte bit, uint64_t cellEndNs, uint32_t cellNs);
    Byte  Read  (uint64_t cellEndNs, uint32_t cellNs);
    void  Clear () { m_events.clear(); }

private:
    static constexpr size_t  kMaxEvents = 16384;

    std::deque<uint64_t>  m_events;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ISmartPortDevice
//
//  A device on a SmartPort chain, in the order the chain lists it. The bus
//  brings each device up to the host's time before the host's IWM accesses
//  the bus, so a device runs with each line change at the time the host
//  made it.
//
////////////////////////////////////////////////////////////////////////////////

class ISmartPortDevice
{
public:
    virtual ~ISmartPortDevice() = default;

    virtual void       Connect      (SmartPortBus * bus, size_t position) = 0;
    virtual void       CatchUp      (uint64_t nowNs) = 0;
    virtual void       SetBusReset  (bool isReset) = 0;

    //  Whether the device's outputs, /ACK and its data, are on the bus.
    virtual bool       IsDrivingBus () const = 0;
    virtual bool       GetAck       () const = 0;

    //  Whether the bus enable passes on to the next device.
    virtual bool       PassesEnable () const = 0;

    virtual BitLine  & GetInput     () = 0;
    virtual BitLine  & GetOutput    () = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  SmartPortBus
//
//  The SmartPort bus as the host's IWM drives it. Phase 0 is REQ; phases 1
//  and 3 together enable the bus; phases 0 and 2 without 1 and 3 reset it.
//  The sense line is /ACK, high when no device drives it. Data goes both
//  ways at 4 microsecond cells, the host's writes to every device and the
//  driving device's writes to the host.
//
//  A device's bus is enabled when phases 1 and 3 are on and every device
//  before it passes the enable on, which is how a chain's devices take their
//  unit numbers one at a time.
//
////////////////////////////////////////////////////////////////////////////////

class SmartPortBus : public IDiskDriveBus
{
public:
    static constexpr uint32_t  kCellNs = 4000;

    //  The devices, in chain order. The caller deletes them.
    void  AddDevice (ISmartPortDevice * device);

    //  Runs each device up to the host's time `nowNs`.
    void  CatchUp (uint64_t nowNs);

    size_t  GetDeviceCount () const { return m_devices.size(); }

    // IDiskDriveBus, the host's side.
    void      SetPhases (bool ph0, bool ph1, bool ph2, bool ph3) override;
    bool      GetSense  () const override;
    Byte      ReadCell  (uint64_t cellEndNs) override;
    void      WriteCell (Byte bit, uint64_t cellEndNs) override;
    void      Advance   (uint64_t elapsedNs) override { (void) elapsedNs; }
    uint32_t  GetCellNs () const override { return kCellNs; }

    // The devices' side.
    bool  GetReq      () const { return m_phases[0]; }
    bool  IsReset     () const { return m_reset; }
    bool  IsEnabledAt (size_t position) const;
    bool  IsLastAt    (size_t position) const { return position + 1 == m_devices.size(); }

private:
    std::vector<ISmartPortDevice *>  m_devices;
    std::array<bool, 4>              m_phases = {};
    bool                             m_reset  = false;
};
