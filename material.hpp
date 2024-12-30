#ifndef MATERIAL_H
#define MATERIAL_H

class Material
{
public:
	Material(float ambient, float diffuse, float specular, int shine);
	~Material();
	float ambientStrength = 0.1f;                       // Ambient coefficient
	float diffuseStrength = 1.0f;                       // Diffuse coefficient
	float specularStrength = 0.5f;                      // Specular coefficient
	int shininess = 32;                                 // Shininess factor for specular highlights

private:

};

Material::Material(float ambient = .1f, float diffuse = 1.f, float specular = .5f, int shine = 32) : 
	ambientStrength(ambient), diffuseStrength(diffuse), specularStrength(specular), shininess(shine)
{
}

Material::~Material()
{
}

#endif // !MATERIAL_H
