#ifndef _TWLM_UPDATER_ROCKETROBZ_HPP
#define _TWLM_UPDATER_ROCKETROBZ_HPP

#include "common.hpp"

class RocketRobz : public Screen {
public:
	void Draw(void) const override;
	void Logic(u32 hDown, u32 hHeld, touchPosition touch) override;
private:
	//bool musicPlayed = false;
	const char* presentedText = "Presented in";
	const char* yearText = "2017-2020 RocketRobz";
};

#endif