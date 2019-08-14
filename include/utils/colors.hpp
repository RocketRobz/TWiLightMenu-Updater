/*
*   This file is part of Universal-Manager
*   Copyright (C) 2019 VoltZ, Epicpkmn11, Flame, RocketRobz, TotallyNotGuy
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
*       * Requiring preservation of specified reasonable legal notices or
*         author attributions in that material or in the Appropriate Legal
*         Notices displayed by works containing it.
*       * Prohibiting misrepresentation of the origin of that material,
*         or requiring that modified versions of such material be marked in
*         reasonable ways as different from the original version.
*/

#ifndef COLORS_HPP
#define COLORS_HPP

#include <citro2d.h>
#include <citro3d.h>

/**
 * @brief Creates a 8 byte RGBA color
 * @param r red component of the color
 * @param g green component of the color
 * @param b blue component of the color
 * @param a alpha component of the color
 */
#define RGBA8(r, g, b, a) ((((r)&0xFF)<<0) | (((g)&0xFF)<<8) | (((b)&0xFF)<<16) | (((a)&0xFF)<<24))

/**
 * @brief Creates a 8 byte ABGR color
 * @param a alpha component of the color
 * @param b blue component of the color
 * @param g green component of the color
 * @param r red component of the color
 */
#define ABGR8(a, b, g, r) ((((a)&0xFF)<<0) | (((b)&0xFF)<<8) | (((g)&0xFF)<<16) | (((r)&0xFF)<<24))

/**
 * @brief Converts a RGB565 color to RGBA8 color (adds maximum alpha)
 * @param rgb 565 to be converted
 * @param a alpha
 */
#define RGB565_TO_RGBA8(rgb, a) \
	(RGBA8(((rgb>>11)&0x1F)*0xFF/0x1F, ((rgb>>5)&0x3F)*0xFF/0x3F, (rgb&0x1F)*0xFF/0x1F, a&0xFF))
	
/**
 * @brief Converts a RGB565 color to ABGR8 color (adds maximum alpha)
 * @param rgb 565 to be converted
 * @param a alpha
 */
#define RGB565_TO_ABGR8(rgb, a) \
	(RGBA8(a&0xFF, (rgb&0x1F)*0xFF/0x1F, ((rgb>>5)&0x3F)*0xFF/0x3F, ((rgb>>11)&0x1F)*0xFF/0x1F))

#define TRANSPARENT C2D_Color32(0, 0, 0, 0)
#define BLACK C2D_Color32(0, 0, 0, 255)
#define WHITE C2D_Color32(255, 255, 255, 255)
#define GRAY RGBA8(127, 127, 127, 255)
#define BLUE RGBA8(0, 0, 255, 255)
#define GREEN RGBA8(0, 255, 0, 255)
#define RED RGBA8(255, 0, 0, 255)

#define TIME RGBA8(16, 0, 0, 223)

typedef u32 Color;
#endif
