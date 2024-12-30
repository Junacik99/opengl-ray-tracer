#ifndef SHAPE_H
#define SHAPE_H

#include <glm/glm.hpp>
#include "../material.hpp"

class Shape
{
public:
	Shape(Material mat);
	~Shape();
	// Declare virtual methods to be implemented by derived classes
	virtual glm::vec3 get_normal(glm::vec3 point) const = 0;
	virtual Intersection get_intersection(Ray ray) const = 0;

	glm::vec3 color;
	Material material;

private:

};

Shape::Shape(Material mat = Material()) : material(mat)
{
}

Shape::~Shape()
{
}

#endif // !SHAPE_H
