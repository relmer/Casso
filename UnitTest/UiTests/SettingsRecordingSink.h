#pragma once

#include "Pch.h"

#include "Ui/Settings/SettingsPanelState.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsRecordingSink
//
//  An ISettingsApplySink for the Settings model tests: each Apply call is
//  stored in a field the test reads back.
//
////////////////////////////////////////////////////////////////////////////////

class SettingsRecordingSink : public ISettingsApplySink
{
public:
    SettingsSpeedMode         lastSpeed                  = SettingsSpeedMode::Authentic;
    SettingsColorMode         lastColor                  = SettingsColorMode::Color;
    bool                      lastFloppySound            = true;
    bool                      lastFastTapeLoading        = true;
    float                     lastTapeVolume             = 1.0f;
    bool                      lastTapeAutoStop           = true;
    bool                      lastTapeIdleStop           = true;
    bool                      lastTapeEightBit           = false;
    std::string               lastMechanism;
    bool                      lastWriteProtect[2]        = { false, false };
    float                     lastDriveMotor             = -1.0f;
    float                     lastDriveHead              = -1.0f;
    float                     lastDriveDoor              = -1.0f;
    float                     lastDriveOnePan            = 0.0f;
    float                     lastDriveTwoPan            = 0.0f;
    bool                      lastExternalDriveConnected = false;
    bool                      lastMouseConnected         = true;
    int                       queuedResetCount           = 0;
    std::vector<std::string>  serialConnects;
    std::string               lastSerialSw1;
    std::string               lastSerialSw2;
    int                       applyCount                 = 0;
    NetworkCardSettings       lastNetworkCard;
    int                       networkCardCount           = 0;

    void ApplySpeedMode    (SettingsSpeedMode mode) override   { lastSpeed = mode; ++applyCount; }
    void ApplyColorMode    (SettingsColorMode mode) override   { lastColor = mode; ++applyCount; }
    void ApplyFloppySound  (bool enabled) override             { lastFloppySound = enabled; ++applyCount; }
    void ApplyFastTapeLoading (bool enabled) override          { lastFastTapeLoading = enabled; }
    void ApplyTapeVolume   (float gain) override               { lastTapeVolume = gain; }
    void ApplyTapeAutoStop (bool enabled) override             { lastTapeAutoStop = enabled; }
    void ApplyTapeIdleStop (bool enabled) override             { lastTapeIdleStop = enabled; }
    void ApplyTapeEightBit (bool enabled) override             { lastTapeEightBit = enabled; }
    void ApplyMechanism    (const std::string & m) override    { lastMechanism = m; ++applyCount; }
    void ApplyDriveVolumes (float motor, float head, float door) override
    {
        lastDriveMotor = motor;
        lastDriveHead  = head;
        lastDriveDoor  = door;
        ++applyCount;
    }

    void ApplyDrivePan     (float driveOnePan, float driveTwoPan) override
    {
        lastDriveOnePan = driveOnePan;
        lastDriveTwoPan = driveTwoPan;
        ++applyCount;
    }

    void ApplyWriteProtect (int drive, bool wp) override
    {
        if (drive >= 0 && drive < 2) lastWriteProtect[drive] = wp;
        ++applyCount;
    }

    void ApplyExternalDriveConnected (bool connected) override
    {
        lastExternalDriveConnected = connected;
        ++applyCount;
    }

    void ApplyMouseConnected (bool connected) override
    {
        lastMouseConnected = connected;
        ++applyCount;
    }

    void ApplySerialPortDevice (int port, const std::string & device, const std::string & path) override
    {
        serialConnects.push_back (path.empty() ? std::format ("{} {}", port, device) : std::format ("{} {} {}", port, device, path));
    }

    void ApplyParallelPrinter (int slot, bool attached) override
    {
        serialConnects.push_back (std::format ("parallel {} {}", slot, attached ? "on" : "off"));
    }

    void ApplySerialCardSwitches (const std::string & sw1, const std::string & sw2) override
    {
        lastSerialSw1 = sw1;
        lastSerialSw2 = sw2;
        ++applyCount;
    }

    void ApplyNetworkCard (const NetworkCardSettings & settings) override
    {
        lastNetworkCard = settings;
        ++networkCardCount;
    }

    void QueueMachineReset() override { ++queuedResetCount; }
};
