#pragma once

#include "Pch.h"

#include "CassoExplorer/Actions/ActionsTypes.h"

class CassoExplorerActions;





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsGoTo
//
//  The preview's Go to and Find boxes: an offset, an address or a line
//  parsed from what is typed, a search's bytes, and finding them in a
//  file or a listing.
//
////////////////////////////////////////////////////////////////////////////////

class ActionsGoTo
{
public:
    explicit ActionsGoTo (CassoExplorerActions & actions);

    //  Up to three whole numbers separated by spaces or commas, as typed into
    //  the raw verbs' prompts. Fewer than `required` fails; a missing count
    //  is one.
    static bool  TryParseNumbers (const std::wstring & text, size_t required, std::vector<int> & outNumbers);

    //  An address as typed: $2000, 0x2000 or 2000 are hexadecimal, as the
    //  Apple II writes addresses; a leading # marks decimal.
    static bool  TryParseAddress (const std::wstring & text, Word & outAddress);

    //  The same forms over the whole 64-bit range, for an offset into a file.
    static bool  TryParseOffset (const std::wstring & text, uint64_t & outValue);

    //  A Go to as typed: an address in TryParseAddress's forms, or one of
    //  them after + or -, which moves that far from `caretAddress`. A range
    //  follows with a hyphen and its last address ($0803-$0810), or with a
    //  comma and a length ($0803,+10). Without a range the first and last
    //  addresses are the same.
    static bool  TryParseGoTo (const std::wstring & text, int64_t caretAddress, int64_t & outFirst, int64_t & outLast);

    //  A search as typed: an even run of hex digits, with spaces anywhere, is
    //  bytes; text in double quotes, or anything else, is characters, and
    //  `outIsText` is set.
    static bool      TryParseSearch (const std::wstring & text, std::vector<Byte> & outBytes, bool & outIsText);
    static size_t    FindBytes      (const std::vector<Byte> & haystack, const std::vector<Byte> & pattern, bool isText, size_t start);
    static uint64_t  FindInSource   (const ActionsTypes::ReadFn & read, uint64_t count, const std::vector<Byte> & pattern, bool isText, uint64_t start);

    static bool  TryParseGoToTarget (const std::wstring & text, int64_t caretAddress, int64_t & outAddress);

private:
    friend class CassoExplorerActions;

    CassoExplorerActions  & m_actions;
};
