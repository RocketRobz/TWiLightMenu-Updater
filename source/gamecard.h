// Game card functions.
#ifndef DSIMENUPP_GAMECARD_H
#define DSIMENUPP_GAMECARD_H

#include <vector>
#include <string>
#include "graphic.h"
#include "pp2d/pp2d.h"

/**
 * Poll for a game card change.
 * @param force If true, force a re-cache. (Use if polling hasn't been done in a while.)
 * @return True if the card has changed; false if not.
 */
bool gamecardPoll(bool force);

enum GameCardType {
	CARD_TYPE_UNKNOWN	= 0,	// Unknown type.
	CARD_TYPE_NTR		= 1,	// DS
	CARD_TYPE_TWL_ENH	= 2,	// DSi-enhanced
	CARD_TYPE_TWL_ONLY	= 3,	// DSi only
	CARD_TYPE_CTR		= 4,	// 3DS
};

/**
 * Is a game card inserted?
 * @return True if a game card is inserted.
 */
bool gamecardIsInserted(void);

/**
 * Get the game card's type.
 * @return Game card type.
 */
GameCardType gamecardGetType(void);

/**
 * Get the game card's game ID.
 * @return Game ID, or NULL if not a TWL card.
 */
const char *gamecardGetGameID(void);

/**
 * Get the game card's game ID as a std::uint32_t.
 * @return Game ID, or 0 if not a TWL card.
 */
std::uint32_t gamecardGetGameID_u32(void);

/**
 * Get the game card's product code.
 * @return Product code, or NULL if not a TWL card.
 */
const char *gamecardGetProductCode(void);

/**
 * Get the game card's Title ID.
 * NOTE: Only applicable to TWL and CTR titles.
 * @return Title ID, or 0 if no card or the card doesn't have a title ID.
 */
std::uint64_t gamecardGetTitleID(void);

/**
 * Get the game card's revision..
 * @return Game card revision. (0xFF if unknown.)
 */
std::uint8_t gamecardGetRevision(void);

#endif /* DSIMENUPP_GAMECARD_H */
