#!/usr/bin/env bash
g++ -std=c++17 -O2 -o CppDepScan src/CppDepScan.cpp
cp -f CppDepScan sample/
