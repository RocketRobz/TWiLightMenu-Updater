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
//Offset3D offset3D[2] = {{0.0f}, {0.0f}};

const char *autostartvaluetext;
const char *autostarttext = "DSiMenu++";

const char *menudescription[2] = {""};
static int menudescription_width = 0;

struct {
	int x;
	int y;
} buttons[] = {
	{ 42,  52},
	{ 42,  102},
	{ 125,  152},
};

size_t button_tex[] = {
	buttontex,
	buttontex,
	smallbuttontex,
};

struct {
	int x;
	int y;
} buttons2[] = {
	{ 125,  42},
	{ 125,  82},
	{ 42,  122},
	{ 42,  162},
};

size_t button_tex2[] = {
	smallbuttontex,
	smallbuttontex,
	buttontex,
	buttontex,
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

int menuSelection = 0;
int menuPage = 0;

bool autoStartDone = false;

void bootPrep(void) {
	SaveSettings();
	if (settings.twl.rainbowLed == 1) {
		redLed();
	} else if (settings.twl.rainbowLed == 2) {
		dsGreenLed();
	} else if (settings.twl.rainbowLed == 3) {
		blueLed();
	} else if (settings.twl.rainbowLed == 4) {
		rainbowLed();
	}
	if (settings.ui.bootscreen != -1) {
		bootSplash();
		fade_whiteToBlack();
	}
}

void launchDSiMenuPP(void) {
	if (aptMainLoop()) bootPrep();
	// Launch DSiMenu++
	if (aptMainLoop()) {
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
	}
}

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
	mkdir("sdmc:/_nds/dsimenuplusplus/gamesettings", 0777);
	mkdir("sdmc:/_nds/dsimenuplusplus/emulators", 0777);

	pp2d_init();
	
	pp2d_set_screen_color(GFX_TOP, TRANSPARENT);
	pp2d_set_3D(0);
	
	Result res = 0;

	pp2d_load_texture_png(homeicontex, "romfs:/graphics/BS_home_icon.png");
	pp2d_load_texture_png(subbgtex, "romfs:/graphics/BS_background.png");
	pp2d_load_texture_png(buttontex, "romfs:/graphics/BS_1_2page_button.png");
	pp2d_load_texture_png(smallbuttontex, "romfs:/graphics/BS_2page_small_button.png");
	pp2d_load_texture_png(leftpagetex, "romfs:/graphics/BS_Left_page_button.png");
	pp2d_load_texture_png(rightpagetex, "romfs:/graphics/BS_Rigt_page_button.png");
	pp2d_load_texture_png(pagenumberframetex, "romfs:/graphics/BS_Page_number_frame.png");
	
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
	
	int fadealpha = 255;
	bool fadein = true;
	bool fadeout = false;

	bool topScreenGraphicLoaded = false;
	
	// Loop as long as the status is not exit
	while(aptMainLoop()) {
		//offset3D[0].logo = CONFIG_3D_SLIDERSTATE * -5.0f;
		//offset3D[1].logo = CONFIG_3D_SLIDERSTATE * 5.0f;
		//offset3D[0].launchertext = CONFIG_3D_SLIDERSTATE * -3.0f;
		//offset3D[1].launchertext = CONFIG_3D_SLIDERSTATE * 3.0f;

		// Scan hid shared memory for input events
		hidScanInput();
		
		const u32 hDown = hidKeysDown();
		const u32 hHeld = hidKeysHeld();

		if (!topScreenGraphicLoaded) {
			if (hHeld & KEY_UP) {
				settings.twl.appName = 0;
			} else if (hHeld & KEY_DOWN) {
				settings.twl.appName = 1;
			} else if (hHeld & KEY_LEFT) {
				settings.twl.appName = 2;
			}
			switch (settings.twl.appName) {
				case 0:
				default:
					pp2d_load_texture_png(topbgtex, "romfs:/graphics/Top screen.png");
					break;
				case 1:
					pp2d_load_texture_png(topbgtex, "romfs:/graphics/Top screen (SRLoader).png");
					break;
				case 2:
					pp2d_load_texture_png(topbgtex, "romfs:/graphics/Top screen (DSisionX).png");
					break;
			}
			topScreenGraphicLoaded = true;
		}

		if (!autoStartDone && settings.ui.autoStart && !(hHeld & KEY_SELECT)) {
			launchDSiMenuPP();
		}
		autoStartDone = true;

		const char *button_titles[] = {
			"Start DSiMenu++",
			"Start last-ran ROM",
			"",
		};

		const char *button_titles2[] = {
			"",
			"",
			"Update DSiMenu++",
			"Update nds-bootstrap",
		};

		if (settings.ui.autoStart) {
			button_titles[2] = "Yes";
		} else {
			button_titles[2] = "No";
		}

		switch (settings.ui.bootscreen) {
			case -1:
			default:
				button_titles2[0] = "Off";
				break;
			case 0:
				button_titles2[0] = "Nintendo DS";
				break;
			case 1:
				button_titles2[0] = "NDS (4:3)";
				break;
			case 2:
				button_titles2[0] = "Nintendo DSi";
				break;
			case 3:
				button_titles2[0] = "NDS (Inverted)";
				break;
			case 4:
				button_titles2[0] = "DSi (Inverted)";
				break;
		}

		switch (settings.twl.rainbowLed) {
			case 0:
			default:
				button_titles2[1] = "Off";
				break;
			case 1:
				button_titles2[1] = "Red";
				break;
			case 2:
				button_titles2[1] = "Green";
				break;
			case 3:
				button_titles2[1] = "Blue";
				break;
			case 4:
				button_titles2[1] = "Rainbow";
				break;
		}

		if (settings.twl.appName == 1) {
			button_titles[0] = "Start SRLoader";
			autostarttext = "SRLoader";
			button_titles2[2] = "Update SRLoader";
		} else if (settings.twl.appName == 2) {
			button_titles[0] = "Start DSisionX";
			autostarttext = "DSisionX";
			button_titles2[2] = "Update DSisionX";
		}

		for (int topfb = GFX_LEFT; topfb <= GFX_RIGHT; topfb++) {
			if (topfb == GFX_LEFT) pp2d_begin_draw(GFX_TOP, (gfx3dSide_t)topfb);
			else pp2d_draw_on(GFX_TOP, (gfx3dSide_t)topfb);
			pp2d_draw_texture(topbgtex, 0, 0);
			if (menuPage == 0) {
				if (menuSelection == 0) {
					if (settings.twl.appName == 0) {
						menudescription[0] = "Press  to reboot into DSiMenu++.";
					} else if (settings.twl.appName == 1) {
						menudescription[0] = "Press  to reboot into SRLoader.";
					} else if (settings.twl.appName == 2) {
						menudescription[0] = "Press  to reboot into DSisionX.";
					}
					menudescription_width = pp2d_get_text_width(menudescription[0], 0.60, 0.60);
					pp2d_draw_text((400-menudescription_width)/2, 152, 0.60, 0.60f, WHITE, menudescription[0]);
				}
				if (menuSelection == 1) {
					menudescription[0] = "Press  to reboot into the ROM";
					if (settings.twl.appName == 0) {
						menudescription[1] = "last-launched in DSiMenu++.";
					} else if (settings.twl.appName == 1) {
						menudescription[1] = "last-launched in SRLoader.";
					} else if (settings.twl.appName == 2) {
						menudescription[1] = "last-launched in DSisionX.";
					}
					menudescription_width = pp2d_get_text_width(menudescription[0], 0.60, 0.60);
					pp2d_draw_text((400-menudescription_width)/2, 144, 0.60, 0.60f, WHITE, menudescription[0]);
					menudescription_width = pp2d_get_text_width(menudescription[1], 0.60, 0.60);
					pp2d_draw_text((400-menudescription_width)/2, 162, 0.60, 0.60f, WHITE, menudescription[1]);
				}
				if (menuSelection == 2) {
					menudescription[0] = "If this is set, hold SELECT after";
					menudescription[1] = "launching this, to enter this menu.";
					menudescription_width = pp2d_get_text_width(menudescription[0], 0.60, 0.60);
					pp2d_draw_text((400-menudescription_width)/2, 144, 0.60, 0.60f, WHITE, menudescription[0]);
					menudescription_width = pp2d_get_text_width(menudescription[1], 0.60, 0.60);
					pp2d_draw_text((400-menudescription_width)/2, 162, 0.60, 0.60f, WHITE, menudescription[1]);
				}
			} else if (menuPage == 1) {
				if (menuSelection == 0) {
					menudescription[0] = "Show DS/DSi boot screen";
					if (settings.twl.appName == 0) {
						menudescription[1] = "before DSiMenu++ appears.";
					} else if (settings.twl.appName == 1) {
						menudescription[1] = "before SRLoader appears.";
					} else if (settings.twl.appName == 2) {
						menudescription[1] = "before DSisionX appears.";
					}
					menudescription_width = pp2d_get_text_width(menudescription[0], 0.60, 0.60);
					pp2d_draw_text((400-menudescription_width)/2, 144, 0.60, 0.60f, WHITE, menudescription[0]);
					menudescription_width = pp2d_get_text_width(menudescription[1], 0.60, 0.60);
					pp2d_draw_text((400-menudescription_width)/2, 162, 0.60, 0.60f, WHITE, menudescription[1]);
				}
				if (menuSelection == 1) {
					menudescription[0] = "Set a color to glow in";
					menudescription[1] = "the Notification LED.";
					menudescription_width = pp2d_get_text_width(menudescription[0], 0.60, 0.60);
					pp2d_draw_text((400-menudescription_width)/2, 144, 0.60, 0.60f, WHITE, menudescription[0]);
					menudescription_width = pp2d_get_text_width(menudescription[1], 0.60, 0.60);
					pp2d_draw_text((400-menudescription_width)/2, 162, 0.60, 0.60f, WHITE, menudescription[1]);
				}
				if (menuSelection >= 2) {
					menudescription[0] = "This feature cannot be used yet.";
					menudescription_width = pp2d_get_text_width(menudescription[0], 0.60, 0.60);
					pp2d_draw_text((400-menudescription_width)/2, 152, 0.60, 0.60f, WHITE, menudescription[0]);
				}
			}
			pp2d_draw_text(336, 222, 0.50, 0.50, WHITE, launcher_vertext);
			if (fadealpha > 0) pp2d_draw_rectangle(0, 0, 400, 240, RGBA8(0, 0, 0, fadealpha)); // Fade in/out effect
		}
		pp2d_draw_on(GFX_BOTTOM, GFX_LEFT);
		pp2d_draw_texture(subbgtex, 0, 0);
		pp2d_draw_text(6, 6, 0.55, 0.55, WHITE, "Settings: CTR-mode stuff");
		pp2d_draw_text(280, 6, 0.55, 0.55, WHITE, "1");
		pp2d_draw_text(300, 6, 0.55, 0.55, WHITE, "2");
		pp2d_draw_texture(pagenumberframetex, 276+(menuPage*20), 5);
		// Draw buttons
		if (menuPage == 0) {
			pp2d_draw_text(42, 152, 0.50, 0.50, WHITE, "Auto-start");
			pp2d_draw_text(42, 170, 0.50, 0.50, WHITE, autostarttext);
			for (int i = (int)(sizeof(buttons)/sizeof(buttons[0]))-1; i >= 0; i--) {
				if (menuSelection == i) {
					// Button is highlighted.
					pp2d_draw_texture(button_tex[i], buttons[i].x, buttons[i].y);
				} else {
					// Button is not highlighted. Darken the texture.
					pp2d_draw_texture_blend(button_tex[i], buttons[i].x, buttons[i].y, GRAY);
				}

				// Determine the text height.
				// NOTE: Button texture size is 132x34.
				const int h = 32;

				// Draw the title.
				int y = buttons[i].y + ((40 - h) / 2);
				int text_width = pp2d_get_text_width(button_titles[i], 0.75, 0.75);
				int x_from_width = (320-text_width)/2;
				if (button_tex[i] == smallbuttontex) {
					x_from_width += 40;
				}
				pp2d_draw_text(x_from_width, y, 0.75, 0.75, BLACK, button_titles[i]);

				y += 16;
			}
		} else if (menuPage == 1) {
			pp2d_draw_text(42, 52, 0.50, 0.50, WHITE, "Boot screen");
			pp2d_draw_text(42, 82, 0.50, 0.50, WHITE, "Notification");
			pp2d_draw_text(42, 100, 0.50, 0.50, WHITE, "LED color");
			for (int i = (int)(sizeof(buttons2)/sizeof(buttons2[0]))-1; i >= 0; i--) {
				if (menuSelection == i) {
					// Button is highlighted.
					pp2d_draw_texture(button_tex2[i], buttons2[i].x, buttons2[i].y);
				} else {
					// Button is not highlighted. Darken the texture.
					pp2d_draw_texture_blend(button_tex2[i], buttons2[i].x, buttons2[i].y, GRAY);
				}

				// Determine the text height.
				// NOTE: Button texture size is 132x34.
				const int h = 32;

				// Draw the title.
				int y = buttons2[i].y + ((40 - h) / 2);
				int text_width = pp2d_get_text_width(button_titles2[i], 0.75, 0.75);
				int x_from_width = (320-text_width)/2;
				if (button_tex2[i] == smallbuttontex) {
					x_from_width += 40;
				}
				pp2d_draw_text(x_from_width, y, 0.75, 0.75, BLACK, button_titles2[i]);

				y += 16;
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
		} else if (fadeout == true && menuPage == 0) {
			fadealpha += 15;
			if (fadealpha > 255) {
				fadealpha = 255;
				fadeout = false;
				if (menuSelection == 0) {
					// Launch DSiMenu++
					launchDSiMenuPP();
				} else if (menuSelection == 1) {
					// Launch last-ran ROM
					if (aptMainLoop()) bootPrep();
					if (aptMainLoop()) {
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
		}

		if (!fadeout) {
			if (hDown & KEY_UP) {
				menuSelection--;
			} else if (hDown & KEY_DOWN) {
				menuSelection++;
			}
			if (hDown & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT)) {
				if(dspfirmfound) {
					sfx_select->stop();
					sfx_select->play();
				}
			}
			
			if (menuPage == 0) {
				if (menuSelection > 2) menuSelection = 0;
				if (menuSelection < 0) menuSelection = 2;
			} else {
				if (menuSelection > 3) menuSelection = 0;
				if (menuSelection < 0) menuSelection = 3;
			}
		}

		if (hDown & KEY_A) {
			if (menuPage == 0) {
				switch (menuSelection) {
					case 0:
					case 1:
					default:
						if (!fadein) fadeout = true;
						break;
					case 2:
						settings.ui.autoStart = !settings.ui.autoStart;
						break;
				}
				if(dspfirmfound) {
					sfx_select->stop();
					sfx_select->play();
				}
			} else if (menuPage == 1) {
				switch (menuSelection) {
					case 0:
					default:
						settings.ui.bootscreen++;
						if (settings.ui.bootscreen > 4) settings.ui.bootscreen = -1;
						if(dspfirmfound) {
							sfx_select->stop();
							sfx_select->play();
						}
						break;
					case 1:
						settings.twl.rainbowLed++;
						if (settings.twl.rainbowLed > 4) settings.twl.rainbowLed = 0;
						if(dspfirmfound) {
							sfx_select->stop();
							sfx_select->play();
						}
						break;
					case 2:
					case 3:
						if(dspfirmfound) {
							sfx_wrong->stop();
							sfx_wrong->play();
						}
						break;
				}
			}
		}

		if (hDown & KEY_L) {
			menuPage--;
			if (menuPage < 0) menuPage = 1;
			menuSelection = 0;
			if(dspfirmfound) {
				sfx_switch->stop();
				sfx_switch->play();
			}
		} else if (hDown & KEY_R) {
			menuPage++;
			if (menuPage > 1) menuPage = 0;
			menuSelection = 0;
			if(dspfirmfound) {
				sfx_switch->stop();
				sfx_switch->play();
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