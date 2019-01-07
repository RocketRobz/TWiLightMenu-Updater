#ifndef DSIMENUPP_SETTINGS_H
#define DSIMENUPP_SETTINGS_H

#include <string>

// 3D offsets.
typedef struct _Offset3D {
	float topbg;
	float twinkle3;
	float twinkle2;
	float twinkle1;
	float updater;
	float logo;
} Offset3D;
extern Offset3D offset3D[2];	// 0 == Left; 1 == Right

#endif /* DSIMENUPP_SETTINGS_H */
