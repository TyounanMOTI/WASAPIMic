#pragma once
#include <memory>
#include "device.h"
#include "debug.h"

namespace wasapi_mic {

extern std::unique_ptr<debug> g_debug;
extern std::unique_ptr<device> g_device;

}
