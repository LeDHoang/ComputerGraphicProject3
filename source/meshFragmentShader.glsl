#version 330 core

// Input from vertex shader
in vec2 UV;

//TODO: P1bTask5 - Modify shader to use position, normal and light positions to compute lighting.

// Uniforms
uniform sampler2D textureSampler; // Texture sampler

// Output color
out vec4 color;

void main() {
    // Sample the texture using the UV coordinates
    color = texture(textureSampler, UV);

    // TODO: P1bTask4 - Find a way to draw the selected part in a brighter color.
    // If implementing picking highlight, you might modify 'color' here based on a picking ID or uniform.
}

