#include <IUnityInterface.h>
#include "globals.h"
#include "runtime_error.h"

using namespace wasapi_mic;

extern "C" {

  void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetDebugLogFunc(debug::log_func func)
  {
    g_debug = std::make_unique<debug>(func);
  }

  void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API Initialize()
  {
    try {
      g_device.reset();
      g_device = std::make_unique<device>();
    } catch (const runtime_error& e) {
      if (g_debug) {
        g_debug->log(e.get_message());
      }
    }
  }

  void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API StartLoopback()
  {
    try {
      if (!g_device) {
        throw runtime_error(L"WASAPIMicプラグインが未初期化です。");
      }
      g_device->start();
    } catch (const runtime_error& e) {
      if (g_debug) {
        g_debug->log(e.get_message());
      }
    }
  }

  void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API StopLoopback()
  {
    try {
      if (!g_device) {
        throw runtime_error(L"WASAPIMicプラグインが未初期化です。");
      }
      g_device->stop();
    } catch (const runtime_error& e) {
      if (g_debug) {
        g_debug->log(e.get_message());
      }
    }
  }
}
