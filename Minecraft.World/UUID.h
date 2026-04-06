#pragma once
#include <cstdint>
#include <string>

struct GameUUID {
	uint64_t msb = 0;
	uint64_t lsb = 0;

	constexpr bool isNil() const { return msb == 0 && lsb == 0; }
	constexpr bool operator==(const GameUUID& o) const { return msb == o.msb && lsb == o.lsb; }
	constexpr bool operator!=(const GameUUID& o) const { return !(*this == o); }
	constexpr bool operator<(const GameUUID& o) const { return msb < o.msb || (msb == o.msb && lsb < o.lsb); }

	void toBytes(uint8_t out[16]) const;
	std::string toString() const;
	std::wstring toWString() const;

	static GameUUID fromBytes(const uint8_t b[16]);
	static GameUUID fromString(const std::string& s);
	static GameUUID fromWString(const std::wstring& s);
	static GameUUID v4(uint64_t high, uint64_t low);
	static GameUUID v5(const GameUUID& ns, const std::string& name);
	static GameUUID fromXuid(uint64_t xuid);
	static GameUUID random();
};

inline constexpr GameUUID MCCONSOLES_NAMESPACE_UUID = { 0x4d696e6563726166ULL, 0x74436f6e736f6c65ULL };
