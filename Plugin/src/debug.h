#pragma once
#include <string>

namespace wasapi_mic {
class debug
{
public:
  using log_func = void(*)(const wchar_t* message);
  debug(log_func func);
  void log(const std::wstring& message);

private:
  log_func func;
};

}
