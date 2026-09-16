#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  BinaryFormat / BinarySegment / BinaryImage
//
////////////////////////////////////////////////////////////////////////////////

enum class BinaryFormat
{
    Raw,
    Dos33Binary,
    IntelHex,
    SRecord,
    AppleSingle,
};

struct BinarySegment
{
    Word               address = 0;
    std::vector<Byte>  bytes;
};

struct BinaryImage
{
    BinaryFormat                format = BinaryFormat::Raw;
    std::vector<BinarySegment>  segments;
    std::optional<Word>         entry;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BinaryImageReader
//
//  A loadable image in any of the formats the assemblers write and cc65
//  produces. Intel HEX, S-records and AppleSingle are known from their
//  content; raw bytes and a DOS 3.3 binary cannot be told apart, so each is
//  chosen explicitly and raw is the default. A given address is where raw
//  bytes load, overrides a DOS 3.3 or AppleSingle file's own, and is refused
//  for a format whose records each carry their own.
//
////////////////////////////////////////////////////////////////////////////////

class BinaryImageReader
{
public:
    static HRESULT  Read (std::span<const Byte>         content,
                          std::optional<BinaryFormat>   explicitFormat,
                          std::optional<Word>           address,
                          BinaryImage                 & image,
                          std::string                 & error);

    static bool     TryGetFormatName (const std::string & word, BinaryFormat & format);
    static const char * GetFormatName (BinaryFormat format);

private:
    static constexpr size_t  kDosHeaderSize = 4;

    static BinaryFormat  Detect        (std::span<const Byte> content, std::optional<BinaryFormat> explicitFormat);
    static HRESULT  ReadRaw            (std::span<const Byte> content, std::optional<Word> address, BinaryImage & image, std::string & error);
    static HRESULT  ReadDos33          (std::span<const Byte> content, std::optional<Word> address, BinaryImage & image, std::string & error);
    static HRESULT  ReadAppleSingle    (std::span<const Byte> content, std::optional<Word> address, BinaryImage & image, std::string & error);
    static HRESULT  ReadIntelHex       (std::span<const Byte> content, std::optional<Word> address, BinaryImage & image, std::string & error);
    static HRESULT  ReadSRecord        (std::span<const Byte> content, std::optional<Word> address, BinaryImage & image, std::string & error);

    static bool     LooksLikeIntelHex  (std::span<const Byte> content);
    static bool     LooksLikeSRecord   (std::span<const Byte> content);
    static void     SplitLines         (std::span<const Byte> content, std::vector<std::string> & lines);
    static bool     TryReadHexBytes    (const std::string & text, std::vector<Byte> & bytes);
    static void     AddBytes           (BinaryImage & image, uint32_t address, std::span<const Byte> bytes);
    static std::string  FirstLine      (std::span<const Byte> content);
};
