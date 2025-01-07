#ifndef BOUNDINGBOX_H
#define BOUNDINGBOX_H

#include "glm/glm.hpp"
#include "src/shapes/triangle.hpp"
#include "src/shapes/sphere.hpp"
#include "src/shapes/wall.hpp"
#include "src/mesh.hpp"


class BoundingBox
{
public:
	BoundingBox();
	~BoundingBox();

	void growToInclude(glm::vec3 point);
	void growToInclude(Triangle triangle);
	void growToInclude(Sphere sphere);
	void growToInclude(Wall wall);
	void growToInclude(Mesh mesh);

	void growToInclude(std::unique_ptr<Shape> &shape);

    //int buildBVH(std::vector<std::unique_ptr<Shape>>& shapes, std::vector<BVHNode>& bvhNodes, int start, int end);

	glm::vec3 Min, Max;

    glm::vec3 center() const {
        return (Min + Max) * 0.5f;
    }

private:
};

BoundingBox::BoundingBox()
{
	Min = glm::vec3(INFINITY);
	Max = glm::vec3(INFINITY * (-1.f));
}

BoundingBox::~BoundingBox()
{
}

inline void BoundingBox::growToInclude(glm::vec3 point)
{
	Min = glm::min(Min, point);
	Max = glm::max(Max, point);
}

inline void BoundingBox::growToInclude(Triangle triangle)
{
    if (std::isfinite(triangle.a.x) && std::isfinite(triangle.b.x) && std::isfinite(triangle.c.x)) {
        growToInclude(triangle.a);
        growToInclude(triangle.b);
        growToInclude(triangle.c);
    }
    else {
        std::cerr << "Invalid triangle vertices!" << std::endl;
        std::cout << "Triangle:" << std::endl;
        std::cout << "P1 " << triangle.a.x << " " << triangle.a.y << " " << triangle.a.z << std::endl;
        std::cout << "P2 " << triangle.b.x << " " << triangle.b.y << " " << triangle.b.z << std::endl;
        std::cout << "P3 " << triangle.c.x << " " << triangle.c.y << " " << triangle.c.z << std::endl;
        std::cout << std::endl;
    }
}

inline void BoundingBox::growToInclude(Sphere sphere)
{
	growToInclude(sphere.m_center + sphere.m_radius);
	growToInclude(sphere.m_center - sphere.m_radius);
}

inline void BoundingBox::growToInclude(Wall wall)
{
	growToInclude(wall.start);

	glm::vec3 arbitrary = glm::vec3(1, 0, 0);
	if (glm::abs(glm::dot(arbitrary, wall.m_normal)) > 0.999f) {
		arbitrary = glm::vec3(0, 1, 0);
	}
	glm::vec3 t1 = glm::normalize(glm::cross(wall.m_normal, arbitrary));
	glm::vec3 t2 = glm::normalize(glm::cross(wall.m_normal, t1));

	glm::vec3 end = wall.start + wall.width * t1 + wall.height * t2;
	
	growToInclude(end);


}

inline void BoundingBox::growToInclude(Mesh mesh)
{
	auto triangles = mesh.mesh2triangles();
	for (auto triangle : triangles) {
		growToInclude(triangle);
	}
}

inline void BoundingBox::growToInclude(std::unique_ptr<Shape> &shape)
{
	if (auto sphere = dynamic_cast<Sphere*>(shape.get()))
		growToInclude(*sphere);
	else if (auto wall = dynamic_cast<Wall*>(shape.get()))
		growToInclude(*wall);
	else if (auto triangle = dynamic_cast<Triangle*>(shape.get()))
		growToInclude(*triangle);
}

//inline int BoundingBox::buildBVH(std::vector<std::unique_ptr<Shape>>& shapes, std::vector<BVHNode>& bvhNodes, int start, int end) {
//    
//}

//inline int BoundingBox::buildBVH(std::vector<std::unique_ptr<Shape>>& shapes, std::vector<BVHNode>& bvhNodes, int start, int end) {
//    //std::cout << "start " << start << "   end " << end << "   size " << shapes.size() << std::endl;
//    //assert(start >= 0 && end <= shapes.size() && start < end);
//    
//    BVHNode node;
//
//    // Calculate bounding box for the current range of shapes
//    BoundingBox bb;
//    for (int i = start; i < end; ++i) {
//        const Shape* shape = shapes[i].get();
//        if (auto sphere = dynamic_cast<const Sphere*>(shape))
//            bb.growToInclude(*sphere);
//        else if (auto wall = dynamic_cast<const Wall*>(shape))
//            bb.growToInclude(*wall);
//        else if (auto triangle = dynamic_cast<const Triangle*>(shape))
//            bb.growToInclude(*triangle);
//        
//    }
//    node.boundsMin = bb.Min;
//    node.boundsMax = bb.Max;
//
//    // Base case: single shape (leaf node)
//    if (end - start <= 1) {
//        node.leftChild = -1;
//        node.rightChild = -1;
//        node.shapeIndex = start; // Store the index of the shape in the leaf node
//        bvhNodes.push_back(node);
//        //std::cout << "added shape: " << start << "   " << bvhNodes.size() - 1 << std::endl;
//        return bvhNodes.size() - 1; // Return the index of the newly added node
//    }
//
//    // Compute the center point of each shape in the bounding box along the best axis
//    int bestAxis = 0;
//    float maxExtent = bb.Max.x - bb.Min.x;
//    if (bb.Max.y - bb.Min.y > maxExtent) {
//        bestAxis = 1;
//        maxExtent = bb.Max.y - bb.Min.y;
//    }
//    if (bb.Max.z - bb.Min.z > maxExtent) {
//        bestAxis = 2;
//    }
//
//    float midpoint = 0.0f;
//    for (int i = start; i < end; ++i) {
//        const Shape* shape = shapes[i].get();
//        BoundingBox shapeBB;
//        if (auto sphere = dynamic_cast<const Sphere*>(shape))
//            shapeBB.growToInclude(*sphere);
//        else if (auto wall = dynamic_cast<const Wall*>(shape))
//            shapeBB.growToInclude(*wall);
//        else if (auto triangle = dynamic_cast<const Triangle*>(shape))
//            shapeBB.growToInclude(*triangle);
//
//        midpoint += shapeBB.center()[bestAxis];
//    }
//    midpoint /= (end - start);
//
//    // Partition shapes based on midpoint along the best axis
//    auto partitionPoint = std::partition(shapes.begin() + start, shapes.begin() + end,
//        [&](const std::unique_ptr<Shape>& shapePtr) {
//            const Shape* shape = shapePtr.get();
//            BoundingBox shapeBB;
//            if (auto sphere = dynamic_cast<const Sphere*>(shape))
//                shapeBB.growToInclude(*sphere);
//            else if (auto wall = dynamic_cast<const Wall*>(shape))
//                shapeBB.growToInclude(*wall);
//            else if (auto triangle = dynamic_cast<const Triangle*>(shape))
//                shapeBB.growToInclude(*triangle);
//
//            return shapeBB.center()[bestAxis] < midpoint;
//        });
//
//    int mid = std::distance(shapes.begin(), partitionPoint);
//
//    // Recursively build child nodes and store their indices
//    node.leftChild = buildBVH(shapes, bvhNodes, start, mid);
//    node.rightChild = buildBVH(shapes, bvhNodes, mid, end);
//    node.shapeIndex = -1; // Non-leaf node
//
//    // Store the current node and return its index
//    bvhNodes.push_back(node);
//    return bvhNodes.size() - 1;
//}


#endif // !BOUNDINGBOX_H
