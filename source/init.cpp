#include "common.hpp"
#include "download.hpp"
#include "dumpdsp.h"
#include "inifile.h"
#include "init.hpp"
#include "thread.h"
#include "updaterScreen.hpp"

#include <3ds.h>
#include <dirent.h>
#include <unistd.h>

#define CONFIG_3D_SLIDERSTATE (*(float *)0x1FF81080)

static touchPosition touch;

int fadealpha = 255;
bool fadein = true;

bool updatingSelf = false;
bool updated3dsx = false;
bool exiting = false;

C2D_SpriteSheet sprites;

Result Init::Initialize() {
	amInit();
	sdmcInit();
	romfsInit();
	acInit();
	cfguInit();
	gfxInitDefault();
	Gui::init();
	Gui::loadSheet("romfs:/gfx/sprites.t3x", sprites);

	loadUsernamePassword();
	Gui::setScreen(std::make_unique<UpdaterScreen>());
	return 0;
}

Result Init::MainLoop() {
	// Initialize everything.
	Initialize();

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
			fadealpha -= 12;
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
	Gui::exit();
	Gui::unloadSheet(sprites);
	gfxExit();
	romfsExit();
	sdmcExit();
	cfguExit();
	acExit();
	amExit();
	return 0;
}