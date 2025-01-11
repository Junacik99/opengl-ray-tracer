#ifndef BOUNDINGBOX_H
#define BOUNDINGBOX_H

#include "glm/glm.hpp"
#include "src/shapes/triangle.hpp"
#include "src/shapes/sphere.hpp"
#include "src/shapes/wall.hpp"
#include "src/mesh.hpp"


class BoundingBox
{
public:
	BoundingBox();
	~BoundingBox();

	void growToInclude(glm::vec3 point);
	void growToInclude(Triangle triangle);
	void growToInclude(Sphere sphere);
	void growToInclude(Wall wall);
	void growToInclude(Mesh mesh);

	void growToInclude(std::unique_ptr<Shape> &shape);

	glm::vec3 Min, Max;

    glm::vec3 center() const {
        return (Min + Max) * 0.5f;
    }

private:
};

BoundingBox::BoundingBox()
{
	Min = glm::vec3(INFINITY);
	Max = glm::vec3(INFINITY * (-1.f));
}

BoundingBox::~BoundingBox()
{
}

inline void BoundingBox::growToInclude(glm::vec3 point)
{
	Min = glm::min(Min, point);
	Max = glm::max(Max, point);
}

inline void BoundingBox::growToInclude(Triangle triangle)
{
    if (std::isfinite(triangle.a.x) && std::isfinite(triangle.b.x) && std::isfinite(triangle.c.x)) {
        growToInclude(triangle.a);
        growToInclude(triangle.b);
        growToInclude(triangle.c);
    }
    else {
        std::cerr << "Invalid triangle vertices!" << std::endl;
        std::cout << "Triangle:" << std::endl;
        std::cout << "P1 " << triangle.a.x << " " << triangle.a.y << " " << triangle.a.z << std::endl;
        std::cout << "P2 " << triangle.b.x << " " << triangle.b.y << " " << triangle.b.z << std::endl;
        std::cout << "P3 " << triangle.c.x << " " << triangle.c.y << " " << triangle.c.z << std::endl;
        std::cout << std::endl;
    }
}

inline void BoundingBox::growToInclude(Sphere sphere)
{
	growToInclude(sphere.m_center + sphere.m_radius);
	growToInclude(sphere.m_center - sphere.m_radius);
}

inline void BoundingBox::growToInclude(Wall wall)
{
	growToInclude(wall.start);
	growToInclude(wall.end());
}

inline void BoundingBox::growToInclude(Mesh mesh)
{
	auto triangles = mesh.mesh2triangles();
	for (auto triangle : triangles) {
		growToInclude(triangle);
	}
}

inline void BoundingBox::growToInclude(std::unique_ptr<Shape> &shape)
{
	if (auto sphere = dynamic_cast<Sphere*>(shape.get()))
		growToInclude(*sphere);
	else if (auto wall = dynamic_cast<Wall*>(shape.get()))
		growToInclude(*wall);
	else if (auto triangle = dynamic_cast<Triangle*>(shape.get()))
		growToInclude(*triangle);
}




#endif // !BOUNDINGBOX_H
