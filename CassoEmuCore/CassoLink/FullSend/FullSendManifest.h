#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendRow
//
//  One disk in a Full Send's manifest. A duplicate has an empty file, and
//  duplicateOf is the earlier image's file name.
//
////////////////////////////////////////////////////////////////////////////////

struct FullSendRow
{
    int          sequence   = 0;
    std::string  file;
    int          drive      = 1;
    std::string  fingerprint;
    std::string  hash;
    std::string  time;
    int          unreadable = 0;
    std::string  status;
    std::string  duplicateOf;
    std::string  verification = "none";
    int          recovered  = 0;
    int          unreliable = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendManifest
//
//  manifest.fullsend in a Full Send's folder, CSV in UTF-8 with one row per
//  disk after # comments: parsed once when a session starts, and formatted again
//  after each disk.
//  Pure: no I/O, no Win32.
//
////////////////////////////////////////////////////////////////////////////////

class FullSendManifest
{
public:
    static constexpr const char *  kFileName        = "manifest.fullsend";
    static constexpr const char *  kLegacyFileName  = "full-send.csv";
    static constexpr const char *  kStatusSent      = "sent";
    static constexpr const char *  kStatusDuplicate = "duplicate";
    static constexpr const char *  kStatusProtected = "likely-copy-protected";
    static constexpr const char *  kStatusDamaged   = "likely-damaged";
    static constexpr const char *  kStatusUnclear   = "unclear";
    static constexpr const char *  kStatusLegacyProtected = "possibly-protected";
    static constexpr const char *  kVerifyNone      = "none";
    static constexpr const char *  kVerifyFull      = "verified";
    static constexpr const char *  kVerifyPartial   = "partly-verified";

    //  ERROR_INVALID_DATA for anything but a whole manifest, with no rows then.
    HRESULT            Parse           (std::span<const Byte> bytes);
    std::vector<Byte>  Format          () const;
    void               Add             (const FullSendRow & row) { m_rows.push_back (row); }
    int                GetNextSequence () const;
    bool               HasFingerprint  (const std::string & fingerprint) const;

    const std::vector<FullSendRow> &  GetRows () const { return m_rows; }

    //  The row of the image with this hash, or nullptr. A duplicate's row has
    //  no image, so it is passed over.
    const FullSendRow *  FindImageByHash (const std::string & hash) const;

    //  The latest row with this fingerprint, or nullptr.
    const FullSendRow *  FindByFingerprint (const std::string & fingerprint) const;

private:
    static constexpr size_t  kColumns       = 12;
    static constexpr size_t  kLegacyColumns = 9;
    static constexpr size_t  kHashChars     = 64;

    static bool         TrySplitRow (std::string_view text, size_t & ioAt, std::vector<std::string> & outCells);
    static bool         TryParseInt (const std::string & text, int & outValue);
    static bool         IsHexHash   (const std::string & text);
    static bool         TryParseRow (const std::vector<std::string> & cells, FullSendRow & outRow);
    static std::string  QuoteCell   (const std::string & cell);

    std::vector<FullSendRow>  m_rows;
};
