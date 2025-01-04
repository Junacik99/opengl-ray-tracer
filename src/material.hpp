#ifndef MATERIAL_H
#define MATERIAL_H

struct FlatMaterial
{
	float ambientStrength;                       // Ambient coefficient
	float diffuseStrength;                       // Diffuse coefficient
	float specularStrength;                      // Specular coefficient
	int shininess;                                 // Shininess factor for specular highlights
};

class Material
{
public:
	Material(float ambient, float diffuse, float specular, int shine);
	~Material();
	float ambientStrength = 0.5f;                       // Ambient coefficient
	float diffuseStrength = 1.0f;                       // Diffuse coefficient
	float specularStrength = 0.5f;                      // Specular coefficient
	int shininess = 32;                                 // Shininess factor for specular highlights

private:

};

Material::Material(float ambient = .4f, float diffuse = 1.f, float specular = .5f, int shine = 32) : 
	ambientStrength(ambient), diffuseStrength(diffuse), specularStrength(specular), shininess(shine)
{
}

Material::~Material()
{
}

#endif // !MATERIAL_H
