#version 330 core

struct Thing
{
    vec3 test;
    vec2 foo;
    mat4 bar;
};

uniform vec3 foo;
uniform float bar;
uniform mat4 baz;
uniform Thing thing;

void main()
{
    // do something
}