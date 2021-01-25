#version 330 core

struct Thing
{
    vec3 test;
    vec2 foo;
    mat4 bar;
};

layout (std140) uniform Stuff_2
{
    float fuu;
    vec4 fee;
    vec3 bag;
    mat4 too;    
};

uniform vec3 foo;
uniform float bar;
uniform mat4 baz;
uniform Thing thing;

void main()
{
    // do something
}