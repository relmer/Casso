#include "Pch.h"

#include "Core/MachineConfigSource.h"

#include "AssetBootstrap.h"
#include "Core/MachineConfigUpgrade.h"
#include "EmbeddedMachineConfigs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConfigSource::ChooseText
//
//  The disk text wins when its stamp is equal to or lower than the embedded
//  stamp, and when it has no stamp, which counts as lower. Text with no
//  embedded copy to compare against is used as it is.
//
////////////////////////////////////////////////////////////////////////////////

MachineConfigChoice MachineConfigSource::ChooseText (const string & diskText, const string & embeddedText)
{
    MachineConfigChoice  choice;
    bool                 hasEmbedded = !embeddedText.empty();



    choice.diskStamp     = MachineConfigUpgrade::ParseStamp (diskText);
    choice.embeddedStamp = MachineConfigUpgrade::ParseStamp (embeddedText);
    choice.useEmbedded   = hasEmbedded && choice.diskStamp > choice.embeddedStamp;
    choice.text          = choice.useEmbedded ? embeddedText : diskText;

    return choice;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConfigSource::ReadConfigText
//
//  Reads the file at `diskPath` and this build's embedded text for
//  `machineName`, and returns the text ChooseText selects. A program with no
//  machine configurations among its resources, such as Casso Explorer, has no
//  embedded text, and gets the disk text.
//
//  Fails with the error from reading the file, which leaves `outText` empty.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConfigSource::ReadConfigText (
    HINSTANCE        hInstance,
    const wstring  & machineName,
    const fs::path & diskPath,
    string         & outText)
{
    HRESULT              hr           = S_OK;
    HRESULT              hrEmbedded   = S_OK;
    string               diskText;
    string               embeddedText;
    MachineConfigChoice  choice;
    bool                 isOpen       = false;
    ifstream             file (diskPath, ios::binary);



    outText.clear();

    isOpen = file.is_open();
    CBREx (isOpen, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    diskText.assign (istreambuf_iterator<char> (file), istreambuf_iterator<char>());

    hrEmbedded = GetEmbeddedText (hInstance, machineName, embeddedText);
    IGNORE_RETURN_VALUE (hrEmbedded, S_OK);

    choice  = ChooseText (diskText, embeddedText);
    outText = choice.text;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConfigSource::GetEmbeddedText
//
//  The embedded config for `machineName` from the resources of `hInstance`.
//  Fails when this build has no config for the machine or when the module has
//  no such resource. A missing resource is an expected result in a program that
//  has none, so it isn't asserted.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConfigSource::GetEmbeddedText (
    HINSTANCE        hInstance,
    const wstring  & machineName,
    string         & outText)
{
    HRESULT                 hr       = S_OK;
    const EmbeddedConfig  * config   = nullptr;
    HRSRC                   resource = nullptr;
    HGLOBAL                 memory   = nullptr;
    DWORD                   size     = 0;
    const void            * data     = nullptr;
    bool                    hasData  = false;



    outText.clear();

    config = AssetBootstrap::FindEmbeddedConfig (machineName);
    CBREx (config != nullptr, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    resource = FindResourceW (hInstance, MAKEINTRESOURCEW (config->resourceId), RT_RCDATA);
    CWR (resource != nullptr);

    size = SizeofResource (hInstance, resource);
    CWR (size != 0);

    memory = LoadResource (hInstance, resource);
    CWR (memory != nullptr);

    data    = LockResource (memory);
    hasData = data != nullptr;
    CBREx (hasData, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    outText.assign (static_cast<const char *> (data), size);

Error:
    return hr;
}
