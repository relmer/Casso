#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgram
//
//  A tokenized Applesoft or Integer BASIC program as a list of lines, and the
//  places in a line where a line number or a memory address is written.
//
//  THE DIALECT IS READ FROM THE BYTES. An Applesoft program is a chain of
//  lines, each a link to the next, a line number, the tokens and $00, ended by
//  a zero link. An Integer BASIC program is a run of lines, each a length
//  byte, a line number, the tokens and $01. A file whose links match its
//  line lengths is Applesoft. Otherwise a file whose length bytes add up to
//  its size is Integer BASIC. A file DOS relinked for another address is
//  accepted as Applesoft last, with the links ignored.
//
//  A LINE REFERENCE is a number after GOTO, GOSUB, THEN, LIST, RUN or DEL.
//  Integer BASIC accepts an expression after GOTO and GOSUB; only a constant
//  that is the whole operand is a reference.
//
////////////////////////////////////////////////////////////////////////////////

class BasicProgram
{
public:
    enum class Dialect { Applesoft, Integer };

    struct Line
    {
        uint32_t           number = 0;
        std::vector<Byte>  body;            // without the byte that ends the line
    };

    struct Program
    {
        Dialect            dialect = Dialect::Applesoft;
        Word               base    = 0;     // Applesoft: the address of the first line
        std::vector<Line>  lines;
        std::vector<Byte>  tail;            // Applesoft: bytes after the zero link
    };

    //  A line number written in a body. Applesoft: ASCII digits. Integer
    //  BASIC: the three bytes of a constant.
    struct LineReference
    {
        size_t    offset = 0;
        size_t    length = 0;
        uint32_t  target = 0;
    };

    //  CALL, PEEK or POKE with a constant address.
    struct AddressReference
    {
        const char  * word    = nullptr;
        int32_t       address = 0;
    };

    static HRESULT  Parse                 (const std::vector<Byte> & bytes, Program & outProgram, std::string & outReason);
    static HRESULT  Emit                  (const Program & program, std::vector<Byte> & outBytes, uint32_t & outLongLine);

    static void     FindLineReferences    (Dialect dialect, const std::vector<Byte> & body, std::vector<LineReference> & outReferences);
    static void     FindAddressReferences (Dialect dialect, const std::vector<Byte> & body, std::vector<AddressReference> & outReferences);
    static void     FindVariables         (Dialect dialect, const std::vector<Byte> & body, std::vector<std::string> & outNames);

    static constexpr Byte  kApplesoftDel   = 0x85;
    static constexpr Byte  kApplesoftCall  = 0x8C;
    static constexpr Byte  kApplesoftGoto  = 0xAB;
    static constexpr Byte  kApplesoftRun   = 0xAC;
    static constexpr Byte  kApplesoftGosub = 0xB0;
    static constexpr Byte  kApplesoftPoke  = 0xB9;
    static constexpr Byte  kApplesoftList  = 0xBC;
    static constexpr Byte  kApplesoftFn    = 0xC2;
    static constexpr Byte  kApplesoftThen  = 0xC4;
    static constexpr Byte  kApplesoftPlus  = 0xC8;
    static constexpr Byte  kApplesoftMinus = 0xC9;
    static constexpr Byte  kApplesoftPeek  = 0xE2;

    static constexpr Byte  kIntegerColon      = 0x03;
    static constexpr Byte  kIntegerThenNumber = 0x24;
    static constexpr Byte  kIntegerPeek       = 0x2E;
    static constexpr Byte  kIntegerUnaryMinus = 0x36;
    static constexpr Byte  kIntegerCallOpen   = 0x3F;
    static constexpr Byte  kIntegerDollar     = 0x40;
    static constexpr Byte  kIntegerCall       = 0x4D;
    static constexpr Byte  kIntegerGosub      = 0x5C;
    static constexpr Byte  kIntegerGoto       = 0x5F;
    static constexpr Byte  kIntegerPoke       = 0x64;
    static constexpr Byte  kIntegerListNumber = 0x74;
    static constexpr Byte  kIntegerListComma  = 0x75;

    static constexpr size_t  kIntegerConstantBytes = 3;
    static constexpr size_t  kIntegerMaxLineBytes  = 255;

private:
    static bool    ParseApplesoft         (const std::vector<Byte> & bytes, bool checkLinks, Program & outProgram);
    static bool    ParseInteger           (const std::vector<Byte> & bytes, Program & outProgram);

    static void    FindApplesoftLines     (const std::vector<Byte> & body, std::vector<LineReference> & outReferences);
    static void    FindIntegerLines       (const std::vector<Byte> & body, std::vector<LineReference> & outReferences);
    static void    FindApplesoftAddresses (const std::vector<Byte> & body, std::vector<AddressReference> & outReferences);
    static void    FindIntegerAddresses   (const std::vector<Byte> & body, std::vector<AddressReference> & outReferences);
    static void    FindApplesoftVariables (const std::vector<Byte> & body, std::vector<std::string> & outNames);
    static void    FindIntegerVariables   (const std::vector<Byte> & body, std::vector<std::string> & outNames);

    static size_t  SkipApplesoftLiteral   (const std::vector<Byte> & body, size_t at);
    static size_t  SkipApplesoftNumber    (const std::vector<Byte> & body, size_t at);
    static size_t  SkipIntegerLiteral     (const std::vector<Byte> & body, size_t at);
    static bool    ReadApplesoftNumber    (const std::vector<Byte> & body, size_t & ioAt, int32_t & outValue);
    static bool    ReadIntegerNumber      (const std::vector<Byte> & body, size_t & ioAt, int32_t & outValue);
    static bool    IsDigit                (Byte b)      { return b >= '0' && b <= '9'; }
    static bool    IsLetter               (Byte b)      { return b >= 'A' && b <= 'Z'; }
    static bool    IsHighDigit            (Byte b)      { return b >= 0xB0 && b <= 0xB9; }
};
