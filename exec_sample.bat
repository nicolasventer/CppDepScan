CppDepScan sample -o result/basic.d2
CppDepScan sample -e sample/build -e sample/external -o result/exclude_build_external.d2
CppDepScan sample -e sample/build -e sample/external -e !sample/external/keep -o result/exclude_build_external_keep.d2

CppDepScan sample/src -i sample/include -o result/import.d2
CppDepScan 'sample/src/*.cpp' -i sample/include -o result/import_glob_scan.d2
CppDepScan 'sample/src/**/*.hpp' -i sample/include -o result/import_glob_recursive_scan.d2

CppDepScan sample/src -i sample/include -a sample/src/main.cpp sample/src/app -a sample/src/main.cpp sample/include -a sample/src/legacy.c sample/src/app -o result/allowed.d2
CppDepScan sample/src -a 'sample/src/*' sample/src/app -i sample/include -o result/glob_allowed.d2

CppDepScan sample -e sample/build -e sample/src/nonexistent.h -i sample/include -g sample/include -g sample/external -g sample/src -o result/group.d2
CppDepScan sample -e sample/build -e sample/src/nonexistent.h -i sample/include -g 'sample/**/*.hpp' -o result/glob_group.d2

CppDepScan sample/src sample/external -i sample/include -g sample/src -g sample/external --std -o result/std.d2
CppDepScan sample/src -i sample/include -o result/import.json
CppDepScan sample/src -i sample/include --brother-links -o result/brother_links.d2
CppDepScan sample/src -i sample/include --group-source-header -o result/group_source_header.d2
CppDepScan sample/src -i sample/include --group-source-header --brother-links -o result/group_source_header_brother_links.d2

@REM Special cases:

@REM exclude unresolved path (bad practice)
CppDepScan sample -e sample/src/extra.hpp -o result/exclude_unresolved.d2
@REM exclude resolved path before exclude path (good practice)
CppDepScan sample -i sample/include -e sample/include/extra.hpp -o result/exclude_import.d2
@REM group ignored for forbidden, unresolved or unspecified modules
CppDepScan sample/src -g sample/src -i sample/include -a sample/src/main.cpp sample/src -a sample/src/app sample/src/app -a sample/src/lib sample/src/lib -o result/ignored_group.d2

pushd result
for %%f in (*.d2) do (
    d2 "%%f" "%%~nf.png"
)
popd
