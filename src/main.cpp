#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <glad/glad.h>
#include <glfw3.h>
#include <iostream>
#include "shader.hpp"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "camera.hpp"
#include "ray.hpp"
#include "shapes/shape.hpp"
#include "shapes/sphere.hpp"
#include "shapes/plane.hpp"
#include "shapes/wall.hpp"
#include "shapes/triangle.hpp"
#include "computeShader.hpp"
#include <vector>
#include "material.hpp"
#include "light.hpp"
#include <glm/gtx/string_cast.hpp>
#include "flatStructures.hpp"
#include "model.hpp"
#include "BoundingBox.hpp"
#include <random>
#include <embree4/rtcore.h>
#include <embree4/rtcore_ray.h>

// For resizing window
void framebuffer_size_callback(GLFWwindow* window, int width, int height);

// Processing input
void processInput(GLFWwindow* window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);

// Render quad with the texture obtained from Compute Shader
void renderQuad();

// Phong shading (for CPU only)
glm::vec3 phong(const glm::vec3& point, const glm::vec3& normal, const glm::vec3& viewDir, const glm::vec3& objectColor, glm::vec3 lightPos, glm::vec3 lightColor, Material material);

void generateScene1();	// Generate scene with monkeys
void generateScene2();	// A scene with the car
void generateScene3();	// Triangle
int SCENE = 3;			// 1 - monkeys | 2 - car | 3 - Triangle

// Animate objects
void bounceSphere(Sphere* sphere, float elapsedTime, float amplitude, float frequency);
void updateWheelAnimations(float elapsedTime);

// Simpler and slower ray-tracing on CPU
void cpuRayTracer(std::vector<float>& pixelData);

// Debugging functions
void printMaterial(Material mat);
void printTriangle(Triangle triangle);
void printPoint(glm::vec3 point);

// Utility functions
float randomFloat(float min, float max);
float randomFloat01();
FlatCamera serializeCamera(Camera cam);
FlatLight serializeLight(Light light);
void serializeScene(FlatScene& flatScene);
void serializeBVH(std::vector<FlatNode>& nodes, std::vector<int>& indices);
FlatShape serializeShape(const std::unique_ptr<Shape>& shape);

// Serialize animated shapes every frame
void updateScene(FlatScene& flatScene, GLuint ssbo);


// BVH
class Node
{
public:

	BoundingBox box;

	int leftChild;
	int rightChild;

	std::vector < int > shapesIndices;

private:

};
void updateBVH();												// Enlarge nodes on animation
void split(std::unique_ptr<Node>& parentNode, int depth = 15);	// Divide volume of node into two if possible
int buildBVH(int maxDepth = 15);								// Build BVH

// Logical structure of the scene (not sent to GPU)
struct Scene
{
	Camera camera;
	Light light;
	std::vector < std::unique_ptr< Shape >> shapes;
	
	std::vector<std::unique_ptr<Node>> bvhNodes;

} scene;

// Arbitrary structure for animation of scene 2 with car
struct Wheel {
	std::vector<int> shapeIndices;
	glm::vec3 rotationAxis;
	float rotationAngle = 0;
	glm::vec3 center;
} wheels[4];


// Window size
const int WIDTH = 800;
const int HEIGHT = 600;

bool rtxon = false;					// Use GPU (true) or CPU ray-tracing
bool animate = false;				// Animate certain objects
bool useMollerTrumbore = false;		// For triangle intersection checks

std::vector<int> animatedIndices;


/* Camera */
// Camera axes
float lastX = WIDTH/2, lastY = HEIGHT/2; // Set cursor to the center
bool firstMouse = true;
bool useMouse = true;


// Frame inference computation
float deltaTime = 0.0f;	// Time between current frame and last frame
float lastFrame = 0.0f; // Time of last frame

// Ray computation
int maxBounces = 3;
bool useFresnel = false;
bool useBVH = true;
Intersect_alg intersectionAlgorithm = EMBREE; // Intersection algorithm (BARYCENTRIC, MT, EMBREE)

// Embree device and scene
RTCDevice g_embreeDevice = nullptr;
RTCScene g_embreeScene = nullptr;
void initEmbree(); 
void cleanupEmbree(); 


int main(void)
{
	// Init glfw
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	/* Window */
	// Create window
	GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "OpenGL project", NULL, NULL);
	if (window == NULL) {
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	// Init glad
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	// Set callback function for resizing the window
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

	// Swap interval to bypass fps lock (60)
	glfwSwapInterval(0);

	// Set mouse callback
	glfwSetCursorPosCallback(window, mouse_callback);

	/* Shaders */
	// Set up texture for compute shader
	unsigned texture;
	glGenTextures(1, &texture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, WIDTH, HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
	glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);

	// Compute shader (cannot be used with others)
	ComputeShader computeShader("src/shaders/cpu_shader.comp");
	ComputeShader computeShaderGPU("src/shaders/gpu_shader.comp");

	// Texture buffer
	std::vector<float> pixelData(WIDTH * HEIGHT * 4, 0.0f); // Initialize to 0

	// VS and FS for screen quad
	Shader screenQuad("src/shaders/shader.vert", "src/shaders/shader.frag");

	/* Embree */
	initEmbree();

	/* Scene */
	switch (SCENE)
	{
	case 1: generateScene1(); break;
	case 2: generateScene2(); break;
	case 3: generateScene3(); break;
	default:
		generateScene1();
		break;
	}

	

	/* SSBO (Shader Storage Buffer Object */
	FlatScene flatScene;
	serializeScene(flatScene); // Serialize scene

	std::cout << "BVH Indices (" << bvhIndices.size() << "):" << std::endl;
	/*for (auto idx : bvhIndices)
		std::cout << idx << std::endl;*/

	std::cout << std::endl << "Nodes (" << flatNodes.size() << "):" << std::endl;
	/*for (int i = 0; i < flatNodes.size(); ++i) {
		std::cout << "node " << i << std::endl;
		std::cout << "Min:" << std::endl;
		printPoint(flatNodes[i].boundsMin);
		std::cout << "Max:" << std::endl;
		printPoint(flatNodes[i].boundsMax);
		std::cout << "L child: " << flatNodes[i].leftChild << " R child: " << flatNodes[i].rightChild << std::endl;
		std::cout << "Start shape idx: " << flatNodes[i].startShapeIdx << " NumShapes: " << flatNodes[i].numShapes << std::endl << std::endl;
	}*/

	// send light
	GLuint ssbolight;
	glGenBuffers(1, &ssbolight);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbolight);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(FlatLight), &flatScene.light, GL_DYNAMIC_COPY);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbolight);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); // unbind

	// send camera
	GLuint ssbocamera;
	glGenBuffers(1, &ssbocamera);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbocamera);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(FlatCamera), &flatScene.camera, GL_DYNAMIC_COPY);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssbocamera);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); // unbind

	// send shapes
	GLuint ssboshapes;
	glGenBuffers(1, &ssboshapes);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboshapes);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(FlatShape) * flatScene.shapes.size(), flatScene.shapes.data(), GL_DYNAMIC_COPY);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssboshapes);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); // unbind

	// send BVH
	GLuint ssbobvhboxes;
	glGenBuffers(1, &ssbobvhboxes);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbobvhboxes);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(FlatNode) * flatNodes.size(), flatNodes.data(), GL_STATIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, ssbobvhboxes);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); // unbind

	GLuint ssbobvhindices;
	glGenBuffers(1, &ssbobvhindices);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbobvhindices);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(int) * bvhIndices.size(), bvhIndices.data(), GL_STATIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, ssbobvhindices);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); // unbind


	/* GUI */
	// imgui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	(void)io;
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 430");


	/* Render loop */
	while (!glfwWindowShouldClose(window)) {
		// Init gui
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		float fps = 1.f / deltaTime;

		// Input
		processInput(window);

		if (!rtxon) { // CPU ray tracing
			// No fresnel, shadows... Just laggy ray tracing with diffuse colors
			/***********************************************************************************************/
			cpuRayTracer(pixelData);

			// Compute shader dispatch
			computeShader.use();
			glDispatchCompute((unsigned)WIDTH, (unsigned)HEIGHT, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			// Render image to quad
			glClearColor(.2f, .3f, .3f, 1.f);
			screenQuad.use();
			screenQuad.setInt("tex", 0);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, texture);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, WIDTH, HEIGHT, GL_RGBA, GL_FLOAT, pixelData.data());
			renderQuad();
			/***********************************************************************************************/
		}
		else { // GPU ray tracing
			/***********************************************************************************************/
			// Update scene
			flatScene.camera = serializeCamera(scene.camera);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbocamera);
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(FlatCamera), &flatScene.camera);

			flatScene.light = serializeLight(scene.light);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbolight);
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(FlatLight), &flatScene.light);

			if (animate) {
				// Only update animated shapes (spheres)
				updateScene(flatScene, ssboshapes);

				// Update BVH
				updateBVH();
				serializeBVH(flatNodes, bvhIndices);
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbobvhboxes);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(FlatNode) * flatNodes.size(), flatNodes.data());

			}
			
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); // unbind

			
			// Compute shader dispatch
			computeShaderGPU.use();
			glDispatchCompute((unsigned)WIDTH, (unsigned)HEIGHT, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			// Set window resolution in shader
			computeShaderGPU.setVec2("screenRes", glm::vec2(WIDTH, HEIGHT));
			computeShaderGPU.setInt("maxBounces", maxBounces);
			computeShaderGPU.setBool("useBVH", useBVH);
			computeShaderGPU.setBool("useFresnel", useFresnel);
			computeShaderGPU.setBool("useMollerTrumbore", useMollerTrumbore);

			// Render image to quad
			glClearColor(0, 0, 0, 1.f);
			screenQuad.use();
			renderQuad();


			/***********************************************************************************************/
		}

		// Create GUI window
		ImGui::Begin("GUI window");
		ImGui::Text("Ray Tracer");
		ImGui::Text("FPS: %.2f", fps);

		ImGui::Checkbox("RTX ON", &rtxon);
		ImGui::SliderInt("Max bounces", &maxBounces, 1, 10);
		ImGui::Checkbox("Use BVH", &useBVH);
		ImGui::Checkbox("Fresnel", &useFresnel);
		ImGui::Checkbox("Animate", &animate);
		ImGui::Checkbox("Moller-Trumbore", &useMollerTrumbore);

		ImGui::Text("Main ball material");
		auto ballColorV = scene.shapes[0]->material.color;
		float ballColor[4] = { ballColorV.r, ballColorV.g, ballColorV.b, 1.f };
		ImGui::ColorEdit4("Diffuse color", ballColor);
		scene.shapes[0]->material.color = glm::vec3(ballColor[0], ballColor[1], ballColor[2]);
		ImGui::SliderFloat("Fresnel strength", &scene.shapes[0]->material.fresnelStrength, 0, 1);
		ImGui::SliderFloat("Ambient", &scene.shapes[0]->material.ambientStrength, 0, 1);
		ImGui::SliderFloat("Diffuse", &scene.shapes[0]->material.diffuseStrength, 0, 1);
		ImGui::SliderFloat("Specular", &scene.shapes[0]->material.specularStrength, 0, 1);
		ImGui::SliderInt("Shininess", &scene.shapes[0]->material.shininess, 0, 100);

		// Dropdown menu for intersection algorithm selection
		const char* items[] = { "Barycentric", "Moller-Trumbore", "Embree" };
		const char* currentItem = items[intersectionAlgorithm];
		if (ImGui::BeginCombo("Intersection algorithm", currentItem)) {
			for (int n = 0; n < IM_ARRAYSIZE(items); n++) {
				bool isSelected = (currentItem == items[n]);
				if (ImGui::Selectable(items[n], isSelected)) {
					currentItem = items[n];
					intersectionAlgorithm = static_cast<Intersect_alg>(n);
				}
				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::Text("Light");
		float lightColor[4] = { scene.light.color.r, scene.light.color.g, scene.light.color.b, 1.f };
		ImGui::ColorEdit4("Color", lightColor);
		scene.light.color = glm::vec3(lightColor[0], lightColor[1], lightColor[2]);
		ImGui::SliderFloat("Intensity", &scene.light.intensity, 0, 100);
		scene.light.updateColor();
		ImGui::SliderFloat("X pos", &scene.light.position.x, -17, 17);
		ImGui::SliderFloat("Y pos", &scene.light.position.y, -17, 17);
		ImGui::SliderFloat("Z pos", &scene.light.position.z, -17, 17);

		if (SCENE == 1) {
			ImGui::Text("Mirror");
			ImGui::SliderFloat("fresnel", &scene.shapes[4]->material.fresnelStrength, 0, 1);
			ImGui::SliderFloat("ambient", &scene.shapes[4]->material.ambientStrength, 0, 1);
			ImGui::SliderFloat("diffuse", &scene.shapes[4]->material.diffuseStrength, 0, 1);
			ImGui::SliderFloat("specular", &scene.shapes[4]->material.specularStrength, 0, 1);
			ImGui::SliderInt("shininess", &scene.shapes[4]->material.shininess, 0, 100);
		}

		ImGui::End();

		// Render UI elements
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		// Animate objects
		if (animate) {
			// Scene 1
			if (SCENE == 1) {
				if (auto* sphere = dynamic_cast<Sphere*>(scene.shapes[0].get()))
					bounceSphere(sphere, currentFrame, 10, 1);
				if (auto* sphere = dynamic_cast<Sphere*>(scene.shapes[1].get()))
					bounceSphere(sphere, currentFrame, 7, 0.8);
				if (auto* sphere = dynamic_cast<Sphere*>(scene.shapes[2].get()))
					bounceSphere(sphere, currentFrame, 15, 1.5);
			}
			// Scene 2
			else if (SCENE == 2) {
				updateWheelAnimations(currentFrame);
			}
			// Scene 3
			else {

			}
			
		}
	
		// swap buffers and poll io events
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwTerminate();
	return 0;
}

// renderQuad() renders a 1x1 XY quad in NDC
// -----------------------------------------
unsigned int quadVAO = 0;
unsigned int quadVBO;
void renderQuad()
{
	if (quadVAO == 0)
	{
		float quadVertices[] = {
			// positions        // texture Coords
			-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
			-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
			 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
			 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
		};
		// setup plane VAO
		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);
		glBindVertexArray(quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	}
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
}

// Resize window and adjust viepowrt callback
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
	glViewport(0, 0, width, height);
}

// Handle input
void processInput(GLFWwindow* window) {
	// Close window
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);


	// Camera control
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		scene.camera.ProcessKeyboard(FORWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		scene.camera.ProcessKeyboard(BACKWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		scene.camera.ProcessKeyboard(LEFT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		scene.camera.ProcessKeyboard(RIGHT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
		scene.camera.ProcessKeyboard(UP, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
		scene.camera.ProcessKeyboard(DOWN, deltaTime);

	scene.camera.MovementSpeed = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)) ? SPEED * SPEEDAMPLIFIER : SPEED;

	useMouse = glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_RELEASE;
	
				
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
	if (firstMouse) // initially set to true
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xOffset = xpos - lastX;
	float yOffset = ypos - lastY;
	lastX = xpos;
	lastY = ypos;

	if (useMouse)
		scene.camera.ProcessMouseMovement(xOffset, yOffset);
}

glm::vec3 phong(const glm::vec3& point, const glm::vec3& normal, const glm::vec3& viewDir, const glm::vec3& objectColor, glm::vec3 lightPos, glm::vec3 lightColor, Material material) {

	// Material properties
	float ambientStrength = material.ambientStrength;
	float diffuseStrength = material.diffuseStrength;
	float specularStrength = material.specularStrength;
	int shininess = material.shininess;

	// Ambient component
	glm::vec3 ambient = ambientStrength * lightColor;

	// Diffuse component
	glm::vec3 lightDir = glm::normalize(lightPos - point); // Direction from point to light source
	float diff = glm::max(glm::dot(normal, lightDir), 0.0f);
	glm::vec3 diffuse = diffuseStrength * diff * lightColor;

	// Specular component
	glm::vec3 specular(0);
	if (diff > 0.f) {
		glm::vec3 reflectDir = glm::reflect(-lightDir, normal); // Reflection direction
		float spec = glm::pow(glm::max(glm::dot(viewDir, reflectDir), 0.0f), shininess);
		specular = specularStrength * spec * lightColor;
	}
	

	// Combine results
	glm::vec3 result = (ambient + diffuse + specular) * objectColor;
	return result;
}

void generateScene1()
{
	// Camera 
	scene.camera = Camera();
	scene.camera.Position = glm::vec3(30.0f, -5.0f, 40.0f);
	scene.camera.aspectRatio = float(WIDTH) / HEIGHT;

	// add light
	scene.light = Light(glm::vec3(0, -14, 0), glm::vec3(1), 50);

	// add shapes
	scene.shapes.push_back(std::make_unique<Sphere>(glm::vec3(0, 10, -8), 5.f));
	scene.shapes[0]->material.color = glm::vec3(0, 0.37f, 0);
	scene.shapes[0]->material.fresnelStrength = 0;
	scene.shapes[0]->material.ambientStrength = 0.2f;
	scene.shapes[0]->material.diffuseStrength = 1;
	scene.shapes[0]->material.specularStrength = 0.1f;
	animatedIndices.push_back(scene.shapes.size() - 1);

	scene.shapes.push_back(std::make_unique<Sphere>(glm::vec3(12, 10, -8), 4.f));
	scene.shapes[scene.shapes.size() - 1]->material.color = glm::vec3(0.58f, 0.18f, 0.48f);
	scene.shapes[scene.shapes.size() - 1]->material.fresnelStrength = 0;
	scene.shapes[scene.shapes.size() - 1]->material.ambientStrength = 0;
	scene.shapes[scene.shapes.size() - 1]->material.diffuseStrength = 0.5f;
	scene.shapes[scene.shapes.size() - 1]->material.specularStrength = 0;
	animatedIndices.push_back(scene.shapes.size() - 1);

	scene.shapes.push_back(std::make_unique<Sphere>(glm::vec3(20, 7.5, -8), 2.5f));
	scene.shapes[scene.shapes.size() - 1]->material.color = glm::vec3(0.8f, 0.2f, 0.8f);
	scene.shapes[scene.shapes.size() - 1]->material.fresnelStrength = 1;
	scene.shapes[scene.shapes.size() - 1]->material.ambientStrength = 0.06f;
	scene.shapes[scene.shapes.size() - 1]->material.diffuseStrength = 0.06f;
	scene.shapes[scene.shapes.size() - 1]->material.specularStrength = 0.5f;
	animatedIndices.push_back(scene.shapes.size() - 1);

	scene.shapes.push_back(std::make_unique<Sphere>(glm::vec3(0, 23, -8), 1.5f));
	scene.shapes[scene.shapes.size() - 1]->material.color = glm::vec3(0, 0.37f, 0);
	scene.shapes[scene.shapes.size() - 1]->material.fresnelStrength = 0;
	scene.shapes[scene.shapes.size() - 1]->material.ambientStrength = 0;
	scene.shapes[scene.shapes.size() - 1]->material.diffuseStrength = 0.5f;
	scene.shapes[scene.shapes.size() - 1]->material.specularStrength = 0;

	// wall (mirror)
	scene.shapes.push_back(std::make_unique<Wall>(glm::vec3(-15, 23, 10), 30, 20, glm::vec3(-1, 0.2f, 0)));
	scene.shapes[scene.shapes.size() - 1]->material.fresnelStrength = 1;
	scene.shapes[scene.shapes.size() - 1]->material.ambientStrength = 0.1f;
	scene.shapes[scene.shapes.size() - 1]->material.diffuseStrength = 0;
	scene.shapes[scene.shapes.size() - 1]->material.specularStrength = 1;

	// triangle
	auto p1 = glm::vec3(-15, 20, 25);
	auto p2 = glm::vec3(-12, 20, 10);
	auto p3 = glm::vec3(-15, 0, 20);
	auto triangle = Triangle(p1, p2, p3);
	triangle.invert_normal();
	scene.shapes.push_back(std::make_unique<Triangle>(triangle));
	scene.shapes[scene.shapes.size() - 1]->material.color = glm::vec3(0.19f, 0.66f, 0.32f);
	scene.shapes[scene.shapes.size() - 1]->material.fresnelStrength = 1;
	scene.shapes[scene.shapes.size() - 1]->material.ambientStrength = 0.06f;
	scene.shapes[scene.shapes.size() - 1]->material.diffuseStrength = 0.06f;
	scene.shapes[scene.shapes.size() - 1]->material.specularStrength = 0.5f;

	/* 3D Model */
	//auto model = Model("models/lowpolymonkey.obj");
	auto model = Model("models/monkey.obj");
	//auto model = Model("models/sphere.obj");
	auto mesh = model.meshes[0];
	mesh.origin = glm::vec3(0, 0, -30);
	auto meshTriangles = mesh.mesh2triangles();
	for (int i = 0; i < meshTriangles.size(); ++i) {
		auto triangle = meshTriangles[i];
		scene.shapes.push_back(std::make_unique<Triangle>(triangle.a, triangle.b, triangle.c));
		scene.shapes[scene.shapes.size() - 1]->material.color = glm::vec3(179.f/255, 165.f/255, 61.f/255);
		scene.shapes[scene.shapes.size() - 1]->material.fresnelStrength = 1;

		scene.shapes[scene.shapes.size() - 1]->material.ambientStrength = 0.2f;
		scene.shapes[scene.shapes.size() - 1]->material.diffuseStrength = 0.8f;
		scene.shapes[scene.shapes.size() - 1]->material.specularStrength = 0.1f;
	}
	std::cout << "Triangles added: " << meshTriangles.size() << std::endl;

	auto model2 = Model("models/lowpolymonkey.obj");
	for (auto mesh : model2.meshes)
	{
		mesh.origin = glm::vec3(50, 0, -30);
		auto meshTriangles = mesh.mesh2triangles();
		for (int i = 0; i < meshTriangles.size(); ++i) {
			auto triangle = meshTriangles[i];
			scene.shapes.push_back(std::make_unique<Triangle>(triangle.a, triangle.b, triangle.c));
			scene.shapes[scene.shapes.size() - 1]->material.color = glm::vec3(0, 1, .9f);
			scene.shapes[scene.shapes.size() - 1]->material.fresnelStrength = 1;

			scene.shapes[scene.shapes.size() - 1]->material.ambientStrength = 0.2f;
			scene.shapes[scene.shapes.size() - 1]->material.diffuseStrength = 0.8f;
			scene.shapes[scene.shapes.size() - 1]->material.specularStrength = 0;
		}
		std::cout << "Triangles added: " << meshTriangles.size() << std::endl;
	}


	// More spheres
	for (int i = 0; i < 25; ++i) {

		float posx = randomFloat(-40, 40);
		float posz = randomFloat(-40, 40);

		scene.shapes.push_back(std::make_unique<Sphere>(glm::vec3(posx, 23, posz), 1.5f));
		scene.shapes[scene.shapes.size() - 1]->material.color = glm::vec3(randomFloat01(), randomFloat01(), randomFloat01());
		/*scene.shapes[scene.shapes.size() - 1]->material.fresnelStrength = randomFloat01();
		scene.shapes[scene.shapes.size() - 1]->material.ambientStrength = randomFloat01();
		scene.shapes[scene.shapes.size() - 1]->material.diffuseStrength = randomFloat01();
		scene.shapes[scene.shapes.size() - 1]->material.specularStrength = randomFloat01();*/
	}


	// bottom
	scene.shapes.push_back(std::make_unique<Wall>(glm::vec3(-100, 25, -100), 210, 210, glm::vec3(0, 1, 0)));
	scene.shapes[scene.shapes.size() - 1]->material.color = glm::vec3(0.65f, 0.17f, 0.35f);
	scene.shapes[scene.shapes.size() - 1]->material.specularStrength = 0;


	scene.camera.LookAt(scene.shapes[0]->origin);

	for (int i : animatedIndices) {
		scene.shapes[i]->animated = true;
	}


	// BVH
	int i = buildBVH(15);
	std::cout << "result: " << i << std::endl;

	std::cout << "shapes: " << scene.shapes.size() << std::endl;
}

void generateScene2() {
	// Camera 
	scene.camera = Camera();
	scene.camera.Position = glm::vec3(0, -10.0f, 40);
	scene.camera.aspectRatio = float(WIDTH) / HEIGHT;

	// add light
	scene.light = Light(glm::vec3(14.8f, -17, 17), glm::vec3(1), 26);

	// 3D Model
	glm::vec3 origin(0, 0, 0);
	auto model = Model("models/car.obj");
	for (int i=0; i < model.meshes.size(); ++i)
	{
		auto mesh = model.meshes[i];
		mesh.origin = origin;

		Material material;
		switch (i)
		{
		case 0: // Car
			material.color = glm::vec3(19.f/255, 7.f/255, 92.f/255);
			material.specularStrength = 0;
			break;
		case 1: // Wheels
		case 2:
		case 3:
		case 4:
			material.color = glm::vec3(.2f, .2f, .2f);
			material.specularStrength = 0;
			break;
		case 5: // Road
			material.color = glm::vec3(0);
			material.specularStrength = .25f;
			break;
		default:
			break;
		}

		glm::vec3 wheelCenter(0);

		auto meshTriangles = mesh.mesh2triangles();
		for (int j = 0; j < meshTriangles.size(); ++j) {
			int idx = scene.shapes.size();
			auto triangle = meshTriangles[j];
			scene.shapes.push_back(std::make_unique<Triangle>(triangle.a, triangle.b, triangle.c));
			scene.shapes[idx]->material = material;

			if (i >= 1 && i <= 4) {
				wheels[i-1].shapeIndices.push_back(idx);
				scene.shapes[idx]->animated = true;
				animatedIndices.push_back(idx);

				wheelCenter += triangle.a;
				wheelCenter += triangle.b;
				wheelCenter += triangle.c;
			}
		}
		std::cout << "Triangles added: " << meshTriangles.size() << std::endl;
		
		if (i >= 1 && i <= 4) {
			wheels[i-1].rotationAxis = glm::vec3(0, 0, 1);
			wheelCenter /= static_cast<float>(meshTriangles.size()*3);
			wheels[i-1].center = wheelCenter;
		}


	}

	// Add spheres as bg
	for (int i = 0; i < 100; ++i) {
		float posx = randomFloat(-30, 30);
		float posy = randomFloat(-15, 0);

		scene.shapes.push_back(std::make_unique<Sphere>(glm::vec3(posx, posy, -10), 1.5f));
		scene.shapes[scene.shapes.size() - 1]->material.color = glm::vec3(randomFloat01(), randomFloat01(), randomFloat01());

	}

	scene.camera.LookAt(origin);

	// BVH
	int i = buildBVH(25);
	std::cout << "result: " << i << std::endl;

	std::cout << "shapes: " << scene.shapes.size() << std::endl;
}

FlatCamera serializeCamera(Camera cam) {
	FlatCamera flatCam;
	flatCam.Position = cam.Position;
	flatCam.aspectRatio = cam.aspectRatio;
	flatCam.Front = cam.Front;
	flatCam.Up = cam.Up;
	flatCam.Right = cam.Right;
	flatCam.fov = cam.fov;

	return flatCam;
}

FlatLight serializeLight(Light light) {
	FlatLight flatLight;
	flatLight.position = light.position;
	flatLight.color = light.color;
	return flatLight;
}

void serializeScene(FlatScene& flatScene) {

	// Serialize camera
	flatScene.camera = serializeCamera(scene.camera);

	// Serialize light
	flatScene.light = serializeLight(scene.light);

	// Serialize shapes
	std::vector<FlatShape> flatShapes;
	for (const auto& shape : scene.shapes) {
		
		FlatShape flatShape = serializeShape(shape);

		flatShapes.push_back(flatShape);
	}

	flatScene.shapes = flatShapes;

	serializeBVH(flatNodes, bvhIndices);

}

void cpuRayTracer(std::vector<float>& pixelData) {
	// Calculate ray hits
	for (int y = 0; y < HEIGHT; ++y) {
		for (int x = 0; x < WIDTH; ++x) {
			Ray ray = scene.camera.GetRay(2.f * x / WIDTH - 1, 1.f - 2.f * y / HEIGHT); // flip y-axis

			glm::vec3 color = glm::vec3(); // BG color
			float closestDist = std::numeric_limits<float>::max();

			for (const auto& shape : scene.shapes) {
				// Set intersection algorithm for triangles
				if (auto triangle = dynamic_cast<Triangle*>(shape.get())) {
					triangle->int_alg = intersectionAlgorithm;
				}

				// Trace ray
				Intersection s_hit = shape->get_intersection(ray);
				if (s_hit.intersect_type == INNER) { // Hit!
					float dist = glm::distance(ray.get_start(), s_hit.hit_point);
					if (dist < closestDist) {
						closestDist = dist;

						auto point = s_hit.hit_point;
						auto normal = shape->get_normal(point);

						// Calculate lighting (Phong)
						color = phong(
							point,
							normal,
							ray.get_dir(),
							shape->material.color,
							scene.light.position,
							scene.light.color,
							shape->material);
					}
				}

				// Set color pixel in fragment shader
				int idx = (y * WIDTH + x) * 4;
				pixelData[idx + 0] = color.r;
				pixelData[idx + 1] = color.g;
				pixelData[idx + 2] = color.b;
				pixelData[idx + 3] = 1.f;
			}
		}
	}
}

void printMaterial(Material mat) {
	std::cout << "Color " << mat.color.r << " " << mat.color.g << " " << mat.color.b << std::endl;
	std::cout << "Fresnel " << mat.fresnelStrength << std::endl;
	std::cout << "Ambient " << mat.ambientStrength << std::endl;
	std::cout << "Diffuse " << mat.diffuseStrength << std::endl;
	std::cout << "Specular " << mat.specularStrength << std::endl;
	std::cout << "Shininess " << mat.shininess << std::endl;
}

void printTriangle(Triangle triangle) {
	using namespace std;

	cout << "Material:" << endl;
	printMaterial(triangle.material);
	cout << endl;

	cout << "Triangle:" << endl;
	cout << "P1 " << triangle.a.x << " " << triangle.a.y << " " << triangle.a.z << endl;
	cout << "P2 " << triangle.b.x << " " << triangle.b.y << " " << triangle.b.z << endl;
	cout << "P3 " << triangle.c.x << " " << triangle.c.y << " " << triangle.c.z << endl;
	cout << endl;

	cout << "Plane:" << endl;
	cout << "normal " << triangle.m_normal.x << " " << triangle.m_normal.y << " " << triangle.m_normal.z << endl;
	cout << "D " << triangle.d << endl;
	cout << endl;

	cout << "Shape: " << endl;
	cout << "origin " << triangle.origin.x << " " << triangle.origin.y << " " << triangle.origin.z << endl;

}

void printPoint(glm::vec3 point) {
	cout << point.x << " " << point.y << " " << point.z << endl;
}

float randomFloat(float min, float max) {
	// Create a random device and seed the generator
	std::random_device rd;
	std::mt19937 gen(rd());

	// Create a uniform real distribution between min and max
	std::uniform_real_distribution<float> dis(min, max);

	// Generate and return a random float
	return dis(gen);
}
float randomFloat01() {
	// Create a random device and seed the generator
	static std::random_device rd;
	static std::mt19937 gen(rd());

	// Create a uniform real distribution between 0.0 and 1.0
	std::uniform_real_distribution<float> dis(0.0f, 1.0f);

	// Generate and return a random float
	return dis(gen);
}

void serializeBVH(std::vector<FlatNode>& nodes, std::vector<int>& indices) {

	nodes.clear();
	indices.clear();

	for (auto& node : scene.bvhNodes) {
		FlatNode flatNode;

		flatNode.boundsMin = node->box.Min;
		flatNode.boundsMax = node->box.Max;
		flatNode.leftChild = node->leftChild;
		flatNode.rightChild = node->rightChild;
		flatNode.startShapeIdx = indices.size();
		flatNode.numShapes = node->shapesIndices.size();

		nodes.push_back(flatNode);

		if (node->leftChild == -1) {
			for (int idx : node->shapesIndices) {
				indices.push_back(idx);
			}
		}

	}
}

void updateScene(FlatScene& flatScene, GLuint ssbo)
{
	for (int i : animatedIndices) {
		flatScene.shapes[i] = serializeShape(scene.shapes[i]);

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(FlatShape) * i, sizeof(FlatShape), &flatScene.shapes[i]);

	}

	
}

FlatShape serializeShape(const std::unique_ptr<Shape>& shape)
{
	FlatShape flatShape;

	if (auto* sphere = dynamic_cast<Sphere*>(shape.get())) {
		flatShape.type = 0; // Sphere

		flatShape.material.color = sphere->material.color;
		flatShape.material.fresnelStrength = sphere->material.fresnelStrength;

		flatShape.material.ambientStrength = sphere->material.ambientStrength;
		flatShape.material.diffuseStrength = sphere->material.diffuseStrength;
		flatShape.material.specularStrength = sphere->material.specularStrength;
		flatShape.material.shininess = sphere->material.shininess;

		flatShape.sphereCenter = sphere->m_center;
		flatShape.sphereRadius = sphere->m_radius;
	}
	else if (auto* wall = dynamic_cast<Wall*>(shape.get())) {
		flatShape.type = 2; // Wall

		flatShape.material.color = wall->material.color;
		flatShape.material.fresnelStrength = wall->material.fresnelStrength;

		flatShape.material.ambientStrength = wall->material.ambientStrength;
		flatShape.material.diffuseStrength = wall->material.diffuseStrength;
		flatShape.material.specularStrength = wall->material.specularStrength;
		flatShape.material.shininess = wall->material.shininess;

		flatShape.planeNormal = wall->m_normal;
		flatShape.planeD = wall->d;

		flatShape.wallStart = wall->start;
		flatShape.wallWidth = wall->width;
		flatShape.wallHeight = wall->height;

	}
	else if (auto* triangle = dynamic_cast<Triangle*>(shape.get())) {
		flatShape.type = 3; // Triangle

		flatShape.material.color = triangle->material.color;
		flatShape.material.fresnelStrength = triangle->material.fresnelStrength;

		flatShape.material.ambientStrength = triangle->material.ambientStrength;
		flatShape.material.diffuseStrength = triangle->material.diffuseStrength;
		flatShape.material.specularStrength = triangle->material.specularStrength;
		flatShape.material.shininess = triangle->material.shininess;

		flatShape.planeNormal = triangle->m_normal;
		flatShape.planeD = triangle->d;

		flatShape.triP1 = triangle->a;
		flatShape.triP2 = triangle->b;
		flatShape.triP3 = triangle->c;

	}
	else if (auto* plane = dynamic_cast<Plane*>(shape.get())) {
		flatShape.type = 1; // Plane

		flatShape.material.color = plane->material.color;
		flatShape.material.fresnelStrength = plane->material.fresnelStrength;

		flatShape.material.ambientStrength = plane->material.ambientStrength;
		flatShape.material.diffuseStrength = plane->material.diffuseStrength;
		flatShape.material.specularStrength = plane->material.specularStrength;
		flatShape.material.shininess = plane->material.shininess;

		flatShape.planeNormal = plane->m_normal;
		flatShape.planeD = plane->d;

	}
	return flatShape;
}

void updateBVH() {
	for (auto& node : scene.bvhNodes) {
		for (int idx : node->shapesIndices)
		{
			if (scene.shapes[idx]->animated) {
				node->box.growToInclude(scene.shapes[idx]);
			}
		}
	}
}

void bounceSphere(Sphere* sphere, float elapsedTime, float amplitude = 2, float frequency = 1) {
	// Bouncing on the Y-axis
	sphere->m_center.y = sphere->origin.y + amplitude * std::sin(frequency * elapsedTime);
}

void updateWheelAnimations(float elapsedTime) {
	float rotationSpeed = 1;


	for (auto& wheel : wheels) {
		// Update rotation angle
		wheel.rotationAngle = rotationSpeed * deltaTime;
		//wheel.rotationAngle = fmod(wheel.rotationAngle, glm::two_pi<float>());

		glm::vec3 center = wheel.center;
		glm::mat4 translateToOrigin = glm::translate(glm::mat4(1.0f), -center);
		glm::mat4 translateBack = glm::translate(glm::mat4(1.0f), center);

		// Apply rotation to each triangle
		glm::mat4 rotationMatrix = glm::rotate(glm::mat4(1.0f), wheel.rotationAngle, wheel.rotationAxis);
		glm::mat4 transform = translateBack * rotationMatrix * translateToOrigin;

		for (int idx : wheel.shapeIndices) {
			if (auto triangle = dynamic_cast<Triangle*>(scene.shapes[idx].get())) {
				triangle->a = glm::vec3(transform * glm::vec4(triangle->a, 1.0f));
				triangle->b = glm::vec3(transform * glm::vec4(triangle->b, 1.0f));
				triangle->c = glm::vec3(transform * glm::vec4(triangle->c, 1.0f));
			}
		}
	}
}

void split(std::unique_ptr<Node>& parentNode, int depth) {

	// child case
	if (depth <= 0) {
		parentNode->leftChild = -1;
		parentNode->rightChild = -1;
		return;
	}

	glm::vec3 size = parentNode->box.Max - parentNode->box.Min;
	int splitAxis = size.x > glm::max(size.y, size.z) ? 0 : size.y > size.z ? 1 : 2;
	float splitPos = parentNode->box.center()[splitAxis];

	auto leftNode = std::make_unique<Node>();
	auto rightNode = std::make_unique<Node>();

	for (auto idx : parentNode->shapesIndices) {
		bool inA = false;
		glm::vec3 center;

		if (auto sphere = dynamic_cast<Sphere*>(scene.shapes[idx].get()))
			center = sphere->m_center;

		else if (auto wall = dynamic_cast<Wall*>(scene.shapes[idx].get()))
		{
			center = (wall->start + wall->end()) * 0.5f;
		}

		else if (auto triangle = dynamic_cast<Triangle*>(scene.shapes[idx].get()))
			center = triangle->center();

		inA = center[splitAxis] < splitPos;
		if (inA) {
			leftNode->box.growToInclude(scene.shapes[idx]);
			leftNode->shapesIndices.push_back(idx);
		}
		else
		{
			rightNode->box.growToInclude(scene.shapes[idx]);
			rightNode->shapesIndices.push_back(idx);
		}

	}

	// If one of the child would be empty, no reason to split anymore
	if (leftNode->shapesIndices.empty() || rightNode->shapesIndices.empty()) {
		parentNode->leftChild = -1;
		parentNode->rightChild = -1;

		return;
	}

	split(leftNode, depth - 1);
	split(rightNode, depth - 1);

	scene.bvhNodes.push_back(std::move(leftNode));
	parentNode->leftChild = scene.bvhNodes.size() - 1;

	scene.bvhNodes.push_back(std::move(rightNode));
	parentNode->rightChild = scene.bvhNodes.size() - 1;


}

int buildBVH(int maxDepth) {
	scene.bvhNodes.clear();

	// Create root node
	auto root = std::make_unique<Node>();

	BoundingBox bb = BoundingBox();
	for (int i = 0; i < scene.shapes.size(); ++i) {
		bb.growToInclude(scene.shapes[i]);
		root->shapesIndices.push_back(i);
	}

	root->box = bb;

	split(root, maxDepth);
	scene.bvhNodes.push_back(std::move(root));

	return 0;
}


void generateScene3() {
	// Camera 
	scene.camera = Camera();
	scene.camera.Position = glm::vec3(0, -10.0f, 40);
	scene.camera.aspectRatio = float(WIDTH) / HEIGHT;

	// add light
	scene.light = Light(glm::vec3(14.8f, -17, 17), glm::vec3(1), 26);

	auto origin = glm::vec3(0, 0, 0);

	auto t = Triangle(origin, origin + glm::vec3(5, 0, 0), origin + glm::vec3(2.5f, -5, 0));
	scene.shapes.push_back(std::make_unique<Triangle>(t));

	//// Add geometry to embree
	//RTCGeometry geom = rtcNewGeometry(g_embreeDevice, RTC_GEOMETRY_TYPE_TRIANGLE);
	//
	//float* vertices = (float*)rtcSetNewGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, 3 * sizeof(float), 3);
	//vertices[0] = t.a.x; vertices[1] = t.a.y; vertices[2] = t.a.z;
	//vertices[3] = t.b.x; vertices[4] = t.b.y; vertices[5] = t.b.z;
	//vertices[6] = t.c.x; vertices[7] = t.c.y; vertices[8] = t.c.z;

	//unsigned* indices = (unsigned*)rtcSetNewGeometryBuffer(geom, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, 3 * sizeof(unsigned), 1);
	//indices[0] = 0; indices[1] = 1; indices[2] = 2;

	/*rtcCommitGeometry(geom);
	rtcAttachGeometryByID(g_embreeScene, geom, 0);
	rtcReleaseGeometry(geom);*/
	rtcCommitScene(g_embreeScene);


	scene.camera.LookAt(origin);

}

void initEmbree() {
	g_embreeDevice = rtcNewDevice(nullptr);
	g_embreeScene = rtcNewScene(g_embreeDevice);
	Triangle::setTriangleScene(&g_embreeScene);
}

void cleanupEmbree() {
	if (g_embreeScene) rtcReleaseScene(g_embreeScene);
	if (g_embreeDevice) rtcReleaseDevice(g_embreeDevice);
}