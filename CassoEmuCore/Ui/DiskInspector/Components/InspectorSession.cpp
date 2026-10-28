#include "Pch.h"

#include "Ui/DiskInspector/Components/InspectorSession.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorSession::InspectorSession
//
//  The scheduler is created without a callback: the window takes analysis
//  results on its frame. The view contexts have the addresses of the
//  analysis, the models and the edits, so a session is never copied.
//
////////////////////////////////////////////////////////////////////////////////

InspectorSession::InspectorSession() :
    scheduler (std::make_unique<AnalysisScheduler> (nullptr))
{
    host             = &nullHost;
    context.analysis = &analysis;
    context.model    = &model;
    context.edits    = &edits;
    contextB.model   = &modelB;
    diskTab          = kTabTracks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorSession::GetFileName
//
//  The image's file name without its folder, or "Untitled disk".
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorSession::GetFileName() const
{
    std::wstring  name;
    size_t        slash = 0;



    if (analysis.copy != nullptr)
    {
        name  = TextEncoding::Utf8ToWide (analysis.copy->fileName);
        slash = name.find_last_of (L"\\/");
        name  = (slash == std::wstring::npos) ? name : name.substr (slash + 1);
    }

    return name.empty() ? std::wstring (L"Untitled disk") : name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorSession::IsWindowDiskA
//
//  True while A is the window's drive as it is now, the one disk the window
//  can edit (FR-121).
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorSession::IsWindowDiskA() const
{
    return sourceA == ComparisonSource { ComparisonSourceKind::DriveNow, drive };
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorSession::GetFileMap
//
//  The map of the volume chosen in the File map tab, or none before the disk's
//  analysis has a map.
//
////////////////////////////////////////////////////////////////////////////////

const FileMap * InspectorSession::GetFileMap() const
{
    const vector<FileMap> &  maps = analysis.fileMaps;



    return (context.hasDisk && !maps.empty()) ? &maps[std::clamp (mapIndex, 0, static_cast<int> (maps.size()) - 1)] : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorSession::IsSectorImage
//
//  A .dsk, .do or .po stores only sector data, so its checksum is always
//  recomputed (FR-098).
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorSession::IsSectorImage() const
{
    DiskFormat  format = (analysis.copy != nullptr) ? analysis.copy->format : DiskFormat::Woz;



    return format == DiskFormat::Dsk || format == DiskFormat::Do || format == DiskFormat::Po;
}
