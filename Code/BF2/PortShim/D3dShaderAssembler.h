#pragma once
// [CHOSEN, not recovered] Assembles the ps/vs 1.x blocks of retail effect files into D3D9 shader
// tokens. Real D3DX does this inside its effect compiler; vkd3d-shader has no assembler.
#include <string>
#include <vector>

namespace portshim {

// Returns true with `words` ending in the end token; otherwise `error` names the line refused.
bool assembleD3dShader(const std::string& source, std::vector<unsigned>* words, std::string* error);

} // namespace portshim
