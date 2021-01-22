#include <stdio.h>
#include <string>
#include <string.h>
#include <vector>
#include <map>

enum Uniform_Type
{
    vec3, mat4, float32
};

struct Uniform_String
{
    std::string type;
    std::string name;
};

std::map<std::string, Uniform_Type> type_map
{
    { "float32", float32 },
    { "vec3", vec3 },
    { "mat4", mat4 }   
};

int main(int argc, char** argv)
{
    std::vector<Uniform_String> uniforms;
    for (int i = 2; i < argc; i++)
    {
        auto file = fopen(argv[i], "r");
        char line[1024];
        while (fgets(line, 1024, file) != NULL)
        {
            const char uniform_text[] = "uniform ";
            // line starts with "uniform "
            // printf("%d: %s", strstr(line, uniform_text) - line, line);
            if (strstr(line, uniform_text) == line)
            {
                // get the type
                char *type_start = line + sizeof(uniform_text) - 1;
                char *type_end = strchr(type_start, ' ');
                // maybe do this better, since this is a quite dirty trick
                *type_end = '\0';

                char *name_start = type_end + 1;
                char *name_end = strchr(name_start, ';');
                *name_end = '\0';

                uniforms.push_back({ {type_start}, {name_start} });
            }
        }
        fclose(file);
    }

    auto out = fopen(argv[1], "w+");
    char struct_name[512];
    char *dot_or_end = strchr(argv[1], '.');
    if (dot_or_end != 0)
    {
        int length = dot_or_end - argv[1];
        memcpy(struct_name, argv[1], length);
        struct_name[length] = 0;
    }
    else
    {
        strcpy(struct_name, argv[1]);
    }
    if (struct_name[0] >= 'a' && struct_name[0] <= 'z')
    {
        struct_name[0] += 'A' - 'a';
    }
    
    fprintf(out,
            "// Warning: This file has been autogenerated by the tool!\n"\
            "#include <glm/glm.hpp>\n"    \
            "#include <glad/glad.h>\n"    \
            "struct %s_Shader {\n"             \
            "    GLuint id;\n"            \
            "    inline void use()\n"     \
            "    {\n"                     \
            "        glUseProgram(id);\n" \
            "    }\n"
        , struct_name);
    for (auto& u : uniforms)
    {
        if (strcmp(u.type.c_str(), "float") == 0)
        {
            u.type = { "float32" };
        }
        fprintf(out, "    GLint %s_location;\n", u.name.c_str());
        fprintf(out, "    inline void %s(glm::%s %s)\n    {\n        ", u.name.c_str(), u.type.c_str(), u.name.c_str());

        switch (type_map[u.type])
        {
        case vec3:
            fprintf(out, "glUniform3fv(%s_location, 1, (float*)&%s);\n", u.name.c_str(), u.name.c_str());
            break;
        case mat4:
            fprintf(out, "glUniformMatrix4fv(%s_location, 1, GL_FALSE, (float*)&%s);\n", u.name.c_str(), u.name.c_str());
            break;
        case float32:
            fprintf(out, "glUniform1f(%s_location, %s);\n", u.name.c_str(), u.name.c_str());
            break;
        default:
            printf("Unknown type: %s", u.type.c_str());
            break;
        }

        fputs("    }\n", out);
    }
    fputs("    inline void set_locations()\n    {\n", out);
    for (auto& u : uniforms)
    {
        fprintf(out, "        %s_location = glGetUniformLocation(id, \"%s\");\n", u.name.c_str(), u.name.c_str());
    }
    fputs("    }\n    inline void uniforms(", out);
    for (int i = 0; i < uniforms.size(); i++)
    {
        fprintf(out, "glm::%s %s_v", uniforms[i].type.c_str(), uniforms[i].name.c_str());
        if (i < uniforms.size() - 1)
        {
            fputc(',', out);
            fputc(' ', out);
        }
    }
    fputs(")\n    {\n", out);
    for (auto& u : uniforms)
    {
        fprintf(out, "        %s(%s_v);\n", u.name.c_str(), u.name.c_str());
    }
    fputs("    }\n};\n", out);
    fclose(out);
}