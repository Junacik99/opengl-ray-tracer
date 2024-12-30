#ifndef INTERSECTION_H
#define INTERSECTION_H

#include <glm/glm.hpp>

enum IntersectType {
	NONE,
	INNER,
	OUTER
};

class Intersection
{
public:
	Intersection(IntersectType itype, glm::vec3 hit);
	~Intersection();
	IntersectType intersect_type;
	glm::vec3 hit_point;

private:

};

Intersection::Intersection(IntersectType itype = NONE, glm::vec3 hit = glm::vec3()): intersect_type(itype), hit_point(hit)
{
}

Intersection::~Intersection()
{
}

#endif // !INTERSECTION_H
