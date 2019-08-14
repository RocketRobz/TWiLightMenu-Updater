#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <3ds.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/stat.h>

#include "download.hpp"
#include "dumpdsp.h"
#include "gui.hpp"
#include "inifile.h"
#include "sound.h"
#include "thread.h"

#define CONFIG_3D_SLIDERSTATE (*(float *)0x1FF81080)

static touchPosition touch;

bool dspfirmfound = false;
bool updatingSelf = false;
bool updated3dsx = false;
static bool musicPlaying = false;
extern C3D_RenderTarget* top;
extern C3D_RenderTarget* bottom;

// Music and sound effects.
sound *mus_settings = NULL;
sound *sfx_launch = NULL;
sound *sfx_select = NULL;
sound *sfx_stop = NULL;
sound *sfx_switch = NULL;
sound *sfx_wrong = NULL;
sound *sfx_back = NULL;

// 3D offsets. (0 == Left, 1 == Right)
struct _Offset3D {
	float topbg;
	float twinkle3;
	float twinkle2;
	float twinkle1;
	float updater;
	float logo;
} offset3D[2] = {0.0f, 0.0f};

struct {
	int x;
	int y;
} buttons2[] = { { 129, 48}, { 220, 48}, { 129, 88}, { 220, 88}, { 129, 128}, { 220, 128}, { 129, 168}, { 220, 168},
};

size_t button_tex2[] = {
	sprites_BS_2page_extra_small_button_idx,
	sprites_BS_2page_extra_small_button_idx,
	sprites_BS_2page_extra_small_button_idx,
	sprites_BS_2page_extra_small_button_idx,
	sprites_BS_2page_extra_small_button_idx,
	sprites_BS_2page_extra_small_button_idx,
	sprites_BS_2page_extra_small_button_idx,
	sprites_BS_2page_extra_small_button_idx,
};

const char *button_titles2[] = {
	"Release",
	"Nightly",
	"Release",
	"Nightly",
	"Release",
	"Nightly",
	"Cheats",
	"Extras",
};

const int title_spacing[] = {
	6,
	10,
	6,
	10,
	6,
	10,
	10,
	17,
};

const char *row_titles2[] = {
	"TWL Menu++",
	"nds-bootstrap",
	"Updater",
	"Downloads",
};

bool updateAvailable[] = {
	false,
	false,
	false,
	false,
	false,
	false,
	false,
	false,
};

static void Play_Music(void) {
	if (!musicPlaying && dspfirmfound) {
		mus_settings->play();
		musicPlaying = true;
	}
}

int menuSelection = 0;

int main() {
	aptInit();
	amInit();
	sdmcInit();
	romfsInit();
	srvInit();
	hidInit();
	acInit();
	cfguInit();
	gfxInitDefault();

	osSetSpeedupEnable(true);	// Enable speed-up for New 3DS users

	// make folders if they don't exist
	mkdir("sdmc:/3ds", 0777);	// For DSP dump
	mkdir("sdmc:/_nds", 0777);
	mkdir("sdmc:/_nds/TWiLightMenu", 0777);
	mkdir("sdmc:/_nds/TWiLightMenu/gamesettings", 0777);
	mkdir("sdmc:/_nds/TWiLightMenu/emulators", 0777);
	mkdir("sdmc:/_nds/TWiLightMenu/extras", 0777);
	mkdir("sdmc:/_nds/TWiLightMenu/extras/updater", 0777);

	Gui::init();

 	if( access( "sdmc:/3ds/dspfirm.cdc", F_OK ) != -1 ) {
		ndspInit();
		dspfirmfound = true;
	}else{
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		set_screen(bottom);
		Draw_Text(12, 16, 0.5f, WHITE, "Dumping DSP firm...");
		Draw_EndFrame();
		dumpDsp();
		if( access( "sdmc:/3ds/dspfirm.cdc", F_OK ) != -1 ) {
			ndspInit();
			dspfirmfound = true;
		} else {
			for (int i = 0; i < 90; i++) {
				C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
				set_screen(bottom);
				Draw_Text(12, 16, 0.5f, WHITE, "DSP firm dumping failed.\n"
						"Running without sound.");
				Draw_EndFrame();
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

	bool buttonShading = false;
	bool setOption = false;
	bool showMessage = false;

	int fadealpha = 255;
	bool fadein = true;

	loadUsernamePassword();
	if(checkWifiStatus()) {
		checkForUpdates();
	}

	// Loop as long as the status is not exit
	createThread((ThreadFunc)Play_Music);
	while(aptMainLoop()) {

		// Scan hid shared memory for input events
		hidScanInput();

		const u32 hDown = hidKeysDown();

		hidTouchRead(&touch);

			C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
			C2D_TargetClear(top, TRANSPARENT);
			C2D_TargetClear(bottom, TRANSPARENT);
			Gui::clearTextBufs();
			set_screen(top);
			Gui::sprite(sprites_top_bg_idx, 0, 0);
			Gui::sprite(sprites_twinkle_3_idx, 133, 61);
			Gui::sprite(sprites_twinkle_2_idx, 157, 81);
			Gui::sprite(sprites_twinkle_1_idx, 184, 107);
			Gui::sprite(sprites_arrow_idx, 41, 25);
			Gui::sprite(sprites_text_updater_idx, 187, 151);
			Gui::sprite(sprites_twlm_logo_idx, 127, 100);
			Draw_Text(336, 222, 0.50, WHITE, VERSION_STRING);
			if (fadealpha > 0) Draw_Rect(0, 0, 400, 240, RGBA8(0, 0, 0, fadealpha)); // Fade in/out effect

		set_screen(bottom);
		Gui::sprite(sprites_BS_background_idx, 0, 0);
		Draw_Text(6, 5, 0.55, WHITE, "Updater menu");
		// Draw buttons
		for (int i = (int)(sizeof(buttons2)/sizeof(buttons2[0]))-1; i >= 0; i--) {
			if (menuSelection == i) {
				// Button is highlighted.
				Gui::sprite(button_tex2[i], buttons2[i].x, buttons2[i].y);
			} else {
				// Button is not highlighted. Darken the texture.
				if (buttonShading) {
					Gui::Draw_ImageBlend(button_tex2[i], buttons2[i].x, buttons2[i].y, GRAY);
				} else {
					Gui::sprite(button_tex2[i], buttons2[i].x, buttons2[i].y);
				}
			}
			// Draw a green dot if an update is availible
			if(updateAvailable[i]) {
				Gui::sprite(sprites_dot_green_idx, buttons2[i].x+75, buttons2[i].y-6);
			}

			// Determine the text height.
			// NOTE: Button texture size is 132x34.
			const int h = 32;

			// Draw the title.
			int y = buttons2[i].y + ((40 - h) / 2);
			int x_from_width = buttons2[i].x + title_spacing[i];
			Draw_Text(x_from_width, y, 0.75, BLACK, button_titles2[i]);

			if(!(i%2)) {
				Draw_Text(0, y, 0.60, WHITE, row_titles2[i/2]);
			}
		}
		//const wchar_t *home_text = TR(STR_RETURN_TO_HOME_MENU);
		//const int home_width = pp2d_get_wtext_width(home_text, 0.50, 0.50) + 16;
		//const int home_x = (320-home_width)/2;
		//pp2d_draw_texture(homeicontex, home_x, 219); // Draw HOME icon
		//pp2d_draw_wtext(home_x+20, 220, 0.50, 0.50, WHITE, home_text);
		if (fadealpha > 0) Draw_Rect(0, 0, 320, 240, RGBA8(0, 0, 0, fadealpha)); // Fade in/out effect
		Draw_EndFrame();

		if (fadein == true) {
			fadealpha -= 15;
			if (fadealpha < 0) {
				fadealpha = 0;
				fadein = false;
			}
		}

		if (hDown & KEY_UP) {
			if (buttonShading) menuSelection -= 2;
		} else if (hDown & KEY_DOWN) {
			if (buttonShading) menuSelection += 2;
		} else if (hDown & KEY_LEFT) {
			if (buttonShading && menuSelection%2) menuSelection--;
		} else if (hDown & KEY_RIGHT) {
			if (buttonShading && !(menuSelection%2)) menuSelection++;
		}
		if (hDown & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT)) {
			buttonShading = true;
			if(dspfirmfound) {
				sfx_select->stop();
				sfx_select->play();
			}
		}

		if (hDown & KEY_TOUCH) {
			buttonShading = false;
		}

		if (menuSelection > 8) menuSelection = 1;
		else if (menuSelection > 7) menuSelection = 0;
		else if (menuSelection < -1) menuSelection = 6;
		else if (menuSelection < 0) menuSelection = 7;

		if (hDown & KEY_A) {
			setOption = true;
		}

		if (hDown & KEY_TOUCH) {
			for (int i = (int)(sizeof(buttons2)/sizeof(buttons2[0]))-1; i >= 0; i--) {
				if(updateAvailable[i]){
					if (touch.py >= (buttons2[i].y-6) && touch.py <= (buttons2[i].y+10) && touch.px >= (buttons2[i].x+75) && touch.px <= (buttons2[i].x+91)) {
						menuSelection = i;
						showMessage = true;
					}
				}
			}
			if(!showMessage) {
				for (int i = (int)(sizeof(buttons2)/sizeof(buttons2[0]))-1; i >= 0; i--) {
					if (touch.py >= buttons2[i].y && touch.py <= (buttons2[i].y+33) && touch.px >= buttons2[i].x && touch.px <= (buttons2[i].x+87)) {
						menuSelection = i;
						setOption = true;
					}
				}
			}
		}

		if (hDown & KEY_Y || showMessage) {
			switch (menuSelection)
			{
				case 0:
					if(dspfirmfound) {
						sfx_select->stop();
						sfx_select->play();
					}
					showReleaseInfo("DS-Homebrew/TWiLightMenu", false);
					break;
				case 1:
					if(dspfirmfound) {
						sfx_select->stop();
						sfx_select->play();
					}
					chooseCommit("TWLBot/Builds", "TWiLightMenu |", false);
					break;
				case 2:
					if(dspfirmfound) {
						sfx_select->stop();
						sfx_select->play();
					}
					showReleaseInfo("ahezard/nds-bootstrap", false);
					break;
				case 3:
					if(dspfirmfound) {
						sfx_select->stop();
						sfx_select->play();
					}
					chooseCommit("TWLBot/Builds", "nds-bootstrap |", false);
					break;
				case 4:
					if(dspfirmfound) {
						sfx_select->stop();
						sfx_select->play();
					}
					showReleaseInfo("RocketRobz/TWiLightMenu-Updater", false);
					break;
				case 5:
					if(dspfirmfound) {
						sfx_select->stop();
						sfx_select->play();
					}
					chooseCommit("TWLBot/Builds", "TWiLightMenu-Updater |", false);
					break;
				default:
					if(dspfirmfound) {
						sfx_wrong->stop();
						sfx_wrong->play();
					}
					break;
			}
			showMessage = false;
		}

		if (setOption) {
			if(checkWifiStatus()) {
				std::string commit;
				switch (menuSelection) {
					case 0:	// TWiLight release
						if(dspfirmfound) {
							sfx_select->stop();
							sfx_select->play();
						}
						if(showReleaseInfo("DS-Homebrew/TWiLightMenu", true))
							updateTWiLight("");
						break;
					case 1:	// TWiLight nightly
						if(dspfirmfound) {
							sfx_select->stop();
							sfx_select->play();
						}
						if((commit = chooseCommit("TWLBot/Builds", "TWiLightMenu |", true)) != "")
							updateTWiLight(commit);
						break;
					case 2:	// nds-bootstrap release
						if(dspfirmfound) {
							sfx_select->stop();
							sfx_select->play();
						}
						if(showReleaseInfo("ahezard/nds-bootstrap", true))
							updateBootstrap("");
						break;
					case 3:	// nds-bootstrap nightly
						if(dspfirmfound) {
							sfx_select->stop();
							sfx_select->play();
						}
						if((commit = chooseCommit("TWLBot/Builds", "nds-bootstrap |", true)) != "")
							updateBootstrap(commit);
						break;
					case 4:	// Updater release
						if(dspfirmfound) {
							sfx_select->stop();
							sfx_select->play();
						}
						if(showReleaseInfo("RocketRobz/TWiLightMenu-Updater", true)) {
							updatingSelf = true;
							updateSelf("");
							updatingSelf = false;
						}
						break;
					case 5:	// Updater nightly
						if(dspfirmfound) {
							sfx_select->stop();
							sfx_select->play();
						}
						if((commit = chooseCommit("TWLBot/Builds", "TWiLightMenu-Updater |", true)) != "") {
							updatingSelf = true;
							updateSelf(commit);
							updatingSelf = false;
						}
						break;
					case 6:	// Cheats
						if(dspfirmfound) {
							sfx_select->stop();
							sfx_select->play();
						}
						updateCheats();
						break;
					case 7:	// Extras
						if(dspfirmfound) {
							sfx_select->stop();
							sfx_select->play();
						}
						downloadExtras();
						break;
					default:
						if(dspfirmfound) {
							sfx_wrong->stop();
							sfx_wrong->play();
						}
						break;
				}
			} else {
				if(dspfirmfound) {
					sfx_wrong->stop();
					sfx_wrong->play();
				}
				notConnectedMsg();
			}
			setOption = false;
		}
		if(hDown & KEY_START || updated3dsx) {
			break;
		}
	}


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

	hidExit();
	srvExit();
	romfsExit();
	sdmcExit();
	cfguExit();
	aptExit();

	return 0;
}
