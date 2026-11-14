#include "Pch.h"

#include "DiskCheck/GivenNames.h"
#include "Core/JsonParser.h"
#include "Core/JsonValue.h"
#include "Core/JsonWriter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GivenNames::GivenNames
//
////////////////////////////////////////////////////////////////////////////////

GivenNames::GivenNames (std::filesystem::path folder) :
    m_path (std::move (folder) / kFileName)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  GivenNames::GetGivenPart
//
////////////////////////////////////////////////////////////////////////////////

std::string GivenNames::GetGivenPart (const std::string & fileName)
{
    size_t       dot    = fileName.find_last_of ('.');
    std::string  stem   = fileName.substr (0, dot);
    size_t       digits = 0;



    while (digits < stem.size() && stem[digits] >= '0' && stem[digits] <= '9')
    {
        digits++;
    }

    if (digits > 0 && digits < stem.size() && stem[digits] == ' ')
    {
        stem = stem.substr (digits + 1);
    }
    else if (digits == stem.size())
    {
        stem.clear();
    }

    while (!stem.empty() && stem.front() == ' ')
    {
        stem.erase (0, 1);
    }

    return stem;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GivenNames::Set
//
//  The file is read again first, so a name written since by another window
//  or process is written back with the rest.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GivenNames::Set (const std::string & fingerprint, const std::string & name)
{
    HRESULT          hr = S_OK;
    std::lock_guard  lock (m_lock);



    BAIL_OUT_IF (fingerprint.empty(), S_OK);

    hr = Load();
    CHR (hr);

    if (name.empty())
    {
        m_names.erase (fingerprint);
    }
    else
    {
        m_names[fingerprint] = name;
    }

    hr = Save();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GivenNames::TryGet
//
////////////////////////////////////////////////////////////////////////////////

bool GivenNames::TryGet (const std::string & fingerprint, std::string & outName)
{
    HRESULT          hr    = S_OK;
    bool             found = false;
    std::lock_guard  lock (m_lock);



    outName.clear();

    BAIL_OUT_IF (fingerprint.empty(), S_OK);

    hr = Load();
    CHR (hr);

    found = m_names.contains (fingerprint);

    if (found)
    {
        outName = m_names[fingerprint];
    }

Error:
    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GivenNames::Load
//
//  With no file there is no name; a file that isn't JSON is an error, so a Set doesn't
//  write over it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GivenNames::Load()
{
    HRESULT         hr     = S_OK;
    std::ifstream   in (m_path, std::ios::binary);
    bool            isOpen = in.is_open();
    std::string     text;
    JsonValue       root;
    JsonParseError  parseError;



    m_names.clear();

    BAIL_OUT_IF (!isOpen, S_OK);

    text.assign (std::istreambuf_iterator<char> (in), std::istreambuf_iterator<char>());

    hr = JsonParser::Parse (text, root, parseError);
    CHR (hr);

    for (const auto & [fingerprint, value] : root.GetObjectEntries())
    {
        if (value.GetType() == JsonType::String)
        {
            m_names[fingerprint] = value.GetString();
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GivenNames::Save
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GivenNames::Save() const
{
    HRESULT                                         hr     = S_OK;
    std::vector<std::pair<std::string, JsonValue>>  fields;
    JsonWriter::Options                             options;
    std::string                                     text;
    std::error_code                                 error;
    std::ofstream                                   out;
    bool                                            isGood = false;



    for (const auto & [fingerprint, name] : m_names)
    {
        fields.emplace_back (fingerprint, JsonValue (name));
    }

    hr = JsonWriter::Write (JsonValue (std::move (fields)), options, text);
    CHR (hr);

    std::filesystem::create_directories (m_path.parent_path(), error);

    out.open (m_path, std::ios::binary | std::ios::trunc);
    out.write (text.data(), (std::streamsize) text.size());
    out.close();

    isGood = !out.fail();
    CBREx (isGood, HRESULT_FROM_WIN32 (ERROR_WRITE_FAULT));

Error:
    return hr;
}
