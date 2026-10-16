#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  UndoText
//
//  The pieces of the text a debug project saves an undo history as: fields
//  apart at one character, hex numbers, and runs of bytes as hex digits two
//  to a byte.
//
////////////////////////////////////////////////////////////////////////////////

class UndoText
{
public:
    static std::vector<std::string>  Split       (const std::string & text, char separator);
    static bool                      TryParseHex (const std::string & text, uint32_t & value);
    static std::string               FormatBytes (std::span<const Byte> bytes);
    static bool                      TryParseBytes (const std::string & text, std::vector<Byte> & bytes);
};
