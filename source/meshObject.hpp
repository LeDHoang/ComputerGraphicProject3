#ifndef meshObject_hpp
#define meshObject_hpp

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <common/shader.hpp>
#include <map>
#include <string> // Added for file paths
#include <vector>  // Added for vertex data storage

class meshObject {
public:
    meshObject(); // Keep default for now, might remove later
    meshObject(const std::string& modelPath, const std::string& texturePath); // New constructor
    ~meshObject();

    void draw(const glm::mat4& view, const glm::mat4& projection);
    void drawPicking(const glm::mat4& view, const glm::mat4& projection);
    void translate(const glm::vec3& translation); // Translate the object
    void rotate(float angle, const glm::vec3& axis); // Rotate the object
    void toggleWireframe(); // Method to toggle wireframe

    int getId() const { return id; } // Getter for the ID

    static meshObject* getMeshObjectById(int id); // Retrieve object by ID

    // TODO: P1bTask4 - Create a list of children.

private:
    // OpenGL Buffers and Shaders
    GLuint VAO, VBO_vertices, VBO_uvs, VBO_normals, EBO;
    GLuint shaderProgram;
    GLuint pickingShaderProgram;
    GLuint textureID; // Texture handle

    // Object State
    glm::mat4 modelMatrix;
    bool showWireframe = false; // Wireframe toggle state

    // Mesh Data (Loaded from OBJ)
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec3> normals;
    std::vector<unsigned int> indices;
    GLsizei numIndices; // Renamed from indices.size() usage

    // Static members for ID management and lookup
    static int nextId; // Static counter for unique IDs
    int id;            // ID for this specific object
    static std::map<int, meshObject*> meshObjectMap; // Static map of ID to Object

    // Private helper methods
    GLuint loadTexture(const std::string& path); // Texture loading function
    void setupBuffers(); // Helper to setup OpenGL buffers
};

#endif
