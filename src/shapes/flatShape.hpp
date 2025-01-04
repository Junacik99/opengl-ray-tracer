#ifndef FLAT_SHAPE_H
#define FLAT_SHAPE_H

#include <glm/glm.hpp>
#include "../material.hpp"

struct FlatShape {
    int type; // 0 for Sphere, 1 for Plane
    alignas(16) glm::vec3 padding;

    FlatMaterial material;

    alignas(16) glm::vec3 sphereCenter;
    float sphereRadius;      

    alignas(16) glm::vec3 planeNormal;
    float planeD;  
};

#endif // !FLAT_SHAPE_H


