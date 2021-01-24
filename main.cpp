#include <stdio.h>
#include <string>
#include <string.h>
#include <vector>
#include <map>
#include "src/string_builder.h"
#include "src/string_util.h"
#include "src/writer.h"

struct Uniform_String
{
    const char* type; // not necessarity owned
    const char* name; // always owned
    const char* location_name; // always owned
};
typedef void (*WriteUniformFunc)(Writer* writer, const Uniform_String& u);

void write_float32(Writer* writer, const Uniform_String& u)
{
    wr_format_line(writer, "glUniform1f(%s, %s);", u.location_name, u.name);
}
void write_vec3(Writer* writer, const Uniform_String& u)
{
    wr_format_line(writer, "glUniform3fv(%s, 1, (float*)&%s);", u.location_name, u.name);
}
void write_vec2(Writer* writer, const Uniform_String& u)
{
    wr_format_line(writer, "glUniform2fv(%s, 1, (float*)&%s);", u.location_name, u.name);
}
void write_mat4(Writer* writer, const Uniform_String& u)
{
    wr_format_line(writer, "glUniformMatrix4fv(%s, 1, GL_FALSE, (float*)&%s);", u.location_name, u.name);
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
inline void write_uniform(Writer* writer, const Uniform_String& u)
{
    if (custom_types.find(u.type) != custom_types.end())
    {
        for (auto member_info : custom_types[u.type])
        {
            write_uniform(writer, wrap_struct_member(u, member_info));
        }
    }
    else
    {
        type_map[u.type](writer, u);
    }
}

inline void write_location_declaration(Writer* writer, const Uniform_String& u)
{
    if (custom_types.find(u.type) != custom_types.end())
    {
        for (auto member_info : custom_types[u.type])
        {
            write_location_declaration(writer, wrap_struct_member(u, member_info));
        }
    }
    else
    {
        wr_format_line(writer, "GLint %s;", u.location_name);
    }
}

inline void write_location(Writer* writer, const Uniform_String& u)
{
    if (custom_types.find(u.type) != custom_types.end())
    {
        for (auto member_info : custom_types[u.type])
        {
            write_location(writer, wrap_struct_member(u, member_info));
        }
    }
    else
    {
        wr_format_line(writer, "%s = glGetUniformLocation(id, \"%s\");", u.location_name, u.name);
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

    // TODO: add support for arrays
    const char* type = try_map_type(type_start, parse_info);

    auto location = sb_create(64);
    sb_cat(location, name_start);
    sb_cat(location, "_location");

    const char* name = string_copy_with_malloc(name_start);

    Uniform_String result = { type, name, sb_build(location) };

    return result;
}

struct Iteration_Option
{
    const char** input_files;
    int input_file_count;
    bool append_to_output;
    const char* output_file;
    const char* output_struct_name;
    const char* uniform_buffer_file;
};

struct Options
{
    int spaces_per_tab;
    std::vector<Iteration_Option> iteration_options;
};

void run_iteration(Iteration_Option iteration_option)
{
    std::vector<Uniform_String> uniforms;

    for (int i = 0; i < iteration_option.input_file_count; i++)
    {
        Parse_Info parse_info { iteration_option.input_files[i], 0 };
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
            // TODO: uniform block layout
            else if (strstr(line, "layout (std140) uniform ") == line)
            {
                // 1. Process exactly as a struct
                // 2. Do NOT add that data into uniform generation.
                //    Instead, write all unique block descriptors into a separate struct, since they may be shared
                //    between multiple shaders. That struct will have methods (or functions, I am not sure yet) for
                //    creating and binding the buffer and for setting a value for the uniform block.
                // 3. Uniform block layouts follow this spec for data layout: 
                //    https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_uniform_buffer_object.txt
                //    maybe use a tool for figuring out the offsets, but the algorithm shouldn't be hard.
                // 
                /*
                    // Example code for creating the uniform buffer.
                    // Done once in e.g. create function of the buffer.
                    uint32_t ubo;
                    glGenBuffers(1, &ubo);
                    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
                    glBufferData(GL_UNIFORM_BUFFER, buffer_size, NULL, GL_STATIC_DRAW);
                    glBindBuffer(GL_UNIFORM_BUFFER, 0);
                */
                /* 
                    // Getting the offset. This offset is per-program.
                    // This should be done in set_locations() in every shader program.
                    uint32_t block_index = glGetUniformBlockIndex(program_id, "Block_Name");   
                    glUniformBlockBinding(program_id, block_index, binding_point);
                */
                /*
                    // The individual offsets of block member fields will be hardcoded in the correspoding functions.
                    // assume data is received as an argument.
                    // There should be such a function for every member of the uniform buffer.
                    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
                    glBufferSubData(GL_UNIFORM_BUFFER, data_offset, data_size, &data);
                */
                // Also, if a shader program is to use such an object, every such program
                // should bind to a particular binding point. This should be done, presumably, in set_locations().
            }
            
        }
        fclose(file);
    }

    Writer writer;
    writer.stream = fopen(iteration_option.output_file, "w+");
    writer.current_indentation_level = 0;
    Writer *wr = &writer;
    
    wr_puts(wr,
        "// Warning: This file has been autogenerated by the tool!\n"\
        "#include <glm/glm.hpp>\n"                                   \
        "#include <glad/glad.h>\n"                                   
    );

    // print custom types
    for (auto const& [type, uniforms] : custom_types)
    {
        wr_format_line(wr, "struct %s", type.c_str());
        wr_start_struct(wr);
        for (auto const& u : uniforms)
        {
            wr_format_line(wr, "%s %s;", u.type, u.name);
        }
        wr_end_struct(wr);
    }

    // print uniform block layout types
    // for (auto const& [type, members] : uniform_blocks)
    // {   
    //     fprintf(out, "struct %s\n{\n", type.c_str());
    //     for (auto const& member : members)
    //     {
    //         fprintf(out, "    ", member.offset
    //     }
    // } 

    wr_format_line(wr, "struct %s_Program", iteration_option.output_struct_name);
    wr_start_struct(wr);
    wr_line(wr, "GLuint id;");
    wr_line(wr, "inline void use()");
    wr_start_block(wr);
    wr_line(wr, "glUseProgram(id);");
    wr_end_block(wr);
        
    // Location declarations
    for (const auto& u : uniforms)
    {
        write_location_declaration(wr, u);
    }

    // Uniform setters
    for (const auto& u : uniforms)
    {
        wr_format_line(wr, "inline void %s(%s %s)", u.name, u.type, u.name);
        wr_start_block(wr);
        write_uniform(wr, u);
        wr_end_block(wr);
    }

    // Initializing locations
    wr_line(wr, "inline void query_locations()");
    wr_start_block(wr);
    for (const auto& u : uniforms)
    {
        write_location(wr, u);
    }
    wr_end_block(wr);

    // Setting all uniforms. The function header.
    wr_print_indent(wr);
    wr_puts(wr, "inline void uniforms(");

    for (int i = 0; i < uniforms.size(); i++)
    {
        wr_format(wr, "%s %s_v", uniforms[i].type, uniforms[i].name);
        if (i < uniforms.size() - 1)
        {
            wr_puts(wr, ", ");
        }
    }
    wr_puts(wr, ")\n");
    wr_start_block(wr);

    // Calling the appropriate uniform setters.
    for (const auto& u : uniforms)
    {
        wr_format_line(wr, "%s(%s_v);", u.name, u.name);
    }

    wr_end_block(wr);
    wr_end_struct(wr);

    fclose(writer.stream);
}

void run(Options* options)
{
    spaces_per_indentation = options->spaces_per_tab;
    for (auto iteration_option : options->iteration_options)
    {
        run_iteration(iteration_option);
    } 
}

int main(int argc, const char** argv)
{
    Iteration_Option option;
    option.input_files = &argv[2];
    option.input_file_count = argc - 2;
    option.append_to_output = false;
    option.output_file = argv[1];

    auto output_struct_name = sb_create(64);
    sb_cat_until(output_struct_name, option.output_file, '.');
    if (output_struct_name.data[0] >= 'a' && output_struct_name.data[0] <= 'z')
    {
        output_struct_name.data[0] += 'A' - 'a';
    }

    option.output_struct_name = sb_build(output_struct_name);
    option.uniform_buffer_file = argv[1];

    Options options = { 4, { option } };

    run(&options);
}