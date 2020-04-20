#pragma once

#include <vector>
#include <string>

namespace shadey {

  std::vector<uint8_t> compileShader(bool fragment, const std::string& glsl);

}