#ifndef _RESPAK_HPP
#define _RESPAK_HPP

#include <rua/bytes.hpp>
#include <rua/file.hpp>
#include <rua/log.hpp>
#include <rua/macros.hpp>
#include <rua/process.hpp>
#include <rua/string.hpp>
#include <rua/types/def.hpp>

#if !defined(RESPAK_USE_ZLIB) && RUA_HAS_INC(<zlib.h>)
#define RESPAK_USE_ZLIB
#endif

#ifdef RESPAK_USE_ZLIB
#include <zlib.h>
#endif

#include <cstdlib>
#include <map>
#include <string>

namespace respak {

inline std::map<std::string, rua::bytes> &_map() {
	static std::map<std::string, rua::bytes> map;
	return map;
}

RUA_INLINE_CONST auto _meta_sz = static_cast<ptrdiff_t>(2 + 2 * sizeof(uint64_t));

inline rua::string_view pack(rua::file &arc) {
	auto pak_sz = arc.size();
	if (pak_sz >= _meta_sz) {
		arc.seek_from_end(-_meta_sz);
		rua::uchar mark[2];
		if (arc.read_full(mark) <= 0) {
			return "respak: reading mark failed!";
		}
		if (mark[0] == 'R' && mark[1] == 'P') {
			uint64_t body_sto_sz;
			if (arc.read_full(rua::as_bytes(body_sto_sz)) <= 0) {
				return "respak: reading body storage size failed!";
			}
			arc.seek_from_end(-static_cast<ptrdiff_t>(body_sto_sz + 2 + sizeof(uint64_t)));
			SetEndOfFile(arc.native_handle());
		} else {
			arc.seek_from_end();
		}
	} else {
		arc.seek_from_end();
	}

	auto &map = _map();
	uint64_t body_sz = 0;
	for (auto &kv : map) {
		body_sz += sizeof(uint16_t) + kv.first.length() + sizeof(uint64_t) + kv.second.size();
	}
	if (!body_sz) {
		arc.write_all(rua::as_bytes("RP"));
		arc.write_all(rua::as_bytes(static_cast<uint64_t>(0)));
		arc.write_all(rua::as_bytes(static_cast<uint64_t>(0)));
		return "";
	}

	rua::bytes body_buf;
	body_buf.reserve(static_cast<size_t>(body_sz));
	for (auto &kv : map) {
		body_buf += rua::as_bytes(static_cast<uint16_t>(kv.first.length()));
		body_buf += rua::as_bytes(kv.first);
		body_buf += rua::as_bytes(static_cast<uint64_t>(kv.second.size()));
		body_buf += rua::as_bytes(kv.second);
	}

#ifdef RESPAK_USE_ZLIB
	rua::bytes body_sto_buf(static_cast<size_t>(body_sz) + static_cast<size_t>(body_sz) / 10);
	uLongf body_sto_sz = body_sto_buf.size();
	auto r =
		compress(body_sto_buf.data(), &body_sto_sz, body_buf.data(), static_cast<uLong>(body_sz));
	if (r != Z_OK) {
		return "respak: compressing body error";
	}
	body_sto_buf.resize(body_sto_sz);
	arc.write_all(body_sto_buf);

	arc.write_all(rua::as_bytes("RP"));
	arc.write_all(rua::as_bytes(static_cast<uint64_t>(body_sto_sz)));
	arc.write_all(rua::as_bytes(static_cast<uint64_t>(body_sz)));
#else
	arc.write_all(body_buf);
	arc.write_all(rua::as_bytes("RP"));
	arc.write_all(rua::as_bytes(static_cast<uint64_t>(body_sz)));
	arc.write_all(rua::as_bytes(static_cast<uint64_t>(body_sz)));
#endif
	return "";
}

inline rua::string_view pack(const rua::file_path &arc_pth) {
	auto arc = rua::touch_file(arc_pth);
	if (!arc) {
		return "respak: creating archive failed!";
	}
	return pack(arc);
}

inline rua::string_view _load_from_arc(const rua::file_path &arc_pth) {
	auto arc = rua::view_file(arc_pth);
	if (!arc) {
		return "respak: archive is not found!";
	}

	if (arc.size() < _meta_sz) {
		return "respak: archive size is unavailabled!";
	}

	arc.seek_from_end(-_meta_sz);
	rua::uchar mark[2];
	if (arc.read_full(mark) <= 0) {
		return "respak: reading mark failed!";
	}
	if (mark[0] != 'R' || mark[1] != 'P') {
		return "respak: archive is unavailabled!";
	}

	uint64_t body_sto_sz, body_sz;
	if (arc.read_full(rua::as_bytes(body_sto_sz)) <= 0) {
		return "respak: reading body storage size failed!";
	}
	if (arc.read_full(rua::as_bytes(body_sz)) <= 0) {
		return "respak: reading body size failed!";
	}
	arc.seek_from_end(-static_cast<ptrdiff_t>(body_sto_sz + _meta_sz));

	rua::bytes body_sto_buf(static_cast<size_t>(body_sto_sz));
	if (arc.read_full(body_sto_buf) <= 0) {
		return "respak: reading body failed!";
	}

#ifdef RESPAK_USE_ZLIB
	rua::bytes body_buf(static_cast<size_t>(body_sz));
	uLongf body_sz_z = body_buf.size();
	auto r = uncompress(body_buf.data(), &body_sz_z, body_sto_buf.data(), body_sto_buf.size());
	if (r != Z_OK) {
		return "respak: uncompressing body error";
	}
	if (body_sz_z != body_sz) {
		return "respak: body size error!";
	}
#else
	auto &body_buf = body_sto_buf;
#endif

	auto &map = _map();
	for (uint64_t i = 0; i < body_sz;) {
		auto pth_sz = body_buf.get<uint16_t>(static_cast<ptrdiff_t>(i));
		i += sizeof(uint16_t);
		auto pth = std::string(rua::as_string(
			body_buf(static_cast<ptrdiff_t>(i), static_cast<ptrdiff_t>(i + pth_sz))));
		i += pth_sz;

		auto data_sz = body_buf.get<uint64_t>(static_cast<ptrdiff_t>(i));
		i += sizeof(uint64_t);
		map[pth] = body_buf(static_cast<ptrdiff_t>(i), static_cast<ptrdiff_t>(i + data_sz));
		i += data_sz;
	}
	return "";
}

inline void _load_from_dir(const rua::file_path &dir,
						   std::initializer_list<rua::string_view> ignores = {}) {
	auto &map = _map();
	for (auto &ety : rua::view_dir(dir, 0)) {
		if (ety.is_dir()) {
			continue;
		}

		auto pth = rua::path(ety.relative_path()).str();

		auto is_ignored = false;
		for (auto &ignore : ignores) {
			if (rua::ends_with(pth, rua::path(ignore).str())) {
				is_ignored = true;
				break;
			}
		}
		if (is_ignored) {
			continue;
		}

		auto f = rua::view_file(ety.path());
		if (!f) {
			continue;
		}
		map[pth] = f.read_all();
	}
}

inline rua::string_view load(const rua::file_path &pth,
							 std::initializer_list<rua::string_view> ignores = {}) {
	if (pth.is_dir()) {
		_load_from_dir(pth, ignores);
		return "";
	}
	return _load_from_arc(pth);
}

inline rua::string_view load() {
	return _load_from_arc(rua::this_process().path());
}

inline rua::bytes_ref access(rua::string_view path) {
	auto &map = _map();
	auto it = map.find(std::string(path));
	if (it == map.end()) {
		// rua::err_log("respak:", path, "is not found!");
		return nullptr;
	}
	return it->second;
}

} // namespace respak

#endif
