#pragma once
// ============================================================================
// NEXA :: SHA-256 (FIPS 180-4) + salted, iterated password hashing.
// Self-contained so AuthService stays testable without OpenSSL/Drogon.
// Passwords are stored as hex(SHA256^N(salt || password)), N = 20000.
// NOTE: for production deployments a memory-hard KDF (Argon2id / bcrypt)
// is recommended; this is documented in the README limitations.
// ============================================================================
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>

namespace nexa::crypto {

class Sha256 {
public:
    static std::string hex(const std::string& data) { unsigned char d[32]; digest(data, d); return toHex(d, 32); }

    static void digest(const std::string& data, unsigned char out[32]) {
        uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
        std::string msg = data;
        uint64_t bitLen = (uint64_t)data.size() * 8;
        msg += (char)0x80;
        while (msg.size() % 64 != 56) msg += (char)0x00;
        for (int i = 7; i >= 0; --i) msg += (char)((bitLen >> (i * 8)) & 0xff);
        for (std::size_t chunk = 0; chunk < msg.size(); chunk += 64) {
            uint32_t w[64];
            for (int i = 0; i < 16; ++i)
                w[i] = ((uint32_t)(unsigned char)msg[chunk + 4 * i] << 24) | ((uint32_t)(unsigned char)msg[chunk + 4 * i + 1] << 16) |
                       ((uint32_t)(unsigned char)msg[chunk + 4 * i + 2] << 8) | (uint32_t)(unsigned char)msg[chunk + 4 * i + 3];
            for (int i = 16; i < 64; ++i) {
                uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
                uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
                w[i] = w[i - 16] + s0 + w[i - 7] + s1;
            }
            uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
            for (int i = 0; i < 64; ++i) {
                uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
                uint32_t ch = (e & f) ^ (~e & g);
                uint32_t t1 = hh + S1 + ch + K[i] + w[i];
                uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
                uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
                uint32_t t2 = S0 + maj;
                hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
            }
            h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
        }
        for (int i = 0; i < 8; ++i) for (int j = 0; j < 4; ++j) out[i * 4 + j] = (unsigned char)((h[i] >> (24 - 8 * j)) & 0xff);
    }

    static std::string toHex(const unsigned char* d, std::size_t n) {
        static const char* hx = "0123456789abcdef";
        std::string s;
        for (std::size_t i = 0; i < n; ++i) { s += hx[d[i] >> 4]; s += hx[d[i] & 15]; }
        return s;
    }

private:
    static uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
    static constexpr uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
};

inline std::string randomHex(std::size_t bytes) {
    std::random_device rd;
    unsigned char buf[64];
    if (bytes > 64) bytes = 64;
    for (std::size_t i = 0; i < bytes; ++i) buf[i] = (unsigned char)(rd() & 0xff);
    return Sha256::toHex(buf, bytes);
}

inline std::string hashPassword(const std::string& password, const std::string& saltHex, int iterations = 20000) {
    std::string h = Sha256::hex(saltHex + ":" + password);
    for (int i = 1; i < iterations; ++i) h = Sha256::hex(h + saltHex);
    return h;
}

/// Constant-time comparison to avoid timing leaks.
inline bool safeEquals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i) diff |= (unsigned char)(a[i] ^ b[i]);
    return diff == 0;
}

} // namespace nexa::crypto
