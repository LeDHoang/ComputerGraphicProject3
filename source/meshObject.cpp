#include "meshObject.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <fstream>      // For file reading (loadOBJ)
#include <sstream>      // For parsing lines (loadOBJ)
#include <string>       // For string manipulation
#include <algorithm>    // For std::replace (if needed)
#include <set>      // For Edge struct and subdivision logic
#include <map>      // For vertex adjacency and edge midpoints

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

    // Initialize smooth mesh data with base mesh data initially
    smoothVertices = vertices;
    smoothUvs = uvs;
    smoothNormals = normals;
    smoothIndices = indices;
    numSmoothIndices = numIndices;

    // Load texture
    textureID = loadTexture(texturePath);
    if (textureID == 0) {
        std::cerr << "Error loading texture file: " << texturePath << std::endl;
        // Handle error (optional: proceed without texture)
    }

    // Setup OpenGL buffers for original and smooth mesh
    setupBuffers();
    setupSmoothBuffers(); // Setup buffers for the (initially identical) smooth mesh

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
    glDeleteVertexArrays(1, &smoothVAO); // Delete smooth buffers
    glDeleteBuffers(1, &smoothVBO_vertices);
    glDeleteBuffers(1, &smoothVBO_uvs);
    glDeleteBuffers(1, &smoothVBO_normals);
    glDeleteBuffers(1, &smoothEBO);
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
    if (shaderProgram == 0) return; // Don't draw if setup failed

    GLuint currentVAO = showSmooth ? smoothVAO : VAO;
    GLsizei currentNumIndices = showSmooth ? numSmoothIndices : numIndices;

    if (currentVAO == 0) return; // Don't draw if the selected VAO is not ready

    glUseProgram(shaderProgram);

    // Set MVP matrix uniform
    glm::mat4 MVP = projection * view * modelMatrix;
    GLuint matrixID = glGetUniformLocation(shaderProgram, "MVP");
    glUniformMatrix4fv(matrixID, 1, GL_FALSE, glm::value_ptr(MVP));

    // Bind texture conditionally
    if (showTexture && textureID != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        // Set the sampler to use texture unit 0
        GLuint textureSamplerID = glGetUniformLocation(shaderProgram, "textureSampler");
        glUniform1i(textureSamplerID, 0);
        // Indicate texture is used (optional, depends on shader)
        GLuint useTextureID = glGetUniformLocation(shaderProgram, "useTexture");
        if (useTextureID != -1) glUniform1i(useTextureID, 1);
    } else {
        // Indicate texture is not used (optional, depends on shader)
        GLuint useTextureID = glGetUniformLocation(shaderProgram, "useTexture");
        if (useTextureID != -1) glUniform1i(useTextureID, 0);
    }

    // Set wireframe mode if toggled (applies to whichever mesh is drawn)
    if (showWireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    // Draw the selected mesh (original or smooth)
    glBindVertexArray(currentVAO);
    glDrawElements(GL_TRIANGLES, currentNumIndices, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    // Reset polygon mode to fill for other objects
    if (showWireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    glUseProgram(0); // Unbind shader program
    if (showTexture && textureID != 0) {
        glBindTexture(GL_TEXTURE_2D, 0); // Unbind texture only if it was bound
    }
}

void meshObject::drawPicking(const glm::mat4& view, const glm::mat4& projection) {
    // Picking usually uses the base mesh for simplicity and consistency
    if (pickingShaderProgram == 0 || VAO == 0) return;

    glUseProgram(pickingShaderProgram);
    glm::mat4 MVP = projection * view * modelMatrix;
    GLuint matrixID = glGetUniformLocation(pickingShaderProgram, "MVP");
    glUniformMatrix4fv(matrixID, 1, GL_FALSE, glm::value_ptr(MVP));

    // TODO: send 'id' as a uniform for color‐coded picking
    GLuint pickingColorID = glGetUniformLocation(pickingShaderProgram, "pickingColor");
    float r = (id & 0xFF) / 255.0f;
    float g = ((id >> 8) & 0xFF) / 255.0f;
    float b = ((id >> 16) & 0xFF) / 255.0f;
    glUniform4f(pickingColorID, r, g, b, 1.0f);

    glBindVertexArray(VAO); // Use base mesh VAO for picking
    glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, 0); // Use base mesh indices
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

void meshObject::toggleSmooth() {
    showSmooth = !showSmooth;
    std::cout << "Smooth Shading Toggled: " << (showSmooth ? "ON" : "OFF") << std::endl;
    if (showSmooth && subdivisionLevel < targetSubdivisionLevel) {
        setSubdivisionLevel(targetSubdivisionLevel); // Apply subdivision if needed
    }
}

void meshObject::toggleTexture() {
    showTexture = !showTexture;
    std::cout << "Texture Mapping Toggled: " << (showTexture ? "ON" : "OFF") << std::endl;
}

void meshObject::setSubdivisionLevel(int level) {
    if (level < 0) level = 0;
    if (level == subdivisionLevel) return; // No change needed

    std::cout << "Setting subdivision level to: " << level << std::endl;

    // Reset to base mesh if needed
    if (level < subdivisionLevel) {
        smoothVertices = vertices;
        smoothUvs = uvs;
        smoothNormals = normals;
        smoothIndices = indices;
        subdivisionLevel = 0;
    }

    // Apply subdivision iteratively
    while (subdivisionLevel < level) {
        applyLoopSubdivision();
        subdivisionLevel++;
        std::cout << "Applied subdivision level: " << subdivisionLevel << std::endl;
    }

    // Recalculate normals for the final subdivided mesh
    calculateNormals(smoothVertices, smoothIndices, smoothNormals);

    // Update smooth buffers with the new data
    numSmoothIndices = static_cast<GLsizei>(smoothIndices.size());
    setupSmoothBuffers(); // Re-setup buffers with new data
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

// Setup VAO, VBOs, EBO for the base mesh
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


// Setup VAO, VBOs, EBO for the smooth (subdivided) mesh
void meshObject::setupSmoothBuffers() {
    // Clean up existing buffers if they exist
    if (smoothVAO != 0) glDeleteVertexArrays(1, &smoothVAO);
    if (smoothVBO_vertices != 0) glDeleteBuffers(1, &smoothVBO_vertices);
    if (smoothVBO_uvs != 0) glDeleteBuffers(1, &smoothVBO_uvs);
    if (smoothVBO_normals != 0) glDeleteBuffers(1, &smoothVBO_normals);
    if (smoothEBO != 0) glDeleteBuffers(1, &smoothEBO);

    glGenVertexArrays(1, &smoothVAO);
    glBindVertexArray(smoothVAO);

    // Vertex Buffer
    glGenBuffers(1, &smoothVBO_vertices);
    glBindBuffer(GL_ARRAY_BUFFER, smoothVBO_vertices);
    glBufferData(GL_ARRAY_BUFFER, smoothVertices.size() * sizeof(glm::vec3), smoothVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(0);

    // UV Buffer
    if (!smoothUvs.empty()) {
        glGenBuffers(1, &smoothVBO_uvs);
        glBindBuffer(GL_ARRAY_BUFFER, smoothVBO_uvs);
        glBufferData(GL_ARRAY_BUFFER, smoothUvs.size() * sizeof(glm::vec2), smoothUvs.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
        glEnableVertexAttribArray(1);
    }

    // Normal Buffer
    if (!smoothNormals.empty()) {
        glGenBuffers(1, &smoothVBO_normals);
        glBindBuffer(GL_ARRAY_BUFFER, smoothVBO_normals);
        glBufferData(GL_ARRAY_BUFFER, smoothNormals.size() * sizeof(glm::vec3), smoothNormals.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
        glEnableVertexAttribArray(2);
    }

    // Element Buffer
    glGenBuffers(1, &smoothEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, smoothEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, smoothIndices.size() * sizeof(unsigned int), smoothIndices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0); // Unbind VAO
}

// Calculate vertex normals based on face normals
void meshObject::calculateNormals(std::vector<glm::vec3>& verts, const std::vector<unsigned int>& inds, std::vector<glm::vec3>& norms) {
    norms.assign(verts.size(), glm::vec3(0.0f)); // Initialize normals to zero

    for (size_t i = 0; i < inds.size(); i += 3) {
        unsigned int i0 = inds[i];
        unsigned int i1 = inds[i + 1];
        unsigned int i2 = inds[i + 2];

        glm::vec3 v0 = verts[i0];
        glm::vec3 v1 = verts[i1];
        glm::vec3 v2 = verts[i2];

        glm::vec3 edge1 = v1 - v0;
        glm::vec3 edge2 = v2 - v0;
        glm::vec3 faceNormal = glm::normalize(glm::cross(edge1, edge2));

        // Accumulate face normal onto vertex normals
        norms[i0] += faceNormal;
        norms[i1] += faceNormal;
        norms[i2] += faceNormal;
    }

    // Normalize all vertex normals
    for (glm::vec3& normal : norms) {
        normal = glm::normalize(normal);
    }
}

// Apply one level of Loop subdivision
void meshObject::applyLoopSubdivision() {
    std::vector<glm::vec3> nextVertices;
    std::vector<glm::vec2> nextUvs;
    std::vector<unsigned int> nextIndices;

    const size_t originalVertexCount = smoothVertices.size();
    nextVertices.resize(originalVertexCount); // Pre-allocate space for updated original vertices
    nextUvs.resize(originalVertexCount);

    // --- Precomputation: Adjacency and Boundary Info ---
    std::map<Edge, std::vector<unsigned int>> edgeOppositeVertices; // Edge -> opposite vertex(es)
    std::map<Edge, int> edgeFaceCount;                          // Edge -> number of faces it belongs to
    std::map<unsigned int, std::set<unsigned int>> vertexNeighbors; // Vertex -> adjacent vertices
    std::map<Edge, unsigned int> edgeMidpointIndices;             // Edge -> index of the new midpoint vertex

    // Build adjacency, edge face counts, and opposite vertices
    for (size_t i = 0; i < smoothIndices.size(); i += 3) {
        unsigned int v[3] = { smoothIndices[i], smoothIndices[i + 1], smoothIndices[i + 2] };

        for (int j = 0; j < 3; ++j) {
            unsigned int v0 = v[j];
            unsigned int v1 = v[(j + 1) % 3];
            unsigned int v_opposite = v[(j + 2) % 3];
            Edge edge = {v0, v1};

            edgeFaceCount[edge]++;
            edgeOppositeVertices[edge].push_back(v_opposite);

            vertexNeighbors[v0].insert(v1);
            vertexNeighbors[v1].insert(v0);
        }
    }

    std::set<Edge> boundaryEdges;
    std::set<unsigned int> boundaryVertices;
    for(const auto& pair : edgeFaceCount) {
        if (pair.second == 1) {
            boundaryEdges.insert(pair.first);
            boundaryVertices.insert(pair.first.v1);
            boundaryVertices.insert(pair.first.v2);
        }
    }

    // --- Step 1: Create new edge vertices (midpoints) --- 
    // Reserve space - approximate number of new vertices
    nextVertices.reserve(originalVertexCount + edgeFaceCount.size()); 
    nextUvs.reserve(originalVertexCount + edgeFaceCount.size());

    unsigned int currentNewVertexIndex = (unsigned int)originalVertexCount;
    for (const auto& pair : edgeOppositeVertices) {
        const Edge& edge = pair.first;
        const std::vector<unsigned int>& opposites = pair.second;

        unsigned int v0 = edge.v1;
        unsigned int v1 = edge.v2;

        glm::vec3 newPos;
        glm::vec2 newUv;

        if (boundaryEdges.count(edge)) { // Boundary edge rule
            newPos = 0.5f * (smoothVertices[v0] + smoothVertices[v1]);
            newUv = 0.5f * (smoothUvs[v0] + smoothUvs[v1]);
        } else { // Interior edge rule
            if (opposites.size() == 2) { // Should always be 2 for interior edges
                unsigned int v_opp1 = opposites[0];
                unsigned int v_opp2 = opposites[1];
                newPos = (3.0f / 8.0f) * (smoothVertices[v0] + smoothVertices[v1]) +
                         (1.0f / 8.0f) * (smoothVertices[v_opp1] + smoothVertices[v_opp2]);
                newUv = (3.0f / 8.0f) * (smoothUvs[v0] + smoothUvs[v1]) +
                        (1.0f / 8.0f) * (smoothUvs[v_opp1] + smoothUvs[v_opp2]);
            } else {
                // Should not happen for a manifold mesh, fallback to boundary rule
                 std::cerr << "Warning: Interior edge with != 2 opposite vertices found. Edge: (" << v0 << ", " << v1 << ") Opposites: " << opposites.size() << std::endl;
                 newPos = 0.5f * (smoothVertices[v0] + smoothVertices[v1]);
                 newUv = 0.5f * (smoothUvs[v0] + smoothUvs[v1]);
            }
        }
        edgeMidpointIndices[edge] = currentNewVertexIndex;
        nextVertices.push_back(newPos);
        nextUvs.push_back(newUv);
        currentNewVertexIndex++;
    }

    // --- Step 2: Update original vertex positions --- 
    for (unsigned int i = 0; i < originalVertexCount; ++i) {
        const std::set<unsigned int>& neighbors = vertexNeighbors[i];
        int k = (int)neighbors.size();

        if (boundaryVertices.count(i)) { // Boundary vertex rule
            std::vector<unsigned int> boundaryNeighbors;
            for (unsigned int neighbor_idx : neighbors) {
                if (boundaryEdges.count({i, neighbor_idx})) {
                    boundaryNeighbors.push_back(neighbor_idx);
                }
            }

            if (boundaryNeighbors.size() == 2) {
                unsigned int n1 = boundaryNeighbors[0];
                unsigned int n2 = boundaryNeighbors[1];
                nextVertices[i] = (1.0f / 8.0f) * smoothVertices[n1] + 
                                  (6.0f / 8.0f) * smoothVertices[i] + 
                                  (1.0f / 8.0f) * smoothVertices[n2];
                nextUvs[i] = (1.0f / 8.0f) * smoothUvs[n1] + 
                             (6.0f / 8.0f) * smoothUvs[i] + 
                             (1.0f / 8.0f) * smoothUvs[n2];
            } else {
                 // Corner or isolated boundary vertex - keep original position for simplicity
                 // More complex corner rules exist but are harder to implement robustly.
                 // std::cerr << "Warning: Boundary vertex " << i << " has " << boundaryNeighbors.size() << " boundary neighbors. Keeping original position." << std::endl;
                 nextVertices[i] = smoothVertices[i];
                 nextUvs[i] = smoothUvs[i];
            }
        } else { // Interior vertex rule
            float beta;
            if (k == 3) {
                beta = 3.0f / 16.0f;
            } else { // k > 3 (k < 3 shouldn't happen for interior)
                beta = (1.0f / k) * (5.0f / 8.0f - pow(3.0f / 8.0f + 0.25f * cos(2.0f * glm::pi<float>() / k), 2.0f));
            }

            glm::vec3 neighborPosSum(0.0f);
            glm::vec2 neighborUvSum(0.0f);
            for (unsigned int neighbor_idx : neighbors) {
                neighborPosSum += smoothVertices[neighbor_idx];
                neighborUvSum += smoothUvs[neighbor_idx];
            }

            nextVertices[i] = (1.0f - k * beta) * smoothVertices[i] + beta * neighborPosSum;
            nextUvs[i] = (1.0f - k * beta) * smoothUvs[i] + beta * neighborUvSum;
        }
    }

    // --- Step 3: Create new faces --- 
    nextIndices.reserve(smoothIndices.size() * 4); // Each triangle becomes 4
    for (size_t i = 0; i < smoothIndices.size(); i += 3) {
        unsigned int v0 = smoothIndices[i];
        unsigned int v1 = smoothIndices[i + 1];
        unsigned int v2 = smoothIndices[i + 2];

        // Get indices of midpoints (handle potential errors if map lookup fails)
        unsigned int m01 = edgeMidpointIndices.at({ v0, v1 });
        unsigned int m12 = edgeMidpointIndices.at({ v1, v2 });
        unsigned int m20 = edgeMidpointIndices.at({ v2, v0 });

        // Add 4 new triangles (indices refer to nextVertices array)
        nextIndices.push_back(v0); nextIndices.push_back(m01); nextIndices.push_back(m20);
        nextIndices.push_back(v1); nextIndices.push_back(m12); nextIndices.push_back(m01);
        nextIndices.push_back(v2); nextIndices.push_back(m20); nextIndices.push_back(m12);
        nextIndices.push_back(m01); nextIndices.push_back(m12); nextIndices.push_back(m20);
    }

    // Update the mesh data
    smoothVertices = std::move(nextVertices);
    smoothUvs = std::move(nextUvs);
    smoothIndices = std::move(nextIndices);
    // Normals will be recalculated after all subdivision levels are applied in setSubdivisionLevel
}
