#include "common.hpp"

void Msg::DisplayMsg(std::string text) {
	Gui::clearTextBufs();
	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
	C2D_TargetClear(Top, TRANSPARENT);
	C2D_TargetClear(Bottom, TRANSPARENT);
	GFX::DrawTop();
	Gui::ScreenDraw(Bottom);
	GFX::DrawSprite(sprites_BS_loading_background_idx, 0, 0);
	Gui::DrawString(24, 32, 0.45f, BLACK, text);
	C3D_FrameEnd(0);
}

// Display a Message, which needs to be confirmed with A/B.
bool Msg::promptMsg(std::string promptMsg)
{
	Gui::clearTextBufs();
	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
	C2D_TargetClear(Top, TRANSPARENT);
	C2D_TargetClear(Bottom, TRANSPARENT);
	GFX::DrawTop();
	Gui::ScreenDraw(Bottom);
	GFX::DrawSprite(sprites_BS_loading_background_idx, 0, 0);
	Gui::DrawString(24, 32, 0.5f, BLACK, promptMsg);
	Gui::DrawStringCentered(0, 180, 0.6f, BLACK, "Press A to confirm, B to cancel.", 390);
	C3D_FrameEnd(0);
	while(1)
	{
		gspWaitForVBlank();
		hidScanInput();
		if(hidKeysDown() & KEY_A) {
			return true;
		} else if(hidKeysDown() & KEY_B) {
			return false;
		}
	}
}