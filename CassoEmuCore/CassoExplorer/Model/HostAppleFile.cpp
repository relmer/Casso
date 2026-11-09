#include "Pch.h"

#include "CassoExplorer/Model/HostAppleFile.h"
#include "CassoExplorer/Model/HostFileNaming.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HostAppleFile::Read
//
//  An AppleSingle container is known by its bytes, whatever its name. A name
//  that parses as a listing converted to text isn't offered: its type is the
//  conversion, not a property of the file.
//
////////////////////////////////////////////////////////////////////////////////

HostAppleFile HostAppleFile::Read (const std::wstring & leafName, const std::vector<Byte> & bytes)
{
    HostAppleFile   file;
    ParsedHostName  parsed;
    std::string     error;
    HRESULT         hr     = S_OK;



    file.leafName = leafName;

    if (AppleSingleCodec::IsAppleSingle (std::span<const Byte> (bytes.data(), bytes.size())))
    {
        hr = AppleSingleCodec::Decode (std::span<const Byte> (bytes.data(), bytes.size()), file.single, error);

        if (SUCCEEDED (hr))
        {
            file.form              = Form::AppleSingle;
            file.entry.name        = file.single.realName.empty() ? TextEncoding::WideToNarrow (std::filesystem::path (leafName).stem().wstring())
                                                                  : file.single.realName;
            file.entry.type        = (Byte) file.single.fileType;
            file.entry.auxType     = (Word) file.single.auxType;
            file.entry.hasAuxType  = file.single.hasProDosInfo;
            file.entry.access      = (Byte) file.single.access;
            file.entry.hasAccess   = file.single.hasProDosInfo;
            file.entry.isLocked    = file.single.hasProDosInfo && (file.single.access & FilePayload::kAccessWriteEnable) == 0;
            file.entry.eofBytes    = (uint32_t) file.single.data.size();
            file.entry.hasEofBytes = true;
            file.entry.hasDates    = true;

            if (file.single.createDate.has_value())
            {
                ToProDosDate (*file.single.createDate, file.entry.createdDate, file.entry.createdTime);
            }

            if (file.single.modifyDate.has_value())
            {
                ToProDosDate (*file.single.modifyDate, file.entry.modifiedDate, file.entry.modifiedTime);
            }
        }

        return file;
    }

    if (!HostFileNaming::Parse (leafName, parsed) || !parsed.hasType || parsed.converted != ParsedHostName::ConvertedKind::None)
    {
        return file;
    }

    file.form              = (leafName.find (L'#') != std::wstring::npos) ? Form::CiderPress : Form::Descriptive;
    file.entry.name        = parsed.catalogName;
    file.entry.type        = parsed.type;
    file.entry.auxType     = parsed.aux;
    file.entry.hasAuxType  = parsed.hasAux;
    file.entry.eofBytes    = (uint32_t) bytes.size();
    file.entry.hasEofBytes = true;

    return file;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostAppleFile::Apply
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HostAppleFile::Apply (const AppleFileProperties::Edits & edits, bool renameHostFile, Change & outChange) const
{
    HRESULT          hr     = S_OK;
    std::string      name   = edits.nameEdited ? TextEncoding::WideToNarrow (edits.name) : entry.name;
    FileAttributes   change = AppleFileProperties::MakeAttributes (entry, VolumeKind::ProDos, edits);
    Byte             type   = change.hasType ? change.type : entry.type;
    Word             aux    = change.hasAuxType ? change.auxType : entry.auxType;
    bool             hasAux = AppleFileProperties::GetAuxKind (VolumeKind::ProDos, type) != AppleFileProperties::AuxKind::None;
    AppleSingleFile  edited = single;
    bool             keeps  = !single.hasOtherEntries;



    outChange          = Change();
    outChange.leafName = leafName;

    CBRAEx (form != Form::None, E_INVALIDARG);

    //  A Descriptive or CiderPress file's properties are its name.
    if (form != Form::AppleSingle)
    {
        outChange.leafName = HostFileNaming::ForRaw (name, VolumeKind::ProDos, type, hasAux, aux,
                                                     form == Form::CiderPress ? HostFileNaming::Style::CiderPress : HostFileNaming::Style::Descriptive);
        return S_OK;
    }

    CBREx (keeps, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

    edited.realName      = name;
    edited.hasProDosInfo = true;
    edited.fileType      = type;
    edited.auxType       = aux;
    edited.access        = change.hasAccess ? change.access : AppleFileProperties::GetAccess (entry);
    edited.nameSetByUser = single.nameSetByUser || edits.nameEdited;

    if (change.hasCreated)
    {
        edited.createDate = (change.createdDate == 0) ? std::optional<int32_t>() : std::optional<int32_t> (FromProDosDate (change.createdDate, change.createdTime));
    }

    if (change.hasModified)
    {
        edited.modifyDate = (change.modifiedDate == 0) ? std::optional<int32_t>() : std::optional<int32_t> (FromProDosDate (change.modifiedDate, change.modifiedTime));
    }

    AppleSingleCodec::Encode (edited, outChange.bytes);

    if (renameHostFile)
    {
        outChange.leafName = MakeAppleSingleName (name);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostAppleFile::MakeAppleSingleName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HostAppleFile::MakeAppleSingleName (const std::string & appleName)
{
    bool  substituted = false;



    return HostFileNaming::MakeHostLegal (appleName, substituted) + L".as";
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostAppleFile::ToProDosDate
//
//  A ProDOS date has a two-digit year, 1940 to 2039.
//
////////////////////////////////////////////////////////////////////////////////

void HostAppleFile::ToProDosDate (int32_t seconds, Word & outDate, Word & outTime)
{
    using namespace std::chrono;

    sys_seconds       when  = sys_days (year (2000) / January / 1) + std::chrono::seconds (seconds);
    sys_days          day   = floor<days> (when);
    year_month_day    ymd   (day);
    hh_mm_ss<std::chrono::seconds>  clock (when - day);
    int               y     = (int) ymd.year();



    outDate = 0;
    outTime = 0;

    if (y < AppleFileProperties::kFirstYear || y > AppleFileProperties::kLastYear)
    {
        return;
    }

    outDate = (Word) (((y % 100) << 9) | ((unsigned) ymd.month() << 5) | (unsigned) ymd.day());
    outTime = (Word) ((clock.hours().count() << 8) | clock.minutes().count());
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostAppleFile::FromProDosDate
//
////////////////////////////////////////////////////////////////////////////////

int32_t HostAppleFile::FromProDosDate (Word date, Word time)
{
    using namespace std::chrono;

    int        twoDigit = (date >> 9) & 0x7F;
    int        y        = twoDigit + ((twoDigit < AppleFileProperties::kPivot) ? 2000 : 1900);
    sys_days   day      = year (y) / month ((date >> 5) & 0x0F) / (date & 0x1F);
    auto       offset   = day - sys_days (year (2000) / January / 1);



    return (int32_t) (duration_cast<std::chrono::seconds> (offset).count() + ((time >> 8) & 0x1F) * 3600 + (time & 0x3F) * 60);
}
