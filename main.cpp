#include <stdio.h>
#include <string>
#include <string.h>
#include <vector>
#include <map>

struct String_Builder
{
    char* data;
    char* current;
};

inline String_Builder sb_create(size_t size)
{
    String_Builder result;
    result.data = (char*)malloc(size);
    result.current = result.data;
    return result;
}

inline void sb_free(String_Builder& sb)
{
    free(sb.data);
}

inline void sb_cat(String_Builder& sb, const char* src)
{
    while(*src != 0)
    {
        *sb.current = *src;
        sb.current++; src++;
    }
}

inline void sb_chr(String_Builder& sb, char ch)
{
    *sb.current = ch;
    sb.current++;
}

inline void sb_null_terminate(String_Builder& sb)
{
    *sb.current = 0;
}

inline void sb_reset(String_Builder& sb)
{
    sb.current = sb.data;
}

inline void sb_cat_until(String_Builder& sb, const char* src, char stop)
{
    while(*src != 0 && *src != stop)
    {
        *sb.current = *src;
        sb.current++; src++;
    }
}

inline const char* sb_build(String_Builder& sb)
{
    sb_null_terminate(sb);
    return sb.data;
}

// Copies the given string into a fresh malloc-ed buffer
inline const char* string_copy_with_malloc(const char* src)
{
    size_t size = strlen(src) + 1;
    char* buff = (char*)malloc(size);
    memcpy(buff, src, size);
    return buff;
}

// Increments the pointer until the first occurence of a character, which isn't whitespace
inline char* trim_front(char* str, char ch = ' ')
{
    while (*str == ch)
    {
        str++;
    }
    return str;
}

// Own the pointers. (so technically unique_ptr<const char> would be more correct, but screw c++)
struct Uniform_String
{
    const char* type;
    const char* name;
    const char* location_name;
};
typedef void (*WriteUniformFunc)(FILE *out, const Uniform_String& u);

void write_float32(FILE *out, const Uniform_String& u)
{
    fprintf(out, "        glUniform1f(%s, %s);\n", u.location_name, u.name);
}
void write_vec3(FILE *out, const Uniform_String& u)
{
    fprintf(out, "        glUniform3fv(%s, 1, (float*)&%s);\n", u.location_name, u.name);
}
void write_vec2(FILE *out, const Uniform_String& u)
{
    fprintf(out, "        glUniform2fv(%s, 1, (float*)&%s);\n", u.location_name, u.name);
}
void write_mat4(FILE *out, const Uniform_String& u)
{
    fprintf(out, "        glUniformMatrix4fv(%s, 1, GL_FALSE, (float*)&%s);\n", u.location_name, u.name);
}


std::map<std::string, const char*> glsl_to_type_map
{
    { "float", "glm::float32" },
    { "vec2", "glm::vec2" },
    { "vec3", "glm::vec3" },
    { "mat4", "glm::mat4" }
};

std::map<std::string, std::vector<Uniform_String>> custom_types;

std::map<std::string, WriteUniformFunc> type_map
{
    { "glm::float32", write_float32 },
    { "glm::vec3", write_vec3 },
    { "glm::vec2", write_vec2 },
    { "glm::mat4", write_mat4 }   
};

// Stores the file currently being processed and the line number
struct Parse_Info
{
    const char* file;
    int line;
};

inline const char* try_map_type(const char* unmapped_type, Parse_Info parse_info)
{
    const char* type;

    auto remapped = glsl_to_type_map.find(unmapped_type);
    if (remapped != glsl_to_type_map.end())
    {
        type = (*remapped).second;
    }
    else
    {
        type = string_copy_with_malloc(unmapped_type);        
    }

    if (type_map.find(type) == type_map.end() && custom_types.find(type) == custom_types.end())
    {
        fprintf(stderr, "shd Error: Unrecognized type: \"%s\" in file %s, line %d.\n", 
            type, parse_info.file, parse_info.line);
        exit(-1);
    }

    return type;
}


// Assume you have the uniform `Thing thing;` which is of user defined type `Thing`.
// Thing in turn has their own members. Assume it has members `vec3 foo` and `float bar`.
// The way you query locations of the members in open gl is by querying the location of
// "thing.foo" for `foo` and "thing.bar" for `bar`.
// Likewise, the location variable would be named `thing_foo_location` and `thing_bar_location` respectively.
//
// This function combines together the info of a custom type uniform definition + struct's member info.
// E.g. { type = "Thing", name = "thing", location = "thing" } 
//    + { type = "glm::vec3", name = "foo", location = "foo_location" }
//    = { type = "glm::vec3", name = "thing.foo", location = "thing_foo_location" } 
inline Uniform_String wrap_struct_member(const Uniform_String uniform, const Uniform_String member_info)
{
    auto name = sb_create(64);
    sb_cat(name, uniform.name);
    sb_chr(name, '.');
    sb_cat(name, member_info.name);
    sb_null_terminate(name);

    auto location = sb_create(64);
    sb_cat(location, uniform.name);
    sb_chr(location, '_');
    sb_cat(location, member_info.location_name);

    Uniform_String result;
    result.type = member_info.type;
    result.name = sb_build(name); 
    result.location_name = sb_build(location);

    return result;
}

// Writes the code for setting the specified uniform to the specified stream.
// TODO: wrap once and pass into thin function a vector of already wrapped things
inline void write_uniform(FILE *out, const Uniform_String& u)
{
    if (custom_types.find(u.type) != custom_types.end())
    {
        for (auto member_info : custom_types[u.type])
        {
            write_uniform(out, wrap_struct_member(u, member_info));
        }
    }
    else
    {
        type_map[u.type](out, u);
    }
}

inline void write_location_declaration(FILE *out, const Uniform_String& u)
{
    if (custom_types.find(u.type) != custom_types.end())
    {
        for (auto member_info : custom_types[u.type])
        {
            write_location_declaration(out, wrap_struct_member(u, member_info));
        }
    }
    else
    {
        fprintf(out, "    GLint %s;\n", u.location_name);
    }
}

inline void write_location(FILE *out, const Uniform_String& u)
{
    if (custom_types.find(u.type) != custom_types.end())
    {
        for (auto member_info : custom_types[u.type])
        {
            write_location(out, wrap_struct_member(u, member_info));
        }
    }
    else
    {
        fprintf(out, "        %s = glGetUniformLocation(id, \"%s\");\n", u.location_name, u.name);
    }
}


Uniform_String parse_as_declaration(char* buffer, Parse_Info parse_info)
{
    char *type_start = buffer;
    char *type_end = strchr(type_start, ' ');
    *type_end = '\0';

    char *name_start = type_end + 1;
    char *name_end = strchr(name_start, ';');
    *name_end = '\0';

    const char* type = try_map_type(type_start, parse_info);

    auto location = sb_create(64);
    sb_cat(location, name_start);
    sb_cat(location, "_location");

    const char* name = string_copy_with_malloc(name_start);

    Uniform_String result = { type, name, sb_build(location) };

    return result;
}

int main(int argc, char** argv)
{
    std::vector<Uniform_String> uniforms;

    auto output_struct_name = sb_create(64);
    sb_cat_until(output_struct_name, argv[1], '.');
    sb_null_terminate(output_struct_name);
    if (output_struct_name.data[0] >= 'a' && output_struct_name.data[0] <= 'z')
    {
        output_struct_name.data[0] += 'A' - 'a';
    }

    for (int i = 2; i < argc; i++)
    {
        Parse_Info parse_info { argv[i], 0 };
        auto file = fopen(parse_info.file, "r");
        char line[1024];
        while (fgets(line, 1024, file) != NULL)
        {
            parse_info.line++;

            // line starts with "uniform "
            if (strstr(line, "uniform ") == line)
            {
                uniforms.push_back(parse_as_declaration(line + sizeof("uniform ") - 1, parse_info));
            }
            // line starts with "struct " custom struct definition
            else if (strstr(line, "struct ") == line)
            {
                char *struct_name_start = line + sizeof("struct ") - 1;
                char *struct_name_end = strchr(struct_name_start, ' ');
                if (struct_name_end == 0)
                {
                    struct_name_end = strchr(struct_name_start, '\n');
                }
                if (struct_name_end != 0)
                {
                    *struct_name_end = '\0';
                }
                std::string struct_name = { struct_name_start };
                custom_types[struct_name] = std::vector<Uniform_String>();
                std::vector<Uniform_String>* members = &custom_types[struct_name];
                
                // } means reached the end of struct
                while (fgets(line, 1024, file) != NULL && strchr(line, '}') != line)
                {
                    // the minimum length line would be of sorts `A a;`, which is 4 characters
                    if (strlen(line) < 4 || strstr(line, "//") == line)
                    {
                        continue;
                    }
                    
                    members->push_back(parse_as_declaration(trim_front(line), parse_info)); 
                }
            }
            
        }
        fclose(file);
    }

    auto out = fopen(argv[1], "w+");
    
    fputs(
        "// Warning: This file has been autogenerated by the tool!\n"\
        "#include <glm/glm.hpp>\n"                                   \
        "#include <glad/glad.h>\n"                                   \
        , out);

    // print custom types
    for (auto const& [type, uniforms] : custom_types)
    {
        fprintf(out, "struct %s\n{\n", type.c_str());
        for (auto const& u : uniforms)
        {
            fprintf(out, "    %s %s;\n", u.type, u.name);
        }
        fputs("};\n\n", out);
    }

    fprintf(out,
                \
            "struct %s_Program {\n"       \
            "    GLuint id;\n"            \
            "    inline void use()\n"     \
            "    {\n"                     \
            "        glUseProgram(id);\n" \
            "    }\n"
        , output_struct_name.data);

    // Location declarations
    for (const auto& u : uniforms)
    {
        write_location_declaration(out, u);
    }

    // Uniform setters
    for (const auto& u : uniforms)
    {
        fprintf(out, "    inline void %s(%s %s)\n    {\n", u.name, u.type, u.name);
        write_uniform(out, u);
        fputs("    }\n", out);
    }

    // Initializing locations
    fputs("    inline void query_locations()\n    {\n", out);
    for (const auto& u : uniforms)
    {
        write_location(out, u);
    }

    // Setting all uniforms. The function header.
    fputs("    }\n    inline void uniforms(", out);
    for (int i = 0; i < uniforms.size(); i++)
    {
        fprintf(out, "%s %s_v", uniforms[i].type, uniforms[i].name);
        if (i < uniforms.size() - 1)
        {
            fputc(',', out);
            fputc(' ', out);
        }
    }
    fputs(")\n    {\n", out);

    // Calling the appropriate uniform setters.
    for (const auto& u : uniforms)
    {
        fprintf(out, "        %s(%s_v);\n", u.name, u.name);
    }
    fputs("    }\n};\n", out);
    fclose(out);
}