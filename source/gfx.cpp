#include "common.hpp"

extern C2D_SpriteSheet sprites;

void GFX::DrawTop(void) {
	Gui::ScreenDraw(Top);
	DrawSprite(sprites_top_bg_idx, 0, 0);
	DrawSprite(sprites_twinkle_3_idx, 133, 61);
	DrawSprite(sprites_twinkle_2_idx, 157, 81);
	DrawSprite(sprites_twinkle_1_idx, 184, 107);
	DrawSprite(sprites_arrow_idx, 41, 25);
	DrawSprite(sprites_text_updater_idx, 187, 151);
	DrawSprite(sprites_twlm_logo_idx, 127, 100);
}


void GFX::DrawSprite(int img, int x, int y, float ScaleX, float ScaleY)
{
	C2D_DrawImageAt(C2D_SpriteSheetGetImage(sprites, img), x, y, 0.5f, NULL, ScaleX, ScaleY);
}

void GFX::DrawSpriteBlend(int img, int x, int y, u32 color, float ScaleX, float ScaleY) {
	C2D_ImageTint tint;
	C2D_SetImageTint(&tint, C2D_TopLeft, color, 1);
	C2D_SetImageTint(&tint, C2D_TopRight, color, 1);
	C2D_SetImageTint(&tint, C2D_BotLeft, color, 1);
	C2D_SetImageTint(&tint, C2D_BotRight, color, 1);
	C2D_DrawImageAt(C2D_SpriteSheetGetImage(sprites, img), x, y, 0.5f, &tint, ScaleX, ScaleY);
}