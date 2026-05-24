@echo off
g++ -std=c++17 -O2 -Isrc -o CppDepScan.exe src/main.cpp
COPY /Y "CppDepScan.exe" "sample/"
