#pragma once

namespace rl {
#include <ThirdParty/raylib.h>
#include <ThirdParty/rlgl.h>
} // namespace rl

namespace Render {
void DrawMeshInstanced_Colors(rl::Mesh mesh, rl::Material material, const rl::Matrix* transforms, rl::Color* colors,
                              int instances);
}
