#include <respak.hpp>

#include <rua/check.hpp>
#include <rua/string.hpp>

int main(int argc, char const *argv[]) {
	RUA_CHECK(argc, ==, 3);
	RUA_CHECK(respak::load(rua::l2u(argv[2])), ==, "");
	RUA_CHECK(respak::pack(rua::l2u(argv[1])), ==, "");
	return 0;
}
