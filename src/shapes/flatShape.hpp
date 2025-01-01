#ifndef FLAT_SHAPE_H
#define FLAT_SHAPE_H

#include <glm/glm.hpp>
#include "../material.hpp"

struct FlatShape {
    int type; // 0 for Sphere, 1 for Plane
    glm::vec3 color;
    Material material;

    glm::vec3 sphereCenter;  
    float sphereRadius;      

    glm::vec3 planeNormal;   
    float planeD;  

    FlatShape(int t, glm::vec3 c, Material m, glm::vec3 vec, float f)
        : type(t), color(c), material(m) {
        if (t == 0) { // Sphere
            sphereCenter = vec;
            sphereRadius = f;
        }
        if (t == 1) { // Plane
            planeNormal = vec;
            planeD = f;
        }
    }


    // Default constructor
    FlatShape() : type(-1), color(0.0f), material() {}
};

#endif // !FLAT_SHAPE_H


