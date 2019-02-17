#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <3ds.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/stat.h>

#include "graphic.h"
#include "pp2d/pp2d.h"
#include "sound.h"
#include "dumpdsp.h"
#include "settings.h"
#include "language.h"
#include "download.hpp"
#include "inifile.h"

#define CONFIG_3D_SLIDERSTATE (*(float *)0x1FF81080)

static touchPosition touch;

bool dspfirmfound = false;
static bool musicPlaying = false;

// Music and sound effects.
sound *mus_settings = NULL;
sound *sfx_launch = NULL;
sound *sfx_select = NULL;
sound *sfx_stop = NULL;
sound *sfx_switch = NULL;
sound *sfx_wrong = NULL;
sound *sfx_back = NULL;

// 3D offsets. (0 == Left, 1 == Right)
Offset3D offset3D[2] = {0.0f, 0.0f};

struct {
	int x;
	int y;
} buttons2[] = {
	{ 129, 48},
	{ 220, 48},
	{ 129, 88},
	{ 220, 88},
	{ 129, 128},
	{ 220, 128},
	{ 129, 168},
	{ 220, 168},
};

size_t button_tex2[] = {
	extrasmallbuttontex,
	extrasmallbuttontex,
	extrasmallbuttontex,
	extrasmallbuttontex,
	extrasmallbuttontex,
	extrasmallbuttontex,
	extrasmallbuttontex,
	extrasmallbuttontex,
};

const char *button_titles2[] = {
	"Release",
	"Nightly",
	"Release",
	"Nightly",
	"Release",
	"Nightly",
	"Boxart",
	"Cheats",
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

void screenoff()
{
    gspLcdInit();\
    GSPLCD_PowerOffBacklight(GSPLCD_SCREEN_BOTH);\
    gspLcdExit();
}

void screenon()
{
    gspLcdInit();\
    GSPLCD_PowerOnBacklight(GSPLCD_SCREEN_BOTH);\
    gspLcdExit();
}

void displayBottomMsg(const char* text) {
	pp2d_begin_draw(GFX_BOTTOM, GFX_LEFT);
	pp2d_draw_texture(loadingbgtex, 0, 0);
	pp2d_draw_text(24, 32, 0.5f, 0.5f, BLACK, text);
	pp2d_end_draw();
}

void checkForUpdates(void) {
	CIniFile ini("sdmc:/_nds/TWiLightMenu/extras/updater/currentVersions.ini");
	if(ini.GetString("TWILIGHTMENU", "RELEASE", "") != getLatestRelease("RocketRobz/TWiLightMenu", "tag_name"))
		updateAvailable[0] = true;
	if(ini.GetString("TWILIGHTMENU", "NIGHTLY", "") != getLatestCommit("RocketRobz/TWiLightMenu", "sha").substr(0,7))
		updateAvailable[1] = true;

	if(ini.GetString("NDS-BOOTSTRAP", "RELEASE", "") != getLatestRelease("ahezard/nds-bootstrap", "tag_name"))
		updateAvailable[2] = true;
	if(ini.GetString("NDS-BOOTSTRAP", "NIGHTLY", "") != getLatestCommit("ahezard/nds-bootstrap", "sha").substr(0,7))
		updateAvailable[3] = true;

	if(ini.GetString("TWILIGHTMENU-UPDATER", "RELEASE", "") != getLatestRelease("RocketRobz/TWiLightMenu-Updater", "tag_name"))
		updateAvailable[4] = true;
	if(ini.GetString("TWILIGHTMENU-UPDATER", "NIGHTLY", "") != getLatestCommit("RocketRobz/TWiLightMenu-Updater", "sha").substr(0,7))
		updateAvailable[5] = true;
}

// Version numbers.
char launcher_vertext[13];

int menuSelection = 0;

int main()
{
	aptInit();
	amInit();
	sdmcInit();
	romfsInit();
	srvInit();
	hidInit();
	acInit();

	osSetSpeedupEnable(true);	// Enable speed-up for New 3DS users

	snprintf(launcher_vertext, 13, "v%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO);

	// make folders if they don't exist
	mkdir("sdmc:/3ds", 0777);	// For DSP dump
	mkdir("sdmc:/_nds", 0777);
	mkdir("sdmc:/_nds/TWiLightMenu", 0777);
	mkdir("sdmc:/_nds/TWiLightMenu/gamesettings", 0777);
	mkdir("sdmc:/_nds/TWiLightMenu/emulators", 0777);
	mkdir("sdmc:/_nds/TWiLightMenu/extras", 0777);
	mkdir("sdmc:/_nds/TWiLightMenu/extras/updater", 0777);

	pp2d_init();
	
	pp2d_set_screen_color(GFX_TOP, TRANSPARENT);
	pp2d_set_3D(1);
	
	pp2d_load_texture_png(homeicontex, "romfs:/graphics/BS_home_icon.png");
	pp2d_load_texture_png(loadingbgtex, "romfs:/graphics/BS_loading_background.png");
	pp2d_load_texture_png(topbgtex, "romfs:/graphics/top_bg.png");
	pp2d_load_texture_png(subbgtex, "romfs:/graphics/BS_background.png");
	pp2d_load_texture_png(buttontex, "romfs:/graphics/BS_1_2page_button.png");
	pp2d_load_texture_png(extrasmallbuttontex, "romfs:/graphics/BS_2page_extra_small_button.png");
	pp2d_load_texture_png(smallbuttontex, "romfs:/graphics/BS_2page_small_button.png");
	pp2d_load_texture_png(leftpagetex, "romfs:/graphics/BS_Left_page_button.png");
	pp2d_load_texture_png(rightpagetex, "romfs:/graphics/BS_Rigt_page_button.png");
	pp2d_load_texture_png(pagenumberframetex, "romfs:/graphics/BS_Page_number_frame.png");
	pp2d_load_texture_png(twinkletex1, "romfs:/graphics/twinkle_1.png");
	pp2d_load_texture_png(twinkletex2, "romfs:/graphics/twinkle_2.png");
	pp2d_load_texture_png(twinkletex3, "romfs:/graphics/twinkle_3.png");
	pp2d_load_texture_png(logotex, "romfs:/graphics/twlm_logo.png");
	pp2d_load_texture_png(arrowtex, "romfs:/graphics/arrow.png");
	pp2d_load_texture_png(updatertex, "romfs:/graphics/text_updater.png");
	pp2d_load_texture_png(bluedot, "romfs:/graphics/dot_blue.png");
	pp2d_load_texture_png(greendot, "romfs:/graphics/dot_green.png");
	
 	if( access( "sdmc:/3ds/dspfirm.cdc", F_OK ) != -1 ) {
		ndspInit();
		dspfirmfound = true;
	}else{
		pp2d_begin_draw(GFX_BOTTOM, GFX_LEFT);
		pp2d_draw_text(12, 16, 0.5f, 0.5f, WHITE, "Dumping DSP firm...");
		pp2d_end_draw();
		dumpDsp();
		if( access( "sdmc:/3ds/dspfirm.cdc", F_OK ) != -1 ) {
			ndspInit();
			dspfirmfound = true;
		} else {
			for (int i = 0; i < 90; i++) {
				pp2d_begin_draw(GFX_BOTTOM, GFX_LEFT);
				pp2d_draw_text(12, 16, 0.5f, 0.5f, WHITE, "DSP firm dumping failed.\n"
						"Running without sound.");
				pp2d_end_draw();
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

	checkForUpdates();
	
	// Loop as long as the status is not exit
	while(aptMainLoop()) {
		offset3D[0].topbg = CONFIG_3D_SLIDERSTATE * -7.0f;
		offset3D[1].topbg = CONFIG_3D_SLIDERSTATE * 7.0f;
		offset3D[0].twinkle3 = CONFIG_3D_SLIDERSTATE * -6.0f;
		offset3D[1].twinkle3 = CONFIG_3D_SLIDERSTATE * 6.0f;
		offset3D[0].twinkle2 = CONFIG_3D_SLIDERSTATE * -5.0f;
		offset3D[1].twinkle2 = CONFIG_3D_SLIDERSTATE * 5.0f;
		offset3D[0].twinkle1 = CONFIG_3D_SLIDERSTATE * -4.0f;
		offset3D[1].twinkle1 = CONFIG_3D_SLIDERSTATE * 4.0f;
		offset3D[0].updater = CONFIG_3D_SLIDERSTATE * -3.0f;
		offset3D[1].updater = CONFIG_3D_SLIDERSTATE * 3.0f;
		offset3D[0].logo = CONFIG_3D_SLIDERSTATE * -2.0f;
		offset3D[1].logo = CONFIG_3D_SLIDERSTATE * 2.0f;

		// Scan hid shared memory for input events
		hidScanInput();
		
		const u32 hDown = hidKeysDown();
		const u32 hHeld = hidKeysHeld();

		hidTouchRead(&touch);

		if (!musicPlaying && dspfirmfound) {
			mus_settings->play();
			musicPlaying = true;
		}

		for (int topfb = GFX_LEFT; topfb <= GFX_RIGHT; topfb++) {
			if (topfb == GFX_LEFT) pp2d_begin_draw(GFX_TOP, (gfx3dSide_t)topfb);
			else pp2d_draw_on(GFX_TOP, (gfx3dSide_t)topfb);
			pp2d_draw_texture(topbgtex, offset3D[topfb].topbg, 0);
			pp2d_draw_texture(twinkletex3, 133+offset3D[topfb].twinkle3, 61);
			pp2d_draw_texture(twinkletex2, 157+offset3D[topfb].twinkle2, 81);
			pp2d_draw_texture(twinkletex1, 184+offset3D[topfb].twinkle1, 107);
			pp2d_draw_texture(arrowtex, 41+offset3D[topfb].twinkle1, 25);
			pp2d_draw_texture(updatertex, 187+offset3D[topfb].updater, 151);
			pp2d_draw_texture(logotex, 127+offset3D[topfb].logo, 100);
			pp2d_draw_text(336, 222, 0.50, 0.50, WHITE, launcher_vertext);
			if (fadealpha > 0) pp2d_draw_rectangle(0, 0, 400, 240, RGBA8(0, 0, 0, fadealpha)); // Fade in/out effect
		}
		pp2d_draw_on(GFX_BOTTOM, GFX_LEFT);
		pp2d_draw_texture(subbgtex, 0, 0);
		pp2d_draw_text(6, 5, 0.55, 0.55, WHITE, "Updater menu");
		// Draw buttons
		for (int i = (int)(sizeof(buttons2)/sizeof(buttons2[0]))-1; i >= 0; i--) {
			if (menuSelection == i) {
				// Button is highlighted.
				pp2d_draw_texture(button_tex2[i], buttons2[i].x, buttons2[i].y);
			} else {
				// Button is not highlighted. Darken the texture.
				if (buttonShading) {
					pp2d_draw_texture_blend(button_tex2[i], buttons2[i].x, buttons2[i].y, GRAY);
				} else {
					pp2d_draw_texture(button_tex2[i], buttons2[i].x, buttons2[i].y);
				}
			}
			// Draw a green dot if an update is availible
			if(updateAvailable[i]) {
				pp2d_draw_texture(greendot, buttons2[i].x+75, buttons2[i].y-6);
			}

			// Determine the text height.
			// NOTE: Button texture size is 132x34.
			const int h = 32;

			// Draw the title.
			int y = buttons2[i].y + ((40 - h) / 2);
			int x_from_width = buttons2[i].x + 10;
			pp2d_draw_text(x_from_width, y, 0.75, 0.75, BLACK, button_titles2[i]);

			if(!(i%2)) {
				pp2d_draw_text(5, y, 0.7, 0.7, WHITE, row_titles2[i/2]);
			}
		}
		const wchar_t *home_text = TR(STR_RETURN_TO_HOME_MENU);
		const int home_width = pp2d_get_wtext_width(home_text, 0.50, 0.50) + 16;
		const int home_x = (320-home_width)/2;
		pp2d_draw_texture(homeicontex, home_x, 219); // Draw HOME icon
		pp2d_draw_wtext(home_x+20, 220, 0.50, 0.50, WHITE, home_text);
		if (fadealpha > 0) pp2d_draw_rectangle(0, 0, 320, 240, RGBA8(0, 0, 0, fadealpha)); // Fade in/out effect
		pp2d_end_draw();
		
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
					showReleaseInfo("RocketRobz/TWiLightMenu");
					break;
				case 1:
					if(dspfirmfound) {
						sfx_select->stop();
						sfx_select->play();
					}
					showCommitInfo("RocketRobz/TWiLightMenu");
					break;
				case 2:
					if(dspfirmfound) {
						sfx_select->stop();
						sfx_select->play();
					}
					showReleaseInfo("ahezard/nds-bootstrap");
					break;
				case 3:
					if(dspfirmfound) {
						sfx_select->stop();
						sfx_select->play();
					}
					showCommitInfo("ahezard/nds-bootstrap");
					break;
				case 4:
					if(dspfirmfound) {
						sfx_select->stop();
						sfx_select->play();
					}
					showReleaseInfo("RocketRobz/TWiLightMenu-Updater");
					break;
				case 5:
					if(dspfirmfound) {
						sfx_select->stop();
						sfx_select->play();
					}
					showCommitInfo("RocketRobz/TWiLightMenu-Updater");
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
			switch (menuSelection) {
				case 0:	// TWiLight release
					/* if(checkWifiStatus()){ */ if(1) { // For testing
						if(dspfirmfound) {
							sfx_select->stop();
							sfx_select->play();
						}
						updateTWiLight(false);
					} else {
						if(dspfirmfound) {
							sfx_wrong->stop();
							sfx_wrong->play();
						}
					}
					break;
				case 1:	// TWiLight nightly
					/* if(checkWifiStatus()){ */ if(1) { // For testing
						if(dspfirmfound) {
							sfx_select->stop();
							sfx_select->play();
						}
						updateTWiLight(true);
					} else {
						if(dspfirmfound) {
							sfx_wrong->stop();
							sfx_wrong->play();
						}
					}
					break;
				case 2:	// nds-bootstrap release
					/* if(checkWifiStatus()){ */ if(1) {
						if(dspfirmfound) {
							sfx_select->stop();
							sfx_select->play();
						}
						updateBootstrap(false);
					} else {
						if(dspfirmfound) {
							sfx_wrong->stop();
							sfx_wrong->play();
						}
					}
					break;
				case 3:	// nds-bootstrap nightly
					/* if(checkWifiStatus()){ */ if(1) {
						if(dspfirmfound) {
							sfx_select->stop();
							sfx_select->play();
						}
						updateBootstrap(true);
					} else {
						if(dspfirmfound) {
							sfx_wrong->stop();
							sfx_wrong->play();
						}
					}
					break;
				case 4:	// Updater release
					/* if(checkWifiStatus()){ */ if(1) {
						if(dspfirmfound) {
							sfx_select->stop();
							sfx_select->play();
						}
						updateSelf(false);
					} else {
						if(dspfirmfound) {
							sfx_wrong->stop();
							sfx_wrong->play();
						}
					}
					break;
				case 5:	// Updater nightly
					/* if(checkWifiStatus()){ */ if(1) {
						if(dspfirmfound) {
							sfx_select->stop();
							sfx_select->play();
						}
						updateSelf(true);
					} else {
						if(dspfirmfound) {
							sfx_wrong->stop();
							sfx_wrong->play();
						}
					}
					break;
				case 6:	// Boxart
					// /* if(checkWifiStatus()){ */ if(1) {
					// 	if(dspfirmfound) {
					// 		sfx_select->stop();
					// 		sfx_select->play();
					// 	}
					// 	updateBootstrap(false);
					// } else {
						if(dspfirmfound) {
							sfx_wrong->stop();
							sfx_wrong->play();
						}
					// }
					break;
				case 7:	// usrcheat.dat
					/* if(checkWifiStatus()){ */ if(1) {
						if(dspfirmfound) {
							sfx_select->stop();
							sfx_select->play();
						}
						updateCheats();
					} else {
						if(dspfirmfound) {
							sfx_wrong->stop();
							sfx_wrong->play();
						}
					}
					break;
				default:
					if(dspfirmfound) {
						sfx_wrong->stop();
						sfx_wrong->play();
					}
					break;
			}
			setOption = false;
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

	pp2d_exit();

	hidExit();
	srvExit();
	romfsExit();
	sdmcExit();
	aptExit();

    return 0;
}
