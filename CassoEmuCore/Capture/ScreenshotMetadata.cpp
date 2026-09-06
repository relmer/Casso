#include "Pch.h"

#include "Capture/ScreenshotMetadata.h"




// PNG-registered keywords, carrying their registered meanings.
static constexpr char  s_kKeySoftware[]     = "Software";
static constexpr char  s_kKeySource[]       = "Source";
static constexpr char  s_kKeyCreationTime[] = "Creation Time";

// Casso's own, prefixed so they cannot collide with a keyword the format
// registers later.
static constexpr char  s_kKeyCapture[]      = "Casso Capture";
static constexpr char  s_kKeyMonitor[]      = "Casso Monitor";
static constexpr char  s_kKeyScenePose[]    = "Casso Scene Pose";
static constexpr char  s_kKeyCrt[]          = "Casso CRT";

// RFC 1123 spells both in English regardless of the machine's locale, so
// these are tables rather than anything locale-aware.
static constexpr const char *  s_kpszDays[]   = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static constexpr const char *  s_kpszMonths[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                                  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

static constexpr int   s_kMinutesPerHour = 60;





////////////////////////////////////////////////////////////////////////////////
//
//  FormatFloat
//
//  Fixed decimals, so a column of CRT values lines up and two captures of the
//  same settings produce byte-identical text. std::format's default would
//  print 1 for 1.0f and 0.5 for 0.5f, which reads as noise in a list.
//
////////////////////////////////////////////////////////////////////////////////

string ScreenshotMetadata::FormatFloat (float value, int decimals)
{
    return std::vformat (decimals == 1 ? "{:.1f}" : "{:.2f}", std::make_format_args (value));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatCreationTime
//
//  RFC 1123, which is what the PNG specification names for Creation Time.
//
//  The offset is signed and expressed as +HHMM: a negative offset west of
//  Greenwich must keep its sign on the HOUR while the minutes stay positive,
//  which is the part that goes wrong when this is written with a bare
//  division.
//
////////////////////////////////////////////////////////////////////////////////

string ScreenshotMetadata::FormatCreationTime (const SYSTEMTIME & when, int utcOffsetMinutes)
{
    const char *  day     = "Sun";
    const char *  month   = "Jan";
    int           total   = utcOffsetMinutes;
    char          sign    = '+';
    int           hours   = 0;
    int           minutes = 0;



    if (when.wDayOfWeek < std::size (s_kpszDays))
    {
        day = s_kpszDays[when.wDayOfWeek];
    }

    if (when.wMonth >= 1 && when.wMonth <= std::size (s_kpszMonths))
    {
        month = s_kpszMonths[when.wMonth - 1];
    }

    if (total < 0)
    {
        sign  = '-';
        total = -total;
    }

    hours   = total / s_kMinutesPerHour;
    minutes = total % s_kMinutesPerHour;

    return std::format ("{}, {:02} {} {:04} {:02}:{:02}:{:02} {}{:02}{:02}",
                        day, (int) when.wDay, month, (int) when.wYear,
                        (int) when.wHour, (int) when.wMinute, (int) when.wSecond,
                        sign, hours, minutes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatCrtParams
//
//  The effect settings that produced the image, in one line.
//
//  Present for the same reason as the scene pose: "the effects render wrong"
//  is the bug class the processed modes exist to report, and without the
//  numbers a reader cannot tell a shader fault from a slider set oddly.
//
//  Bloom carries both of its numbers as strength/radius, because either alone
//  says little about what the halation actually looked like.
//
////////////////////////////////////////////////////////////////////////////////

string ScreenshotMetadata::FormatCrtParams (const CaptureCrtParams & crt)
{
    return "brightness "  + FormatFloat (crt.brightness, 2)
         + "  contrast "  + FormatFloat (crt.contrast, 2)
         + "  gamma "     + FormatFloat (crt.gamma, 2)
         + "  scanlines " + FormatFloat (crt.scanlineIntensity, 2)
         + "  bloom "     + FormatFloat (crt.bloomStrength, 2)
         + "/"            + FormatFloat (crt.bloomRadius, 2)
         + "  bleed "     + FormatFloat (crt.colorBleedWidth, 2)
         + "  persistence " + FormatFloat (crt.persistence, 2);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatScenePose
//
//  One spelling of the pose, for the readout on the picture and the entry in
//  the file both.
//
//  The precisions are the readout's own and are kept: a tenth of a degree is
//  the finest orbit step a drag produces, and three decimals of pan is what
//  distinguishes two positions that look the same but frame differently.
//
////////////////////////////////////////////////////////////////////////////////

string ScreenshotMetadata::FormatScenePose (float yawRad, float pitchRad,
                                            float zoom, float panX, float panY)
{
    float   yawDeg   = yawRad   * 180.0f / (float) std::numbers::pi;
    float   pitchDeg = pitchRad * 180.0f / (float) std::numbers::pi;



    return std::format ("yaw {:.1f}  pitch {:.1f}  zoom {:.2f}  pan {:.3f} {:.3f}",
                        yawDeg, pitchDeg, zoom, panX, panY);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Compose
//
//  The contract's entry set for this mode, in the contract's order.
//
//  Three entries are unconditional -- who made the file, what machine it came
//  from, and when. Two more describe the setup: which mode, and which monitor
//  under which color mode. The last two are conditional and the conditions
//  are the whole point:
//
//    Scene Pose  scene captures only. The other modes have no scene, and a
//                pose recorded against a picture would be a fact about
//                nothing.
//    CRT         the two processed modes. Raw had no CRT parameters applied,
//                so emitting them would assert something false about the
//                image the reader is holding.
//
//  An empty pose is skipped even in scene mode: the desk scene may not have
//  composed yet, and a blank value is worse than an absent entry.
//
////////////////////////////////////////////////////////////////////////////////

vector<MetadataEntry> ScreenshotMetadata::Compose (const ScreenshotFacts & facts)
{
    vector<MetadataEntry>   entries;



    entries.push_back ({ s_kKeySoftware,     facts.versionString });
    entries.push_back ({ s_kKeySource,       facts.machineDisplayName });
    entries.push_back ({ s_kKeyCreationTime, FormatCreationTime (facts.when, facts.utcOffsetMinutes) });
    entries.push_back ({ s_kKeyCapture,      ScreenshotModeToken::Format (facts.mode) });
    entries.push_back ({ s_kKeyMonitor,      facts.monitorKey });

    if (facts.mode == ScreenshotMode::Scene && !facts.scenePose.empty())
    {
        entries.push_back ({ s_kKeyScenePose, facts.scenePose });
    }

    if (facts.mode != ScreenshotMode::Raw)
    {
        entries.push_back ({ s_kKeyCrt, FormatCrtParams (facts.crt) });
    }

    return entries;
}
