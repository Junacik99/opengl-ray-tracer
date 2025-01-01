#ifndef PLANE_H
#define PLANE_H

#include<glm/glm.hpp>
#include "../ray.hpp"
#include "../intersection.hpp"
#include "shape.hpp"

class Plane : public Shape
{
public:
	Plane(glm::vec3 normal, glm::vec3 point);
	~Plane();
	glm::vec3 get_normal(glm::vec3 position) const override;
	Intersection get_intersection(Ray ray) const override;
	void serialize(FlatShape& out) const override;
	
	glm::vec3 m_normal;
	float d;

private:
	
};

Plane::Plane(glm::vec3 normal, glm::vec3 point)
{
	m_normal = glm::normalize(normal);
	d = -(glm::dot(m_normal, point));
	color = glm::vec3(0, 0, 1);
}

Plane::~Plane()
{
}

inline glm::vec3 Plane::get_normal(glm::vec3 position) const
{
	return m_normal;
}

inline Intersection Plane::get_intersection(Ray ray) const
{
	float np = glm::dot(m_normal, ray.get_dir());
	if (np == 0)
		return Intersection(NONE); // no intersection
	float t = -(d + glm::dot(m_normal, ray.get_start())) / np;
	if (t > 0) {
		return Intersection((np > 0) ? INNER : OUTER, ray.get_point(t));
	}
	else
	{
		return Intersection(NONE);
	}
}

inline void Plane::serialize(FlatShape& out) const {
	out.type = 1; // Plane
	out.color = color;
	out.material = material;
	out.planeNormal = m_normal;
	out.planeD = d;
}

#endif // !PLANE_H
