#ifndef SHAPE_H
#define SHAPE_H

#include <glm/glm.hpp>
#include "ray.hpp"

class Shape
{
public:
	Shape();
	~Shape();
	// Declare virtual methods to be implemented by derived classes
	virtual glm::vec3 get_normal(glm::vec3 point) const = 0;
	virtual Intersection get_intersection(Ray ray) const = 0;

	glm::vec3 color;

private:

};

Shape::Shape()
{
}

Shape::~Shape()
{
}

#endif // !SHAPE_H
