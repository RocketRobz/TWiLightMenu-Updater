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
#include "dsbootsplash.h"
#include "language.h"

#define CONFIG_3D_SLIDERSTATE (*(float *)0x1FF81080)

bool dspfirmfound = false;

// Sound effects.
sound *sfx_launch = NULL;
sound *sfx_select = NULL;
sound *sfx_stop = NULL;
sound *sfx_switch = NULL;
sound *sfx_wrong = NULL;
sound *sfx_back = NULL;

// 3D offsets. (0 == Left, 1 == Right)
Offset3D offset3D[2] = {{0.0f}, {0.0f}};

const char *bootscreenvaluetext;
const char *rainbowledvaluetext;

struct {
	int x;
	int y;
} buttons[] = {
	{ 17,  39},
	{169,  39},
	{ 17,  87},
	{169,  87},
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

// Version numbers.
char launcher_vertext[13];

int main()
{
	aptInit();
	amInit();
	sdmcInit();
	romfsInit();
	srvInit();
	hidInit();

	snprintf(launcher_vertext, 13, "v%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO);

	// make folders if they don't exist
	mkdir("sdmc:/3ds", 0777);	// For DSP dump
	mkdir("sdmc:/_nds", 0777);
	mkdir("sdmc:/_nds/dsimenuplusplus", 0777);
	mkdir("sdmc:/_nds/dsimenuplusplus/emulators", 0777);

	pp2d_init();
	
	pp2d_set_screen_color(GFX_TOP, TRANSPARENT);
	pp2d_set_3D(1);
	
	Result res = 0;

	pp2d_load_texture_png(homeicontex, "romfs:/graphics/homeicon.png");
	pp2d_load_texture_png(topbgtex, "romfs:/graphics/top_bg.png");
	pp2d_load_texture_png(subbgtex, "romfs:/graphics/sub_bg.png");
	pp2d_load_texture_png(logotex, "romfs:/graphics/logo.png");
	pp2d_load_texture_png(buttontex, "romfs:/graphics/button.png");
	
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
		sfx_launch = new sound("romfs:/sounds/launch.wav", 2, false);
		sfx_select = new sound("romfs:/sounds/select.wav", 2, false);
		sfx_stop = new sound("romfs:/sounds/stop.wav", 2, false);
		sfx_switch = new sound("romfs:/sounds/switch.wav", 2, false);
		sfx_wrong = new sound("romfs:/sounds/wrong.wav", 2, false);
		sfx_back = new sound("romfs:/sounds/back.wav", 2, false);
	}
	
	LoadSettings();

	int menuSelection = 0;
	
	int fadealpha = 255;
	bool fadein = true;
	bool fadeout = false;
	
	// Loop as long as the status is not exit
	while(aptMainLoop()) {
		offset3D[0].logo = CONFIG_3D_SLIDERSTATE * -5.0f;
		offset3D[1].logo = CONFIG_3D_SLIDERSTATE * 5.0f;
		//offset3D[0].launchertext = CONFIG_3D_SLIDERSTATE * -3.0f;
		//offset3D[1].launchertext = CONFIG_3D_SLIDERSTATE * 3.0f;

		switch (settings.ui.bootscreen) {
			case -1:
			default:
				bootscreenvaluetext = "Off";
				break;
			case 0:
				bootscreenvaluetext = "Nintendo DS";
				break;
			case 1:
				bootscreenvaluetext = "Nintendo DS (4:3)";
				break;
			case 2:
				bootscreenvaluetext = "Nintendo DSi";
				break;
		}

		switch (settings.twl.rainbowLed) {
			case 0:
			default:
				rainbowledvaluetext = "color: Off";
				break;
			case 1:
				rainbowledvaluetext = "color: Green";
				break;
			case 2:
				rainbowledvaluetext = "color: Rainbow";
				break;
		}

		const char *button_titles[] = {
			"Start DSiMenu++",
			"Start last-ran ROM",
			"Boot screen",
			"Notification LED",
			"Update DSiMenu++",
		};
		const char *button_desc[] = {
			NULL,
			NULL,
			bootscreenvaluetext,
			rainbowledvaluetext,
		};

		// Scan hid shared memory for input events
		hidScanInput();
		
		const u32 hDown = hidKeysDown();
		const u32 hHeld = hidKeysHeld();

		for (int topfb = GFX_LEFT; topfb <= GFX_RIGHT; topfb++) {
			if (topfb == GFX_LEFT) pp2d_begin_draw(GFX_TOP, (gfx3dSide_t)topfb);
			else pp2d_draw_on(GFX_TOP, (gfx3dSide_t)topfb);
			pp2d_draw_texture(topbgtex, 0, 0);
			pp2d_draw_texture(logotex, offset3D[topfb].logo+400/2 - 256/2, 240/2 - 128/2);
			pp2d_draw_text(offset3D[topfb].logo+272, 138, 0.50, 0.50, BLACK, launcher_vertext);
			if (fadealpha > 0) pp2d_draw_rectangle(0, 0, 400, 240, RGBA8(0, 0, 0, fadealpha)); // Fade in/out effect
		}
		pp2d_draw_on(GFX_BOTTOM, GFX_LEFT);
		pp2d_draw_texture(subbgtex, 0, 0);
		pp2d_draw_text(2, 2, 0.75, 0.75, BLACK, "Settings: CTR-mode stuff");
		// Draw buttons
		for (int i = (int)(sizeof(buttons)/sizeof(buttons[0]))-1; i >= 0; i--) {
			if (menuSelection == i) {
				// Button is highlighted.
				pp2d_draw_texture(buttontex, buttons[i].x, buttons[i].y);
			} else {
				// Button is not highlighted. Darken the texture.
				pp2d_draw_texture_blend(buttontex, buttons[i].x, buttons[i].y, GRAY);
			}

			// Determine the text height.
			// NOTE: Button texture size is 132x34.
			const int h = 32;

			// Draw the title.
			int y = buttons[i].y + ((34 - h) / 2);
			int w = 0;
			int x = ((2 - w) / 2) + buttons[i].x;
			pp2d_draw_text(x, y, 0.50, 0.50, BLACK, button_titles[i]);

			y += 16;

			// Draw the value.
			w = 0;
			x = ((2 - w) / 2) + buttons[i].x;
			pp2d_draw_text(x, y, 0.50, 0.50, BLACK, button_desc[i]);
		}
		if (menuSelection == 0) {
			pp2d_draw_text(8, 184, 0.60, 0.60f, BLACK, "Press  to reboot into DSiMenu++.");
		}
		if (menuSelection == 1) {
			pp2d_draw_text(8, 184, 0.60, 0.60f, BLACK, "Press  to reboot into the ROM");
			pp2d_draw_text(8, 198, 0.60, 0.60f, BLACK, "last-launched in DSiMenu++.");
		}
		if (menuSelection == 2) {
			pp2d_draw_text(8, 184, 0.60, 0.60f, BLACK, "Show DS/DSi boot screen");
			pp2d_draw_text(8, 198, 0.60, 0.60f, BLACK, "before DSiMenu++ appears.");
		}
		if (menuSelection == 3) {
			pp2d_draw_text(8, 184, 0.60, 0.60f, BLACK, "Set a color to glow in");
			pp2d_draw_text(8, 198, 0.60, 0.60f, BLACK, "the Notification LED.");
		}
		const wchar_t *home_text = TR(STR_RETURN_TO_HOME_MENU);
		const int home_width = pp2d_get_wtext_width(home_text, 0.50, 0.50) + 16;
		const int home_x = (320-home_width)/2;
		pp2d_draw_texture(homeicontex, home_x, 219); // Draw HOME icon
		pp2d_draw_wtext(home_x+20, 220, 0.50, 0.50, BLACK, home_text);
		if (fadealpha > 0) pp2d_draw_rectangle(0, 0, 320, 240, RGBA8(0, 0, 0, fadealpha)); // Fade in/out effect
		pp2d_end_draw();
		
		if (fadein == true) {
			fadealpha -= 15;
			if (fadealpha < 0) {
				fadealpha = 0;
				fadein = false;
			}
		} else if (fadeout == true) {
			fadealpha += 15;
			if (fadealpha > 255) {
				fadealpha = 255;
				fadeout = false;
				SaveSettings();
				if (settings.twl.rainbowLed == 1) {
					dsGreenLed();
				} else if (settings.twl.rainbowLed == 2) {
					rainbowLed();
				}
				if (settings.ui.bootscreen != -1) {
					bootSplash();
					fade_whiteToBlack();
				}
				if (menuSelection == 0 && aptMainLoop()) {
					// Launch DSiMenu++
					while(1) {
						// Buffers for APT_DoApplicationJump().
						u8 param[0x300];
						u8 hmac[0x20];
						// Clear both buffers
						memset(param, 0, sizeof(param));
						memset(hmac, 0, sizeof(hmac));

						APT_PrepareToDoApplicationJump(0, 0x0004801553524C41ULL, MEDIATYPE_NAND);
						// Tell APT to trigger the app launch and set the status of this app to exit
						APT_DoApplicationJump(param, sizeof(param), hmac);
					}
				} else if (menuSelection == 1 && aptMainLoop()) {
					// Launch last-ran ROM
					while(1) {
						// Buffers for APT_DoApplicationJump().
						u8 param[0x300];
						u8 hmac[0x20];
						// Clear both buffers
						memset(param, 0, sizeof(param));
						memset(hmac, 0, sizeof(hmac));

						APT_PrepareToDoApplicationJump(0, 0x00048015534C524EULL, MEDIATYPE_NAND);
						// Tell APT to trigger the app launch and set the status of this app to exit
						APT_DoApplicationJump(param, sizeof(param), hmac);
					}
				}
			}
		}

		if (!fadeout) {
			if (hDown & KEY_UP) {
				menuSelection -= 2;
			} else if (hDown & KEY_DOWN) {
				menuSelection += 2;
			} else if (hDown & KEY_LEFT) {
				if (menuSelection == 1 || menuSelection == 3) menuSelection--;
			} else if (hDown & KEY_RIGHT) {
				if (menuSelection == 0 || menuSelection == 2) menuSelection++;
			}
			if (hDown & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT)) {
				if(dspfirmfound) {
					sfx_select->stop();
					sfx_select->play();
				}
			}
			
			if (menuSelection > 3) menuSelection = 0;
			if (menuSelection < 0) menuSelection = 3;
		}
		
		if (hDown & KEY_A) {
			switch (menuSelection) {
				case 0:
				case 1:
				default:
					if (!fadein) fadeout = true;
					break;
				case 2:
					settings.ui.bootscreen++;
					if (settings.ui.bootscreen > 2) settings.ui.bootscreen = -1;
					break;
				case 3:
					settings.twl.rainbowLed++;
					if (settings.twl.rainbowLed > 2) settings.twl.rainbowLed = 0;
					break;
			}
			if(dspfirmfound) {
				sfx_select->stop();
				sfx_select->play();
			}
		}
	}

	
	SaveSettings();

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
