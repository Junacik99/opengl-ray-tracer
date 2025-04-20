#ifndef TRIANGLE_H
#define TRIANGLE_H

#include "glm/glm.hpp"
#include "plane.hpp"
#include <embree4/rtcore.h>
#include <embree4/rtcore_ray.h>
#include <iostream>

enum Intersect_alg
{
	BARYCENTRIC,
	MT,
	EMBREE
};

class Triangle : public Plane
{
public:
	Triangle(glm::vec3 p1, glm::vec3 p2, glm::vec3 p3);
	~Triangle();

	glm::vec3 a;
	glm::vec3 b;
	glm::vec3 c;

	glm::vec3 center() const {
		return (a + b + c) / 3.0f;
	}

	void invert_normal();

	Intersection get_intersection(Ray ray) const override;

	// Intersection algorithm
	Intersect_alg int_alg = BARYCENTRIC;

	// Set global scene for triangles;
	static void setTriangleScene(RTCScene* scene);
	
private:
	glm::vec3 get_normal(glm::vec3 p1, glm::vec3 p2, glm::vec3 p3);
	static RTCScene* globalScene;
};

Triangle::Triangle(glm::vec3 p1, glm::vec3 p2, glm::vec3 p3)
	: a(p1), b(p2), c(p3), Plane(get_normal(p1, p2, p3), p1)
{
	if (globalScene) {
		RTCGeometry geom = rtcNewGeometry(rtcGetSceneDevice(*globalScene), RTC_GEOMETRY_TYPE_TRIANGLE);
		float* vertices = (float*)rtcSetNewGeometryBuffer(
			geom, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, 3 * sizeof(float), 3);

		vertices[0] = p1.x; vertices[1] = p1.y; vertices[2] = p1.z;
		vertices[3] = p2.x; vertices[4] = p2.y; vertices[5] = p2.z;
		vertices[6] = p3.x; vertices[7] = p3.y; vertices[8] = p3.z;

		unsigned* indices = (unsigned*)rtcSetNewGeometryBuffer(
			geom, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, 3 * sizeof(unsigned), 1);

		indices[0] = 0; indices[1] = 1; indices[2] = 2;

		rtcCommitGeometry(geom);
		rtcAttachGeometry(*globalScene, geom);
		rtcReleaseGeometry(geom);

		// Commit the scene to finalize the geometry
		rtcCommitScene(*globalScene);

	}
}

Triangle::~Triangle()
{
}

RTCScene* Triangle::globalScene = nullptr;

inline void Triangle::setTriangleScene(RTCScene* scene)
{
	globalScene = scene;
}

inline glm::vec3 Triangle::get_normal(glm::vec3 p1, glm::vec3 p2, glm::vec3 p3) {
	// https://stackoverflow.com/questions/19350792/calculate-normal-of-a-single-triangle-in-3d-space
	auto A = p2 - p1;
	auto B = p3 - p1;
	glm::vec3 result;
	result.x = A.y * B.z - A.z * B.y;
	result.y = A.z * B.x - A.x * B.z;
	result.z = A.x * B.y - A.y * B.x;
	return result;
}

inline void Triangle::invert_normal() {
	m_normal = -m_normal;
	d = -(glm::dot(m_normal, a));
}

inline Intersection Triangle::get_intersection(Ray ray) const {
	// Intersection using Barycentric coordinates
	if (int_alg == BARYCENTRIC) {
		Intersection baseIntersection = Plane::get_intersection(ray);
		if (baseIntersection.intersect_type == NONE) return Intersection(NONE);

		auto hitPoint = baseIntersection.hit_point;

		// Compute vectors for edges and point-to-vertex
		glm::vec3 edge1 = b - a;
		glm::vec3 edge2 = c - a;
		glm::vec3 toPoint = hitPoint - a;

		// Barycentric coordinates
		float d00 = glm::dot(edge1, edge1);
		float d01 = glm::dot(edge1, edge2);
		float d11 = glm::dot(edge2, edge2);
		float d20 = glm::dot(toPoint, edge1);
		float d21 = glm::dot(toPoint, edge2);

		float denom = d00 * d11 - d01 * d01;
		float v = (d11 * d20 - d01 * d21) / denom;
		float w = (d00 * d21 - d01 * d20) / denom;
		float u = 1.0 - v - w;

		if (u < 0 || v < 0 || w < 0) {
			return Intersection(NONE);
		}

		return baseIntersection;
	}
	// Moller-Trumbore
	else if (int_alg == MT) {

	}
	// TODO: intel embree check
	else if (int_alg == EMBREE) {
		if (globalScene == nullptr) return Intersection(NONE);

		// Set up Embree ray
		RTCRayHit rayhit;
		memset(&rayhit, 0, sizeof(RTCRayHit));

		rayhit.ray.org_x = ray.get_start().x;
		rayhit.ray.org_y = ray.get_start().y;
		rayhit.ray.org_z = ray.get_start().z;

		rayhit.ray.dir_x = ray.get_dir().x;
		rayhit.ray.dir_y = ray.get_dir().y;
		rayhit.ray.dir_z = ray.get_dir().z;

		rayhit.ray.tnear = 0.0f;
		rayhit.ray.tfar = std::numeric_limits<float>::infinity();
		rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;

		// Intersect with Embree
		rtcIntersect1(*globalScene, &rayhit);

		// TODO: must be an error here, ray never hits the triangle??
		if (rayhit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
			// Hit
			glm::vec3 hitPoint = ray.get_start() + ray.get_dir() * rayhit.ray.tfar;
			return Intersection(INNER, hitPoint);
		}
		else {
			return Intersection(NONE);
		}
	}
	
	else {
		std::cout << "Invalid CPU triangle intersection type" << std::endl;
		return Intersection(NONE);
	}
	
}

#endif // !TRIANGLE_H
