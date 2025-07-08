#version 450

layout(location = 0) in vec3 fragColour;   // Interpolated colour from verex

layout(location = 0) out vec4 outColour;  // Final output colour

void main() {
	outColour = vec4(fragColour, 1.0);
}