#include "common.hpp"
extern int fadealpha;
extern bool fadein;
extern void wide3DSwap(void);

void Msg::DisplayMsg(std::string text) {
	Gui::clearTextBufs();
	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
	C2D_TargetClear(Top, TRANSPARENT);
	C2D_TargetClear(TopRight, TRANSPARENT);
	C2D_TargetClear(Bottom, TRANSPARENT);
	GFX::DrawTop(false);
	Gui::ScreenDraw(Bottom);
	GFX::DrawSprite(sprites_BS_loading_background_idx, 0, 0);
	Gui::DrawString(24, 32, 0.45f, BLACK, text);
	C3D_FrameEnd(0);
}

// Display a Message, which needs to be confirmed with A/B.
bool Msg::promptMsg(std::string promptMsg)
{
	while(1)
	{
		Gui::clearTextBufs();
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		C2D_TargetClear(Top, TRANSPARENT);
		C2D_TargetClear(TopRight, TRANSPARENT);
		C2D_TargetClear(Bottom, TRANSPARENT);
		GFX::DrawTop(false);
		Gui::ScreenDraw(Bottom);
		GFX::DrawSprite(sprites_BS_loading_background_idx, 0, 0);
		Gui::DrawString(24, 32, 0.5f, BLACK, promptMsg);
		Gui::DrawStringCentered(0, 180, 0.6f, BLACK, "Press  to confirm,  to cancel.", 390);
		if (fadealpha > 0) Gui::Draw_Rect(0, 0, 400, 240, C2D_Color32(fadecolor, fadecolor, fadecolor, fadealpha)); // Fade in/out effect
		C3D_FrameEnd(0);
		wide3DSwap();

		Gui::fadeEffects(16, 16);

		hidScanInput();
		if(hidKeysDown() & KEY_A) {
			return true;
		} else if(hidKeysDown() & KEY_B) {
			return false;
		}
	}
	return false;
}