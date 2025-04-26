#version 330 core

// Input from vertex shader
in vec2 UV;

//TODO: P1bTask5 - Modify shader to use position, normal and light positions to compute lighting.

// Uniforms
uniform sampler2D textureSampler; // Texture sampler
uniform bool useTexture; // Flag to control texture usage

// Output color
out vec4 color;

void main() {
    vec4 texColor = texture(textureSampler, UV);

    // Use texture color if useTexture is true, otherwise use a default color (e.g., white)
    color = useTexture ? texColor : vec4(0.8, 0.8, 0.8, 1.0); // Default to light grey

    // TODO: P1bTask4 - Find a way to draw the selected part in a brighter color.
    // If implementing picking highlight, you might modify 'color' here based on a picking ID or uniform.
}

