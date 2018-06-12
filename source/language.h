// Language functions.
#ifndef DSIMENUPP_LANGUAGE_H
#define DSIMENUPP_LANGUAGE_H

#include <3ds/types.h>

// Active language ID.
extern u8 language;
// System language ID.
extern u8 sys_language;

/**
 * Initialize translations.
 * Uses the language ID specified in settings.ui.language.
 *
 * Check the language variable outside of settings to determine
 * the actual language in use.
 */
void langInit(void);

/**
 * Clear the translations cache.
 */
void langClear(void);

// String IDs.
typedef enum _StrID {
	
	/** DS/DSi boot screen **/
	STR_DSSPLASH_WARNING_HS = 0,								// "WARNING - HEALTH AND SAFETY"
	STR_DSSPLASH_WARNING_HS1,									// "BEFORE PLAYING, READ THE HEALTH"
	STR_DSSPLASH_WARNING_HS2,									// "AND SAFETY PRECAUTIONS BOOKLET"
	STR_DSSPLASH_WARNING_HS3,									// "FOR IMPORTANT INFORMATION"
	STR_DSSPLASH_WARNING_HS4,									// "ABOUT YOUR HEALTH AND SAFETY."
	STR_DSSPLASH_WARNING_HS5,									// "TO GET AN EXTRA COPY FOR YOUR REGION, GO ONLINE AT"
	STR_DSSPLASH_WARNING_HS6,									// "www.nintendo.com/healthsafety/"
	STR_DSSPLASH_WARNING_HS7,									// Reserved for other languages
	
	STR_DSSPLASH_TOUCH,											// "Touch the Touch Screen to continue."
	
	/** GUI **/
	STR_RETURN_TO_HOME_MENU,									// "Return to HOME Menu"
	
	STR_MAX
} StrID;

/**
 * Get a translation.
 *
 * NOTE: Call langInit() before using TR().
 *
 * @param strID String ID.
 * @return Translation, or error string if strID is invalid.
 */
const wchar_t *TR(StrID strID);

#endif /* DSIMENUPP_LANGUAGE_H */
