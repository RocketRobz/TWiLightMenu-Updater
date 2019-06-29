// Game card functions.
#include "gamecard.h"
#include "language.h"
#include "textfns.h"

#include <3ds.h>

#include <malloc.h>
#include "gui.hpp"

#include <string>
#include <vector>
using std::string;
using std::vector;
using std::wstring;

// Current card information.
static union {
	// TODO: Use this field for CTR as well?
	u32 d;
	char id4[4];
} twl_gameid;	// 4-character game ID
static bool card_inserted = false;
static GameCardType card_type = CARD_TYPE_UNKNOWN;
static char card_product_code[20] = { };
static u8 card_revision = 0xFF;
static u64 card_tid = 0;
static vector<wstring> card_text;

/**
 * Clear the cached icon and text.
 */
void gamecardClearCache(void)
{
	// NOTE: card_inserted is NOT reset here.
	twl_gameid.d = 0;
	card_type = CARD_TYPE_UNKNOWN;
	card_product_code[0] = 0;
	card_revision = 0xFF;
	card_tid = 0;
	card_text.clear();
}

/**
 * Poll for a game card change.
 * @param force If true, force a re-cache. (Use if polling hasn't been done in a while.)
 * @return True if the card has changed; false if not.
 */
bool gamecardPoll(bool force)
{
	// Check if a TWL card is present.
	bool inserted;
	if (R_FAILED(FSUSER_CardSlotIsInserted(&inserted)) || !inserted) {
		// Card is not present.
		if (card_inserted || force) {
			gamecardClearCache();
			card_inserted = false;
			return true;
		}
		return false;
	}

	FS_CardType type;
	if (R_FAILED(FSUSER_GetCardType(&type))) {
		// Unable to get card type.
		// This may be a blocked flashcard.
		if (card_inserted || force) {
			gamecardClearCache();
			return true;
		}
		return false;
	}

	if (card_inserted && !force) {
		// No card change.
		return false;
	}

	// New card detected.
	gamecardClearCache();
	card_inserted = true;

	return true;
}

/**
 * Is a game card inserted?
 * @return True if a game card is inserted.
 */
bool gamecardIsInserted(void)
{
	return card_inserted;
}

/**
 * Get the game card's type.
 * @return Game card type.
 */
GameCardType gamecardGetType(void)
{
	return card_type;
}

/**
 * Get the game card's game ID.
 * @return Game ID, or NULL if not a TWL card.
 */
const char *gamecardGetGameID(void)
{
	return (twl_gameid.d != 0 ? twl_gameid.id4 : NULL);
}

/**
 * Get the game card's game ID as a std::uint32_t.
 * @return Game ID, or 0 if not a TWL card.
 */
std::uint32_t gamecardGetGameID_u32(void)
{
	return twl_gameid.d;
}

/**
 * Get the game card's product code.
 * @return Product code, or NULL if not a TWL card.
 */
const char *gamecardGetProductCode(void)
{
	return card_product_code;
}

/**
 * Get the game card's revision..
 * @return Game card revision. (0xFF if unknown.)
 */
std::uint8_t gamecardGetRevision(void)
{
	return card_revision;
}

/**
 * Get the game card's Title ID.
 * NOTE: Only applicable to TWL and CTR titles.
 * @return Title ID, or 0 if no card or the card doesn't have a title ID.
 */
std::uint64_t gamecardGetTitleID(void)
{
	return card_tid;
}
