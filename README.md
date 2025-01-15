# OpenGL Ray Tracer
A simple ray-tracer written in GLSL for Compute Shader.
This project serves as a demo to showcase the ray-tracer working in real-time with animations. The ray-tracer runs in the compute shader, however, calculations for animations, BVH and camera control runs on CPU side. 
There are two sample scenes inside this demo. Scene 1 - a monkey scene contains 1240 shapes (planes, spheres and triangles) with animated spheres. The scene runs in resolution 800x600 on NVIDIA RTX 3070 with 30+ FPS. Scene 2 - a car scene contains 4022 triangles and 100 spheres and runs with 10 FPS on average. The car has rotating wheels. 

Demo video:

<a href="https://youtu.be/EGRmm_cyw8g" target="_blank"><img src="http://img.youtube.com/vi/EGRmm_cyw8g/0.jpg" 
alt="Demo video of the project" width="240" height="180" border="10" /></a>

## Implementation Details
The ray-tracer runs in the Compute shader. It requires certain flatten structures (padded to 16bit) to understand the scene:
| Light |
|-------|
|vec3 position|
|float padding1|
|vec3 color|
|float padding2|

| Camera |
|--------|
|vec3 Position|
|float aspectRatio|
|vec3 Front|
|float padding2|
|vec3 Up|
|float padding3|
|vec3 Right|
|float padding4|
|float fov|
|vec3 padding5|

| Material |
|-------|
|vec3 color|
|float fresnelStrength|
|float ambientStrength|
|float diffuseStrength|
|float specularStrength|
|int shininess|

| Shape |
|-------|
|int type|
|vec3 padding|
|Material material|
|vec3 sphereCenter| 
|float sphereRadius|
|vec3 planeNormal|
|float planeD|
|vec3 wallStart|
|float wallWidth|
|float wallHeight|
|vec3 padding1|
|vec3 triP1|
|float padding2|
|vec3 triP2|
|float padding3|
|vec3 triP3|
|float padding4|

Shape *type* is 0 for Sphere, 1 for Plane, 2 for Wall (a wall is a plane with boundaries), 3 for Triangle

| Node |
|------|
vec3 boundsMin
float padding1
vec3 boundsMax
float padding2
int leftChild
int rightChild
int startShapeIdx
int numShapes

This is a BVH node structure with its bounding box properties, index of left and right child in a linked list of nodes, first index in the *bvhIndices* (contains a shape index on this position) and number of shapes inside the bounding box (how many indices to draw from *bvhIndices*.

Those structures are binded to SSBOs on 5 locations. 
```
layout(rgba32f, binding = 0) uniform image2D imgOutput;
layout(std430, binding = 1) buffer LightBuffer{
    Light light;
};
layout(std430, binding = 2) buffer CameraBuffer{
    Camera camera;
};
layout(std430, binding = 3) buffer ShapesBuffer{
    Shape shapes[];
};
layout(std430, binding = 4) buffer BVHBuffer{
    Node bvhNodes[];
};
layout(std430, binding = 5) buffer IndicesBuffer{
    int bvhIndices[];
};
```
The first layout is used for the output texture, which is then rendered on the quad in fragment shader. This is just to make output of the compute shader visible, since it doesn't have access to the frame buffer.

Before running the shader code in your application, ensure uniforms are set as well.
