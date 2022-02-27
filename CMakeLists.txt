cmake_minimum_required(VERSION 3.0)

project(respak)

include("cmake/depu.cmake")

depu(
	rua
	GIT_REPO "https://github.com/yulon/rua.git"
)

depu(
	zlib
	PKG_URL "https://zlib.net/zlib1211.zip"
	PKG_HASH SHA256=d7510a8ee1918b7d0cad197a089c0a2cd4d6df05fee22389f67f115e738b178d
	SRC_FIX "zlib-1.2.11"
)

target_include_directories(zlibstatic PUBLIC "${DEPU_zlib_SRC}" "${DEPU_zlib_BUILD}")

add_library(respak INTERFACE)
set(respak_INCLUDE_DIR "include")
target_include_directories(respak INTERFACE ${respak_INCLUDE_DIR})
target_link_libraries(respak INTERFACE rua)

add_executable(respak-cmd "src/cmd.cpp")
set_target_properties(respak-cmd PROPERTIES
	CXX_STANDARD 17
	RUNTIME_OUTPUT_NAME "respak"
)
target_link_libraries(respak-cmd respak zlibstatic)
