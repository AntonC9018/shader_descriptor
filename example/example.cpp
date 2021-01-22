// Warning: This file has been autogenerated by the tool!
#include <glm/glm.hpp>
#include <glad/glad.h>
struct Example_Shader {
    GLuint id;
    inline void use()
    {
        glUseProgram(id);
    }
    GLint foo_location;
    inline void foo(glm::vec3 foo)
    {
        glUniform3fv(foo_location, 1, (float*)&foo);
    }
    GLint bar_location;
    inline void bar(glm::float32 bar)
    {
        glUniform1f(bar_location, bar);
    }
    GLint baz_location;
    inline void baz(glm::mat4 baz)
    {
        glUniformMatrix4fv(baz_location, 1, GL_FALSE, (float*)&baz);
    }
    inline void set_locations()
    {
        foo_location = glGetUniformLocation(id, "foo");
        bar_location = glGetUniformLocation(id, "bar");
        baz_location = glGetUniformLocation(id, "baz");
    }
    inline void uniforms(glm::vec3 foo_v, glm::float32 bar_v, glm::mat4 baz_v)
    {
        foo(foo_v);
        bar(bar_v);
        baz(baz_v);
    }
};
