#include <stdio.h>
#include <string>
#include <string.h>
#include <vector>
#include <map>
#include "src/string_builder.h"
#include "src/string_util.h"
#include "src/writer.h"

struct Uniform
{
    const char* type; // not necessarity owned
    const char* name; // always owned
    const char* location_name; // always owned
};
typedef void (*WriteUniformFunc)(Writer* writer, const Uniform& u);

struct Uniform_Type_Info
{
    WriteUniformFunc write_func;
    uint32_t size_in_bytes;
};

void write_float32(Writer* writer, const Uniform& u)
{
    wr_format_line(writer, "glUniform1f(%s, %s);", u.location_name, u.name);
}
void write_vec4(Writer* writer, const Uniform& u)
{
    wr_format_line(writer, "glUniform4fv(%s, 1, (float*)&%s);", u.location_name, u.name);
}
void write_vec3(Writer* writer, const Uniform& u)
{
    wr_format_line(writer, "glUniform3fv(%s, 1, (float*)&%s);", u.location_name, u.name);
}
void write_vec2(Writer* writer, const Uniform& u)
{
    wr_format_line(writer, "glUniform2fv(%s, 1, (float*)&%s);", u.location_name, u.name);
}
void write_mat4(Writer* writer, const Uniform& u)
{
    wr_format_line(writer, "glUniformMatrix4fv(%s, 1, GL_FALSE, (float*)&%s);", u.location_name, u.name);
}


std::map<std::string, const char*> glsl_to_uniform_type_map
{
    { "float", "glm::float32" },
    { "vec2", "glm::vec2" },
    { "vec3", "glm::vec3" },
    { "vec4", "glm::vec4" },
    { "mat4", "glm::mat4" }
};

std::map<std::string, std::vector<Uniform>> custom_types;

std::map<std::string, Uniform_Type_Info> uniform_type_map
{
    { "glm::float32", { write_float32, 4         } },
    { "glm::vec4",    { write_vec4,    4 * 4     } },
    { "glm::vec3",    { write_vec3,    3 * 4     } },
    { "glm::vec2",    { write_vec2,    2 * 4     } },
    { "glm::mat4",    { write_mat4,    4 * 4 * 4 } }  
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

    auto remapped = glsl_to_uniform_type_map.find(unmapped_type);
    if (remapped != glsl_to_uniform_type_map.end())
    {
        type = (*remapped).second;
    }
    else
    {
        type = string_copy_with_malloc(unmapped_type);        
    }

    if (uniform_type_map.find(type) == uniform_type_map.end() && custom_types.find(type) == custom_types.end())
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
inline Uniform wrap_struct_member(const Uniform uniform, const Uniform member_info)
{
    auto name = sb_create(64);
    sb_cat(name, uniform.name);
    sb_chr(name, '.');
    sb_cat(name, member_info.name);

    auto location = sb_create(64);
    sb_cat(location, uniform.name);
    sb_chr(location, '_');
    sb_cat(location, member_info.location_name);

    Uniform result;
    result.type = member_info.type;
    result.name = sb_build(name); 
    result.location_name = sb_build(location);

    return result;
}

// Writes the code for setting the specified uniform to the specified stream.
// TODO: wrap once and pass into thin function a vector of already wrapped things
inline void write_uniform(Writer* writer, const Uniform& u)
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
        uniform_type_map[u.type].write_func(writer, u);
    }
}

inline void write_location_declaration(Writer* writer, const Uniform& u)
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

inline void write_location(Writer* writer, const Uniform& u)
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


Uniform parse_as_declaration(char* buffer, Parse_Info parse_info)
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

    Uniform result = { type, name, sb_build(location) };

    return result;
}

typedef std::vector<Uniform> Struct_Members;

struct Struct
{
    const char* name;
    std::vector<Uniform> members;
};

Struct parse_as_struct(char* buffer, FILE* file, char* struct_name_start, Parse_Info parse_info)
{
    Struct result;
    char *struct_name_end = strchr(struct_name_start, ' ');
    if (struct_name_end == 0)
    {
        struct_name_end = strchr(struct_name_start, '\n');
    }
    *struct_name_end = '\0';

    result.name = string_copy_with_malloc(struct_name_start);
    // } means reached the end of struct
    while (fgets(buffer, 1024, file) != NULL && strchr(buffer, '}') != buffer)
    {
        // the minimum length line would be of sorts `A a;`, which is 4 characters
        if (strlen(buffer) < 4 || strstr(buffer, "//") == buffer)
        {
            continue;
        }
        
        result.members.push_back(parse_as_declaration(trim_front(buffer), parse_info)); 
    }

    return std::move(result);
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

struct Uniform_Block
{
    std::vector<uint32_t> offsets;
    std::vector<uint32_t> pad_bytes;
    uint32_t total_size;
    std::vector<Uniform> members;
};

void run_iteration(Iteration_Option iteration_option)
{
    std::vector<Uniform> uniforms;
    std::map<std::string, Uniform_Block> uniform_blocks;

    for (int i = 0; i < iteration_option.input_file_count; i++)
    {
        Parse_Info parse_info { iteration_option.input_files[i], 0 };
        auto file = fopen(parse_info.file, "r");
        char buffer[1024];
        while (fgets(buffer, 1024, file) != NULL)
        {
            parse_info.line++;

            // line starts with "uniform"
            if (strstr(buffer, "uniform") == buffer)
            {
                uniforms.push_back(parse_as_declaration(buffer + sizeof("uniform"), parse_info));
            }
            // line starts with "struct" custom struct definition
            else if (strstr(buffer, "struct") == buffer)
            {
                auto _struct = parse_as_struct(buffer, file, buffer + sizeof("struct"), parse_info);
                custom_types[{ _struct.name }] = std::move(_struct.members);
            }
            // TODO: uniform block layout
            else if (strstr(buffer, "layout (std140) uniform") == buffer)
            {
                // 1. Process exactly as a struct
                auto _struct = parse_as_struct(buffer, file, buffer + sizeof("layout (std140) uniform"), parse_info);
                // 2. Do NOT add that data into uniform generation.
                //    Instead, write all unique block descriptors into a separate struct, since they may be shared
                //    between multiple shaders. That struct will have methods (or functions, I am not sure yet) for
                //    creating and binding the buffer and for setting a value for the uniform block.
                // 3. Uniform block layouts follow this spec for data layout: 
                //    https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_uniform_buffer_object.txt
                //    maybe use a tool for figuring out the offsets, but the algorithm shouldn't be hard.
                uniform_blocks[{ _struct.name }] = Uniform_Block{};
                Uniform_Block* block = &uniform_blocks[{ _struct.name }];

                uint32_t current_offset = 0;
                uint32_t dword_x4_fullness = 0;
                for (const auto& member : _struct.members)
                {
                    auto member_size = uniform_type_map[{ member.type }].size_in_bytes;
                    // If adding the member goes over the float "socket" size,
                    // skip the remaining bytes in that unoccupied memory.
                    // E.g. having { float, vec4 }, the offsets would be float at 0 to 4, skip 12 bytes, vec4 at 16 to 32.
                    // E.g. { float, vec3 } would go to { 0 -> 4, 4 -> 16 } instead, since they both fit in that 4 * dword 
                    if (dword_x4_fullness > 0 && (member_size + dword_x4_fullness > 16))
                    {
                        auto skipped_bytes = 16 - dword_x4_fullness;
                        block->pad_bytes.push_back(skipped_bytes);
                        current_offset += skipped_bytes;
                        dword_x4_fullness = 0;
                    }
                    else
                    {
                        block->pad_bytes.push_back(0);
                    }
                    
                    block->offsets.push_back(current_offset);
                    current_offset += member_size;
                    dword_x4_fullness += member_size % 16;
                }
                // offset to full dword for last member
                current_offset += 16 - dword_x4_fullness;

                block->total_size = current_offset;
                block->members = std::move(_struct.members);
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

    // Print uniform block layout types
    for (auto const& [type, block] : uniform_blocks)
    {   
        wr_line(wr, "#pragma pack(1)");
        wr_format_line(wr, "struct %s", type.c_str());
        wr_start_struct(wr);
        // Although the padding is inserted automatically, it is arch dependent
        // Which is why we'd better add it manually.
        // UPDATE: pack(16) is also an option, but I'm not sure how it plays out with the opengl format.
        uint32_t pad_count = 0;
        for (int i = 0; i < block.members.size(); i++)
        {
            if (block.pad_bytes[i] > 0)
            {
                wr_format_line(wr, "char _padding_%d[%d];", pad_count, block.pad_bytes[i]);
                pad_count++;
            }
            const auto& member = block.members[i];
            wr_format_line(wr, "%s %s;", member.type, member.name);
        }
        wr_end_struct(wr);

        wr_format_line(wr, "struct %s_Block", type.c_str());
        wr_start_struct(wr);
        
        // Buffer id
        wr_line(wr, "GLuint id;");
        wr_line(wr, "GLuint binding_point;");
       
        // Create method
        wr_line(wr, "inline void create()");
        wr_start_block(wr);
        wr_line(wr, "glGenBuffers(1, &id);");
        wr_line(wr, "glBindBuffer(GL_UNIFORM_BUFFER, id);");
        wr_format_line(wr, "glBufferData(GL_UNIFORM_BUFFER, %d, NULL, GL_STATIC_DRAW);", block.total_size);
        wr_end_block(wr);

        // Bind method
        wr_line(wr, "inline void bind()");
        wr_start_block(wr);
        wr_line(wr, "glBindBuffer(GL_UNIFORM_BUFFER, id);");
        wr_end_block(wr);

        // Set-all method
        wr_format_line(wr, "inline void data(%s* data)", type.c_str()); 
        wr_start_block(wr);
        wr_format_line(wr, "glBufferData(GL_UNIFORM_BUFFER, %d, data, GL_STATIC_DRAW);", block.total_size);
        wr_end_block(wr);

        // Member offsets
        for (int i = 0; i < block.offsets.size(); i++)
        {
            const auto& member = block.members[i];
            uint32_t offset = block.offsets[i];
            wr_format_line(wr, "const GLuint %s_offset = %d;", member.name, offset);
        }

        // Setting data
        for (int i = 0; i < block.offsets.size(); i++)
        {
            const auto& member = block.members[i];
            uint32_t offset = block.offsets[i];

            wr_format_line(wr, "inline void %s(%s* %s)", member.name, member.type, member.name);
            wr_start_block(wr);
            wr_format_line(wr, "glBufferSubData(GL_UNIFORM_BUFFER, %s_offset, %d, %s);", 
                member.name, block.total_size, member.name);
            wr_end_block(wr);
        }

        wr_end_struct(wr);
    } 

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

    // Uniform blocks indices
    for (auto const& [type, _] : uniform_blocks)
    {
        wr_format_line(wr, "GLint %s_block_index;", type.c_str());  
    }

    // Uniform setters
    for (const auto& u : uniforms)
    {
        wr_format_line(wr, "inline void %s(%s %s)", u.name, u.type, u.name);
        wr_start_block(wr);
        write_uniform(wr, u);
        wr_end_block(wr);
    }

    // Uniform block setters.
    for (auto const& [type, _] : uniform_blocks)
    {
        wr_format_line(wr, "inline void %s_block(%s_Block %s_block)", type.c_str(), type.c_str(), type.c_str());
        wr_start_block(wr);
        wr_format_line(wr, "glUniformBlockBinding(id, %s_block_index, %s_block.binding_point);", type.c_str(), type.c_str());
        wr_end_block(wr);
    }

    // Initializing locations
    wr_line(wr, "inline void query_locations()");
    wr_start_block(wr);

    for (const auto& u : uniforms)
    {
        write_location(wr, u);
    }

    // Getting the indices for uniform blocks.
    for (auto const& [type, _] : uniform_blocks)
    {
        wr_format_line(wr, "%s_block_index = glGetUniformBlockIndex(id, \"%s\");", type.c_str(), type.c_str());
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