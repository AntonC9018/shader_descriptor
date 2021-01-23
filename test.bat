@echo off

call build
cd example
..\..\build\shader_descriptor.exe example.h example.fs
cd ..