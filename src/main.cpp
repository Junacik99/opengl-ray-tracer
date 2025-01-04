#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <glad/glad.h>
#include <glfw3.h>
#include <iostream>
#include "shader.hpp"
#include "stb_image.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "camera.hpp"
#include "ray.hpp"
#include "shapes/shape.hpp"
#include "shapes/sphere.hpp"
#include "shapes/plane.hpp"
#include "shapes/wall.hpp"
#include "computeShader.hpp"
#include <vector>
#include "material.hpp"
#include "light.hpp"
#include <glm/gtx/string_cast.hpp>
#include "flatStructures.hpp"

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void renderQuad();
glm::vec3 phong(const glm::vec3& point, const glm::vec3& normal, const glm::vec3& viewDir, const glm::vec3& objectColor, glm::vec3 lightPos, glm::vec3 lightColor, Material material);
void generateScene();
void cpuRayTracer(std::vector<float>& pixelData);

struct Scene
{
	Camera camera;
	Light light;
	std::vector < std::unique_ptr< Shape >> shapes;
} scene;

FlatCamera serializeCamera(Camera cam);
FlatScene serializeScene(const Scene& scene);

// Window size
const int WIDTH = 800;
const int HEIGHT = 600;

bool wireframe = false;
bool zKeyPressed = false;
bool rtxon = true;
bool animate = true;


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
bool useFresnel = true;
float fresnelStrength = 1.f;

void bounceSphere(Sphere* sphere, float elapsedTime, float amplitude=2, float frequency=1) {
	// Bouncing on the Y-axis
	sphere->m_center.y = sphere->origin.y + amplitude * std::sin(frequency * elapsedTime);
}


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

	/* Scene */
	generateScene();

	

	/*for (const auto& shape : scene.shapes) {
		std::cout << shape->color.r*255 << ' ' << shape->color.g*255 << ' ' << shape->color.b*255 << std::endl;
	}*/

	/* SSBO (Shader Storage Buffer Object */
	FlatScene flatScene = serializeScene(scene); // Serialize scene

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

		// Input
		processInput(window);

		if (!rtxon) { // CPU ray tracing
			// No phong, fresnel, shadows... Just laggy ray tracing with diffuse colors
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
			flatScene = serializeScene(scene);

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbocamera);
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(FlatCamera), &flatScene.camera);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); // Unbind

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbolight);
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(FlatLight), &flatScene.light);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); // Unbind

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboshapes);
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(FlatShape) * flatScene.shapes.size(), flatScene.shapes.data());
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); // Unbind

			
			// Compute shader dispatch
			computeShaderGPU.use();
			glDispatchCompute((unsigned)WIDTH, (unsigned)HEIGHT, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			// Set window resolution in shader
			computeShaderGPU.setVec2("screenRes", glm::vec2(WIDTH, HEIGHT));
			computeShaderGPU.setInt("maxBounces", maxBounces);
			computeShaderGPU.setBool("useFresnel", useFresnel);

			// Render image to quad
			glClearColor(0, 0, 0, 1.f);
			screenQuad.use();
			renderQuad();


			/***********************************************************************************************/
		}

		// Create GUI window
		ImGui::Begin("GUI window");
		ImGui::Text("Ray Tracer");

		ImGui::Checkbox("RTX ON", &rtxon);
		ImGui::SliderInt("Max bounces", &maxBounces, 1, 10);
		ImGui::Checkbox("Fresnel", &useFresnel);
		ImGui::Checkbox("Animate", &animate);

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

		ImGui::Text("Light");
		float lightColor[4] = { scene.light.color.r, scene.light.color.g, scene.light.color.b, 1.f };
		ImGui::ColorEdit4("Color", lightColor);
		scene.light.color = glm::vec3(lightColor[0], lightColor[1], lightColor[2]);
		ImGui::SliderFloat("Intensity", &scene.light.intensity, 0, 100);
		scene.light.updateColor();
		ImGui::SliderFloat("X pos", &scene.light.position.x, -17, 17);
		ImGui::SliderFloat("Y pos", &scene.light.position.y, -17, 17);
		ImGui::SliderFloat("Z pos", &scene.light.position.z, -17, 17);

		ImGui::Text("Mirror");
		ImGui::SliderFloat("fresnel", &scene.shapes[4]->material.fresnelStrength, 0, 1);
		ImGui::SliderFloat("ambient", &scene.shapes[4]->material.ambientStrength, 0, 1);
		ImGui::SliderFloat("diffuse", &scene.shapes[4]->material.diffuseStrength, 0, 1);
		ImGui::SliderFloat("specular", &scene.shapes[4]->material.specularStrength, 0, 1);
		ImGui::SliderInt("shininess", &scene.shapes[4]->material.shininess, 0, 100);

		ImGui::End();

		// Render UI elements
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		// Animate objects
		if (animate) {
			if (auto* sphere = dynamic_cast<Sphere*>(scene.shapes[0].get()))
				bounceSphere(sphere, currentFrame, 10);
			if (auto* sphere = dynamic_cast<Sphere*>(scene.shapes[1].get()))
				bounceSphere(sphere, currentFrame, 7, 0.8);
			if (auto* sphere = dynamic_cast<Sphere*>(scene.shapes[2].get()))
				bounceSphere(sphere, currentFrame, 15, 1.5);
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

	// Toggle wireframe mode on Z key press
	if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) {
		if (!zKeyPressed) { // Detect key press event
			if (wireframe)
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			else
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			wireframe = !wireframe;
			zKeyPressed = true; // Mark the key as pressed
		}
	}
	else if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_RELEASE) {
		zKeyPressed = false; // Reset the state when the key is released
	}

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

	useMouse = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_RELEASE;
	
				
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

void generateScene()
{
	// Camera 
	scene.camera = Camera();
	scene.camera.Position = glm::vec3(30.0f, -5.0f, 40.0f);
	scene.camera.MovementSpeed = 3.f;
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

	scene.shapes.push_back(std::make_unique<Sphere>(glm::vec3(12, 10, -8), 4.f));
	scene.shapes[scene.shapes.size() - 1]->material.color = glm::vec3(0.58f, 0.18f, 0.48f);
	scene.shapes[scene.shapes.size() - 1]->material.fresnelStrength = 0;
	scene.shapes[scene.shapes.size() - 1]->material.ambientStrength = 0;
	scene.shapes[scene.shapes.size() - 1]->material.diffuseStrength = 0.5f;
	scene.shapes[scene.shapes.size() - 1]->material.specularStrength = 0;

	scene.shapes.push_back(std::make_unique<Sphere>(glm::vec3(20, 7.5, -8), 2.5f));
	scene.shapes[scene.shapes.size() - 1]->material.color = glm::vec3(0.8f, 0.2f, 0.8f);
	scene.shapes[scene.shapes.size() - 1]->material.fresnelStrength = 1;
	scene.shapes[scene.shapes.size() - 1]->material.ambientStrength = 0.06f;
	scene.shapes[scene.shapes.size() - 1]->material.diffuseStrength = 0.06f;
	scene.shapes[scene.shapes.size() - 1]->material.specularStrength = 0.5f;

	scene.shapes.push_back(std::make_unique<Sphere>(glm::vec3(0, 23, -8), 1.5f));
	scene.shapes[0]->material.color = glm::vec3(0, 0.37f, 0);
	scene.shapes[scene.shapes.size() - 1]->material.fresnelStrength = 0;
	scene.shapes[scene.shapes.size() - 1]->material.ambientStrength = 0;
	scene.shapes[scene.shapes.size() - 1]->material.diffuseStrength = 0.5f;
	scene.shapes[scene.shapes.size() - 1]->material.specularStrength = 0;

	// wall (mirror)
	scene.shapes.push_back(std::make_unique<Wall>(glm::vec3(-15, 23, -15), 10, 20, glm::vec3(-1, 0.2f, -1)));
	scene.shapes[scene.shapes.size() - 1]->material.fresnelStrength = 1;
	scene.shapes[scene.shapes.size() - 1]->material.ambientStrength = 0.1f;
	scene.shapes[scene.shapes.size() - 1]->material.diffuseStrength = 0;
	scene.shapes[scene.shapes.size() - 1]->material.specularStrength = 1;


	// bottom
	scene.shapes.push_back(std::make_unique<Plane>(glm::vec3(0, 1, 0), glm::vec3(0, 25, 0)));
	scene.shapes[scene.shapes.size() - 1]->material.color = glm::vec3(0.65f, 0.17f, 0.35f);
	scene.shapes[scene.shapes.size() - 1]->material.specularStrength = 0;
	// top
	scene.shapes.push_back(std::make_unique<Plane>(glm::vec3(0, -1, 0), glm::vec3(0, -25, 0)));
	scene.shapes[scene.shapes.size() - 1]->material.color = glm::vec3(0.65f, 0.17f, 0.35f);
	scene.shapes[scene.shapes.size() - 1]->material.specularStrength = 0;
	// left
	scene.shapes.push_back(std::make_unique<Plane>(glm::vec3(-1, 0, 0), glm::vec3(-25, 0, 0)));
	scene.shapes[scene.shapes.size() - 1]->material.specularStrength = 0;
	scene.shapes[scene.shapes.size() - 1]->material.color = glm::vec3(0, 0, 1);

	// right
	scene.shapes.push_back(std::make_unique<Plane>(glm::vec3(1, 0, 0), glm::vec3(25, 0, 0)));
	scene.shapes[scene.shapes.size() - 1]->material.specularStrength = 0;
	scene.shapes[scene.shapes.size() - 1]->material.color = glm::vec3(1, 0, 0);
	// front
	scene.shapes.push_back(std::make_unique<Plane>(glm::vec3(0, 0, 1), glm::vec3(0, 0, 25)));
	scene.shapes[scene.shapes.size() - 1]->material.color = glm::vec3(1, 1, 0.35f);
	scene.shapes[scene.shapes.size() - 1]->material.specularStrength = 0;
	// back
	scene.shapes.push_back(std::make_unique<Plane>(glm::vec3(0, 0, -1), glm::vec3(0, 0, -25)));
	scene.shapes[scene.shapes.size() - 1]->material.color = glm::vec3(1, .5f, 0);
	scene.shapes[scene.shapes.size() - 1]->material.specularStrength = 0;

	scene.camera.LookAt(scene.shapes[0]->origin);
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

FlatScene serializeScene(const Scene& scene) {
	FlatScene flatScene;

	// Serialize camera
	flatScene.camera = serializeCamera(scene.camera);

	// Serialize light
	flatScene.light.position = scene.light.position;
	flatScene.light.color = scene.light.color;

	// Serialize shapes
	std::vector<FlatShape> flatShapes;
	for (const auto& shape : scene.shapes) {
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
		

		flatShapes.push_back(flatShape);
	}

	flatScene.shapes = flatShapes;

	return flatScene;
}

void cpuRayTracer(std::vector<float>& pixelData) {
	// Calculate ray hits
	for (int y = 0; y < HEIGHT; ++y) {
		for (int x = 0; x < WIDTH; ++x) {
			Ray ray = scene.camera.GetRay(2.f * x / WIDTH - 1, 1.f - 2.f * y / HEIGHT); // flip y-axis

			glm::vec3 color = glm::vec3(); // BG color
			float closestDist = std::numeric_limits<float>::max();

			for (const auto& shape : scene.shapes) {
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