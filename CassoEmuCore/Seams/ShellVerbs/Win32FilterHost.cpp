#include "Pch.h"

#include "Seams/ShellVerbs/Win32FileContents.h"
#include "Seams/ShellVerbs/Win32FilterHost.h"

static constexpr const wchar_t *  s_kpszMetadataProperties[] =
{
    L"System.Title",
    L"System.Subject",
    L"System.Comment",
    L"System.Keywords",
    L"System.Author",
    L"System.Copyright",
    L"System.Photo.CameraManufacturer",
    L"System.Photo.CameraModel",
};





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FilterHost::TryRunFromCommandLine
//
//  The host's command line is the switch alone. Critical errors and crash
//  dialogs are turned off first, so a filter that faults ends the process
//  without a window.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32FilterHost::TryRunFromCommandLine (int & outExitCode)
{
    HRESULT   hr     = S_OK;
    HRESULT   hrCom  = E_FAIL;
    int       argc   = 0;
    LPWSTR  * argv   = CommandLineToArgvW (GetCommandLineW(), &argc);
    bool      helper = argv != nullptr && argc == 2 && wcscmp (argv[1], kSwitch) == 0;



    BAIL_OUT_IF (!helper, S_OK);

    SetErrorMode (SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

    hrCom = CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED);
    CHR (hrCom);

    hr = Serve();
    CHR (hr);

Error:
    if (SUCCEEDED (hrCom))
    {
        CoUninitialize();
    }

    if (argv != nullptr)
    {
        LocalFree (argv);
    }

    if (helper)
    {
        outExitCode = (int) hr;
    }

    return helper;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FilterHost::Serve
//
//  Serves requests until the input closes, which is the normal end. A reply
//  that cannot be written means the other end is gone, which also ends the
//  loop.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32FilterHost::Serve()
{
    HRESULT       hr        = S_OK;
    HRESULT       hrRead    = S_OK;
    HRESULT       hrExtract = S_OK;
    HANDLE        input     = GetStdHandle (STD_INPUT_HANDLE);
    HANDLE        output    = GetStdHandle (STD_OUTPUT_HANDLE);
    bool          hasInput  = input != nullptr && input != INVALID_HANDLE_VALUE;
    bool          hasOutput = output != nullptr && output != INVALID_HANDLE_VALUE;
    std::wstring  path;
    std::wstring  text;



    CBR (hasInput);
    CBR (hasOutput);

    hrRead = ReadRequest (input, path);

    while (SUCCEEDED (hrRead))
    {
        text.clear();

        hrExtract = ExtractText (path, text);

        if (FAILED (hrExtract))
        {
            text.clear();
        }

        hr = WriteReply (output, hrExtract, text);
        CHR (hr);

        hrRead = ReadRequest (input, path);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FilterHost::ReadExact
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32FilterHost::ReadExact (HANDLE input, void * buffer, DWORD count)
{
    HRESULT  hr     = S_OK;
    Byte   * cursor = (Byte *) buffer;
    DWORD    got    = 0;
    BOOL     read   = FALSE;
    bool     hasAll = false;



    while (count > 0)
    {
        read = ReadFile (input, cursor, count, &got, nullptr);
        CWR (read);

        hasAll = got != 0;
        CBREx (hasAll, HRESULT_FROM_WIN32 (ERROR_HANDLE_EOF));

        cursor += got;
        count  -= got;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FilterHost::WriteAll
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32FilterHost::WriteAll (HANDLE output, const void * buffer, DWORD count)
{
    HRESULT      hr      = S_OK;
    const Byte * cursor  = (const Byte *) buffer;
    DWORD        written = 0;
    BOOL         wrote   = FALSE;



    while (count > 0)
    {
        wrote = WriteFile (output, cursor, count, &written, nullptr);
        CWR (wrote);

        cursor += written;
        count  -= written;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FilterHost::ReadRequest
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32FilterHost::ReadRequest (HANDLE input, std::wstring & outPath)
{
    HRESULT   hr     = S_OK;
    uint32_t  length = 0;



    hr = ReadExact (input, &length, sizeof (length));
    CHR (hr);

    CBREx (length > 0 && length <= kMaxRequestPath, E_INVALIDARG);

    outPath.resize (length);

    hr = ReadExact (input, outPath.data(), length * sizeof (wchar_t));
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FilterHost::WriteReply
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32FilterHost::WriteReply (HANDLE output, HRESULT status, const std::wstring & text)
{
    HRESULT      hr     = S_OK;
    ReplyHeader  header;



    header.status = status;
    header.length = (uint32_t) text.size();

    hr = WriteAll (output, &header, sizeof (header));
    CHR (hr);

    hr = WriteAll (output, text.data(), (DWORD) (text.size() * sizeof (wchar_t)));
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FilterHost::LoadFilter
//
//  query.dll's LoadIFilter finds the filter registered for the path's
//  extension. It is looked up at run time, so no program that links this
//  library has to link query.lib.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32FilterHost::LoadFilter (const std::wstring & path, IFilter ** outFilter)
{
    typedef HRESULT (STDAPICALLTYPE * LoadIFilterFn) (PCWSTR path, IUnknown * outer, void ** filter);



    HRESULT         hr           = S_OK;
    HMODULE         query        = LoadLibraryW (L"query.dll");
    LoadIFilterFn   loadIFilter  = nullptr;



    *outFilter = nullptr;

    CWR (query != nullptr);

    loadIFilter = (LoadIFilterFn) GetProcAddress (query, "LoadIFilter");
    CWR (loadIFilter != nullptr);

    hr = loadIFilter (path.c_str(), nullptr, (void **) outFilter);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FilterHost::CreateFilter
//
//  For a filter that LoadIFilter can't open, as one that reads a stream and
//  not a file: the class registered for the extension is created and the
//  file is loaded into it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32FilterHost::CreateFilter (const std::wstring & path, IFilter ** outFilter)
{
    HRESULT                          hr        = S_OK;
    std::wstring                     extension = std::filesystem::path (path).extension().wstring();
    std::wstring                     classId;
    CLSID                            clsid     = {};
    Microsoft::WRL::ComPtr<IFilter>  filter;
    bool                             found     = false;



    *outFilter = nullptr;

    found = Win32FileContents::TryGetFilterClass (extension, classId);
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    hr = CLSIDFromString (classId.c_str(), &clsid);
    CHR (hr);

    hr = CoCreateInstance (clsid, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&filter));
    CHR (hr);

    hr = InitializeFilter (filter.Get(), path);
    CHR (hr);

    *outFilter = filter.Detach();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FilterHost::InitializeFilter
//
//  Loads the file into a filter through IPersistFile, or through a stream
//  when the filter reads one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32FilterHost::InitializeFilter (IFilter * filter, const std::wstring & path)
{
    HRESULT                                          hr         = S_OK;
    HRESULT                                          hrFile     = E_NOINTERFACE;
    Microsoft::WRL::ComPtr<IPersistFile>             file;
    Microsoft::WRL::ComPtr<IInitializeWithStream>    init;
    Microsoft::WRL::ComPtr<IStream>                  stream;



    hrFile = filter->QueryInterface (IID_PPV_ARGS (&file));

    if (SUCCEEDED (hrFile))
    {
        hr = file->Load (path.c_str(), STGM_READ | STGM_SHARE_DENY_NONE);
        CHR (hr);
    }
    else
    {
        hr = filter->QueryInterface (IID_PPV_ARGS (&init));
        CHR (hr);

        hr = SHCreateStreamOnFileEx (path.c_str(), STGM_READ | STGM_SHARE_DENY_NONE, FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, &stream);
        CHR (hr);

        hr = init->Initialize (stream.Get(), STGM_READ);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FilterHost::ExtractText
//
//  The text of every text chunk and the string values of every value chunk,
//  such as an image's metadata, up to kMaxChars. When the filter returns
//  no text, as the pixel filter does for an image, the text of the file's
//  title, comment and similar properties is used instead. A chunk that cannot be read
//  is skipped. A filter that fails partway returns the text read before the
//  failure, and the request fails only when no text was read.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32FilterHost::ExtractText (const std::wstring & path, std::wstring & outText)
{
    constexpr ULONG  kInitFlags = IFILTER_INIT_CANON_PARAGRAPHS | IFILTER_INIT_CANON_SPACES | IFILTER_INIT_APPLY_INDEX_ATTRIBUTES |
                                  IFILTER_INIT_APPLY_OTHER_ATTRIBUTES | IFILTER_INIT_FILTER_OWNED_VALUE_OK;



    HRESULT                             hr         = S_OK;
    HRESULT                             hrChunk    = S_OK;
    Microsoft::WRL::ComPtr<IFilter>     filter;
    ULONG                               flags      = 0;
    STAT_CHUNK                          chunk      = {};
    bool                                isFull     = false;
    bool                                hasText    = false;
    bool                                isFinished = false;



    hr = LoadFilter (path, filter.GetAddressOf());

    if (FAILED (hr))
    {
        hr = CreateFilter (path, filter.ReleaseAndGetAddressOf());
    }

    CHR (hr);

    hr = filter->Init (kInitFlags, 0, nullptr, &flags);
    CHR (hr);

    hrChunk = filter->GetChunk (&chunk);

    while (!isFinished)
    {
        if (SUCCEEDED (hrChunk) && (chunk.flags & CHUNK_TEXT) != 0)
        {
            ReadChunkText (filter.Get(), outText);
        }
        else if (SUCCEEDED (hrChunk))
        {
            ReadChunkValues (filter.Get(), outText);
        }

        isFull     = outText.size() >= kMaxChars;
        isFinished = isFull;

        //  A chunk that is an embedding or link the filter cannot open is
        //  passed over; anything else that is not a chunk ends the filter.
        if (!isFull)
        {
            isFinished = FAILED (hrChunk) && hrChunk != FILTER_E_EMBEDDING_UNAVAILABLE && hrChunk != FILTER_E_LINK_UNAVAILABLE;
        }

        if (!isFinished)
        {
            hrChunk = filter->GetChunk (&chunk);
        }
    }

    if (outText.empty())
    {
        ReadMetadata (path, outText);
    }

    if (outText.size() > kMaxChars)
    {
        outText.resize (kMaxChars);
    }

    hasText = !outText.empty();
    CBREx (hasText, FILTER_E_END_OF_CHUNKS);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FilterHost::ReadChunkText
//
//  Appends the current chunk's text and a space after it, so a word at the
//  end of one chunk doesn't join the first of the next.
//
////////////////////////////////////////////////////////////////////////////////

void Win32FilterHost::ReadChunkText (IFilter * filter, std::wstring & text)
{
    HRESULT  hr         = S_OK;
    wchar_t  buffer[kReadBufferChars];
    ULONG    count      = 0;
    bool     isFinished = false;
    bool     isFull     = false;



    while (!isFinished)
    {
        count = kReadBufferChars;
        hr    = filter->GetText (&count, buffer);

        if (SUCCEEDED (hr) && count > 0)
        {
            text.append (buffer, count);
        }

        //  A filter that returns success with no text would loop forever.
        isFull     = text.size() >= kMaxChars;
        isFinished = FAILED (hr) || hr == FILTER_S_LAST_TEXT || count == 0 || isFull;
    }

    text.push_back (L' ');
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FilterHost::ReadChunkValues
//
//  Appends each string value of the current chunk, with a space after it.
//
////////////////////////////////////////////////////////////////////////////////

void Win32FilterHost::ReadChunkValues (IFilter * filter, std::wstring & text)
{
    HRESULT        hr         = S_OK;
    PROPVARIANT  * value      = nullptr;
    bool           isFinished = false;
    bool           isFull     = false;



    while (!isFinished)
    {
        value = nullptr;

        hr = filter->GetValue (&value);

        if (SUCCEEDED (hr) && value != nullptr)
        {
            AppendValue (*value, text);

            PropVariantClear (value);
            CoTaskMemFree (value);
        }

        isFull     = text.size() >= kMaxChars;
        isFinished = FAILED (hr) || isFull;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FilterHost::AppendValue
//
//  A string, or a vector of them, as text. Numbers and dates are left out, as
//  a search for a word has no use for them.
//
////////////////////////////////////////////////////////////////////////////////

void Win32FilterHost::AppendValue (const PROPVARIANT & value, std::wstring & text)
{
    HRESULT  hr        = S_OK;
    PWSTR    converted = nullptr;
    VARTYPE  type      = (VARTYPE) (value.vt & ~VT_VECTOR);
    bool     isString  = type == VT_LPWSTR || type == VT_LPSTR || type == VT_BSTR;



    BAIL_OUT_IF (!isString, S_OK);

    hr = PropVariantToStringAlloc (value, &converted);
    CHR (hr);

    text.append (converted);
    text.push_back (L' ');

Error:
    if (converted != nullptr)
    {
        CoTaskMemFree (converted);
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FilterHost::ReadMetadata
//
//  The string values of the file's descriptive properties, such as the title
//  of an image. Other properties are left out: the file's name and
//  folder are properties too, and a search of contents must not match them.
//
////////////////////////////////////////////////////////////////////////////////

void Win32FilterHost::ReadMetadata (const std::wstring & path, std::wstring & text)
{
    HRESULT                                     hr    = S_OK;
    Microsoft::WRL::ComPtr<IPropertyStore>      store;
    PROPERTYKEY                                 key   = {};
    PROPVARIANT                                 value = {};



    hr = SHGetPropertyStoreFromParsingName (path.c_str(), nullptr, GPS_DEFAULT, IID_PPV_ARGS (&store));
    CHR (hr);

    for (const wchar_t * name : s_kpszMetadataProperties)
    {
        hr = PSGetPropertyKeyFromName (name, &key);

        if (SUCCEEDED (hr))
        {
            PropVariantInit (&value);

            hr = store->GetValue (key, &value);

            if (SUCCEEDED (hr))
            {
                AppendValue (value, text);
            }

            PropVariantClear (&value);
        }
    }

Error:
    return;
}
