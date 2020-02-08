#ifndef UPDATERSCREEN_HPP
#define UPDATERSCREEN_HPP

#include "common.hpp"
#include "structs.hpp"

#include <vector>

class UpdaterScreen : public Screen
{
public:
	void Draw(void) const override;
	void Logic(u32 hDown, u32 hHeld, touchPosition touch) override;
	UpdaterScreen();

private:
	bool buttonShading = false;
	bool setOption = false;
	bool showMessage = false;
	int menuSelection = 0;
};

#endif