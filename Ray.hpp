#ifndef RAY_H
#define RAY_H

#include <glm/glm.hpp>

class Ray
{
public:
	Ray(glm::vec3 start, glm::vec3 dir);
	~Ray();
	glm::vec3 get_start();
	glm::vec3 get_dir();
	glm::vec3 get_point(float t);

private:
	glm::vec3 m_start, m_dir;
};

Ray::Ray(glm::vec3 start, glm::vec3 dir)
{
	m_start = start;
	m_dir = dir;
}

glm::vec3 Ray::get_start() {
	return m_start;
}

glm::vec3 Ray::get_dir() {
	return m_dir;
}

glm::vec3 Ray::get_point(float t) {
	return m_start + t*m_dir;
}

Ray::~Ray()
{
}

#endif