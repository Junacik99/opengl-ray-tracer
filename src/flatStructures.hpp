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
    int type; // 0 for Sphere, 1 for Plane, 2 for Wall, 3 for Triangle
    alignas(16) glm::vec3 padding;

    FlatMaterial material;

	// Sphere
    alignas(16) glm::vec3 sphereCenter;
    float sphereRadius;      

	// Plane
    alignas(16) glm::vec3 planeNormal;
    float planeD;  

	// Wall
	alignas(16) glm::vec3 wallStart;
	float wallWidth;

	float wallHeight;
	alignas(16) glm::vec3 padding1;

	// Triangle
	alignas(16) glm::vec3 triP1;
	float padding2;

	alignas(16) glm::vec3 triP2;
	float padding3;

	alignas(16) glm::vec3 triP3;
	float padding4;

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

struct FlatBBox {
	alignas(16) glm::vec3 min;
	float padding1;

	alignas(16) glm::vec3 max;
	float padding2;
};

struct FlatNode {
	alignas(16) glm::vec3 boundsMin;
	float padding1;

	alignas(16) glm::vec3 boundsMax;
	float padding2;
	
	int leftChild;       // Index of the left child (-1 for leaf)
	int rightChild;      // Index of the right child (-1 for leaf)
	
	int startShapeIdx;   // Index of the triangle (only valid for leaf nodes)
	int numShapes;
};
std::vector<FlatNode> flatNodes;
std::vector<int> bvhIndices; // Node 1 - shape indices; Node 2...


#endif // !FLAT_STRUCTURES_H


