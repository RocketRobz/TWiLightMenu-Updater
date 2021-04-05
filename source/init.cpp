#include "common.hpp"
#include "init.hpp"
#include "thread.h"
#include "updaterScreen.hpp"

#include <3ds.h>
#include <dirent.h>
#include <unistd.h>

#define CONFIG_3D_SLIDERSTATE (*(float *)0x1FF81080)

static touchPosition touch;

u8 consoleModel = 0;
bool exiting = false;

C2D_SpriteSheet sprites;

Result Init::Initialize() {
	romfsInit();
	Result res = cfguInit();
	if (R_SUCCEEDED(res)) {
		CFGU_GetSystemModel(&consoleModel);
		cfguExit();
	}
	gfxInitDefault();
	(CONFIG_3D_SLIDERSTATE==0) ? gfxSetWide(consoleModel != 3) : gfxSet3D(true);
	Gui::init();
	amInit();
	acInit();
	Gui::loadSheet("romfs:/gfx/sprites.t3x", sprites);
	fadein = true;
	fadealpha = 255;
	hidSetRepeatParameters(25, 5);

	Gui::setScreen(std::make_unique<UpdaterScreen>(), false); // Set Screen to the Updater ones.
	return 0;
}

void wide3DSwap(void) {
	if (consoleModel == 3 || consoleModel == 5) return;

	if (CONFIG_3D_SLIDERSTATE==0 && gfxIs3D()) {
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		C2D_TargetClear(Top, BLACK);
		C2D_TargetClear(TopRight, BLACK);
		C3D_FrameEnd(0);

		gfxSet3D(false);
		gfxSetWide(true);
		Gui::reinit();
	} else if (CONFIG_3D_SLIDERSTATE>0 && gfxIsWide()) {
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		C2D_TargetClear(Top, BLACK);
		C3D_FrameEnd(0);

		gfxSetWide(false);
		gfxSet3D(true);
		Gui::reinit();
	}
}

Result Init::MainLoop() {
	// Initialize everything.
	Initialize();

	// Loop as long as the status is not exiting.
	while (aptMainLoop())
	{
		hidScanInput();
		u32 hHeld = hidKeysDownRepeat();
		u32 hDown = hidKeysDown();
		hidTouchRead(&touch);
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		C2D_TargetClear(Top, BLACK);
		C2D_TargetClear(TopRight, BLACK);
		C2D_TargetClear(Bottom, BLACK);
		Gui::clearTextBufs();
		Gui::DrawScreen();
		Gui::ScreenLogic(hDown, hHeld, touch, true); // Call the logic of the current screen here.
		C3D_FrameEnd(0);
		wide3DSwap();
		if (exiting) {
			if (!fadeout)	break;
		}
		Gui::fadeEffects(16, 16);
	}
	// Exit all services and exit the app.
	Exit();
	return 0;
}

Result Init::Exit() {
	Gui::exit();
	Gui::unloadSheet(sprites);
	romfsExit();
	amExit();
	acExit();
	return 0;
}