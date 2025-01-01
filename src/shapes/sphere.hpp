#ifndef SPHERE_H
#define SPHERE_H

#include <glm/glm.hpp>
#include "../ray.hpp"
#include "../intersection.hpp"
#include "shape.hpp"

class Sphere : public Shape
{
public:
	Sphere(glm::vec3 center, float radius);
	~Sphere();
	glm::vec3 get_normal(glm::vec3 position) const override;
	Intersection get_intersection(Ray ray) const override;
	void serialize(FlatShape& out) const override;

	glm::vec3 m_center;
	float m_radius;

private:
	
};

Sphere::Sphere(glm::vec3 center, float radius)
{
	m_center = center;
	m_radius = radius;
	color = glm::vec3(0, 1, 0);
}

Sphere::~Sphere()
{
}

inline glm::vec3 Sphere::get_normal(glm::vec3 position) const {
	return glm::normalize(position - m_center);
}

inline Intersection Sphere::get_intersection(Ray ray) const {
	auto start = ray.get_start();
	auto dir = ray.get_dir();

	float aa = glm::dot(dir, dir);
	float bb = 2 * (glm::dot(dir, start - m_center));
	float cc = glm::dot(start - m_center, start - m_center) - m_radius * m_radius;
	float D = bb * bb - 4 * aa * cc;
	if (D > 0) {
		float sD = sqrt(D);
		float t1 = (-bb - sD) / (2 * aa);
		if (t1 > 0) // Inner intersection
			return Intersection(INNER, ray.get_point(t1));
		float t2 = (-bb + sD) / (2 * aa);
		if (t2 > 0) // Outer intersection
			return Intersection(OUTER, ray.get_point(t2));
	}

	return Intersection(NONE); // return empty vector if no intersection
}

inline void Sphere::serialize(FlatShape& out) const {
	out.type = 0; // Sphere
	out.color = color;
	out.material = material;
	out.sphereCenter = m_center;
	out.sphereRadius = m_radius;
}

#endif // !SPHERE_H
