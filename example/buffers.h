#pragma once
// Warning: This file has been autogenerated by the tool!
#include <glm/glm.hpp>
#include <glad/glad.h>
#pragma pack(1)
struct Stuff
{
    glm::float32 fuu;
    char _padding_0[12];
    glm::vec4 fee;
    glm::vec3 bag;
    char _padding_1[4];
    glm::mat4 too;
};
struct Stuff_Block
{
    GLuint id;
    GLuint binding_point;
    inline void create()
    {
        glGenBuffers(1, &id);
        glBindBuffer(GL_UNIFORM_BUFFER, id);
        glBufferData(GL_UNIFORM_BUFFER, 128, NULL, GL_STATIC_DRAW);
    }
    inline void bind()
    {
        glBindBuffer(GL_UNIFORM_BUFFER, id);
    }
    inline void data(Stuff* data)
    {
        glBufferData(GL_UNIFORM_BUFFER, 128, data, GL_STATIC_DRAW);
    }
    const GLuint fuu_offset = 0;
    const GLuint fee_offset = 16;
    const GLuint bag_offset = 32;
    const GLuint too_offset = 48;
    inline void fuu(glm::float32* fuu)
    {
        glBufferSubData(GL_UNIFORM_BUFFER, fuu_offset, 128, fuu);
    }
    inline void fee(glm::vec4* fee)
    {
        glBufferSubData(GL_UNIFORM_BUFFER, fee_offset, 128, fee);
    }
    inline void bag(glm::vec3* bag)
    {
        glBufferSubData(GL_UNIFORM_BUFFER, bag_offset, 128, bag);
    }
    inline void too(glm::mat4* too)
    {
        glBufferSubData(GL_UNIFORM_BUFFER, too_offset, 128, too);
    }
};
#pragma pack(1)
struct Stuff_2
{
    glm::float32 fuu;
    char _padding_0[12];
    glm::vec4 fee;
    glm::vec3 bag;
    char _padding_1[4];
    glm::mat4 too;
};
struct Stuff_2_Block
{
    GLuint id;
    GLuint binding_point;
    inline void create()
    {
        glGenBuffers(1, &id);
        glBindBuffer(GL_UNIFORM_BUFFER, id);
        glBufferData(GL_UNIFORM_BUFFER, 128, NULL, GL_STATIC_DRAW);
    }
    inline void bind()
    {
        glBindBuffer(GL_UNIFORM_BUFFER, id);
    }
    inline void data(Stuff_2* data)
    {
        glBufferData(GL_UNIFORM_BUFFER, 128, data, GL_STATIC_DRAW);
    }
    const GLuint fuu_offset = 0;
    const GLuint fee_offset = 16;
    const GLuint bag_offset = 32;
    const GLuint too_offset = 48;
    inline void fuu(glm::float32* fuu)
    {
        glBufferSubData(GL_UNIFORM_BUFFER, fuu_offset, 128, fuu);
    }
    inline void fee(glm::vec4* fee)
    {
        glBufferSubData(GL_UNIFORM_BUFFER, fee_offset, 128, fee);
    }
    inline void bag(glm::vec3* bag)
    {
        glBufferSubData(GL_UNIFORM_BUFFER, bag_offset, 128, bag);
    }
    inline void too(glm::mat4* too)
    {
        glBufferSubData(GL_UNIFORM_BUFFER, too_offset, 128, too);
    }
};
