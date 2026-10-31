#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendRow
//
//  One disk in a full send's manifest. A duplicate has an empty file, and
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
};





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendManifest
//
//  full-send.csv in a full send's folder, in UTF-8 with one row per disk:
//  parsed once when a session starts, and formatted again after each disk.
//  Pure: no I/O, no Win32.
//
////////////////////////////////////////////////////////////////////////////////

class FullSendManifest
{
public:
    static constexpr const char *  kFileName        = "full-send.csv";
    static constexpr const char *  kStatusSent      = "sent";
    static constexpr const char *  kStatusDuplicate = "duplicate";
    static constexpr const char *  kStatusProtected = "possibly-protected";

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

private:
    static constexpr size_t  kColumns   = 9;
    static constexpr size_t  kHashChars = 64;

    static bool         TrySplitRow (std::string_view text, size_t & ioAt, std::vector<std::string> & outCells);
    static bool         TryParseInt (const std::string & text, int & outValue);
    static bool         IsHexHash   (const std::string & text);
    static bool         TryParseRow (const std::vector<std::string> & cells, FullSendRow & outRow);
    static std::string  QuoteCell   (const std::string & cell);

    std::vector<FullSendRow>  m_rows;
};
