#include "common.hpp"
#include "exiting.hpp"

void Exiting::Draw(void) const {
	GFX::DrawTop(true);
	Gui::ScreenDraw(Bottom);
	Gui::Draw_Rect(0, 0, 320, 240, C2D_Color32(fadecolor, fadecolor, fadecolor, 255)); // Fade in/out effect
}

void Exiting::Logic(u32 hDown, u32 hHeld, touchPosition touch) { }