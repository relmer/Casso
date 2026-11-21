#include "Pch.h"

#include "DiskFilePayload.h"
#include "AppleTextCodec.h"
#include "DiskCommandText.h"
#include "DiskHelpPage.h"
#include "IntegerBasicDetokenizer.h"
#include "IntegerBasicTokenizer.h"
#include "CassoExplorer/Model/HostAppleFile.h"
#include "CassoExplorer/Model/HostFileNaming.h"
#include "Core/TextEncoding.h"
#include "FileTypes/FileFormatTable.h"
#include "Machines/Apple2/Common/CpmVolume.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/PascalVolume.h"
#include "Machines/Apple2/Common/ProDosTypeNames.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"





//  Integer BASIC stops numbering lines at 32767, so the high bit of a line
//  number is clear and zero is not a line at all.
static constexpr Word    s_kMaxIntegerLineNumber = 32767;


//  How many links have to agree before a chain is a chain rather than a
//  coincidence. One proves nothing on a short file.
static constexpr size_t  s_kLinesThatProveAChain = 3;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFilePayload::ApplyEncoding
//
//  Turns the file's stored bytes into what the caller asked for. Verbatim is
//  the absence of a conversion and is the default, so a caller who says nothing
//  gets the bytes the disk holds.
//
//  AN ENCODING THIS BUILD CANNOT PERFORM IS REFUSED, NOT IGNORED. A flag that
//  is parsed and then silently dropped is worse than one that does not exist:
//  the caller reads "converted to a listing" in the help, gets tokenized bytes,
//  and has no way to tell the difference from a file that needed no conversion.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskFilePayload::ApplyEncoding (
    const CommandLineOptions &  options,
    FilePayload              &  payload,
    DiskCommandResult        &  result)
{
    HRESULT                   hr                   = S_OK;
    std::string               hostText;
    ApplesoftListingError     listingError;
    IntegerBasicListingError  integerError;
    char                      note[200]            = {};
    bool                      integer              = payload.type == Dos33Volume::kTypeInteger || payload.type == kProDosIntegerType;



    switch (options.disk.encoding)
    {
        case CommandLineOptions::DiskOptions::Encoding::Verbatim:
            break;

        case CommandLineOptions::DiskOptions::Encoding::Text:
            // Both conventions decode identically -- once the high bit is
            // ignored, Apple high-ASCII and plain-ASCII text differ in nothing -- so
            // this choice has no effect on the read path.
            if (payload.textForm == PayloadTextForm::PascalPages)
            {
                PascalVolume::DecodeText (payload.bytes, hostText);
            }
            else if (payload.textForm == PayloadTextForm::CpmRecords)
            {
                CpmVolume::DecodeText (payload.bytes, hostText);
            }
            else
            {
                AppleTextCodec::Decode (payload.bytes, AppleTextConvention::HighAscii, hostText);
            }

            payload.bytes.assign (hostText.begin(), hostText.end());
            payload.encoding = PayloadEncoding::HostText;
            break;

        case CommandLineOptions::DiskOptions::Encoding::Basic:
            if (payload.textForm != PayloadTextForm::AppleHighAscii)
            {
                result.diagnostics += DiskHelpPage::ApplyPrefixes ("%Lbasic means nothing on this volume: it has no BASIC programs\n", options.flagPrefix);
                result.exitStatus   = DiskCommandResult::kNoOutput;
                hr                  = HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED);
                break;
            }

            //  An Integer BASIC program lists through its own tokens, as its
            //  preview does.
            if (integer)
            {
                hr = IntegerBasicDetokenizer::Detokenize (payload.bytes, hostText, integerError);

                if (FAILED (hr))
                {
                    snprintf (note, sizeof (note), "byte %u: %s\n", (unsigned) integerError.offset, integerError.reason.c_str());

                    result.diagnostics += DiskHelpPage::ApplyPrefixes ("%Lbasic cannot read this file as an Integer BASIC program: ", options.flagPrefix) + note;
                    result.exitStatus   = DiskCommandResult::kNoOutput;
                    break;
                }

                payload.bytes.assign (hostText.begin(), hostText.end());
                payload.encoding = PayloadEncoding::HostText;
                break;
            }

            hr = ApplesoftTokenizer::Detokenize (payload.bytes, hostText, listingError);

            if (FAILED (hr))
            {
                result.diagnostics += DiskCommandText::DescribeListingRefusal (
                    DiskHelpPage::ApplyPrefixes ("%Lbasic cannot read this file as an Applesoft "
                                   "BASIC program", options.flagPrefix).c_str(), listingError);

                result.exitStatus   = DiskCommandResult::kNoOutput;
                break;
            }

            payload.bytes.assign (hostText.begin(), hostText.end());
            payload.encoding = PayloadEncoding::ApplesoftListing;
            break;

        default:
            result.diagnostics    += "Error: unknown encoding\n";
            result.exitStatus      = DiskCommandResult::kNoOutput;
            result.badCommandLine  = true;
            hr                  = E_INVALIDARG;
            break;
    }

    //  Text and a listing for the PC end each line with Windows' CR LF, as
    //  every conversion of Casso Explorer's does.
    if (SUCCEEDED (hr) && options.disk.encoding != CommandLineOptions::DiskOptions::Encoding::Verbatim)
    {
        ToWindowsLineEndings (payload.bytes);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFilePayload::ToWindowsLineEndings
//
//  Each line feed with no carriage return before it gets one. A CR LF is
//  left as it is.
//
////////////////////////////////////////////////////////////////////////////////

void DiskFilePayload::ToWindowsLineEndings (std::vector<Byte> & inOutBytes)
{
    std::vector<Byte>  converted;



    converted.reserve (inOutBytes.size() + inOutBytes.size() / 16);

    for (size_t i = 0; i < inOutBytes.size(); i++)
    {
        if (inOutBytes[i] == '\n' && (i == 0 || inOutBytes[i - 1] != '\r'))
        {
            converted.push_back ('\r');
        }

        converted.push_back (inOutBytes[i]);
    }

    inOutBytes = std::move (converted);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFilePayload::ApplyOutputEncoding
//
////////////////////////////////////////////////////////////////////////////////

void DiskFilePayload::ApplyOutputEncoding (CommandLineOptions & inOutOptions)
{
    size_t          lastSeparator = inOutOptions.disk.hostFile.find_last_of ("/\\");
    std::string     leaf          = (lastSeparator == std::string::npos) ? inOutOptions.disk.hostFile
                                                                         : inOutOptions.disk.hostFile.substr (lastSeparator + 1);
    ParsedHostName  parsed;



    if (inOutOptions.disk.encoding != CommandLineOptions::DiskOptions::Encoding::Verbatim || leaf.empty()
        || !HostFileNaming::Parse (TextEncoding::NarrowToWide (leaf), parsed))
    {
        return;
    }

    if (parsed.converted == ParsedHostName::ConvertedKind::ApplesoftListing || parsed.converted == ParsedHostName::ConvertedKind::IntegerListing)
    {
        inOutOptions.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Basic;
    }
    else if (parsed.converted == ParsedHostName::ConvertedKind::Text)
    {
        inOutOptions.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Text;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFilePayload::ApplyHostFileConversion
//
//  The conversion Casso Explorer writes for the file, when the output file's
//  name ends in that conversion's suffix and the command line has no --text
//  or --basic. Any other output name gets the file's bytes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskFilePayload::ApplyHostFileConversion (
    const CommandLineOptions  & options,
    VolumeKind                  kind,
    const std::string         & name,
    FilePayload               & inOutPayload,
    DiskCommandResult         & result)
{
    HRESULT        hr            = S_OK;
    size_t         lastSeparator = options.disk.hostFile.find_last_of ("/\\");
    std::wstring   leaf          = TextEncoding::NarrowToWide ((lastSeparator == std::string::npos) ? options.disk.hostFile
                                                                                                    : options.disk.hostFile.substr (lastSeparator + 1));
    size_t         lastSlash     = name.find_last_of ('/');
    FileFormat     bySuffix      = FileFormat::None;
    FileFormat     format        = FileFormat::None;
    bool           matches       = false;
    FileEntry      entry;
    FileFacts      facts;
    DecodedFile    decoded;
    vector<Byte>   converted;
    std::wstring   error;



    BAIL_OUT_IF (options.disk.encoding != CommandLineOptions::DiskOptions::Encoding::Verbatim, S_OK);

    bySuffix = FileFormatTable::GetBySuffix (leaf);
    BAIL_OUT_IF (bySuffix == FileFormat::None, S_OK);

    entry.name           = (lastSlash == std::string::npos) ? name : name.substr (lastSlash + 1);
    entry.type           = inOutPayload.type;
    entry.auxType        = inOutPayload.auxType;
    entry.hasAuxType     = inOutPayload.hasAuxType;
    entry.loadAddress    = inOutPayload.loadAddress;
    entry.hasLoadAddress = inOutPayload.hasLoadAddress;

    //  Several formats have one suffix, so the file's format is compared by
    //  its suffix.
    format  = FileFormatTable::Identify (entry, kind, inOutPayload.bytes);
    matches = format != FileFormat::None && _wcsicmp (FileFormatTable::GetSuffix (format), FileFormatTable::GetSuffix (bySuffix)) == 0;

    CBRFEx (matches, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA),
            result.FailAction (options.disk.imagePath, "convert " + name,
                               "it is not a file with a " + TextEncoding::WideToNarrow (FileFormatTable::GetSuffix (bySuffix)) + " conversion"));

    facts              = FileFacts::From (entry, kind);
    facts.resourceFork = inOutPayload.resourceFork;

    hr = FileFormatTable::Decode (format, inOutPayload.bytes, facts, DecodeOptions(), decoded, error);

    if (SUCCEEDED (hr))
    {
        hr = FileFormatTable::Export (decoded, converted);
    }

    CHRF (hr, result.FailAction (options.disk.imagePath, "convert " + name,
                                 error.empty() ? std::string ("it could not be decoded") : TextEncoding::WideToNarrow (error)));

    //  The conversion is a host file: it has no resource fork and no load
    //  address.
    inOutPayload.bytes           = converted;
    inOutPayload.hasResourceFork = false;
    inOutPayload.hasLoadAddress  = false;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFilePayload::GetAppleDoublePath
//
//  `._NAME` beside `NAME`, the name AppleDouble's convention gives the file of
//  a data fork's other entries.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskFilePayload::GetAppleDoublePath (const std::string & hostFile)
{
    std::filesystem::path  path = hostFile;



    return (path.parent_path() / ("._" + path.filename().string())).string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFilePayload::GetOnDiskName
//
//  --as when the caller gave one, and otherwise the last component of the
//  component. Nothing is stripped or shortened on the way: the caller already
//  chose that name, and inventing a different one would leave them looking for
//  a file the catalog does not hold. A name the filesystem cannot store is
//  refused by the volume layer, which is where that rule belongs.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskFilePayload::GetOnDiskName (const CommandLineOptions & options)
{
    size_t       lastSeparator = options.disk.hostFile.find_last_of ("/\\");
    std::string  name          = options.disk.path;



    if (name.empty())
    {
        name = (lastSeparator == std::string::npos)
                   ? options.disk.hostFile
                   : options.disk.hostFile.substr (lastSeparator + 1);
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFilePayload::DetectFileType
//
//  What a file's own bytes say it is, or 0 when they say nothing.
//
//  APPLESOFT IS A LINKED LIST AND THAT IS WHAT IS WALKED. Every line is a
//  two-byte pointer to the next line, a two-byte line number, tokens, and a
//  zero terminator; a zero pointer ends the program. Walking that chain with
//  the pointers rising and the line numbers rising, and landing EXACTLY on the
//  last byte of the file, is a structure arbitrary data does not have. The
//  exact landing is what makes a false positive vanishingly unlikely: a binary
//  whose first four bytes happen to look like a header still has to have every
//  later line agree, and end where the file ends.
//
//  INTEGER BASIC IS THE SAME SHAPE INSIDE OUT: a leading LENGTH byte rather
//  than a trailing zero. Walked the same way, to the same exact landing.
//
//  A DOS 3.3 FOUR-BYTE HEADER IS DELIBERATELY NOT READ AS A TYPE. `put --load`
//  writes that header itself, so a file already carrying one is the doubled-
//  header mistake the worked example warns about, and quietly agreeing with it
//  would file bytes the guest runs as code.
//
////////////////////////////////////////////////////////////////////////////////

Byte DiskFilePayload::DetectFileType (const vector<Byte> & bytes, VolumeKind kind)
{
    Byte    type     = 0;
    bool    isDos    = kind == VolumeKind::Dos33;
    size_t  at       = 0;
    Word    previous = 0;
    Word    lastLine = 0;
    size_t  seen     = 0;



    //  Applesoft: a chain of lines ending on a zero next-pointer.
    //  Two bytes, not four: the chain ENDS on a bare zero pointer, and a
    //  loop that demanded a whole line would walk past it and never see
    //  the end of the program.
    while (at + 2 <= bytes.size())
    {
        Word    next       = (Word) (bytes[at] | (bytes[at + 1] << 8));
        Word    line       = 0;
        size_t  terminator = at + 4;



        if (next == 0)
        {
            //  The chain ends here, and it has to end WITH the file.
            bool  landedOnTheEnd = (at + 2 == bytes.size());

            if (landedOnTheEnd && seen > 0)
            {
                type = isDos ? Dos33Volume::kTypeApplesoft : ProDosVolume::kTypeBasic;
            }

            break;
        }

        if (at + 4 > bytes.size())
        {
            break;
        }

        line = (Word) (bytes[at + 2] | (bytes[at + 3] << 8));

        //  Pointers rise, and so do line numbers.
        if (next <= previous || (seen > 0 && line <= lastLine))
        {
            break;
        }

        while (terminator < bytes.size() && bytes[terminator] != 0)
        {
            terminator++;
        }

        if (terminator >= bytes.size())
        {
            break;
        }

        previous = next;
        lastLine = line;
        seen++;
        at       = terminator + 1;
    }

    //  Integer BASIC: a chain of length-prefixed lines, and DOS 3.3's alone.
    //
    //  TIGHTER THAN THE APPLESOFT WALK BECAUSE IT HAS LESS TO GO ON. Applesoft
    //  carries a pointer per line that has to rise AND land on the last byte,
    //  and measured over 200,000 random blobs it never once fired. Integer has
    //  only a length byte, so a short file can chain to its own end by
    //  accident: the same measurement put it at 0.39% on 16-byte blobs.
    //
    //  Two free constraints close that. A line number stops at 32767 on this
    //  machine, so the high bit is clear and zero is not a line; and three
    //  links have to agree rather than one. Re-measured, 0.39% becomes 0.0025%,
    //  and what a rejected file falls back to is B, which is where it would
    //  have gone anyway.
    at       = 0;
    lastLine = 0;
    seen     = 0;

    while (type == 0 && isDos && at + 3 <= bytes.size())
    {
        Byte  length = bytes[at];
        Word  line   = (Word) (bytes[at + 1] | (bytes[at + 2] << 8));



        if (length < 4 || at + length > bytes.size())
        {
            break;
        }

        if (line == 0 || line > s_kMaxIntegerLineNumber)
        {
            break;
        }

        if (seen > 0 && line <= lastLine)
        {
            break;
        }

        lastLine = line;
        seen++;
        at      += length;

        if (at == bytes.size())
        {
            type = (seen >= s_kLinesThatProveAChain) ? Dos33Volume::kTypeInteger : (Byte) 0;
            break;
        }
    }

    return type;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFilePayload::ResolveFileType
//
//  The two filesystems number their types differently and name them
//  differently, so the forms accepted here are each one's own. A letter
//  that means Applesoft on DOS 3.3 and nothing on ProDOS must not resolve to
//  whatever ProDOS keeps at that number.
//
//  A caller who named no type gets the one that matches the conversion they
//  asked for -- text when they asked for text, a binary otherwise, which is
//  what a build loop places.
//
//  AN UNRECOGNIZED FORM IS REFUSED, not defaulted. Defaulting would place
//  the file under a type the caller did not ask for and say nothing, and the
//  guest would report the mismatch much later as a file that will not load.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskFilePayload::ResolveFileType (
    const CommandLineOptions  & options,
    VolumeKind                  kind,
    const vector<Byte>        & hostBytes,
    Byte                      & outType,
    DiskCommandResult         & result)
{
    HRESULT      hr         = S_OK;
    bool         isDos      = kind == VolumeKind::Dos33;
    bool         isText     = options.disk.encoding == CommandLineOptions::DiskOptions::Encoding::Text;
    bool         isBasic    = options.disk.encoding == CommandLineOptions::DiskOptions::Encoding::Basic;
    bool         named      = !options.disk.typeName.empty();
    bool         recognized = true;
    size_t       i          = 0;
    std::string  form       = options.disk.typeName;
    bool         isReadOnly = false;
    bool         isSystem   = false;



    if (kind == VolumeKind::Pascal)
    {
        hr = ResolvePascalKind (options, outType, result);
        BAIL_OUT_IF (true, hr);
    }

    if (kind == VolumeKind::Cpm)
    {
        outType = 0;

        //  The attributes are read from the same word in BuildPutPayload.
        recognized = !named || ParseCpmAttributes (options.disk.typeName, isReadOnly, isSystem);

        CBRFEx (recognized, E_NOTIMPL,
                (result.diagnostics += DiskHelpPage::ApplyPrefixes ("%Ltype means nothing on this volume: CP/M has no file types, only the attributes RO and SYS\n", options.flagPrefix),
                 result.exitStatus   = DiskCommandResult::kNoOutput));

        BAIL_OUT_IF (true, S_OK);
    }

    if (isBasic)
    {
        // A tokenized program stored under any other type is a file the guest
        // will not RUN, so the conversion picks the type rather than leaving it
        // to a default meant for a build loop's binaries.
        outType = isDos ? Dos33Volume::kTypeApplesoft : ProDosVolume::kTypeBasic;
    }
    else if (isText)
    {
        outType = isDos ? Dos33Volume::kTypeText : ProDosVolume::kTypeText;
    }
    else
    {
        //  What the file says it is, and a binary when it says nothing.
        Byte  detected = DetectFileType (hostBytes, kind);

        outType = (detected != 0)
                      ? detected
                      : (isDos ? Dos33Volume::kTypeBinary : ProDosVolume::kTypeBinary);
    }

    BAIL_OUT_IF (!named, S_OK);

    for (i = 0; i < form.size(); i++)
    {
        if (form[i] >= 'a' && form[i] <= 'z')
        {
            form[i] = (char) (form[i] - 'a' + 'A');
        }
    }

    if (isDos)
    {
        if      (form == "T" || form == "TXT") { outType = Dos33Volume::kTypeText; }
        else if (form == "I" || form == "INT") { outType = Dos33Volume::kTypeInteger; }
        else if (form == "A" || form == "BAS") { outType = Dos33Volume::kTypeApplesoft; }
        else if (form == "B" || form == "BIN") { outType = Dos33Volume::kTypeBinary; }
        else if (form == "R" || form == "REL") { outType = Dos33Volume::kTypeRelocatable; }
        else                                           { recognized = false; }
    }
    else
    {
        if      (form == "T" || form == "TXT") { outType = ProDosVolume::kTypeText; }
        else if (form == "B" || form == "BIN") { outType = ProDosVolume::kTypeBinary; }
        else if (form == "A" || form == "BAS") { outType = ProDosVolume::kTypeBasic; }
        else if (form == "I" || form == "INT") { outType = kProDosIntegerType; }
        else if (form == "S" || form == "SYS") { outType = ProDosVolume::kTypeSystem; }
        else                                   { recognized = ProDosTypeNames::TryParse (form, outType); }
    }

    if (!recognized)
    {
        result.diagnostics += DiskHelpPage::ApplyPrefixes ("%Ltype ", options.flagPrefix) + options.disk.typeName
                            + " means nothing on this volume: "
                            + (isDos ? "DOS 3.3 takes T, I, A, B or R"
                                     : "ProDOS takes a type's abbreviation, such as TXT, BIN, BAS, INT or SYS, or its number, such as $C0")
                            + "\n";

        result.exitStatus   = DiskCommandResult::kNoOutput;
        hr                  = E_NOTIMPL;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFilePayload::ParseCpmAttributes
//
//  RO, R/O or SYS, or two of them with a comma, from what STAT accepts.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskFilePayload::ParseCpmAttributes (const std::string & text, bool & outReadOnly, bool & outSystem)
{
    size_t  start = 0;



    outReadOnly = false;
    outSystem   = false;

    while (start <= text.size())
    {
        size_t       comma = text.find (',', start);
        std::string  word  = text.substr (start, (comma == std::string::npos) ? std::string::npos : comma - start);

        if (_stricmp (word.c_str(), "RO") == 0 || _stricmp (word.c_str(), "R/O") == 0)
        {
            outReadOnly = true;
        }
        else if (_stricmp (word.c_str(), "SYS") == 0)
        {
            outSystem = true;
        }
        else
        {
            return false;
        }

        if (comma == std::string::npos)
        {
            break;
        }

        start = comma + 1;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFilePayload::ResolvePascalKind
//
//  The kind --type selects. With no --type, text is TEXT, and anything else
//  takes its kind from the name in the volume, as Pascal's Filer does.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskFilePayload::ResolvePascalKind (const CommandLineOptions & options, Byte & outType, DiskCommandResult & result)
{
    static constexpr struct { const char * form; Byte kind; }  kForms[] =
    {
        { "BAD",  PascalVolume::kKindBad  },
        { "CODE", PascalVolume::kKindCode },
        { "TEXT", PascalVolume::kKindText },
        { "INFO", PascalVolume::kKindInfo },
        { "DATA", PascalVolume::kKindData },
        { "GRAF", PascalVolume::kKindGraf },
        { "FOTO", PascalVolume::kKindFoto },
    };
    HRESULT  hr         = S_OK;
    bool     recognized = options.disk.typeName.empty();



    outType = (options.disk.encoding == CommandLineOptions::DiskOptions::Encoding::Text) ? PascalVolume::kKindText : PascalVolume::kKindNone;

    for (const auto & row : kForms)
    {
        if (_stricmp (options.disk.typeName.c_str(), row.form) == 0)
        {
            outType    = row.kind;
            recognized = true;
        }
    }

    if (!recognized)
    {
        result.diagnostics += DiskHelpPage::ApplyPrefixes ("%Ltype ", options.flagPrefix) + options.disk.typeName
                            + " means nothing on this volume: Apple Pascal takes BAD, CODE, TEXT, INFO, DATA, GRAF or FOTO\n";
        result.exitStatus   = DiskCommandResult::kNoOutput;
        hr                  = E_NOTIMPL;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFilePayload::BuildPutPayload
//
//  What a file from the host becomes on the disk.
//
//  VERBATIM IS THE ABSENCE OF A CHARACTER CONVERSION AND NOTHING MORE. The
//  bytes go down as they came in; the length and whatever header the type
//  carries are still applied below this, because those record where the file
//  ENDS and are its identity rather than a transformation of it. That is what
//  makes extract-edit-replace safe: the bytes the caller did not touch come
//  back exactly as they were.
//
//  Text is written in the form VolumeImage::GetTextConvention returns: plain
//  ASCII with $0D line ends on ProDOS and HFS, which is what BASIC.SYSTEM and
//  GS/OS write, and high-bit text with $8D line ends on DOS 3.3. The read path
//  decodes either form.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskFilePayload::BuildPutPayload (
    const CommandLineOptions  & options,
    VolumeKind                  kind,
    const vector<Byte>        & hostBytes,
    FilePayload               & outPayload,
    DiskCommandResult         & result)
{
    HRESULT                hr           = S_OK;
    Byte                   type         = 0;
    size_t                 badOffset    = 0;
    char                   note[160]    = {};
    std::string            hostText;
    ApplesoftListingError  listingError;
    bool                   hasAddresses = kind == VolumeKind::Dos33 || kind == VolumeKind::ProDos || kind == VolumeKind::Archive;
    bool                   isReadOnly   = false;
    bool                   integer      = false;
    bool                   allowed      = hasAddresses || (!options.disk.hasLoadAddress && options.disk.encoding != CommandLineOptions::DiskOptions::Encoding::Basic);



    CBRFEx (allowed, HRESULT_FROM_WIN32 (ERROR_INVALID_PARAMETER),
            (result.diagnostics += DiskHelpPage::ApplyPrefixes ("%Lload and %Lbasic mean nothing on this volume: it has no load addresses or BASIC programs\n", options.flagPrefix),
             result.exitStatus   = DiskCommandResult::kNoOutput));

    hr = ResolveFileType (options, kind, hostBytes, type, result);
    CHR (hr);

    integer = (kind == VolumeKind::Dos33) ? type == Dos33Volume::kTypeInteger : type == kProDosIntegerType;

    outPayload.type           = type;
    outPayload.loadAddress    = options.disk.loadAddress;
    outPayload.hasLoadAddress = options.disk.hasLoadAddress;

    if (kind == VolumeKind::Cpm && !options.disk.typeName.empty() && ParseCpmAttributes (options.disk.typeName, isReadOnly, outPayload.isSystem) && isReadOnly)
    {
        outPayload.access    = FilePayload::kAccessLocked;
        outPayload.hasAccess = true;
    }

    switch (options.disk.encoding)
    {
        case CommandLineOptions::DiskOptions::Encoding::Verbatim:
            outPayload.bytes    = hostBytes;
            outPayload.encoding = PayloadEncoding::Verbatim;
            break;

        case CommandLineOptions::DiskOptions::Encoding::Text:
            hostText.assign (hostBytes.begin(), hostBytes.end());

            if (kind == VolumeKind::Cpm)
            {
                CpmVolume::EncodeText (hostText, outPayload.bytes);
            }
            else
            {
                hr = (kind == VolumeKind::Pascal) ? PascalVolume::EncodeText (hostText, outPayload.bytes, badOffset)
                                                  : AppleTextCodec::Encode (hostText, VolumeImage::GetTextConvention (kind), outPayload.bytes, badOffset);
            }

            if (FAILED (hr))
            {
                // Naming the offset is the whole value of the refusal. A smart
                // quote pasted into a listing looks identical to a plain one in
                // an editor, and "somewhere in this file" is not actionable.
                snprintf (note, sizeof (note),
                          "byte %u of the file has no Apple II text representation, "
                          "so nothing was converted\n",
                          (unsigned) badOffset);

                result.diagnostics += note;
                result.exitStatus   = DiskCommandResult::kNoOutput;
                break;
            }

            outPayload.encoding = PayloadEncoding::HostText;
            break;

        case CommandLineOptions::DiskOptions::Encoding::Basic:
            if (options.disk.hasLoadAddress)
            {
                // An Applesoft program loads where Applesoft keeps its program
                // and nowhere else, so an address here is a request that cannot
                // be honored. Accepting and ignoring it would place the program
                // and leave the caller believing it loads somewhere it does not.
                result.diagnostics += DiskHelpPage::ApplyPrefixes (
                    integer ? "%Lload means nothing with %Lbasic: "
                              "an Integer BASIC program has no load address\n"
                            : "%Lload means nothing with %Lbasic: "
                              "an Applesoft BASIC program always loads at $0801\n", options.flagPrefix);
                result.exitStatus   = DiskCommandResult::kNoOutput;
                hr                  = HRESULT_FROM_WIN32 (ERROR_INVALID_PARAMETER);
                break;
            }

            hostText.assign (hostBytes.begin(), hostBytes.end());

            //  A type of I or INT selects Integer BASIC's stored form. ProDOS
            //  has no load address for one, so the auxiliary type is not set.
            if (integer)
            {
                hr = IntegerBasicTokenizer::Tokenize (hostText, outPayload.bytes, listingError);

                if (FAILED (hr))
                {
                    result.diagnostics += DiskCommandText::DescribeListingRefusal (
                        DiskHelpPage::ApplyPrefixes ("%Lbasic cannot make an Integer BASIC program of "
                                       "this listing", options.flagPrefix).c_str(), listingError);

                    result.exitStatus   = DiskCommandResult::kNoOutput;
                    break;
                }

                outPayload.hasLoadAddress = false;
                outPayload.encoding       = PayloadEncoding::HostText;
                break;
            }

            hr = ApplesoftTokenizer::Tokenize (hostText, outPayload.bytes, listingError);

            if (FAILED (hr))
            {
                result.diagnostics += DiskCommandText::DescribeListingRefusal (
                    DiskHelpPage::ApplyPrefixes ("%Lbasic cannot make an Applesoft BASIC program of "
                                   "this listing", options.flagPrefix).c_str(), listingError);

                result.exitStatus   = DiskCommandResult::kNoOutput;
                break;
            }

            // ProDOS records where a BASIC program loads in the auxiliary type;
            // DOS 3.3 records nothing and ignores this. Setting it on both keeps
            // the branch out of here, where it would be a second statement of
            // which filesystem stores what.
            outPayload.auxType        = ApplesoftTokenizer::kProgramBase;
            outPayload.hasAuxType     = true;
            outPayload.hasLoadAddress = false;
            outPayload.encoding       = PayloadEncoding::ApplesoftListing;
            break;

        default:
            result.diagnostics    += "Error: unknown encoding\n";
            result.exitStatus      = DiskCommandResult::kNoOutput;
            result.badCommandLine  = true;
            hr                  = E_INVALIDARG;
            break;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFilePayload::ApplyHostFileName
//
//  Read a host name in one of the forms Casso Explorer writes on Copy or a
//  drag: `NAME.Applesoft BASIC.txt`, `NAME#062000`, `NAME.Binary.$2000.bin`
//  and the rest. With no --as, the entry takes NAME. With or without it,
//  the conversion of a listing's form applies when the command line has no
//  conversion switch.
//  Any other host name is left as it is: `PROG.BIN` goes in as PROG.BIN.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskFilePayload::ApplyHostFileName (CommandLineOptions & inOutOptions, ParsedHostName & outParsed)
{
    size_t        lastSeparator = inOutOptions.disk.hostFile.find_last_of ("/\\");
    std::string   leaf          = (lastSeparator == std::string::npos) ? inOutOptions.disk.hostFile
                                                                       : inOutOptions.disk.hostFile.substr (lastSeparator + 1);
    bool          unconverted   = inOutOptions.disk.encoding == CommandLineOptions::DiskOptions::Encoding::Verbatim
                               && inOutOptions.disk.typeName.empty();



    if (!HostFileNaming::Parse (TextEncoding::NarrowToWide (leaf), outParsed) || outParsed.catalogName.empty())
    {
        return false;
    }

    //  A conversion with no import is a host file like any other. One with
    //  an import is text on a file system the import isn't for.
    if (outParsed.converted == ParsedHostName::ConvertedKind::Format && FileFormatTable::GetImportBySuffix (TextEncoding::NarrowToWide (leaf)) == FileFormat::None)
    {
        return false;
    }

    if (inOutOptions.disk.path.empty())
    {
        inOutOptions.disk.path = outParsed.catalogName;
    }

    if (unconverted && outParsed.converted == ParsedHostName::ConvertedKind::ApplesoftListing)
    {
        inOutOptions.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Basic;
    }
    else if (unconverted && outParsed.converted != ParsedHostName::ConvertedKind::None)
    {
        inOutOptions.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Text;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFilePayload::ApplyHostFileType
//
//  The ProDOS type in a host name, mapped to DOS 3.3's on a DOS 3.3 image,
//  with the aux type as the load address of a binary file, as Casso Explorer
//  puts the same file.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskFilePayload::ApplyHostFileType (const ParsedHostName & parsed, VolumeKind kind, FilePayload & inOutPayload)
{
    HRESULT  hr      = S_OK;
    bool     isDos   = kind == VolumeKind::Dos33;
    Byte     dosType = 0;
    bool     mapped  = false;



    if (kind == VolumeKind::Pascal || kind == VolumeKind::Cpm)
    {
        inOutPayload.type = (kind == VolumeKind::Pascal) ? HostFileNaming::MapProDosToPascalKind (parsed.type) : (Byte) 0;
        BAIL_OUT_IF (true, S_OK);
    }

    mapped = !isDos || HostFileNaming::TryMapProDosToDos33 (parsed.type, dosType);
    CBREx (mapped, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    inOutPayload.type = isDos ? dosType : parsed.type;

    if (parsed.hasAux)
    {
        inOutPayload.hasAuxType     = !isDos;
        inOutPayload.auxType        = parsed.aux;
        inOutPayload.hasLoadAddress = inOutPayload.type == (isDos ? Dos33Volume::kTypeBinary : ProDosVolume::kTypeBinary);
        inOutPayload.loadAddress    = parsed.aux;
    }

Error:
    return hr;
}
