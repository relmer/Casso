#pragma once

#include "AssemblerTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  OutputFormats
//
////////////////////////////////////////////////////////////////////////////////

class OutputFormats
{
public:
    static void WriteBinary   (const std::vector<Byte> & data, std::ostream & stream, Byte fillByte);
    static void WriteSRecord  (const std::vector<Byte> & data, Word startAddr, Word endAddr, Word entryPoint, std::ostream & stream);
    static void WriteIntelHex (const std::vector<Byte> & data, Word startAddr, Word endAddr, Word entryPoint, std::ostream & stream);
};
