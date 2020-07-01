#include "common.hpp"
extern int fadealpha;

// 3D offsets.
typedef struct _Offset3D {
	float topbg;
	float twinkle3;
	float twinkle2;
	float twinkle1;
	float updater;
	float logo;
} Offset3D;
Offset3D offset3D[2] = {0.0f, 0.0f};

extern C2D_SpriteSheet sprites;

void GFX::DrawTop(bool showVer) {
	if (gfxIs3D()) {
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
	}

	for (int d = 0; d <= 1; d++) {
		Gui::ScreenDraw(d==1 ? TopRight : Top);
		DrawSprite(sprites_top_bg_idx, 0+offset3D[d].topbg, 0);
		DrawSprite(sprites_twinkle_3_idx, 133+offset3D[d].twinkle3, 61);
		DrawSprite(sprites_twinkle_2_idx, 157+offset3D[d].twinkle2, 81);
		DrawSprite(sprites_twinkle_1_idx, 184+offset3D[d].twinkle1, 107);
		DrawSprite(sprites_arrow_idx, 41+offset3D[d].updater, 25);
		DrawSprite(sprites_text_updater_idx, 187+offset3D[d].updater, 151);
		DrawSprite(sprites_twlm_logo_idx, 127+offset3D[d].logo, 100);
		if (showVer) Gui::DrawString(336, 222, 0.50, WHITE, VERSION_STRING);
		if (fadealpha > 0) Gui::Draw_Rect(0, 0, 400, 240, C2D_Color32(fadecolor, fadecolor, fadecolor, fadealpha)); // Fade in/out effect
	}
}


void GFX::DrawSprite(int img, int x, int y, float ScaleX, float ScaleY)
{
	Gui::DrawSprite(sprites, img, x, y, ScaleX, ScaleY);
}

void GFX::DrawSpriteBlend(int img, int x, int y, u32 color, float ScaleX, float ScaleY) {
	C2D_ImageTint tint;
	C2D_SetImageTint(&tint, C2D_TopLeft, color, 1);
	C2D_SetImageTint(&tint, C2D_TopRight, color, 1);
	C2D_SetImageTint(&tint, C2D_BotLeft, color, 1);
	C2D_SetImageTint(&tint, C2D_BotRight, color, 1);
	C2D_DrawImageAt(C2D_SpriteSheetGetImage(sprites, img), x, y, 0.5f, &tint, ScaleX, ScaleY);
}