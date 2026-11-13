#include "Pch.h"

#include "CassoLink/FullSend/FullSendManifest.h"





static constexpr std::string_view  s_kHeader  = "sequence,file,drive,fingerprint,hash,time,unreadable,status,duplicateOf,verification,recovered,unreliable";
static constexpr std::string_view  s_kLegacyHeader = "sequence,file,drive,fingerprint,hash,time,unreadable,status,duplicateOf";
static constexpr std::string_view  s_kComment = "# Casso Full Send manifest. Do not edit, rename or move this file.\r\n"
                                                "# Casso uses it to recognize disks already received in this folder.\r\n"
                                                "# Changing it can make disks get skipped or read again.\r\n";





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendManifest::Parse
//
//  Comments come first. The header is required, so a file that only has this
//  file name ends the session rather than being replaced.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FullSendManifest::Parse (std::span<const Byte> bytes)
{
    constexpr std::string_view  kBom  = "\xEF\xBB\xBF";
    HRESULT                     hr    = S_OK;
    std::string                 text (bytes.begin(), bytes.end());
    std::string_view            view  = text;
    size_t                      at    = view.starts_with (kBom) ? kBom.size() : 0;
    size_t                      end   = 0;
    bool                        whole = false;
    std::vector<std::string>    cells;
    std::vector<FullSendRow>    rows;
    FullSendRow                 row;



    m_rows.clear();

    while (at < view.size() && view[at] == '#')
    {
        end = view.find ('\n', at);
        at  = end == std::string_view::npos ? view.size() : end + 1;
    }

    end   = view.find_first_of ("\r\n", at);
    whole = end != std::string_view::npos && (view.substr (at, end - at) == s_kHeader || view.substr (at, end - at) == s_kLegacyHeader);
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
    bool  valid = cells.size() == kColumns || cells.size() == kLegacyColumns;



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
             && (outRow.status == kStatusSent || outRow.status == kStatusDuplicate || outRow.status == kStatusProtected || outRow.status == kStatusRawCapture || outRow.status == kStatusDamaged || outRow.status == kStatusUnclear || outRow.status == kStatusLegacyProtected);
    }

    if (valid && cells.size() == kColumns)
    {
        outRow.verification = cells[9];

        valid = TryParseInt (cells[10], outRow.recovered) && TryParseInt (cells[11], outRow.unreliable);
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
    std::string  text = std::string (s_kComment) + std::string (s_kHeader) + "\r\n";



    for (const FullSendRow & row : m_rows)
    {
        text += std::format ("{},{},{},{},{},{},{},{},{},{},{},{}\r\n",
                             row.sequence, QuoteCell (row.file), row.drive, row.fingerprint, row.hash, row.time, row.unreadable, row.status, QuoteCell (row.duplicateOf),
                             row.verification, row.recovered, row.unreliable);
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
//  FullSendManifest::FindByFingerprint
//
////////////////////////////////////////////////////////////////////////////////

const FullSendRow * FullSendManifest::FindByFingerprint (const std::string & fingerprint) const
{
    auto  found = std::find_if (m_rows.rbegin(), m_rows.rend(), [&] (const FullSendRow & row) { return row.fingerprint == fingerprint; });



    return found == m_rows.rend() ? nullptr : &*found;
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





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendManifest::RenameImage
//
////////////////////////////////////////////////////////////////////////////////

bool FullSendManifest::RenameImage (const std::string & oldName, const std::string & newName)
{
    bool  changed = false;



    for (FullSendRow & row : m_rows)
    {
        if (!row.file.empty() && _stricmp (row.file.c_str(), oldName.c_str()) == 0)
        {
            row.file = newName;
            changed  = true;
        }

        if (!row.duplicateOf.empty() && _stricmp (row.duplicateOf.c_str(), oldName.c_str()) == 0)
        {
            row.duplicateOf = newName;
            changed         = true;
        }
    }

    return changed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendManifest::RemoveImage
//
//  The earliest row with a file that repeats the deleted image takes its
//  place: its duplicateOf becomes the deleted row's, and a duplicate takes
//  the deleted row's status, as the images are the same. Each other row that
//  repeats the deleted image then repeats that one. With no such row, the
//  rows that repeat it have no file, and are removed.
//
////////////////////////////////////////////////////////////////////////////////

FullSendRemoval FullSendManifest::RemoveImage (const std::string & name)
{
    FullSendRemoval           removal;
    std::vector<FullSendRow>  others;
    const FullSendRow       * deleted  = nullptr;
    const FullSendRow       * promoted = nullptr;
    std::string               original;



    for (const FullSendRow & row : m_rows)
    {
        bool  image   = !row.file.empty() && _stricmp (row.file.c_str(), name.c_str()) == 0;
        bool  repeats = !row.file.empty() && !image && _stricmp (row.duplicateOf.c_str(), name.c_str()) == 0;

        deleted  = (image && deleted == nullptr) ? &row : deleted;
        promoted = (repeats && (promoted == nullptr || row.sequence < promoted->sequence)) ? &row : promoted;
    }

    original = (promoted != nullptr) ? promoted->file : std::string();

    for (const FullSendRow & row : m_rows)
    {
        bool         image   = !row.file.empty() && _stricmp (row.file.c_str(), name.c_str()) == 0;
        bool         repeats = !image && !row.duplicateOf.empty() && _stricmp (row.duplicateOf.c_str(), name.c_str()) == 0;
        FullSendRow  changed = row;

        if (image || (repeats && promoted == nullptr))
        {
            removal.removed.push_back (row);
            continue;
        }

        if (repeats && &row == promoted)
        {
            changed.duplicateOf = (deleted != nullptr) ? deleted->duplicateOf : std::string();
            changed.status      = (row.status != kStatusDuplicate) ? row.status : (deleted != nullptr) ? deleted->status : std::string (kStatusSent);
        }
        else if (repeats)
        {
            changed.duplicateOf = original;
        }

        if (repeats)
        {
            removal.changed.push_back (row);
        }

        others.push_back (changed);
    }

    m_rows = std::move (others);

    return removal;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendManifest::RestoreRows
//
//  The rows a deletion changed are found by their sequence and hash, since
//  a rename after the deletion changes only a row's file.
//
////////////////////////////////////////////////////////////////////////////////

void FullSendManifest::RestoreRows (const FullSendRemoval & removal)
{
    for (const FullSendRow & before : removal.changed)
    {
        auto  found = std::find_if (m_rows.begin(), m_rows.end(), [&before] (const FullSendRow & row)
        {
            return row.sequence == before.sequence && row.hash == before.hash;
        });

        if (found != m_rows.end())
        {
            found->duplicateOf = before.duplicateOf;
            found->status      = before.status;
        }
    }

    for (const FullSendRow & row : removal.removed)
    {
        bool  listed = std::any_of (m_rows.begin(), m_rows.end(), [&row] (const FullSendRow & other)
        {
            return other.sequence == row.sequence && other.file == row.file && other.fingerprint == row.fingerprint;
        });

        if (!listed)
        {
            m_rows.push_back (row);
        }
    }

    std::stable_sort (m_rows.begin(), m_rows.end(), [] (const FullSendRow & a, const FullSendRow & b) { return a.sequence < b.sequence; });
}
