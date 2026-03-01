CppDepScan sample -o result/basic.d2
CppDepScan sample -e sample/build -e sample/external -o result/exclude_build_external.d2
CppDepScan sample -e sample/build -e sample/external -e !sample/external/keep -o result/exclude_build_external_keep.d2

CppDepScan sample/src -I sample/include -o result/include.d2

CppDepScan sample/src -I sample/include -A sample/src/main sample/src/app -A sample/src/main sample/include -A sample/src/legacy.c sample/src/app -o result/allowed.d2

CppDepScan sample -e sample/build -e sample/src/nonexistent.h -I sample/include -g sample/include -g sample/external -g sample/src -o result/group.d2

CppDepScan sample/src sample/external -I sample/include -g sample/src -g sample/external --std -o result/std.d2
CppDepScan sample/src -I sample/include -o result/include.json

@REM Special cases: exclude unresolved path (bad practice)
CppDepScan sample -e sample/src/extra.hpp -o result/exclude_unresolved.d2
@REM Special cases: resolved before exclude path (good practice)
CppDepScan sample -I sample/include -e sample/include/extra.hpp -o result/exclude_include.d2
@REM Special cases: group ignored for forbidden or unresolved includes
CppDepScan sample/src -g sample/src -I sample/include -A sample/src/main.cpp sample/src -A sample/src/app sample/src/app -A sample/src/lib sample/src/lib -o result/ignored_group.d2

pushd result
for %%f in (*.d2) do (
    d2 "%%f" "%%~nf.png"
)
popd
