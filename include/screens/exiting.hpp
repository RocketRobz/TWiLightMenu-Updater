#ifndef _TWILIGHT_MENU_UPDATER_EXITING_HPP
#define _TWILIGHT_MENU_UPDATER_EXITING_HPP

#include "common.hpp"

class Exiting : public Screen {
public:
	void Draw(void) const override;
	void Logic(u32 hDown, u32 hHeld, touchPosition touch) override;
};

#endif