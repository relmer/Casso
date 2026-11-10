#include "Pch.h"

#include "Config/DiskInspectorPrefs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorPrefs::ToJson
//
//  Follow head, the last drive and whether the window was open at exit are
//  written only for Casso (FR-080).
//
////////////////////////////////////////////////////////////////////////////////

JsonValue DiskInspectorPrefs::ToJson (bool hasDriveState) const
{
    vector<pair<string, JsonValue>>  fields;



    fields.emplace_back ("x",           JsonValue ((double) x));
    fields.emplace_back ("y",           JsonValue ((double) y));
    fields.emplace_back ("w",           JsonValue ((double) w));
    fields.emplace_back ("h",           JsonValue ((double) h));
    fields.emplace_back ("maximized",   JsonValue (isMaximized));
    fields.emplace_back ("timingMode",  JsonValue (isTimingMode));
    fields.emplace_back ("timingRange", JsonValue (timingRange));
    fields.emplace_back ("alignment",   JsonValue (isAlignment));
    fields.emplace_back ("files",       JsonValue (isFiles));
    fields.emplace_back ("reads",       JsonValue (isReads));
    fields.emplace_back ("writes",      JsonValue (isWrites));
    fields.emplace_back ("scans",       JsonValue (isScans));
    fields.emplace_back ("showDeleted", JsonValue (isShowDeleted));

    if (hasDriveState)
    {
        fields.emplace_back ("followHead", JsonValue (isFollowHead));
    }

    fields.emplace_back ("zoom", JsonValue (zoom));
    fields.emplace_back ("tilt", JsonValue (tilt));

    if (hasDriveState)
    {
        fields.emplace_back ("lastDrive", JsonValue ((double) lastDrive));
        fields.emplace_back ("open",      JsonValue (isOpen));
    }

    fields.emplace_back ("walkthroughSeen", JsonValue (isWalkthroughSeen));
    fields.emplace_back ("goToKind",        JsonValue ((double) goToKind));

    return JsonValue (std::move (fields));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorPrefs::FromJson
//
//  A field is read only when its key is present with the right type.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorPrefs::FromJson (const JsonValue & obj)
{
    ReadInt    (obj, "x",               x);
    ReadInt    (obj, "y",               y);
    ReadInt    (obj, "w",               w);
    ReadInt    (obj, "h",               h);
    ReadBool   (obj, "maximized",       isMaximized);
    ReadBool   (obj, "timingMode",      isTimingMode);
    ReadNumber (obj, "timingRange",     timingRange);
    ReadBool   (obj, "alignment",       isAlignment);
    ReadBool   (obj, "files",           isFiles);
    ReadBool   (obj, "reads",           isReads);
    ReadBool   (obj, "writes",          isWrites);
    ReadBool   (obj, "scans",           isScans);
    ReadBool   (obj, "showDeleted",     isShowDeleted);
    ReadBool   (obj, "followHead",      isFollowHead);
    ReadNumber (obj, "zoom",            zoom);
    ReadNumber (obj, "tilt",            tilt);
    ReadInt    (obj, "lastDrive",       lastDrive);
    ReadBool   (obj, "open",            isOpen);
    ReadBool   (obj, "walkthroughSeen", isWalkthroughSeen);
    ReadInt    (obj, "goToKind",        goToKind);

    w = std::max (w, 0);
    h = std::max (h, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorPrefs::ReadBool
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorPrefs::ReadBool (const JsonValue & obj, const char * key, bool & inOut)
{
    bool  value = inOut;



    inOut = obj.HasBool (key, value) ? value : inOut;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorPrefs::ReadInt
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorPrefs::ReadInt (const JsonValue & obj, const char * key, int & inOut)
{
    int  value = inOut;



    inOut = obj.HasInt (key, value) ? value : inOut;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorPrefs::ReadNumber
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorPrefs::ReadNumber (const JsonValue & obj, const char * key, double & inOut)
{
    double  value = inOut;



    inOut = obj.HasNumber (key, value) ? value : inOut;
}
