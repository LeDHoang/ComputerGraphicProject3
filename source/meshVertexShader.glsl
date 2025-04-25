#version 330 core

// Input vertex attributes (from VBO)
layout(location = 0) in vec3 position; // Vertex position
layout(location = 1) in vec2 vertexUV; // Texture coordinates
//TODO: P1bTask5 - Input Normals for lighting

// Output to fragment shader
out vec2 UV;

// Uniforms
uniform mat4 MVP; // Combined Model-View-Projection matrix


void main() {
    // Transform the vertex position
    gl_Position = MVP * vec4(position, 1.0);

    // Pass UV coordinates to the fragment shader
    UV = vertexUV;
}
