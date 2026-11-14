#pragma once

#include "Pch.h"

class IScsiTarget;





////////////////////////////////////////////////////////////////////////////////
//
//  ScsiPhase
//
////////////////////////////////////////////////////////////////////////////////

enum class ScsiPhase : Byte
{
    BusFree,
    Selection,
    MessageOut,
    Command,
    DataIn,
    DataOut,
    Status,
    MessageIn,
};





////////////////////////////////////////////////////////////////////////////////
//
//  ScsiInitiatorLines
//
//  The lines the initiator drives. `driveData` puts `data` on the bus.
//
////////////////////////////////////////////////////////////////////////////////

struct ScsiInitiatorLines
{
    Byte  data      = 0;
    bool  driveData = false;
    bool  bsy       = false;
    bool  sel       = false;
    bool  atn       = false;
    bool  ack       = false;
    bool  rst       = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ScsiBus
//
//  A SCSI-1 bus with one initiator and up to seven targets. The initiator
//  sets its lines; the bus runs the selected target through the phases
//  BusFree, Selection, MessageOut, Command, DataIn or DataOut, Status,
//  MessageIn and back to BusFree, with the REQ/ACK handshake for each byte.
//  A selection of an ID with no selectable target is not answered, and the
//  initiator's own code gives up on it.
//
//  A target's delay holds REQ off until the clock reaches it, so a drive's
//  spin-up and seek time are visible to the initiator as a wait for REQ.
//
////////////////////////////////////////////////////////////////////////////////

class ScsiBus
{
public:
    static constexpr int  kMaxIds = 8;

    void  AttachTarget  (IScsiTarget * target);
    void  DetachTargets ();
    void  SetClock      (const uint64_t * clock) { m_clock = clock; }

    void  SetInitiatorLines (const ScsiInitiatorLines & lines);

    //  The data lines: the initiator's byte when it drives them, ORed with
    //  the target's in an input phase.
    Byte       GetData  () const;
    bool       IsBsy    () const { return m_initiator.bsy || m_targetBsy; }
    bool       IsSel    () const { return m_initiator.sel; }
    bool       IsAtn    () const { return m_initiator.atn; }
    bool       IsAck    () const { return m_initiator.ack; }
    bool       IsRst    () const { return m_initiator.rst; }
    bool       IsReq    () const;
    bool       IsMsg    () const { return m_msg; }
    bool       IsCd     () const { return m_cd; }
    bool       IsIo     () const { return m_io; }
    ScsiPhase  GetPhase () const { return m_phase; }

    //  The command bytes of the last command, for a log.
    const std::vector<Byte> & GetLastCommand () const { return m_lastCommand; }

private:
    static size_t  GetCommandLength (Byte opcode);

    uint64_t  GetNow           () const { return m_clock != nullptr ? *m_clock : 0; }
    void      Release          ();
    void      TrySelect        ();
    void      EnterPhase       (ScsiPhase phase, uint64_t delayCycles);
    void      OnAckAsserted    ();
    void      OnAckReleased    ();
    void      RunCommand       ();
    void      FinishDataOut    ();

    std::array<IScsiTarget *, kMaxIds>  m_targets      = {};
    const uint64_t                    * m_clock        = nullptr;
    ScsiInitiatorLines                  m_initiator;
    IScsiTarget                       * m_selected     = nullptr;
    ScsiPhase                           m_phase        = ScsiPhase::BusFree;
    bool                                m_targetBsy    = false;
    bool                                m_req          = false;
    uint64_t                            m_reqAtCycle   = 0;
    bool                                m_msg          = false;
    bool                                m_cd           = false;
    bool                                m_io           = false;
    Byte                                m_targetData   = 0;
    std::vector<Byte>                   m_buffer;
    size_t                              m_index        = 0;
    size_t                              m_expected     = 0;
    Byte                                m_status       = 0;
    std::vector<Byte>                   m_lastCommand;
};
