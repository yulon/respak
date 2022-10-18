#include <respak.hpp>

#include <rua/check.hpp>
#include <rua/string.hpp>

int main(int argc, char const *argv[]) {
	RUA_CHECK(argc, ==, 3);
	respak res;
	RUA_SUCCESS(!res.load(rua::l2u(argv[2])));
	RUA_SUCCESS(!res.pack(rua::l2u(argv[1])));
	return 0;
}
