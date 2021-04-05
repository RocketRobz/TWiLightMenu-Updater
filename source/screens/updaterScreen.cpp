#include "exiting.hpp"
#include "updaterScreen.hpp"

extern bool updatingSelf;
extern bool exiting;

UpdaterScreen::UpdaterScreen() {
}

void UpdaterScreen::Draw(void) const {
	GFX::DrawTop(true);

	Gui::ScreenDraw(Bottom);
	GFX::DrawSprite(sprites_BS_background_idx, 0, 0);
	Gui::DrawString(6, 5, 0.55, WHITE, "Discontinued");
	Gui::DrawString(8, 72, 0.70, WHITE, "TWiLight Menu++ Updater");
	Gui::DrawString(8, 92, 0.70, WHITE, "has been discontinued.");
	Gui::DrawString(8, 136, 0.70, WHITE, "Please switch to Universal-Updater");
	Gui::DrawString(8, 156, 0.70, WHITE, "for a stable updating experience.");
	Gui::DrawString(8, 218, 0.50, WHITE, "Press START to exit.");

	if (fadealpha > 0) Gui::Draw_Rect(0, 0, 400, 240, C2D_Color32(fadecolor, fadecolor, fadecolor, fadealpha)); // Fade in/out effect
}


void UpdaterScreen::Logic(u32 hDown, u32 hHeld, touchPosition touch) {
	if(hDown & KEY_START) {
		exiting = true;
		fadecolor = 0;
		Gui::setScreen(std::make_unique<Exiting>(), true);
	}
}