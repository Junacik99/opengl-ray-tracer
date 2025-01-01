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
#include "computeShader.hpp"
#include <vector>
#include "material.hpp"
#include "light.hpp"
#include <glm/gtx/string_cast.hpp>
#include "shapes/flatShape.hpp"

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void renderQuad();
glm::vec3 phong(const glm::vec3& point, const glm::vec3& normal, const glm::vec3& viewDir, const glm::vec3& objectColor, glm::vec3 lightPos, glm::vec3 lightColor, Material material);


// Window size
const int WIDTH = 800;
const int HEIGHT = 600;

bool wireframe = false;
bool zKeyPressed = false;
bool rtxon = false;


/* Camera */
// Camera axes
float lastX = WIDTH/2, lastY = HEIGHT/2; // Set cursor to the center
bool firstMouse = true;


// Frame inference computation
float deltaTime = 0.0f;	// Time between current frame and last frame
float lastFrame = 0.0f; // Time of last frame

struct Scene
{
	Camera camera;
	Light light;
	std::vector < std::unique_ptr< Shape >> shapes;
} scene;

struct FlatScene {
	Camera camera;
	Light light;
	std::vector <FlatShape> shapes;
};

FlatScene serializeScene(const Scene& scene) {
	FlatScene flatScene;

	flatScene.camera = scene.camera;
	flatScene.light = scene.light;
	
	std::vector<FlatShape> flatShapes;
	for (const auto& shape : scene.shapes) {
		FlatShape flatShape;

		if (auto* sphere = dynamic_cast<Sphere*>(shape.get())) {
			flatShape.type = 0; // Sphere
			flatShape.color = sphere->color;
			flatShape.material = sphere->material;
			flatShape.sphereCenter = sphere->m_center;
			flatShape.sphereRadius = sphere->m_radius;
		}
		else if (auto* plane = dynamic_cast<Plane*>(shape.get())) {
			flatShape.type = 1; // Plane
			flatShape.color = plane->color;
			flatShape.material = plane->material;
			flatShape.planeNormal = plane->m_normal;
			flatShape.planeD = plane->d;
		}

		flatShapes.push_back(flatShape);
	}

	flatScene.shapes = flatShapes;

	return flatScene;
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
	// Camera 
	scene.camera = Camera();
	scene.camera.Position = glm::vec3(0.0f, 0.0f, 5.0f);
	scene.camera.MovementSpeed = 3.f;
	scene.camera.aspectRatio = float(WIDTH) / HEIGHT;

	// add light
	scene.light = Light(glm::vec3(0, -15, 0), glm::vec3(1));

	// add shapes
	scene.shapes.push_back(std::make_unique<Sphere>(glm::vec3(0, 0, -8), 5.f));
	scene.shapes.push_back(std::make_unique<Plane>(glm::vec3(-1, 0, 0), glm::vec3(-10, -10, -25)));
	scene.shapes.push_back(std::make_unique<Plane>(glm::vec3(1, 0, 0), glm::vec3(10, -10, -25)));
	scene.shapes[2]->color = glm::vec3(1, 0, 0);
	for (int i = 0; i < 10; ++i) {
		auto pos = glm::vec3(rand() % 21 - 10, rand() % 41 - 20, (rand() % 150 + 10) * (-1));
		scene.shapes.push_back(std::make_unique<Sphere>(pos, 5.f));
		scene.shapes[i + 3]->color = glm::vec3(((float)rand()) / RAND_MAX, ((float)rand()) / RAND_MAX, ((float)rand()) / RAND_MAX);
		scene.shapes[i + 3]->material.ambientStrength = ((float)rand()) / RAND_MAX;
		scene.shapes[i + 3]->material.diffuseStrength = ((float)rand()) / RAND_MAX;
		scene.shapes[i + 3]->material.specularStrength = ((float)rand()) / RAND_MAX;
		scene.shapes[i + 3]->material.shininess = rand() % 100;
	}

	/* SSBO (Shader Storage Buffer Object */
	FlatScene flatScene = serializeScene(scene); // Serialize scene
	GLuint ssbo;
	glGenBuffers(1, &ssbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);

	// Calculate the size of the FlatScene data
	size_t flatSceneSize = sizeof(flatScene);
	size_t shapesSize = sizeof(FlatShape) * flatScene.shapes.size();

	// Total size (camera + light + shapes)
	size_t totalSize = flatSceneSize + shapesSize;

	//glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(flatScene), &flatScene, GL_DYNAMIC_COPY);
	// Allocate the SSBO with the appropriate size
	glBufferData(GL_SHADER_STORAGE_BUFFER, totalSize, nullptr, GL_STATIC_DRAW);

	// Copy the serialized scene data into the buffer
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, flatSceneSize, &flatScene.camera);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, flatSceneSize, flatSceneSize + shapesSize, flatScene.shapes.data());

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo); 
	//glBindBuffer(GL_SHADER_STORAGE_BUFFER, 1);


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
			/***********************************************************************************************/
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
									shape->color, 
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

			// Compute shader dispatch
			computeShaderGPU.use();
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
			// Compute shader dispatch
			computeShader.use();
			glDispatchCompute((unsigned)WIDTH, (unsigned)HEIGHT, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			// Render image to quad
			glClearColor(.2f, .3f, .3f, 1.f);
			screenQuad.use();
			renderQuad();


			/***********************************************************************************************/
		}

		// Create GUI window
		ImGui::Begin("GUI window");
		ImGui::Text("Ray Tracer");
		ImGui::Checkbox("RTX ON", &rtxon);
		ImGui::End();

		// Render UI elements
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	
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