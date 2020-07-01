#include "rocketRobz.hpp"

static int subMode = 0;
static int prevSubMode = -1;
extern int delay;
static int rr_fadeAlpha = 0;
static int rr_fadeType = true;
extern u8 consoleModel;

void RocketRobz::Draw(void) const {
	fadecolor = 0;	// Always use black color for fading effects

	for (int d = 0; d <= 1; d++) {
		if (d==1 && gfxIsWide()) break;
		Gui::ScreenDraw(d==1 ? TopRight : Top);

		GFX::DrawSprite(sprites_logo_rocketrobz_idx, 0, 0, 0.5);
		Gui::DrawString(8, 218, 0.50, WHITE, this->yearText);

		if (fadealpha > 0) Gui::Draw_Rect(0, 0, 400, 240, C2D_Color32(fadecolor, fadecolor, fadecolor, fadealpha)); // Fade in/out effect
	}

	Gui::ScreenDraw(Bottom);
	if (subMode == 0) {
		GFX::DrawSprite(sprites_logo_UniversalCore_idx, 0, 26);
	} else {
		Gui::DrawStringCentered(0, 48, 0.75, WHITE, this->presentedText);
		GFX::DrawSprite(sprites_logo_horiHD_idx, 32, 85);
	}
	if (rr_fadeAlpha > 0) Gui::Draw_Rect(0, 0, 320, 240, C2D_Color32(0, 0, 0, rr_fadeAlpha)); // Fade in/out effect
	if (fadealpha > 0) Gui::Draw_Rect(0, 0, 320, 240, C2D_Color32(fadecolor, fadecolor, fadecolor, fadealpha)); // Fade in/out effect
	
	if (rr_fadeType) {
		rr_fadeAlpha -= 12;
		if (rr_fadeAlpha < 0) rr_fadeAlpha = 0;
	} else {
		rr_fadeAlpha += 12;
		if (rr_fadeAlpha > 255) rr_fadeAlpha = 255;
	}

	if (delay > 60*3 && consoleModel != 3) {
		if (prevSubMode != 0) {
			rr_fadeType = false;
		}
		if (rr_fadeAlpha == 255) {
			subMode = 1;
			rr_fadeType = true;
			prevSubMode++;
		}
	}
}


void RocketRobz::Logic(u32 hDown, u32 hHeld, touchPosition touch) { }