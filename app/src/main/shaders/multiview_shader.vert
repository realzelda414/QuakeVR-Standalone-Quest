#version 320 es
#extension GL_OVR_multiview2 : enable

layout(num_views = 2) in;

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aNormal;

layout(std140, binding = 0) uniform EyeMatrices {
    mat4 uViewMatrix[2];
    mat4 uProjMatrix[2];
};

uniform mat4 uModelMatrix;

out vec2 vTexCoord;
out vec3 vNormal;

void main() {
    vTexCoord = aTexCoord;
    vNormal = mat3(uModelMatrix) * aNormal;
    
    // gl_ViewID_OVR is 0 for Left Eye and 1 for Right Eye
    mat4 viewProj = uProjMatrix[gl_ViewID_OVR] * uViewMatrix[gl_ViewID_OVR];
    gl_Position = viewProj * uModelMatrix * vec4(aPosition, 1.0);
}
