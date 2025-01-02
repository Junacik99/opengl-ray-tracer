#ifndef LIGHT_H
#define LIGHT_H

#include <glm/glm.hpp>

class Light
{
public:
	Light(glm::vec3 pos, glm::vec3 c);
	~Light();
	glm::vec3 position;
	glm::vec3 color;

private:

};

Light::Light(glm::vec3 pos = glm::vec3(0), glm::vec3 c = glm::vec3(1)) : position(pos), color(c)
{
}

Light::~Light()
{
}

#endif // !LIGHT_H
