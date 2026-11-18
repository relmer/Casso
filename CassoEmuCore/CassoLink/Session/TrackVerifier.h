#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ITrackVerifierIo
//
//  A drive's track, as a verified read uses it. Sectors are physical. A sector
//  that doesn't read has its entry false in outRead.
//
////////////////////////////////////////////////////////////////////////////////

class ITrackVerifierIo
{
public:
    virtual ~ITrackVerifierIo () = default;

    //  outData has 256 bytes per sector, in the order of `sectors`.
    virtual HRESULT  ReadData (std::span<const Byte> sectors, std::vector<Byte> & outData, std::vector<bool> & outRead) = 0;

    //  Each sector's CRC, read and computed on the Apple.
    virtual HRESULT  ReadCrcs (std::span<const Byte> sectors, std::vector<Word> & outCrcs, std::vector<bool> & outRead) = 0;

    //  The head away from the track and back, so that a retry doesn't repeat a
    //  read from one position.
    virtual HRESULT  MoveHead () = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  TrackVerifier
//
//  A track read three times and compared per sector. Reads that disagree are
//  repeated up to nine times, and a strict majority is kept. A sector with read
//  errors is accepted once two good reads match; failed reads don't vote. Only
//  the first read carries data: later ones carry CRCs, and a sector's data is
//  read again only when the first read's isn't the data kept. Pure: the Apple is
//  injected.
//
////////////////////////////////////////////////////////////////////////////////

class TrackVerifier
{
public:
    enum class Outcome
    {
        Clean,          // every read matched
        Recovered,      // a majority, or two matching reads among failures
        Unreliable,     // one good read, or no majority
        Unreadable,     // no good read; zeros
    };

    struct Result
    {
        std::array<Outcome, 16>  outcomes  = {};
        std::vector<Byte>        data;   // 16 sectors of 256 bytes, physical order
        bool                     noSectors = false;   // nothing read at all, and retries were stopped
    };

    static constexpr int  kSectors   = 16;
    static constexpr int  kBaseReads = 3;
    static constexpr int  kMaxReads  = 9;

    //  With no sector read at all after this many tries, the track is given up,
    //  which is the cost of a copy-protected disk's nonstandard track.
    static constexpr int  kEmptyTrackReads = 2;

    static HRESULT  Verify (ITrackVerifierIo & io, Result & outResult);

private:
    struct Variant
    {
        Word               crc   = 0;
        int                count = 0;
        std::vector<Byte>  data;
    };

    struct SectorState
    {
        std::vector<Variant>  variants;
        int                   tries = 0;
        int                   good  = 0;
    };

    static void      Note      (SectorState & state, Word crc, std::span<const Byte> data);
    static bool      IsSettled (const SectorState & state);
    static Variant * GetTop    (SectorState & state);
    static Outcome   Judge     (SectorState & state);
    static HRESULT   FetchData (ITrackVerifierIo & io, Byte sector, SectorState & state, Variant & top);
};
