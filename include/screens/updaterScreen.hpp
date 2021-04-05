#ifndef UPDATERSCREEN_HPP
#define UPDATERSCREEN_HPP

#include "common.hpp"
#include "structs.hpp"

#include <array>

class UpdaterScreen : public Screen
{
public:
	void Draw(void) const override;
	void Logic(u32 hDown, u32 hHeld, touchPosition touch) override;
	UpdaterScreen();
};

#endif