#include "gridObject.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <vector>

gridObject::gridObject() {
    modelMatrix = glm::mat4(1.0f);

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glBindVertexArray(VAO);

    std::vector<GLfloat> vertices;
    std::vector<GLuint>  indices;
    GLuint index = 0;

    // Draw integer grid on Y=0 from (-5,-5) to (+5,+5)
    for (int z = -5; z <= 5; ++z) {
        vertices.insert(vertices.end(), { -5.0f, 0.0f, float(z),  0.5f,0.5f,0.5f });
        vertices.insert(vertices.end(), { 5.0f, 0.0f, float(z),  0.5f,0.5f,0.5f });
        indices.push_back(index++);
        indices.push_back(index++);
    }
    for (int x = -5; x <= 5; ++x) {
        vertices.insert(vertices.end(), { float(x), 0.0f, -5.0f,  0.5f,0.5f,0.5f });
        vertices.insert(vertices.end(), { float(x), 0.0f,  5.0f,  0.5f,0.5f,0.5f });
        indices.push_back(index++);
        indices.push_back(index++);
    }

    // Positive X axis (red)
    vertices.insert(vertices.end(), { 0,0,0, 1,0,0 });
    vertices.insert(vertices.end(), { 5,0,0, 1,0,0 });
    indices.push_back(index++); indices.push_back(index++);
    // Positive Y axis (green)
    vertices.insert(vertices.end(), { 0,0,0, 0,1,0 });
    vertices.insert(vertices.end(), { 0,5,0, 0,1,0 });
    indices.push_back(index++); indices.push_back(index++);
    // Positive Z axis (blue)
    vertices.insert(vertices.end(), { 0,0,0, 0,0,1 });
    vertices.insert(vertices.end(), { 0,0,5, 0,0,1 });
    indices.push_back(index++); indices.push_back(index++);

    numIndices = static_cast<GLsizei>(indices.size());

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER,
        vertices.size() * sizeof(GLfloat),
        vertices.data(),
        GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        indices.size() * sizeof(GLuint),
        indices.data(),
        GL_STATIC_DRAW);

    // Position loc 0, Color loc 1
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    shaderProgram = LoadShaders("gridVertexShader.glsl", "gridFragmentShader.glsl");
}

gridObject::~gridObject() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(shaderProgram);
}

void gridObject::draw(const glm::mat4& view, const glm::mat4& projection) {
    glUseProgram(shaderProgram);
    glm::mat4 MVP = projection * view * modelMatrix;
    GLuint   uniMVP = glGetUniformLocation(shaderProgram, "MVP");
    glUniformMatrix4fv(uniMVP, 1, GL_FALSE, glm::value_ptr(MVP));

    glBindVertexArray(VAO);
    glDrawElements(GL_LINES, numIndices, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}
