#include "meshObject.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <fstream>      // For file reading (loadOBJ)
#include <sstream>      // For parsing lines (loadOBJ)
#include <string>       // For string manipulation
#include <algorithm>    // For std::replace (if needed)

// Define STB_IMAGE_IMPLEMENTATION in exactly one .cpp file
#define STB_IMAGE_IMPLEMENTATION
#include "../common/stb_image.h" // For texture loading
#include "../common/objloader.hpp" // Include the common OBJ loader

// Initialize static member
int meshObject::nextId = 1;
std::map<int, meshObject*> meshObject::meshObjectMap;

// Default constructor (can be removed or adapted if not needed)
meshObject::meshObject() : id(nextId++) {
    meshObjectMap[id] = this;
    modelMatrix = glm::mat4(1.0f);
    // Initialize other members to default values if necessary
    VAO = VBO_vertices = VBO_uvs = VBO_normals = EBO = 0;
    shaderProgram = pickingShaderProgram = textureID = 0;
    numIndices = 0;
    showWireframe = false;
    std::cerr << "Warning: Default meshObject constructor called. No model loaded." << std::endl;
    // Consider loading a default shape or throwing an error
}

// Constructor to load model and texture
meshObject::meshObject(const std::string& modelPath, const std::string& texturePath) : id(nextId++) {
    meshObjectMap[id] = this;
    modelMatrix = glm::mat4(1.0f);
    showWireframe = false;

    // Load mesh data using the common loader
    bool res = loadOBJ(modelPath.c_str(), vertices, uvs, normals, indices);
    if (!res) {
        std::cerr << "Error loading OBJ file: " << modelPath << std::endl;
        // Handle error appropriately (e.g., load default, throw exception)
        return;
    }
    numIndices = static_cast<GLsizei>(indices.size()); // Update numIndices after loading

    // Load texture
    textureID = loadTexture(texturePath);
    if (textureID == 0) {
        std::cerr << "Error loading texture file: " << texturePath << std::endl;
        // Handle error (optional: proceed without texture)
    }

    // Setup OpenGL buffers
    setupBuffers();

    // Load shaders (ensure these shaders handle textures)
    shaderProgram = LoadShaders("meshVertexShader.glsl", "meshFragmentShader.glsl");
    pickingShaderProgram = LoadShaders("pickingVertexShader.glsl", "pickingFragmentShader.glsl");
}

meshObject::~meshObject() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO_vertices);
    glDeleteBuffers(1, &VBO_uvs);
    glDeleteBuffers(1, &VBO_normals);
    glDeleteBuffers(1, &EBO);
    if (textureID != 0) {
        glDeleteTextures(1, &textureID);
    }
    if (shaderProgram != 0) {
        glDeleteProgram(shaderProgram);
    }
    if (pickingShaderProgram != 0) {
        glDeleteProgram(pickingShaderProgram);
    }
    meshObjectMap.erase(id);
}

void meshObject::draw(const glm::mat4& view, const glm::mat4& projection) {
    if (shaderProgram == 0 || VAO == 0) return; // Don't draw if setup failed

    glUseProgram(shaderProgram);

    // Set MVP matrix uniform
    glm::mat4 MVP = projection * view * modelMatrix;
    GLuint matrixID = glGetUniformLocation(shaderProgram, "MVP");
    glUniformMatrix4fv(matrixID, 1, GL_FALSE, glm::value_ptr(MVP));

    // Bind texture
    if (textureID != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        // Set the sampler to use texture unit 0
        GLuint textureSamplerID = glGetUniformLocation(shaderProgram, "textureSampler");
        glUniform1i(textureSamplerID, 0);
    }

    // Set wireframe mode if toggled
    if (showWireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    // Draw the mesh
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    // Reset polygon mode to fill for other objects
    if (showWireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    glUseProgram(0); // Unbind shader program
    glBindTexture(GL_TEXTURE_2D, 0); // Unbind texture
}

void meshObject::drawPicking(const glm::mat4& view, const glm::mat4& projection) {
    glUseProgram(pickingShaderProgram);
    glm::mat4 MVP = projection * view * modelMatrix;
    GLuint matrixID = glGetUniformLocation(pickingShaderProgram, "MVP");
    glUniformMatrix4fv(matrixID, 1, GL_FALSE, glm::value_ptr(MVP));

    // TODO: send 'id' as a uniform for color‐coded picking

    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    glUseProgram(0);
}

void meshObject::translate(const glm::vec3& translation) {
    modelMatrix = glm::translate(modelMatrix, translation);
}

void meshObject::rotate(float angle, const glm::vec3& axis) {
    modelMatrix = glm::rotate(modelMatrix, glm::radians(angle), axis);
}

meshObject* meshObject::getMeshObjectById(int searchId) {
    auto it = meshObjectMap.find(searchId);
    return (it != meshObjectMap.end()) ? it->second : nullptr;
}

void meshObject::toggleWireframe() {
    showWireframe = !showWireframe;
}

// --- Private Helper Functions ---

// The custom loadOBJ function is removed as we now use the one from common/objloader.hpp

// Texture loading using stb_image
GLuint meshObject::loadTexture(const std::string& path) {
    GLuint textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    // stbi_set_flip_vertically_on_load(true); // Uncomment if texture appears upside down
    unsigned char *data = stbi_load(path.c_str(), &width, &height, &nrComponents, 0);
    if (data) {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;
        else {
            std::cerr << "Unknown number of components in texture: " << path << std::endl;
            stbi_image_free(data);
            glDeleteTextures(1, &textureID);
            return 0;
        }

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        // Set texture wrapping and filtering options
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    } else {
        std::cerr << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data); // data is likely null here anyway, but safe to call
        glDeleteTextures(1, &textureID);
        return 0;
    }

    glBindTexture(GL_TEXTURE_2D, 0); // Unbind texture
    return textureID;
}

// Setup VAO, VBOs, EBO
void meshObject::setupBuffers() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO_vertices);
    glGenBuffers(1, &VBO_uvs);
    glGenBuffers(1, &VBO_normals);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    // Load data into vertex buffers
    glBindBuffer(GL_ARRAY_BUFFER, VBO_vertices);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, VBO_uvs);
    glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(glm::vec2), uvs.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, VBO_normals);
    glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3), normals.data(), GL_STATIC_DRAW);

    // Load data into element buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // Set the vertex attribute pointers
    // Vertex Positions (location = 0)
    glBindBuffer(GL_ARRAY_BUFFER, VBO_vertices);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);

    // Vertex Texture Coords (location = 1)
    glBindBuffer(GL_ARRAY_BUFFER, VBO_uvs);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
    glEnableVertexAttribArray(1);

    // Vertex Normals (location = 2)
    glBindBuffer(GL_ARRAY_BUFFER, VBO_normals);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(2);

    glBindVertexArray(0); // Unbind VAO
}
