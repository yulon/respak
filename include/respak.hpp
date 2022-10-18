#ifndef _RESPAK_HPP
#define _RESPAK_HPP

#include <rua/binary.hpp>
#include <rua/error.hpp>
#include <rua/file.hpp>
#include <rua/log.hpp>
#include <rua/process.hpp>
#include <rua/string.hpp>
#include <rua/util.hpp>

#if !defined(RESPAK_USE_ZLIB) && RUA_HAS_INC(<zlib.h>)
#define RESPAK_USE_ZLIB
#endif

#ifdef RESPAK_USE_ZLIB
#include <zlib.h>
#endif

#include <cstdlib>
#include <map>
#include <string>

class respak {
  public:
	respak() = default;

	respak(const rua::file_path &pth, std::initializer_list<rua::string_view> ignores = {}) {
		load(pth, ignores);
	}

	rua::error_i pack(rua::file &arc) {
		auto pak_sz = arc.size();
		if (pak_sz >= _meta_sz) {
			arc.seek_from_end(-_meta_sz);
			rua::uchar mark[2];
			if (arc.read_full(mark) <= 0) {
				return rua::string_error("respak: reading mark failed!");
			}
			if (mark[0] == 'R' && mark[1] == 'P') {
				uint64_t body_sto_sz;
				if (arc.read_full(rua::as_bytes(body_sto_sz)) <= 0) {
					return rua::string_error("respak: reading body storage size failed!");
				}
				arc.seek_from_end(-static_cast<ptrdiff_t>(body_sto_sz + 2 + sizeof(uint64_t)));
				SetEndOfFile(arc.native_handle());
			} else {
				arc.seek_from_end();
			}
		} else {
			arc.seek_from_end();
		}

		uint64_t body_sz = 0;
		for (auto &kv : _res) {
			body_sz += sizeof(uint16_t) + kv.first.length() + sizeof(uint64_t) + kv.second.size();
		}
		if (!body_sz) {
			arc.write_all(rua::as_bytes("RP"));
			arc.write_all(rua::as_bytes(static_cast<uint64_t>(0)));
			arc.write_all(rua::as_bytes(static_cast<uint64_t>(0)));
			return nullptr;
		}

		rua::bytes body_buf;
		body_buf.reserve(static_cast<size_t>(body_sz));
		for (auto &kv : _res) {
			body_buf += rua::as_bytes(static_cast<uint16_t>(kv.first.length()));
			body_buf += rua::as_bytes(kv.first);
			body_buf += rua::as_bytes(static_cast<uint64_t>(kv.second.size()));
			body_buf += rua::as_bytes(kv.second);
		}

#ifdef RESPAK_USE_ZLIB
		rua::bytes body_sto_buf(static_cast<size_t>(body_sz) + static_cast<size_t>(body_sz) / 10);
		auto body_sto_sz = static_cast<uLongf>(body_sto_buf.size());
		auto r = compress(
			body_sto_buf.data(), &body_sto_sz, body_buf.data(), static_cast<uLong>(body_sz));
		if (r != Z_OK) {
			return rua::string_error("respak: compressing body error");
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
		return nullptr;
	}

	rua::error_i pack(const rua::file_path &arc_pth) {
		auto arc = rua::touch_file(arc_pth);
		if (!arc) {
			return rua::string_error("respak: creating archive failed!");
		}
		return pack(arc);
	}

	rua::error_i load(const rua::file_path &pth,
					  std::initializer_list<rua::string_view> ignores = {}) {
		if (pth.is_dir()) {
			load_from_dir(pth, ignores);
			return nullptr;
		}
		return load_from_archive(pth);
	}

	rua::error_i load_from_archive(const rua::file_path &arc_pth) {
		auto arc = rua::view_file(arc_pth);
		if (!arc) {
			return rua::string_error("respak: archive is not found!");
		}

		if (arc.size() < _meta_sz) {
			return rua::string_error("respak: archive size is unavailabled!");
		}

		arc.seek_from_end(-_meta_sz);
		rua::uchar mark[2];
		if (arc.read_full(mark) <= 0) {
			return rua::string_error("respak: reading mark failed!");
		}
		if (mark[0] != 'R' || mark[1] != 'P') {
			return rua::string_error("respak: archive is unavailabled!");
		}

		uint64_t body_sto_sz, body_sz;
		if (arc.read_full(rua::as_bytes(body_sto_sz)) <= 0) {
			return rua::string_error("respak: reading body storage size failed!");
		}
		if (arc.read_full(rua::as_bytes(body_sz)) <= 0) {
			return rua::string_error("respak: reading body size failed!");
		}
		arc.seek_from_end(-static_cast<ptrdiff_t>(body_sto_sz + _meta_sz));

		rua::bytes body_sto_buf(static_cast<size_t>(body_sto_sz));
		if (arc.read_full(body_sto_buf) <= 0) {
			return rua::string_error("respak: reading body failed!");
		}

#ifdef RESPAK_USE_ZLIB
		rua::bytes body_buf(static_cast<size_t>(body_sz));
		auto body_sz_z = static_cast<uLongf>(body_buf.size());
		auto r = uncompress(body_buf.data(),
							&body_sz_z,
							body_sto_buf.data(),
							static_cast<uLong>(body_sto_buf.size()));
		if (r != Z_OK) {
			return rua::string_error("respak: uncompressing body error");
		}
		if (body_sz_z != body_sz) {
			return rua::string_error("respak: body size error!");
		}
#else
		auto &body_buf = body_sto_buf;
#endif
		for (uint64_t i = 0; i < body_sz;) {
			auto pth_sz = body_buf.get<uint16_t>(static_cast<ptrdiff_t>(i));
			i += sizeof(uint16_t);
			auto pth = std::string(rua::as_string(
				body_buf(static_cast<ptrdiff_t>(i), static_cast<ptrdiff_t>(i + pth_sz))));
			i += pth_sz;

			auto data_sz = body_buf.get<uint64_t>(static_cast<ptrdiff_t>(i));
			i += sizeof(uint64_t);
			_res[pth] = body_buf(static_cast<ptrdiff_t>(i), static_cast<ptrdiff_t>(i + data_sz));
			i += data_sz;
		}
		return nullptr;
	}

	void load_from_dir(const rua::file_path &dir,
					   std::initializer_list<rua::string_view> ignores = {}) {

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
			_res[pth] = f.read_all();
		}
	}

	rua::bytes_ref access(rua::string_view path) {
		auto it = _res.find(std::string(path));
		if (it == _res.end()) {
			return nullptr;
		}
		return it->second;
	}

	operator bool() const {
		return _res.size();
	}

	void reset() {
		_res.clear();
	}

  private:
	std::map<std::string, rua::bytes> _res;

	static constexpr auto _meta_sz = static_cast<ptrdiff_t>(2 + 2 * sizeof(uint64_t));
};

#endif
