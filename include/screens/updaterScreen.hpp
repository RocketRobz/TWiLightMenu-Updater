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

private:
	void checkUpdates();
	bool buttonShading = false;
	bool setOption = false;
	bool showMessage = false;
	int menuSelection = 0;

	const std::array<const char *, 8> button_titles2 = {
		"Release",
		"Nightly",
		"Release",
		"Nightly",
		"Release",
		"Nightly",
		"Cheats",
		"Extras",
	};

	const std::array<const int, 8> title_spacing = {
		6,
		10,
		6,
		10,
		6,
		10,
		10,
		17,
	};

	const std::array<const char *, 4> row_titles2 = {
		"TWL Menu++",
		"nds-bootstrap",
		"Updater",
		"Downloads",
	};
};

#endif