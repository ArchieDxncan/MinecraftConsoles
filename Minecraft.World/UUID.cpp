#include "stdafx.h"
#include "UUID.h"
#include "Random.h"
#include <cstring>
static void sha1(const uint8_t* data, size_t len, uint8_t out[20])
{
	uint32_t h[5] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
	uint64_t bitLen = len * 8;
	size_t fullBlocks = len / 64;
	for (size_t blk = 0; blk < fullBlocks; blk++) {
		const uint8_t* p = data + blk * 64;
		uint32_t w[80];
		for (int i = 0; i < 16; i++)
			w[i] = (p[i * 4] << 24) | (p[i * 4 + 1] << 16) | (p[i * 4 + 2] << 8) | p[i * 4 + 3];
		for (int i = 16; i < 80; i++) {
			uint32_t x = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
			w[i] = (x << 1) | (x >> 31);
		}
		uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
		for (int i = 0; i < 80; i++) {
			uint32_t f, k;
			if (i < 20)      { f = (b & c) | (~b & d);           k = 0x5A827999; }
			else if (i < 40) { f = b ^ c ^ d;                    k = 0x6ED9EBA1; }
			else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
			else              { f = b ^ c ^ d;                    k = 0xCA62C1D6; }
			uint32_t tmp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
			e = d; d = c; c = (b << 30) | (b >> 2); b = a; a = tmp;
		}
		h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
	}
	uint8_t tail[128] = {};
	size_t rem = len - fullBlocks * 64;
	if (rem) memcpy(tail, data + fullBlocks * 64, rem);
	tail[rem] = 0x80;
	size_t tailLen = (rem < 56) ? 64 : 128;
	for (int i = 0; i < 8; i++)
		tail[tailLen - 1 - i] = (uint8_t)(bitLen >> (i * 8));

	for (size_t off = 0; off < tailLen; off += 64) {
		uint32_t w[80];
		for (int i = 0; i < 16; i++)
			w[i] = (tail[off + i * 4] << 24) | (tail[off + i * 4 + 1] << 16) |
			       (tail[off + i * 4 + 2] << 8) | tail[off + i * 4 + 3];
		for (int i = 16; i < 80; i++) {
			uint32_t x = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
			w[i] = (x << 1) | (x >> 31);
		}
		uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
		for (int i = 0; i < 80; i++) {
			uint32_t f, k;
			if (i < 20)      { f = (b & c) | (~b & d);           k = 0x5A827999; }
			else if (i < 40) { f = b ^ c ^ d;                    k = 0x6ED9EBA1; }
			else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
			else              { f = b ^ c ^ d;                    k = 0xCA62C1D6; }
			uint32_t tmp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
			e = d; d = c; c = (b << 30) | (b >> 2); b = a; a = tmp;
		}
		h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
	}

	for (int i = 0; i < 5; i++) {
		out[i * 4]     = (uint8_t)(h[i] >> 24);
		out[i * 4 + 1] = (uint8_t)(h[i] >> 16);
		out[i * 4 + 2] = (uint8_t)(h[i] >> 8);
		out[i * 4 + 3] = (uint8_t)(h[i]);
	}
}
static constexpr char HEX[] = "0123456789abcdef";
static constexpr uint8_t hexVal(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return 10 + c - 'a';
	if (c >= 'A' && c <= 'F') return 10 + c - 'A';
	return 0;
}

void GameUUID::toBytes(uint8_t out[16]) const
{
	for (int i = 7; i >= 0; i--) out[7 - i] = (uint8_t)(msb >> (i * 8));
	for (int i = 7; i >= 0; i--) out[15 - i] = (uint8_t)(lsb >> (i * 8));
}

GameUUID GameUUID::fromBytes(const uint8_t b[16])
{
	GameUUID u;
	for (int i = 0; i < 8; i++) u.msb = (u.msb << 8) | b[i];
	for (int i = 0; i < 8; i++) u.lsb = (u.lsb << 8) | b[8 + i];
	return u;
}
std::string GameUUID::toString() const
{
	uint8_t b[16];
	toBytes(b);
	char buf[37];
	int p = 0;
	for (int i = 0; i < 16; i++) {
		if (i == 4 || i == 6 || i == 8 || i == 10) buf[p++] = '-';
		buf[p++] = HEX[b[i] >> 4];
		buf[p++] = HEX[b[i] & 0xf];
	}
	buf[p] = '\0';
	return buf;
}

std::wstring GameUUID::toWString() const
{
	std::string s = toString();
	return { s.begin(), s.end() };
}

GameUUID GameUUID::fromString(const std::string& s)
{
	uint8_t b[16] = {};
	int bi = 0;
	for (size_t i = 0; i < s.size() && bi < 16; i++) {
		if (s[i] == '-') continue;
		if (i + 1 >= s.size()) break;
		b[bi++] = (hexVal(s[i]) << 4) | hexVal(s[i + 1]);
		i++;
	}
	return fromBytes(b);
}

GameUUID GameUUID::fromWString(const std::wstring& s)
{
	return fromString({ s.begin(), s.end() });
}

GameUUID GameUUID::v4(uint64_t high, uint64_t low)
{
	return { (high & ~0xF000ULL) | 0x4000ULL, (low & ~0xC000000000000000ULL) | 0x8000000000000000ULL };
}

GameUUID GameUUID::v5(const GameUUID& ns, const std::string& name)
{
	uint8_t input[256];
	ns.toBytes(input);
	size_t total = 16 + name.size();
	// names over 240 chars would be insane but just in case
	if (name.size() <= sizeof(input) - 16)
		memcpy(input + 16, name.data(), name.size());

	uint8_t hash[20];
	sha1(input, total, hash);
	GameUUID u = fromBytes(hash);
	u.msb = (u.msb & ~0xF000ULL) | 0x5000ULL;
	u.lsb = (u.lsb & ~0xC000000000000000ULL) | 0x8000000000000000ULL;
	return u;
}

GameUUID GameUUID::fromXuid(uint64_t xuid)
{
	return v5(MCCONSOLES_NAMESPACE_UUID, std::to_string(xuid));
}

GameUUID GameUUID::random()
{
	Random r;
	return v4(r.nextLong(), r.nextLong());
}
