#ifndef INIT_HPP
#define INIT_HPP

#include <3ds.h>

namespace Init {
	// Init, Mainloop & Exit.
	Result Initialize();
	Result MainLoop();
	Result Exit();
}

#endif