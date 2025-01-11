#ifndef WALL_H
#define WALL_H

#include "plane.hpp"

class Wall : public Plane
{
public:
	Wall(glm::vec3 s, float w, float h, glm::vec3 normal);
	~Wall();

	glm::vec3 start;
	float width, height;
	Intersection get_intersection(Ray ray) const override;

	glm::vec3 end() const {
		glm::vec3 tangent1, tangent2;

		// Compute tangent vectors based on the wall's normal
		if (glm::abs(m_normal.x) > glm::abs(m_normal.y)) {
			tangent1 = glm::normalize(glm::vec3(-m_normal.z, 0, m_normal.x));
		}
		else {
			tangent1 = glm::normalize(glm::vec3(0, -m_normal.z, m_normal.y));
		}

		tangent2 = glm::normalize(glm::cross(m_normal, tangent1));

		// Calculate the end point
		return start + (width * tangent1) + (height * tangent2);
	}

private:

};

Wall::Wall(glm::vec3 s, float w, float h, glm::vec3 normal)
	: Plane(normal, s), start(s), width(w), height(h)
{
}

Wall::~Wall()
{
}

inline Intersection Wall::get_intersection(Ray ray) const {
	Intersection baseIntersection = Plane::get_intersection(ray);
	if (baseIntersection.intersect_type == NONE) return Intersection(NONE);

	auto hitPoint = baseIntersection.hit_point;

	// Define two orthogonal directions on the plane based on the normal
	glm::vec3 u = glm::normalize(glm::cross(m_normal, glm::vec3(0, 1, 0)));
	if (glm::length(u) < 1e-4) u = glm::normalize(glm::cross(m_normal, glm::vec3(1, 0, 0))); // Handle edge case when normal is (0,1,0)
	glm::vec3 v = glm::normalize(glm::cross(m_normal, u));

	// Calculate local coordinates relative to the start point
	glm::vec3 localPoint = hitPoint - start;
	float uProj = glm::dot(localPoint, u); // Projection onto u-axis
	float vProj = glm::dot(localPoint, v); // Projection onto v-axis

	// Check if the hit point is within the rectangle bounds
	if (uProj < 0 || uProj > width || vProj < 0 || vProj > height) {
		return Intersection(NONE);
	}

	return baseIntersection;
}

#endif // !WALL_H
