#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkPacketText
//
//  A packet of Casso's protocol as a line of plain English for the CassoLink
//  log: what the command does, then its fields by name, as "Read sectors:
//  drive 1, track 17, sectors 5, 9". Text the Apple draws is quoted, each of
//  its lines after a slash. An unknown command is its number and length.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkPacketText
{
public:
    static std::string  Describe     (Byte command, std::span<const Byte> payload);
    static std::string  GetKeyName   (Byte key);
    static std::string  GetErrorText (Byte error);

private:
    static std::string  GetSectors   (std::span<const Byte> payload);
    static std::string  GetQuietText (Byte drive);
    static std::string  GetText      (std::span<const Byte> text);
};
