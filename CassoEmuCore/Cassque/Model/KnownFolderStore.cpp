#include "Pch.h"

#include "Cassque/Model/KnownFolderStore.h"
#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  KnownFolderStore::KnownFolderStore
//
////////////////////////////////////////////////////////////////////////////////

KnownFolderStore::KnownFolderStore (IFileSystem & fs, const std::wstring & baseDir, bool crossProcessLock)
    : m_fs               (fs),
      m_filePath         (GetFilePath (baseDir)),
      m_crossProcessLock (crossProcessLock)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  KnownFolderStore::GetFilePath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring KnownFolderStore::GetFilePath (const std::wstring & baseDir)
{
    std::wstring  path = baseDir;



    if (!path.empty() && path.back() != L'\\' && path.back() != L'/')
    {
        path += L'\\';
    }

    return path + kFileName;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KnownFolderStore::ArePathsEqual
//
//  Case-insensitive, separators unified, a trailing separator ignored: the
//  way the host itself decides two folder paths are one folder.
//
////////////////////////////////////////////////////////////////////////////////

bool KnownFolderStore::ArePathsEqual (const std::wstring & a, const std::wstring & b)
{
    std::wstring  left  = a;
    std::wstring  right = b;



    for (wchar_t & c : left)  { if (c == L'/') { c = L'\\'; } }
    for (wchar_t & c : right) { if (c == L'/') { c = L'\\'; } }

    while (left.size() > 1 && left.back() == L'\\')   { left.pop_back(); }
    while (right.size() > 1 && right.back() == L'\\') { right.pop_back(); }

    return _wcsicmp (left.c_str(), right.c_str()) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KnownFolderStore::MergePickerFolders
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::filesystem::path> KnownFolderStore::MergePickerFolders (
    const std::vector<Entry>           & known,
    const std::vector<DiskMru::Entry>  & mru)
{
    std::vector<std::filesystem::path>  merged;
    std::vector<std::filesystem::path>  recent = DiskMru::DistinctFolders (mru);



    for (const Entry & entry : known)
    {
        bool  already = std::any_of (merged.begin(), merged.end(), [&entry] (const std::filesystem::path & p)
                                     { return ArePathsEqual (p.wstring(), entry.path); });

        if (!already)
        {
            merged.push_back (std::filesystem::path (entry.path));
        }
    }

    for (const std::filesystem::path & folder : recent)
    {
        bool  already = std::any_of (merged.begin(), merged.end(), [&folder] (const std::filesystem::path & p)
                                     { return ArePathsEqual (p.wstring(), folder.wstring()); });

        if (!already)
        {
            merged.push_back (folder);
        }
    }

    return merged;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KnownFolderStore::LoadPickerFolders
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::filesystem::path> KnownFolderStore::LoadPickerFolders (
    IFileSystem                        & fs,
    const std::wstring                 & baseDir,
    const std::vector<DiskMru::Entry>  & mru,
    int64_t                              nowUnix)
{
    KnownFolderStore    store (fs, baseDir);
    std::vector<Entry>  known;
    HRESULT             hr    = S_OK;



    hr = store.SeedFromMru (mru, nowUnix);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = store.Load (known);

    if (FAILED (hr))
    {
        known.clear();
    }

    return MergePickerFolders (known, mru);
}





////////////////////////////////////////////////////////////////////////////////
//
//  KnownFolderStore::Exists
//
////////////////////////////////////////////////////////////////////////////////

bool KnownFolderStore::Exists() const
{
    return m_fs.Exists (m_filePath);
}





////////////////////////////////////////////////////////////////////////////////
//
//  KnownFolderStore::AcquireLock
//
//  The named mutex, or null when the store was built without one. A wait
//  that times out proceeds anyway: a wedged writer elsewhere must not stop
//  this one from recording a hand-off, and the atomic replace keeps the file
//  whole either way.
//
////////////////////////////////////////////////////////////////////////////////

HANDLE KnownFolderStore::AcquireLock() const
{
    HANDLE  lock   = nullptr;
    DWORD   waited = 0;



    if (!m_crossProcessLock)
    {
        return nullptr;
    }

    lock = CreateMutexW (nullptr, FALSE, kMutexName);

    if (lock != nullptr)
    {
        waited = WaitForSingleObject (lock, kLockTimeoutMs);

        IGNORE_RETURN_VALUE (waited, WAIT_OBJECT_0);
    }

    return lock;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KnownFolderStore::ReleaseLock
//
////////////////////////////////////////////////////////////////////////////////

void KnownFolderStore::ReleaseLock (HANDLE lock) const
{
    BOOL  released = FALSE;
    BOOL  closed   = FALSE;



    if (lock == nullptr)
    {
        return;
    }

    released = ReleaseMutex (lock);
    IGNORE_RETURN_VALUE (released, TRUE);

    closed = CloseHandle (lock);
    IGNORE_RETURN_VALUE (closed, TRUE);
}





////////////////////////////////////////////////////////////////////////////////
//
//  KnownFolderStore::ReadEntries
//
//  Every well-formed row of the file. A row without a path is skipped rather
//  than refused, so one bad edit cannot hide the rest of the list.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KnownFolderStore::ReadEntries (std::vector<Entry> & outEntries) const
{
    HRESULT            hr      = S_OK;
    std::string        content;
    JsonValue          root;
    JsonParseError     parseError;
    const JsonValue *  folders = nullptr;
    bool               present = m_fs.Exists (m_filePath);
    size_t             i       = 0;



    outEntries.clear();

    BAIL_OUT_IF (!present, S_OK);

    hr = m_fs.ReadAllText (m_filePath, content);
    CHR (hr);

    hr = JsonParser::Parse (content, root, parseError);
    CHR (hr);

    if (root.HasArray ("folders", folders))
    {
        for (i = 0; i < folders->GetArraySize(); i++)
        {
            const JsonValue &  row  = folders->GetArrayElement (i);
            std::string        path;
            double             when = 0;
            Entry              entry;

            if (row.GetType() != JsonType::Object || !row.HasString ("path", path) || path.empty())
            {
                continue;
            }

            if (row.HasNumber ("lastUsedUnix", when))
            {
                entry.lastUsedUnix = (int64_t) when;
            }

            entry.path = TextEncoding::NarrowToWide (path);

            outEntries.push_back (entry);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KnownFolderStore::WriteEntries
//
//  The whole file, replaced atomically by the file-system seam's write.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KnownFolderStore::WriteEntries (const std::vector<Entry> & entries) const
{
    HRESULT                                         hr      = S_OK;
    std::vector<JsonValue>                          rows;
    std::vector<std::pair<std::string, JsonValue>>  root;
    std::string                                     text;
    JsonWriter::Options                             options;



    for (const Entry & entry : entries)
    {
        std::vector<std::pair<std::string, JsonValue>>  row;

        row.emplace_back ("path",         JsonValue (TextEncoding::WideToNarrow (entry.path)));
        row.emplace_back ("lastUsedUnix", JsonValue ((double) entry.lastUsedUnix));

        rows.emplace_back (std::move (row));
    }

    root.emplace_back ("folders", JsonValue (std::move (rows)));

    hr = JsonWriter::Write (JsonValue (std::move (root)), options, text);
    CHR (hr);

    hr = m_fs.WriteAllText (m_filePath, text);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KnownFolderStore::Load
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KnownFolderStore::Load (std::vector<Entry> & outEntries) const
{
    return ReadEntries (outEntries);
}





////////////////////////////////////////////////////////////////////////////////
//
//  KnownFolderStore::Append
//
//  Read, merge, replace, under the lock. A folder already listed keeps its
//  place and takes the new stamp.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KnownFolderStore::Append (const std::wstring & folder, int64_t nowUnix)
{
    HRESULT             hr        = S_OK;
    HANDLE              lock      = nullptr;
    bool                found     = false;
    bool                hasFolder = !folder.empty();
    std::vector<Entry>  entries;



    CBREx (hasFolder, E_INVALIDARG);

    lock = AcquireLock();

    hr = ReadEntries (entries);
    CHR (hr);

    for (Entry & entry : entries)
    {
        if (ArePathsEqual (entry.path, folder))
        {
            entry.lastUsedUnix = nowUnix;
            found              = true;
        }
    }

    if (!found)
    {
        entries.push_back (Entry { folder, nowUnix });
    }

    hr = WriteEntries (entries);
    CHR (hr);

Error:
    ReleaseLock (lock);

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KnownFolderStore::Remove
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KnownFolderStore::Remove (const std::wstring & folder)
{
    HRESULT             hr   = S_OK;
    HANDLE              lock = nullptr;
    std::vector<Entry>  entries;
    std::vector<Entry>  kept;



    lock = AcquireLock();

    hr = ReadEntries (entries);
    CHR (hr);

    for (const Entry & entry : entries)
    {
        if (!ArePathsEqual (entry.path, folder))
        {
            kept.push_back (entry);
        }
    }

    hr = WriteEntries (kept);
    CHR (hr);

Error:
    ReleaseLock (lock);

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KnownFolderStore::SeedFromMru
//
//  Only when there is no file: once the list exists it is the user's, and the
//  recent-disks list keeps its own job for the emulator's picker.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KnownFolderStore::SeedFromMru (const std::vector<DiskMru::Entry> & mru, int64_t nowUnix)
{
    HRESULT                             hr      = S_OK;
    HANDLE                              lock    = nullptr;
    bool                                present = false;
    std::vector<std::filesystem::path>  folders;
    std::vector<Entry>                  entries;



    lock    = AcquireLock();
    present = m_fs.Exists (m_filePath);

    BAIL_OUT_IF (present, S_OK);

    folders = DiskMru::DistinctFolders (mru);

    for (const std::filesystem::path & folder : folders)
    {
        entries.push_back (Entry { folder.wstring(), nowUnix });
    }

    hr = WriteEntries (entries);
    CHR (hr);

Error:
    ReleaseLock (lock);

    return hr;
}
