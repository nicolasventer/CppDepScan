#!/usr/bin/env bash
g++ -std=c++17 -O2 -o CppDepScan src/main.cpp
cp -f CppDepScan sample/
