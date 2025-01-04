#ifndef FLAT_STRUCTURES_H
#define FLAT_STRUCTURES_H

#include <glm/glm.hpp>
#include <vector>

struct FlatMaterial
{
	alignas(16) glm::vec3 color;
	float fresnelStrength;

	float ambientStrength; // Ambient coefficient

	float diffuseStrength; // Diffuse coefficient

	float specularStrength; // Specular coefficient

	int shininess; // Shininess factor for specular highlights

};

struct FlatShape {
    int type; // 0 for Sphere, 1 for Plane, 2 for Wall
    alignas(16) glm::vec3 padding;

    FlatMaterial material;

    alignas(16) glm::vec3 sphereCenter;
    float sphereRadius;      

    alignas(16) glm::vec3 planeNormal;
    float planeD;  

	alignas(16) glm::vec3 wallStart;
	float wallWidth;

	float wallHeight;
	alignas(16) glm::vec3 padding1;
};

struct FlatCamera {
	glm::vec3 Position;
	float aspectRatio;

	glm::vec3 Front;
	float padding2;

	glm::vec3 Up;
	float padding3;

	glm::vec3 Right;
	float padding4;

	float fov;
	glm::vec3 padding5;
};

struct FlatLight {
	glm::vec3 position;
	float padding1;

	glm::vec3 color;
	float padding2;
};

struct FlatScene {
	FlatCamera camera;
	FlatLight light;
	std::vector <FlatShape> shapes;
};

#endif // !FLAT_STRUCTURES_H


