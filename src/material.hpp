#ifndef MATERIAL_H
#define MATERIAL_H

class Material
{
public:
	Material(glm::vec3 c, float fresnel, float ambient, float diffuse, float specular, int shine);
	~Material();
	
	glm::vec3 color;
	float fresnelStrength;

	float ambientStrength = 0.5f; // Ambient coefficient
	float diffuseStrength = 1.0f; // Diffuse coefficient
	float specularStrength = 0.5f; // Specular coefficient
	int shininess = 32; // Shininess factor for specular highlights


private:

};

Material::Material(glm::vec3 c = glm::vec3(1), float fresnel = 1.f, float ambient = .4f, float diffuse = 1.f, float specular = .5f, int shine = 32) : 
	color(c), fresnelStrength(fresnel), ambientStrength(ambient), diffuseStrength(diffuse), specularStrength(specular), shininess(shine)
{
}

Material::~Material()
{
}

#endif // !MATERIAL_H
