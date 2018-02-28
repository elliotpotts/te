#!/usr/bin/env bash
g++ -g -Wall -Werror -std=c++17 -Iglm-0.9.8.5 -Iglfw-3.2.1/include -Iglad/include main.cpp glad/src/glad.c -lFreeImage -Lglfw-3.2.1/build/src -lglfw3 -lgdi32 -lopengl32 && ./a.exe