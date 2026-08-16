#pragma once

#include "AssemblerTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  OutputFormats
//
//  The shapes an assembled image can be written in. Every writer takes the
//  assembled bytes plus whatever addressing the format needs and emits to a
//  stream, so the caller owns the file and these stay testable against a
//  string stream.
//
//  Three binary shapes exist because they answer different questions:
//
//      Raw        the assembled span and nothing else. What a modern
//                 toolchain wants to hand to a linker, a disk tool, or a
//                 comparison.
//      DosBinary  the span behind a 4-byte load-address / length header --
//                 the on-disk layout of an Apple DOS 3.3 binary file, so it
//                 can be BLOADed once placed on a disk.
//      FlatImage  the full 64 KB address space, padded. File offset equals
//                 absolute address, which is what a ROM burner, an emulator
//                 memory loader, or a byte-for-byte reference comparison
//                 indexes by. This is the as65-compatible shape.
//
////////////////////////////////////////////////////////////////////////////////

class OutputFormats
{
public:
    static void WriteRaw       (const std::vector<Byte> & data, std::ostream & stream);
    static void WriteDosBinary (const std::vector<Byte> & data, Word startAddr, std::ostream & stream);
    static void WriteFlatImage (const std::vector<Byte> & data, Word startAddr, Byte fillByte, std::ostream & stream);
    static void WriteSRecord   (const std::vector<Byte> & data, Word startAddr, Word endAddr, Word entryPoint, std::ostream & stream);
    static void WriteIntelHex  (const std::vector<Byte> & data, Word startAddr, Word endAddr, Word entryPoint, std::ostream & stream);

    //  The largest span DosBinary's 16-bit length field can describe. A span
    //  of exactly 64 KB would write a length of zero, so the caller must
    //  reject one rather than emit a file that claims to be empty.
    static constexpr size_t  kMaxDosBinaryLength = 0xFFFF;
};
