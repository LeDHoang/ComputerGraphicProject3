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
#include <set>     // For edge representation in subdivision
#include <map>     // For vertex adjacency in subdivision

// Structure to represent an edge (pair of vertex indices)
struct Edge {
    unsigned int v1, v2;
    bool operator<(const Edge& other) const {
        // Ensure consistent ordering for map/set keys
        unsigned int min1 = std::min(v1, v2);
        unsigned int max1 = std::max(v1, v2);
        unsigned int min2 = std::min(other.v1, other.v2);
        unsigned int max2 = std::max(other.v1, other.v2);
        if (min1 != min2) return min1 < min2;
        return max1 < max2;
    }
};

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
    void toggleSmooth();    // Method to toggle smooth subdivision view
    void toggleTexture();   // Method to toggle texture mapping
    void setSubdivisionLevel(int level); // Set the target subdivision level

    int getId() const { return id; } // Getter for the ID

    static meshObject* getMeshObjectById(int id); // Retrieve object by ID

    // TODO: P1bTask4 - Create a list of children.

private:
    // OpenGL Buffers and Shaders
    GLuint VAO, VBO_vertices, VBO_uvs, VBO_normals, EBO;
    GLuint smoothVAO, smoothVBO_vertices, smoothVBO_uvs, smoothVBO_normals, smoothEBO; // Buffers for subdivided mesh
    GLuint shaderProgram;
    GLuint pickingShaderProgram;
    GLuint textureID; // Texture handle

    // Object State
    glm::mat4 modelMatrix;
    bool showWireframe = false; // Wireframe toggle state
    bool showSmooth = false;    // Smooth subdivision toggle state
    bool showTexture = true;    // Texture toggle state
    int subdivisionLevel = 0;   // Current subdivision level applied
    int targetSubdivisionLevel = 2; // Target level for smooth toggle

    // Mesh Data (Loaded from OBJ)
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec3> normals;
    std::vector<unsigned int> indices;
    GLsizei numIndices; // Renamed from indices.size() usage

    // Subdivided Mesh Data
    std::vector<glm::vec3> smoothVertices;
    std::vector<glm::vec2> smoothUvs;
    std::vector<glm::vec3> smoothNormals;
    std::vector<unsigned int> smoothIndices;
    GLsizei numSmoothIndices = 0;

    // Static members for ID management and lookup
    static int nextId; // Static counter for unique IDs
    int id;            // ID for this specific object
    static std::map<int, meshObject*> meshObjectMap; // Static map of ID to Object

    // Private helper methods
    GLuint loadTexture(const std::string& path); // Texture loading function
    void setupBuffers(); // Helper to setup OpenGL buffers
    void setupSmoothBuffers(); // Helper to setup buffers for the smooth mesh
    void applyLoopSubdivision(); // Performs one level of Loop subdivision
    void calculateNormals(std::vector<glm::vec3>& verts, const std::vector<unsigned int>& inds, std::vector<glm::vec3>& norms); // Calculates vertex normals
};

#endif
