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
#include "sphere.hpp"
#include "plane.hpp"

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);

// Window size
const int WIDTH = 800;
const int HEIGHT = 600;

bool wireframe = false;
bool zKeyPressed = false;


/* Camera */
// Camera axes
float lastX = WIDTH/2, lastY = HEIGHT/2; // Set cursor to the center
bool firstMouse = true;
Camera camera;


// Frame inference computation
float deltaTime = 0.0f;	// Time between current frame and last frame
float lastFrame = 0.0f; // Time of last frame

glm::vec3 phong(glm::vec3 point, glm::vec3 normal) {
	return glm::vec3();
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

	// Set mouse callback
	glfwSetCursorPosCallback(window, mouse_callback);

	/* Shaders */
	// Compute shader (cannot be used with others)
	Shader shader("shader.comp");
	shader.use();
	glDispatchCompute(WIDTH, HEIGHT, 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);


	



	/* Camera */
	camera = Camera();
	camera.Position = glm::vec3(0.0f, 0.0f, 5.0f);
	camera.MovementSpeed = 3.f;
	camera.aspectRatio = float(WIDTH) / HEIGHT;


	/* Scene */
	// TODO: add sphere and plane
	auto sphere = Sphere(glm::vec3(0), 5.f);
	auto plane = Plane(glm::vec3(0, 0, -1), glm::vec3(0, 0, 10));


	/* Render loop */
	while (!glfwWindowShouldClose(window)) {
		// Input
		processInput(window);

		//// Background color
		//glClearColor(.2f, .3f, .3f, 1.f);
		//glClear(GL_COLOR_BUFFER_BIT);

		// Render objects
		for (int y = 0; y < HEIGHT; ++y) {
			for (int x = 0; x < WIDTH; ++x) {
				Ray ray = camera.GetRay(2.f * x / WIDTH - 1, 1.f - 2.f * y / HEIGHT); // flip y-axis

				// Trace ray
				auto s_hit = sphere.get_intersection(ray);
				glm::vec3 color = glm::vec3();
				if (s_hit.intersect_type != NONE) { // Hit!
					auto point = s_hit.hit_point;
					auto normal = sphere.get_normal(point);

					// TODO: Calculate lighting (Phong)

					color = glm::vec3(1, 0, 0);

				}


				// TODO: Set color pixel in fragment shader
			}
		}



		// swap buffers and poll io events
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfwTerminate();
	return 0;
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
		camera.ProcessKeyboard(FORWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		camera.ProcessKeyboard(BACKWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		camera.ProcessKeyboard(LEFT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		camera.ProcessKeyboard(RIGHT, deltaTime);

				
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

	camera.ProcessMouseMovement(xOffset, yOffset);
}
