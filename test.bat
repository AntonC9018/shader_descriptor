@echo off

call build
cd example
..\..\build\shader_descriptor.exe types.h buffers.h example.h;example.fs;example.vs
cd ..