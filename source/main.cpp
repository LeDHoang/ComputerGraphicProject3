#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include "meshObject.hpp"
#include "gridObject.hpp"

const GLuint windowWidth = 1024;
const GLuint windowHeight = 768;
GLFWwindow* window;

// Function prototypes
int  initWindow();
void mouseCallback(GLFWwindow*, int, int, int);
int  getPickedId();

int main() {
    if (initWindow() != 0) return -1;

    // Projection: 45° FOV, aspect 4:3, near=0.1, far=100
    glm::mat4 projectionMatrix = glm::perspective(
        glm::radians(45.0f),
        float(windowWidth) / float(windowHeight),
        0.1f,
        100.0f
    );

    // Scene
    gridObject grid;
    meshObject  obj;
    obj.translate(glm::vec3(0.0f, 0.0f, 3.0f));

    // Camera state
    bool cameraSelected = false, cWasPressed = false;
    bool rWasPressed = false;
    float horizontalAngle = 0.0f;
    float verticalAngle = 0.0f;
    const float cameraSpeed = glm::radians(90.0f);  // 90°/sec
    const float cameraRadius = 15.0f;                // distance

    double lastFrameTime = glfwGetTime();
    double lastFPSTime = lastFrameTime;
    int    nbFrames = 0;

    while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
        !glfwWindowShouldClose(window))
    {
        // --- timing ---
        double currentTime = glfwGetTime();
        nbFrames++;
        if (currentTime - lastFPSTime >= 1.0) {
            std::cout << 1000.0 / double(nbFrames) << " ms/frame\n";
            nbFrames = 0;
            lastFPSTime += 1.0;
        }
        float deltaTime = float(currentTime - lastFrameTime);
        lastFrameTime = currentTime;

        // --- toggle camera ON/OFF with C ---
        bool cPressed = (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS);
        if (cPressed && !cWasPressed) {
            cameraSelected = !cameraSelected;
            std::cout << (cameraSelected ? "Camera ON\n" : "Camera OFF\n");
        }
        cWasPressed = cPressed;

        // --- reset view with R ---
        bool rPressed = (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS);
        if (rPressed && !rWasPressed) {
            cameraSelected = false;
            horizontalAngle = 0.0f;
            verticalAngle = 0.0f;
            std::cout << "View reset to startup state\n";
        }
        rWasPressed = rPressed;

        // --- when camera is ON, handle arrow keys ---
        if (cameraSelected) {
            if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
                horizontalAngle -= cameraSpeed * deltaTime;
            if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
                horizontalAngle += cameraSpeed * deltaTime;
            if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
                verticalAngle += cameraSpeed * deltaTime;
            if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
                verticalAngle -= cameraSpeed * deltaTime;

            // clamp pitch to avoid gimbal flip
            float limit = glm::half_pi<float>() - 0.01f;
            verticalAngle = glm::clamp(verticalAngle, -limit, limit);
        }

        // --- spherical to Cartesian ---
        glm::vec3 cameraPos = glm::vec3(
            cameraRadius * cos(verticalAngle) * sin(horizontalAngle),
            cameraRadius * sin(verticalAngle),
            cameraRadius * cos(verticalAngle) * cos(horizontalAngle)
        );

        // --- dynamic up vector ---
        glm::vec3 target = glm::vec3(0.0f);
        glm::vec3 direction = glm::normalize(target - cameraPos);
        glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 right = glm::normalize(glm::cross(worldUp, direction));
        glm::vec3 upDirection = glm::cross(direction, right);

        glm::mat4 viewMatrix = glm::lookAt(
            cameraPos,
            target,
            upDirection
        );

        // --- render ---
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        grid.draw(viewMatrix, projectionMatrix);
        obj.draw(viewMatrix, projectionMatrix);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

int initWindow() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    window = glfwCreateWindow(windowWidth, windowHeight, "Lastname,FirstName(ufid)", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to open GLFW window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    glewExperimental = true;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW\n";
        return -1;
    }

    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_FALSE);
    glfwSetCursorPos(window, windowWidth / 2, windowHeight / 2);
    glfwSetMouseButtonCallback(window, mouseCallback);

    glClearColor(0.0f, 0.0f, 0.4f, 0.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);

    return 0;
}

void mouseCallback(GLFWwindow* /*win*/, int button, int action, int /*mods*/) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        std::cout << "Left mouse button pressed\n";
    }
}

int getPickedId() {
    glFlush(); glFinish();
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    unsigned char data[4] = { 0, 0, 0, 0 };
    // TODO: implement cursor-to-buffer coordinate conversion & glReadPixels(...)
    return int(data[0]);
}
