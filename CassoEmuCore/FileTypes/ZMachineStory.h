#pragma once

#include "Pch.h"

#include "FileTypes/FileFormatTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ZMachineStory
//
//  An Infocom story file, or any Z-machine story of versions 1 to 8, as
//  the Z-Machine Standards Document 1.1 specifies it: the header, the
//  abbreviations, the dictionary, the object tree with each object's
//  attributes and properties, and the strings.
//
//  The strings are the ones the code prints: the routines found from the
//  first instruction, from the routines each calls, and from the routine
//  addresses in the objects' properties are followed instruction by
//  instruction, and each print's text and each print_paddr's string is
//  listed. The strings after the last routine follow them.
//
//  A file matches when its header is a story's and its checksum is the one
//  the header has, or, for an early story with no length in its header,
//  when its name ends in a Z-machine extension.
//
////////////////////////////////////////////////////////////////////////////////

class ZMachineStory
{
public:
    static bool     Matches (const FileFacts & facts, std::span<const Byte> bytes);
    static HRESULT  Decode  (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

    struct Header
    {
        int       version        = 0;
        Byte      flags1         = 0;
        Word      release        = 0;
        Word      highMemory     = 0;
        Word      initialPc      = 0;
        Word      dictionary     = 0;
        Word      objects        = 0;
        Word      globals        = 0;
        Word      staticMemory   = 0;
        Word      flags2         = 0;
        char      serial[7]      = {};
        Word      abbreviations  = 0;
        uint32_t  length         = 0;     // in bytes; 0 when the header has no length
        Word      checksum       = 0;
        Word      routinesOffset = 0;
        Word      stringsOffset  = 0;
        Word      alphabet       = 0;
        Word      extension      = 0;
    };

    //  The header of a story, false for bytes that can't be one.
    static bool      ReadHeader      (std::span<const Byte> bytes, Header & outHeader);
    static Word      ComputeChecksum (std::span<const Byte> bytes, uint32_t length);

    //  Whether bytes are a story whose checksum matches its header's.
    static bool      IsVerifiedStory (std::span<const Byte> bytes);

    static constexpr size_t  kHeaderBytes = 64;

private:
    struct Story
    {
        std::span<const Byte>  bytes;
        Header                 header;
        wchar_t                alphabets[3][26] = {};
        std::vector<wchar_t>   unicode;
    };

    struct Operand
    {
        bool      isVariable = false;
        Word      value      = 0;
    };

    struct Instruction
    {
        size_t                address     = 0;
        size_t                length      = 0;
        std::string           name;
        std::vector<Operand>  operands;
        bool                  stores      = false;
        bool                  branches    = false;
        bool                  branchOn    = false;
        int                   branchTo    = 0;        // an offset; 0 and 1 return
        bool                  hasText     = false;
        std::wstring          text;
        bool                  ends        = false;    // execution doesn't go on to the next one
        bool                  isJump      = false;
        bool                  isCall      = false;
        bool                  printsPaddr = false;
    };

    struct Found
    {
        std::map<size_t, std::wstring>  strings;
        std::set<size_t>                routines;
        size_t                          codeEnd = 0;
    };

    static void          LoadAlphabets   (Story & story);
    static std::wstring  ReadString      (const Story & story, size_t address, size_t & outEnd, bool abbreviations);
    static void          AppendZscii     (const Story & story, int zscii, std::wstring & ioText);
    static size_t        UnpackRoutine   (const Story & story, Word packed);
    static size_t        UnpackString    (const Story & story, Word packed);
    static Word          GetWord         (const Story & story, size_t address);
    static Byte          GetByte         (const Story & story, size_t address);

    static void          ListHeader        (const Story & story, std::vector<std::wstring> & ioLines);
    static void          ListAbbreviations (const Story & story, std::vector<std::wstring> & ioLines);
    static void          ListDictionary    (const Story & story, std::vector<std::wstring> & ioLines);
    static size_t        ListObjects       (const Story & story, std::vector<std::wstring> & ioLines, std::vector<Word> & outPropertyWords);
    static void          ListTree          (const Story & story, size_t count, std::vector<std::wstring> & ioLines);
    static void          ListStrings       (const Story & story, const std::vector<Word> & propertyWords, std::vector<std::wstring> & ioLines);

    static size_t        GetObjectAddress  (const Story & story, size_t number);
    static size_t        GetPropertyTable  (const Story & story, size_t number);
    static int           GetObjectLink     (const Story & story, size_t number, int which);
    static std::wstring  GetObjectName     (const Story & story, size_t number);

    static bool          DecodeInstruction (const Story & story, size_t address, Instruction & outInstruction);
    static bool          NameOpcode        (const Story & story, int count, int number, bool extended, Instruction & ioInstruction);
    static void          FollowRoutine     (const Story & story, size_t routine, Found & ioFound, std::vector<size_t> & ioPending);
    static bool          IsRoutine         (const Story & story, size_t address);

    static std::wstring  Quote             (const std::wstring & text);

    //  The third alphabet from Z-character 6, the first place the escape.
    static constexpr const wchar_t *  s_kA2v1 = L" 0123456789.,!?_#'\"/\\<-:()";
    static constexpr const wchar_t *  s_kA2   = L" \n0123456789.,!?_#'\"/\\-:()";

    static constexpr size_t  s_kMaxObjects     = 65535;
    static constexpr size_t  s_kMaxStringChars = 8192;
    static constexpr size_t  s_kMaxRoutines    = 20000;
};
