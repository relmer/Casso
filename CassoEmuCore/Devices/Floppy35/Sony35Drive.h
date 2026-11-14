#pragma once

#include "Pch.h"

#include "Devices/BlockDevice/BlockDeviceMedia.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Floppy35/IDiskDriveBus.h"
#include "Devices/Floppy35/IFloppy35EventSink.h"

class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  Sony35Drive
//
//  The Sony 800K mechanism in Apple's 3.5-inch drives, from its controller's
//  it. The controller selects one of sixteen registers with CA1, CA0, SEL and
//  CA2 (bit 3 down to bit 0) and reads it on the sense line, 0 meaning yes:
//
//      $0 step direction (0 in)   $1, $3 head 0, head 1    $2 disk in place
//      $4 stepping                $6 write protected       $8 motor on
//      $9 single-sided            $A at track 0            $B ready
//      $D drive installed         $E tachometer            $F HD disk
//
//  A command is the register index strobed by LSTRB, CA2 its value: $0 and
//  $1 step direction in and out, $4 step, $8 and $9 motor on and off, $D
//  eject. SEL is also the head.
//
//  The disk is a block image of 800K or 400K, its tracks built as GCR bit
//  streams the first time the head is on them; a track written to is decoded
//  back to its blocks on Flush, which turning the motor off calls. A WOZ 2
//  3.5-inch image's tracks are its bit streams, TMAP entry track * 2 + side,
//  and Flush writes the changed tracks back into the WOZ file. The drive
//  moves one bit cell, 2 microseconds, a call to ReadCell or WriteCell while
//  the motor runs.
//
////////////////////////////////////////////////////////////////////////////////

class Sony35Drive : public IFloppy35EventSource, public IDiskDriveBus
{
public:
    static constexpr int       kSides  = 2;
    static constexpr uint32_t  kCellNs = 2000;

    explicit Sony35Drive (int driveIndex) : m_driveIndex (driveIndex) {}
    ~Sony35Drive() override;

    // IFloppy35EventSource
    void  SetFloppy35EventSink (IFloppy35EventSink * sink) override { m_sink = sink; }

    HRESULT  Insert (IDiskFileIo & fileIo, const std::string & path, const WriteProtectInfo & hostProtect, std::wstring & outError);
    HRESULT  Eject  ();
    HRESULT  Flush  ();

    bool  HasDisk          () const { return m_media.IsAttached() || m_woz != nullptr; }
    bool  IsWriteProtected () const { return (m_woz != nullptr) ? m_wozProtected : m_media.IsWriteProtected(); }
    bool  IsWoz            () const { return m_woz != nullptr; }

    // IDiskDriveBus: the phases are CA0, CA1, CA2 and LSTRB; time passing,
    // in nanoseconds, runs stepping, spin-up and eject; a cell is the bit
    // under the head, read or written.
    void      SetPhases (bool ca0, bool ca1, bool ca2, bool lstrb) override;
    bool      GetSense  () const override;
    Byte      ReadCell  () override;
    void      WriteCell (Byte bit) override;
    void      Advance   (uint64_t elapsedNs) override;
    uint32_t  GetCellNs () const override { return kCellNs; }

    void      SetSel    (bool sel);

    bool  IsMotorOn  () const { return m_motorOn; }
    int   GetTrack   () const { return m_track; }
    int   GetSide    () const { return m_sel ? 1 : 0; }

    BlockDeviceMedia  & GetMedia () { return m_media; }

private:
    static constexpr uint64_t  kStepNs     = 12'000'000;
    static constexpr uint64_t  kSpinUpNs   = 50'000'000;
    static constexpr uint64_t  kEjectNs    = 100'000'000;

    int                  GetRegister  () const;
    void                 RunCommand   (int command);
    std::vector<Byte>  & GetTrackBits ();
    void                 Emit         (Floppy35Op op);
    HRESULT              InsertWoz    (IDiskFileIo & fileIo, const std::string & path, const std::vector<Byte> & bytes, const WriteProtectInfo & hostProtect, std::wstring & outError);
    HRESULT              FlushBlocks  ();
    HRESULT              FlushWoz     ();

    int                                         m_driveIndex = 0;
    BlockDeviceMedia                            m_media;
    IFloppy35EventSink                        * m_sink       = nullptr;
    int                                         m_sides      = kSides;
    bool                                        m_ca0        = false;
    bool                                        m_ca1        = false;
    bool                                        m_ca2        = false;
    bool                                        m_lstrb      = false;
    bool                                        m_sel        = false;
    bool                                        m_stepOut    = false;
    bool                                        m_motorOn    = false;
    int                                         m_track      = 0;
    uint64_t                                    m_stepLeftNs = 0;
    uint64_t                                    m_spinLeftNs = 0;
    uint64_t                                    m_ejectLeftNs = 0;
    size_t                                      m_position   = 0;
    std::map<int, std::vector<Byte>>            m_tracks;
    std::set<int>                               m_dirty;
    std::unique_ptr<DiskImage>                  m_woz;
    IDiskFileIo                               * m_wozIo        = nullptr;
    std::string                                 m_wozPath;
    bool                                        m_wozProtected = false;
};
