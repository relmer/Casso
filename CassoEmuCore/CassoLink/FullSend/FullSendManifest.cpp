#include "Pch.h"

#include "CassoLink/FullSend/FullSendManifest.h"





static constexpr std::string_view  s_kHeader = "sequence,file,drive,fingerprint,hash,time,unreadable,status,duplicateOf";





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendManifest::Parse
//
//  The header is required, so a file that only has this file name ends the
//  session rather than being replaced.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendManifest::Parse (std::span<const Byte> bytes)
{
    constexpr std::string_view  kBom  = "\xEF\xBB\xBF";
    HRESULT                     hr    = S_OK;
    std::string                 text (bytes.begin(), bytes.end());
    std::string_view            view  = text;
    size_t                      at    = view.starts_with (kBom) ? kBom.size() : 0;
    size_t                      end   = view.find_first_of ("\r\n");
    bool                        whole = false;
    std::vector<std::string>    cells;
    std::vector<FullSendRow>    rows;
    FullSendRow                 row;



    m_rows.clear();

    whole = end != std::string_view::npos && view.substr (at, end - at) == s_kHeader;
    CBREx (whole, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    whole = TrySplitRow (view, at, cells);
    CBREx (whole, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    while (at < view.size())
    {
        whole = TrySplitRow (view, at, cells);
        CBREx (whole, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        whole = TryParseRow (cells, row);
        CBREx (whole, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        rows.push_back (row);
    }

    m_rows = std::move (rows);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendManifest::TrySplitRow
//
//  One row's cells from ioAt, a row ending in LF or CRLF or at the end of the
//  text. False for a quote that is never closed or text directly after one.
//
////////////////////////////////////////////////////////////////////////////////

bool FullSendManifest::TrySplitRow (std::string_view text, size_t & ioAt, std::vector<std::string> & outCells)
{
    std::string  cell;
    bool         quoted = false;
    bool         closed = false;



    outCells.clear();

    while (ioAt < text.size())
    {
        char  ch = text[ioAt++];

        if (quoted)
        {
            if (ch != '"')
            {
                cell += ch;
            }
            else if (ioAt < text.size() && text[ioAt] == '"')
            {
                cell += '"';
                ioAt++;
            }
            else
            {
                quoted = false;
                closed = true;
            }

            continue;
        }

        if (ch == '\r' || ch == '\n')
        {
            ioAt += (ch == '\r' && ioAt < text.size() && text[ioAt] == '\n') ? 1 : 0;
            break;
        }

        if (ch == ',')
        {
            outCells.push_back (std::move (cell));
            cell.clear();
            closed = false;
        }
        else if (closed)
        {
            return false;
        }
        else if (ch == '"' && cell.empty())
        {
            quoted = true;
        }
        else
        {
            cell += ch;
        }
    }

    outCells.push_back (std::move (cell));

    return !quoted;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendManifest::TryParseInt
//
////////////////////////////////////////////////////////////////////////////////

bool FullSendManifest::TryParseInt (const std::string & text, int & outValue)
{
    const char             * end    = text.data() + text.size();
    std::from_chars_result   result = std::from_chars (text.data(), end, outValue);



    return !text.empty() && result.ec == std::errc() && result.ptr == end && outValue >= 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendManifest::IsHexHash
//
////////////////////////////////////////////////////////////////////////////////

bool FullSendManifest::IsHexHash (const std::string & text)
{
    return text.size() == kHashChars && std::all_of (text.begin(), text.end(), [] (char ch) { return isxdigit ((unsigned char) ch) != 0; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendManifest::TryParseRow
//
////////////////////////////////////////////////////////////////////////////////

bool FullSendManifest::TryParseRow (const std::vector<std::string> & cells, FullSendRow & outRow)
{
    bool  valid = cells.size() == kColumns;



    outRow = {};

    if (valid)
    {
        outRow.file        = cells[1];
        outRow.fingerprint = cells[3];
        outRow.hash        = cells[4];
        outRow.time        = cells[5];
        outRow.status      = cells[7];
        outRow.duplicateOf = cells[8];

        valid = TryParseInt (cells[0], outRow.sequence)
             && TryParseInt (cells[2], outRow.drive)
             && TryParseInt (cells[6], outRow.unreadable)
             && (outRow.drive == 1 || outRow.drive == 2)
             && IsHexHash (outRow.fingerprint)
             && IsHexHash (outRow.hash)
             && (outRow.status == kStatusSent || outRow.status == kStatusDuplicate || outRow.status == kStatusProtected);
    }

    return valid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendManifest::QuoteCell
//
////////////////////////////////////////////////////////////////////////////////

std::string FullSendManifest::QuoteCell (const std::string & cell)
{
    std::string  text = "\"";



    if (cell.find_first_of (",\"\r\n") == std::string::npos)
    {
        return cell;
    }

    for (char ch : cell)
    {
        text += ch;

        if (ch == '"')
        {
            text += '"';
        }
    }

    return text + "\"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendManifest::Format
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> FullSendManifest::Format() const
{
    std::string  text = std::string (s_kHeader) + "\r\n";



    for (const FullSendRow & row : m_rows)
    {
        text += std::format ("{},{},{},{},{},{},{},{},{}\r\n",
                             row.sequence, QuoteCell (row.file), row.drive, row.fingerprint, row.hash, row.time, row.unreadable, row.status, QuoteCell (row.duplicateOf));
    }

    return std::vector<Byte> (text.begin(), text.end());
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendManifest::GetNextSequence
//
////////////////////////////////////////////////////////////////////////////////

int FullSendManifest::GetNextSequence() const
{
    int  highest = 0;



    for (const FullSendRow & row : m_rows)
    {
        highest = std::max (highest, row.sequence);
    }

    return highest + 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendManifest::HasFingerprint
//
////////////////////////////////////////////////////////////////////////////////

bool FullSendManifest::HasFingerprint (const std::string & fingerprint) const
{
    return std::any_of (m_rows.begin(), m_rows.end(), [&] (const FullSendRow & row) { return row.fingerprint == fingerprint; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendManifest::FindImageByHash
//
////////////////////////////////////////////////////////////////////////////////

const FullSendRow * FullSendManifest::FindImageByHash (const std::string & hash) const
{
    auto  found = std::find_if (m_rows.begin(), m_rows.end(), [&] (const FullSendRow & row) { return row.hash == hash && !row.file.empty(); });



    return found == m_rows.end() ? nullptr : &*found;
}
