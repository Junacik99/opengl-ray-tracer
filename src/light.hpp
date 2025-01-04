#ifndef LIGHT_H
#define LIGHT_H

#include <glm/glm.hpp>

class Light
{
public:
	Light(glm::vec3 pos, glm::vec3 c, float intens);
	~Light();
	glm::vec3 position;
	glm::vec3 color;
	glm::vec3 baseColor;
	float intensity;

	void updateColor();

private:

};

Light::Light(glm::vec3 pos = glm::vec3(0), glm::vec3 c = glm::vec3(1), float intens = 1) : 
	position(pos), baseColor(c), intensity(intens)
{
	updateColor();
}

Light::~Light()
{
}

inline void Light::updateColor()
{
	color = intensity * baseColor;
}

#endif // !LIGHT_H
