#ifndef TRIANGLE_H
#define TRIANGLE_H

#include "glm/glm.hpp"
#include "plane.hpp"

class Triangle : public Plane
{
public:
	Triangle(glm::vec3 p1, glm::vec3 p2, glm::vec3 p3);
	~Triangle();

	glm::vec3 a;
	glm::vec3 b;
	glm::vec3 c;

	glm::vec3 center() const {
		return (a + b + c) / 3.0f;
	}

	void invert_normal();

	Intersection get_intersection(Ray ray) const override;


private:
	glm::vec3 get_normal(glm::vec3 p1, glm::vec3 p2, glm::vec3 p3);
};

Triangle::Triangle(glm::vec3 p1, glm::vec3 p2, glm::vec3 p3) 
	: a(p1), b(p2), c(p3), Plane(get_normal(p1, p2, p3), p1)
{
}

Triangle::~Triangle()
{
}

inline glm::vec3 Triangle::get_normal(glm::vec3 p1, glm::vec3 p2, glm::vec3 p3) {
	// https://stackoverflow.com/questions/19350792/calculate-normal-of-a-single-triangle-in-3d-space
	auto A = p2 - p1;
	auto B = p3 - p1;
	glm::vec3 result;
	result.x = A.y * B.z - A.z * B.y;
	result.y = A.z * B.x - A.x * B.z;
	result.z = A.x * B.y - A.y * B.x;
	return result;
}

inline void Triangle::invert_normal() {
	m_normal = -m_normal;
	d = -(glm::dot(m_normal, a));
}

inline Intersection Triangle::get_intersection(Ray ray) const {
	Intersection baseIntersection = Plane::get_intersection(ray);
	if (baseIntersection.intersect_type == NONE) return Intersection(NONE);

	auto hitPoint = baseIntersection.hit_point;

	// Compute vectors for edges and point-to-vertex
	glm::vec3 edge1 = b - a;
	glm::vec3 edge2 = c - a;
	glm::vec3 toPoint = hitPoint - a;

	// Barycentric coordinates
	float d00 = glm::dot(edge1, edge1);
	float d01 = glm::dot(edge1, edge2);
	float d11 = glm::dot(edge2, edge2);
	float d20 = glm::dot(toPoint, edge1);
	float d21 = glm::dot(toPoint, edge2);

	float denom = d00 * d11 - d01 * d01;
	float v = (d11 * d20 - d01 * d21) / denom;
	float w = (d00 * d21 - d01 * d20) / denom;
	float u = 1.0 - v - w;

	if (u < 0 || v < 0 || w < 0) {
		return Intersection(NONE);
	}

	return baseIntersection;
}

#endif // !TRIANGLE_H
