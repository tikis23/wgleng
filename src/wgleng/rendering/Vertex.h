#pragma once

#include <glm/glm.hpp>
#include <stdint.h>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    int32_t materialId{0};
};

struct Material {
    glm::vec4 diffuse{1, 0, 1, 1};
};