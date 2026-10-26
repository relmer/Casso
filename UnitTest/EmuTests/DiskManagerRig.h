#pragma once

#include "Pch.h"

#include "Config/UserConfigStore.h"
#include "Machines/Apple2/Common/Disk2AudioSource.h"
#include "Shell/CpuManager.h"
#include "Ui/Chrome/DriveWidget.h"
#include "Ui/DriveWidgetController.h"
#include "Ui/DriveWidgetState.h"
#include "UiTests/InMemoryFileSystem.h"
#include "WasapiAudio.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskManagerRig
//
//  Everything a DiskManager has references to, beside the machine and its
//  store. The machine name stays empty and no saved disk path is seeded, so
//  MountCommandLineDisks never runs its saved-path branch, which reads the
//  real file system.
//
////////////////////////////////////////////////////////////////////////////////

struct DiskManagerRig
{
    std::vector<std::unique_ptr<Disk2AudioSource>>  audioSources;
    WasapiAudio                                     audio;
    DriveWidgetController                           widgets;
    std::array<DriveWidgetState, 2>                 widgetState;
    std::array<DriveWidget, 2>                      chrome;
    CpuManager                                      cpuManager;
    std::wstring                                    machineName;
    UserConfigStore                                 config { L"C:\\Casso\\User" };
    InMemoryFileSystem                              fs;
    std::array<bool, 2>                             writeProtect {};
    std::string                                     workingDirectory = "C:\\work";
};