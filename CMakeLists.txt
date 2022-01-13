cmake_minimum_required(VERSION 3.0)

project(respack)

include("cmake/DepPull.cmake")

DepPull(
	rua
	GIT_REPO "https://github.com/yulon/rua.git"
)

add_library(respak INTERFACE)
set(respak_INCLUDE_DIR "include")
target_include_directories(respak INTERFACE ${respak_INCLUDE_DIR})
target_link_libraries(respak INTERFACE rua)

add_executable(respack-cmd "src/cmd.cpp")
set_target_properties(respack-cmd PROPERTIES
	CXX_STANDARD 17
	RUNTIME_OUTPUT_NAME "respack"
)
target_link_libraries(respack-cmd rua respak)
