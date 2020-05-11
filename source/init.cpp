#include "common.hpp"
#include "download.hpp"
#include "dumpdsp.h"
#include "inifile.h"
#include "init.hpp"
#include "sound.h"
#include "thread.h"
#include "updaterScreen.hpp"

#include <3ds.h>
#include <dirent.h>
#include <unistd.h>

#define CONFIG_3D_SLIDERSTATE (*(float *)0x1FF81080)

static touchPosition touch;

int fadealpha = 255;
bool fadein = true;

bool dspfirmfound = false;
bool updatingSelf = false;
bool updated3dsx = false;
static bool musicPlaying = false;
bool exiting = false;

// Music and sound effects.
sound *mus_settings = NULL;
sound *sfx_launch = NULL;
sound *sfx_select = NULL;
sound *sfx_stop = NULL;
sound *sfx_switch = NULL;
sound *sfx_wrong = NULL;
sound *sfx_back = NULL;

C2D_SpriteSheet sprites;

void Play_Music(void) {
	if (!musicPlaying && dspfirmfound) {
		mus_settings->play();
		musicPlaying = true;
	}
}

Result Init::Initialize() {
	romfsInit();
	cfguInit();
	gfxInitDefault();
	Gui::init();
	amInit();
	acInit();
	Gui::loadSheet("romfs:/gfx/sprites.t3x", sprites);

	osSetSpeedupEnable(true);	// Enable speed-up for New 3DS users

	// make folders if they don't exist
	mkdir("sdmc:/3ds", 0777);	// For DSP dump
	mkdir("sdmc:/_nds", 0777);
	mkdir("sdmc:/_nds/TWiLightMenu", 0777);
	mkdir("sdmc:/_nds/TWiLightMenu/gamesettings", 0777);
	mkdir("sdmc:/_nds/TWiLightMenu/emulators", 0777);
	mkdir("sdmc:/_nds/TWiLightMenu/extras", 0777);
	mkdir("sdmc:/_nds/TWiLightMenu/extras/updater", 0777);

 	if (access("sdmc:/3ds/dspfirm.cdc", F_OK ) != -1 ) {
		ndspInit();
		dspfirmfound = true;
	} else{
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		Gui::ScreenDraw(Bottom);
		Gui::DrawString(12, 16, 0.5f, WHITE, "Dumping DSP firm...");
		C3D_FrameEnd(0);
		dumpDsp();
		if(access("sdmc:/3ds/dspfirm.cdc", F_OK ) != -1 ) {
			ndspInit();
			dspfirmfound = true;
		} else {
			for (int i = 0; i < 90; i++) {
				C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
				C2D_TargetClear(Bottom, TRANSPARENT); // clear Bottom Screen to avoid Text overdraw.
				Gui::ScreenDraw(Bottom);
				Gui::DrawString(12, 16, 0.5f, WHITE, "DSP firm dumping failed.\n"
						"Running without sound.");
				C3D_FrameEnd(0);
			}
		}
	}
	// Load the sound effects if DSP is available.
	if (dspfirmfound) {
		mus_settings = new sound("romfs:/sounds/settings.wav", 1, true);
		sfx_launch = new sound("romfs:/sounds/launch.wav", 2, false);
		sfx_select = new sound("romfs:/sounds/select.wav", 2, false);
		sfx_stop = new sound("romfs:/sounds/stop.wav", 2, false);
		sfx_switch = new sound("romfs:/sounds/switch.wav", 2, false);
		sfx_wrong = new sound("romfs:/sounds/wrong.wav", 2, false);
		sfx_back = new sound("romfs:/sounds/back.wav", 2, false);
	}

	loadUsernamePassword();
	Gui::setScreen(std::make_unique<UpdaterScreen>()); // Set Screen to the Updater ones.
	return 0;
}

Result Init::MainLoop() {
	// Initialize everything.
	Initialize();

	Play_Music();

	// Loop as long as the status is not exiting.
	while (aptMainLoop() && !exiting)
	{
		hidScanInput();
		u32 hHeld = hidKeysHeld();
		u32 hDown = hidKeysDown();
		hidTouchRead(&touch);
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		C2D_TargetClear(Top, BLACK);
		C2D_TargetClear(Bottom, BLACK);
		Gui::clearTextBufs();
		Gui::mainLoop(hDown, hHeld, touch);
		C3D_FrameEnd(0);

		if (fadein == true) {
			fadealpha -= 3;
			if (fadealpha < 0) {
				fadealpha = 0;
				fadein = false;
			}
		}
	}
	// Exit all services and exit the app.
	Exit();
	return 0;
}

Result Init::Exit() {
	delete mus_settings;
	delete sfx_launch;
	delete sfx_select;
	delete sfx_stop;
	delete sfx_switch;
	delete sfx_wrong;
	delete sfx_back;
	if (dspfirmfound) {
		ndspExit();
	}
	Gui::exit();
	Gui::unloadSheet(sprites);
	romfsExit();
	cfguExit();
	amExit();
	acExit();
	return 0;
}