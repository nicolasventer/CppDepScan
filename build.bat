@echo off
g++ -std=c++17 -O2 -o CppDepScan.exe src/CppDepScan.cpp
COPY /Y "CppDepScan.exe" "sample/"
