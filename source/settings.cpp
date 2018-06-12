#include "settings.h"
#include "inifile.h"

#include <unistd.h>
#include <string>
using std::string;
using std::wstring;

#include <3ds.h>

static CIniFile settingsini( "sdmc:/_nds/dsimenuplusplus/settings.ini" );
static CIniFile bootstrapini( "sdmc:/_nds/nds-bootstrap.ini" );

// Settings
Settings_t settings;

static Handle ptmsysmHandle = 0;

static inline Result ptmsysmInit(void)
{
    return srvGetServiceHandle(&ptmsysmHandle, "ptm:sysm");
}

static inline Result ptmsysmExit(void)
{
    return svcCloseHandle(ptmsysmHandle);
}

typedef struct
{
    u32 ani;
    u8 r[32];
    u8 g[32];
    u8 b[32];
} RGBLedPattern;

static Result ptmsysmSetInfoLedPattern(const RGBLedPattern* pattern)
{
    u32* ipc = getThreadCommandBuffer();
    ipc[0] = 0x8010640;
    memcpy(&ipc[1], pattern, 0x64);
    Result ret = svcSendSyncRequest(ptmsysmHandle);
    if(ret < 0) return ret;
    return ipc[1];
}

/**
 * Set a rainbow cycle pattern on the notification LED.
 * @return 0 on success; non-zero on error.
 */
int rainbowLed(void) {
	static const RGBLedPattern pat = {
		32,	// Number of valid entries.

		//marcus@Werkstaetiun:/media/marcus/WESTERNDIGI/dev_threedee/MCU_examples/RGB_rave$ lua graphics/colorgen.lua

		// Red
		{128, 103,  79,  57,  38,  22,  11,   3,   1,   3,  11,  22,  38,  57,  79, 103,
		 128, 153, 177, 199, 218, 234, 245, 253, 255, 253, 245, 234, 218, 199, 177, 153},

		// Green
		{238, 248, 254, 255, 251, 242, 229, 212, 192, 169, 145, 120,  95,  72,  51,  33,
		  18,   8,   2,   1,   5,  14,  27,  44,  65,  87, 111, 136, 161, 184, 205, 223},

		// Blue
		{ 18,  33,  51,  72,  95, 120, 145, 169, 192, 212, 229, 242, 251, 255, 254, 248,
		 238, 223, 205, 184, 161, 136, 111,  87,  64,  44,  27,  14,   5,   1,   2,   8},
	};

	if (ptmsysmInit() < 0)
		return -1;
	ptmsysmSetInfoLedPattern(&pat);
	ptmsysmExit();
	return 0;
}

/**
 * Set green color for notification LED
 * @return 0 on success; non-zero on error.
 */
int dsGreenLed(void) {
	RGBLedPattern pattern;
	pattern.ani = 32;	// Need to be 32 in order to be it constant

	// Set the color values to a single RGB value.
	memset(&pattern.r, (u8)0, sizeof(pattern.r));
	memset(&pattern.g, (u8)255, sizeof(pattern.g));
	memset(&pattern.b, (u8)0, sizeof(pattern.b));

	if (ptmsysmInit() < 0)
		return -1;
	ptmsysmSetInfoLedPattern(&pattern);
	ptmsysmExit();
	return 0;
}

void LoadSettings(void) {
	// UI settings.
	settings.ui.bootscreen = settingsini.GetInt("CTR-SETTINGS", "BOOT_ANIMATION", 0);

	// TWL settings.
	settings.twl.consoleModel = settingsini.GetInt("SRLOADER", "CONSOLE_MODEL", 0);
	settings.twl.rainbowLed = settingsini.GetInt("CTR-SETTINGS", "NOTIFICATION_LED", 0);
}

/**
 * Save settings.
 */
void SaveSettings(void) {
	bool isNew = 0;
	APT_CheckNew3DS(&isNew);
	if (isNew) {
		settings.twl.consoleModel = 3;
	} else {
		settings.twl.consoleModel = 2;
	}

	// UI settings.
	settingsini.SetInt("CTR-SETTINGS", "BOOT_ANIMATION", settings.ui.bootscreen);

	// TWL settings.
	settingsini.SetInt("SRLOADER", "CONSOLE_MODEL", settings.twl.consoleModel);
	settingsini.SetInt("CTR-SETTINGS", "NOTIFICATION_LED", settings.twl.rainbowLed);
	settingsini.SaveIniFile("sdmc:/_nds/dsimenuplusplus/settings.ini");

	bootstrapini.SetInt("NDS-BOOTSTRAP", "CONSOLE_MODEL", settings.twl.consoleModel);
	bootstrapini.SaveIniFile("sdmc:/_nds/nds-bootstrap.ini");
}
