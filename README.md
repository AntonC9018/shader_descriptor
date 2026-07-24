This simple script converts glsl shader source code into c++ struct file, which can be used in code for convenience.

Currently, only some types are supported. I will add more if I need to. Custom types are also supported.

Please note that the script is VERY primitive but satisfies my current needs.

See an example in the `example` directory. Please inspect `test.bat` for instruction for running the example.

## Building

Install [Premake 5](https://premake.github.io/) and ensure `premake5` is on
`PATH`. Generate GNU Makefiles and build with GCC:

```sh
premake5 gmake
make -C build config=release
```

The executable is written to `bin/Release/shader_descriptor`.
