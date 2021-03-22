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
    uint32_t base_alignment;
};

struct Struct
{
    const char* name;
    std::vector<Uniform> members;
};

struct Uniform_Block
{
    std::vector<uint32_t> offsets;
    std::vector<uint32_t> pad_bytes;
    uint32_t total_size;
    std::vector<Uniform> members;
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
    { "glm::float32", { write_float32, 4,         4 } },
    { "glm::vec4",    { write_vec4,    4 * 4,     16 } },
    { "glm::vec3",    { write_vec3,    3 * 4,     16 } },
    { "glm::vec2",    { write_vec2,    2 * 4,     8 } },
    { "glm::mat4",    { write_mat4,    4 * 4 * 4, 16 } }  
};

std::map<std::string, Uniform_Block> uniform_blocks;

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

inline void write_header(Writer* writer)
{
    wr_puts(writer,
        "#pragma once\n" \
        "// Warning: This file has been autogenerated by the tool!\n"\
        "#include <glm/glm.hpp>\n" \
        "#include <glad/glad.h>\n"
    );
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

void write_struct_declaration(Writer* wr, const std::string& type, const std::vector<Uniform>& uniforms)
{
    wr_format_line(wr, "struct %s", type.c_str());
    wr_start_struct(wr);
    for (auto const& u : uniforms)
    {
        wr_format_line(wr, "%s %s;", u.type, u.name);
    }
    wr_end_struct(wr);
}

void write_custom_type_declarations(Writer* wr)
{
    // print custom types
    for (const auto& [type, uniforms] : custom_types)
    {
        write_struct_declaration(wr, type, uniforms);
    }
}

void write_uniform_buffer_declaration(Writer* wr, const std::string& type, const Uniform_Block& block)
{
    // wr_line(wr, "#pragma push");
    // wr_line(wr, "#pragma pack(1)");
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
            wr_format_line(wr, "char _padding_%u[%u];", pad_count, block.pad_bytes[i]);
            pad_count++;
        }
        const auto& member = block.members[i];
        wr_format_line(wr, "%s %s;", member.type, member.name);
    }
    wr_end_struct(wr);
    // wr_line(wr, "#pragma pop");

    wr_format_line(wr, "struct %s_Block", type.c_str());
    wr_start_struct(wr);
    
    // Buffer id
    wr_line(wr, "GLuint id;");
    wr_line(wr, "GLuint binding_point;");
    
    // Create method
    wr_line(wr, "inline void create(GLuint binding_point)");
    wr_start_block(wr);
    wr_line(wr, "glGenBuffers(1, &id);");
    wr_line(wr, "glBindBuffer(GL_UNIFORM_BUFFER, id);");
    wr_format_line(wr, "glBufferData(GL_UNIFORM_BUFFER, %u, NULL, GL_STATIC_DRAW);", block.total_size);
    wr_line(wr, "glBindBuffer(GL_UNIFORM_BUFFER, 0);");
    wr_line(wr, "this->binding_point = binding_point;");
    wr_line(wr, "glBindBufferBase(GL_UNIFORM_BUFFER, binding_point, id);");
    wr_end_block(wr);

    // Bind method
    wr_line(wr, "inline void bind()");
    wr_start_block(wr);
    wr_line(wr, "glBindBuffer(GL_UNIFORM_BUFFER, id);");
    wr_end_block(wr);

    // Set-all method
    wr_format_line(wr, "inline void data(%s* data)", type.c_str()); 
    wr_start_block(wr);
    wr_format_line(wr, "glBufferData(GL_UNIFORM_BUFFER, %u, data, GL_STATIC_DRAW);", block.total_size);
    wr_end_block(wr);

    // Member offsets
    for (int i = 0; i < block.offsets.size(); i++)
    {
        const auto& member = block.members[i];
        uint32_t offset = block.offsets[i];
        wr_format_line(wr, "const GLuint %s_offset = %u;", member.name, offset);
    }

    // Setting data
    for (int i = 0; i < block.offsets.size(); i++)
    {
        const auto& member = block.members[i];
        uint32_t offset = block.offsets[i];

        wr_format_line(wr, "inline void %s(%s %s)", member.name, member.type, member.name);
        wr_start_block(wr);
        wr_format_line(wr, "glBufferSubData(GL_UNIFORM_BUFFER, %s_offset, %u, glm::value_ptr(%s));", 
            member.name, uniform_type_map[{ member.type }].size_in_bytes, member.name);
        wr_end_block(wr);
    }

    wr_end_struct(wr);
}


void write_uniform_buffer_declarations(Writer* wr)
{
    // Print uniform block layout types
    for (auto const& [type, block] : uniform_blocks)
    {   
        write_uniform_buffer_declaration(wr, type, block);
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
    std::vector<const char*> input_files;
    const char* output_file;
    const char* output_struct_name;
};

struct Options
{
    int spaces_per_tab;
    const char* uniform_buffer_file;
    const char* custom_types_file;
    std::vector<Iteration_Option> iteration_options;
};

void run_iteration(Options* options, Iteration_Option* iteration_option)
{
    std::map<std::string, Uniform> uniforms;

    for (auto input_file : iteration_option->input_files)
    {
        Parse_Info parse_info { input_file, 0 };
        auto file = fopen(parse_info.file, "r");
        char buffer[1024];
        while (fgets(buffer, 1024, file) != NULL)
        {
            parse_info.line++;

            // line starts with "uniform"
            if (strstr(buffer, "uniform") == buffer)
            {
                auto uniform = parse_as_declaration(buffer + sizeof("uniform"), parse_info);
                uniforms[{ uniform.name }] = uniform;
            }
            // line starts with "struct" custom struct definition
            else if (strstr(buffer, "struct") == buffer)
            {
                auto _struct = parse_as_struct(buffer, file, buffer + sizeof("struct"), parse_info);
                // TODO: Check if the members are the same. 
                // If not, notify the user that different structs with same name are not allowed.
                custom_types[{ _struct.name }] = std::move(_struct.members);
            }
            // Uniform block layout
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
                for (const auto& member : _struct.members)
                {
                    const auto& member_type_info = uniform_type_map[{ member.type }];
                    auto member_size = member_type_info.size_in_bytes;
                    auto member_alignment = member_type_info.base_alignment;
                    auto current_alignment = current_offset % member_alignment;

                    // if the member is not properly aligned, do so
                    // E.g. the alignment of a float is N, so it will always fit
                    // The alignment of a vec2 is 2N, which means that if a vec2 follows a float,
                    // the float would be in the first 4 bytes, the next 4 bytes will be skipped 
                    // and then would go the vec2.
                    if (current_alignment != 0)
                    {
                        auto skipped_bytes = member_alignment - current_alignment;
                        block->pad_bytes.push_back(skipped_bytes);
                        current_offset += skipped_bytes;
                    }
                    else
                    {
                        block->pad_bytes.push_back(0);
                    }
                    
                    block->offsets.push_back(current_offset);
                    current_offset += member_size;
                }

                block->total_size = current_offset;
                block->members = std::move(_struct.members);
            }
            
        }
        fclose(file);
    }

    Writer writer;
    writer.stream = fopen(iteration_option->output_file, "w+");
    writer.current_indentation_level = 0;
    writer.spaces_per_tab = options->spaces_per_tab;
    Writer *wr = &writer;
    
    write_header(wr);
    wr_format_line(wr, "#include \"%s\"", options->custom_types_file);
    wr_format_line(wr, "#include \"%s\"", options->uniform_buffer_file);

    wr_format_line(wr, "struct %s_Program", iteration_option->output_struct_name);
    wr_start_struct(wr);
    wr_line(wr, "GLuint id;");
    wr_line(wr, "inline void use()");
    wr_start_block(wr);
    wr_line(wr, "glUseProgram(id);");
    wr_end_block(wr);
        
    // Location declarations
    for (const auto& [_, u] : uniforms)
    {
        write_location_declaration(wr, u);
    }

    // Uniform blocks indices
    for (auto const& [type, _] : uniform_blocks)
    {
        wr_format_line(wr, "GLint %s_block_index;", type.c_str());  
    }

    // Uniform setters
    for (const auto& [_, u] : uniforms)
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

    for (const auto& [_, u] : uniforms)
    {
        write_location(wr, u);
    }

    // Getting the indices for uniform blocks.
    for (const auto & [type, _] : uniform_blocks)
    {
        wr_format_line(wr, "%s_block_index = glGetUniformBlockIndex(id, \"%s\");", type.c_str(), type.c_str());
    }
    
    wr_end_block(wr);

    // Setting all uniforms. The function header.
    wr_print_indent(wr);
    wr_puts(wr, "inline void uniforms(");

    {
        int i = 0;
        int num_uniforms = uniforms.size();
        for (const auto& [_, u] : uniforms)
        {
            wr_format(wr, "%s %s_v", u.type, u.name);
            i++;
            if (i < num_uniforms)
            {
                wr_puts(wr, ", ");
            }
        }
    }

    wr_puts(wr, ")\n");
    wr_start_block(wr);

    // Calling the appropriate uniform setters.
    for (const auto& [_, u] : uniforms)
    {
        wr_format_line(wr, "%s(%s_v);", u.name, u.name);
    }

    wr_end_block(wr);
    wr_end_struct(wr);

    fclose(writer.stream);
}

void run(Options* options)
{
    for (Iteration_Option& iteration_option : options->iteration_options)
    {
        run_iteration(options, &iteration_option);
    }
    Writer writer;
    writer.current_indentation_level = 0;
    writer.spaces_per_tab = options->spaces_per_tab;

    writer.stream = fopen(options->uniform_buffer_file, "w+");
    write_header(&writer);
    wr_line(&writer, "#include <glm/gtc/type_ptr.hpp>");
    write_uniform_buffer_declarations(&writer);

    writer.stream = fopen(options->custom_types_file, "w+");
    write_header(&writer);
    write_custom_type_declarations(&writer);
}

int main(int argc, char** argv)
{
    if (argc < 4)
    {
        // -spaces_per_tab=4
        // -types_output=required/null
        // -uniform_buffer_output=required/null
        // [OUTPUT;[INPUT;]]
        fputs("No output-input group provided. Usage: shd <custom_types_output_file> <uniform_buffer_output_file> <ouput_file>;[<input_file>+]\nSeparate the input files by ;", stderr);
        exit(-1);
    }

    Options options;

    for (int i = 3; i < argc; i++)
    {
        char* output_file = strtok(argv[i], ";");

        std::vector<const char*> input_files;
        char* input_file = strtok(NULL, ";");
        if (input_file == NULL)
        {
            fprintf(stderr, "No input file provided for the output file %s", output_file);
            exit(-1);
        }
        while(input_file != NULL)
        {
            input_files.push_back(input_file);
            input_file = strtok(NULL, ";");
        };

        Iteration_Option option;
        option.input_files = std::move(input_files);
        option.output_file = output_file;

        auto output_struct_name = sb_create(64);
        const char* last_slash = strrchr(output_file, '/'); 
        const char* last_back_slash = strrchr(output_file, '\\');
        const char* actual_file_name = max({ last_slash + 1, last_back_slash + 1, output_file });

        sb_cat_until(output_struct_name, actual_file_name, '.');
        if (output_struct_name.data[0] >= 'a' && output_struct_name.data[0] <= 'z')
        {
            output_struct_name.data[0] += 'A' - 'a';
        }

        option.output_struct_name = sb_build(output_struct_name);
        options.iteration_options.push_back(option);
    }

    options.spaces_per_tab = 4;
    options.custom_types_file = argv[1];
    options.uniform_buffer_file = argv[2];

    run(&options);
}