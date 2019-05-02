/*
MIT License

Copyright (c) 2019 Reed A. Cartwright <reed@cartwrig.ht>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#ifndef SPARKYRNG_HPP
#define SPARKYRNG_HPP

#include <array>
#include <cassert>
#include <cfloat>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#if defined(_WIN64) || defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

#if __cpluscplus >= 201103L
#include <chrono>
#include <random>
#else
#include <ctime>
#endif

namespace sparkyrng {

namespace detail {

inline uint64_t rotl(const uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

/*
This is a fixed-increment version of Java 8's SplittableRandom generator
See http://dx.doi.org/10.1145/2714064.2660195 and
http://docs.oracle.com/javase/8/docs/api/java/util/SplittableRandom.html

Based on code written in 2015 by Sebastiano Vigna (vigna@acm.org)
*/

inline uint64_t splitmix64(uint64_t *state) {
    assert(state != nullptr);
    uint64_t z = (*state += UINT64_C(0x9e3779b97f4a7c15));
    z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
    return z ^ (z >> 31);
}

/*
C++11 compatible PRNG engine implementing xoshiro256**

This is xoshiro256** 1.0, our all-purpose, rock-solid generator. It has
excellent (sub-ns) speed, a state (256 bits) that is large enough for
any parallel application, and it passes all tests we are aware of.

The state must be seeded so that it is not everywhere zero. If you have
a 64-bit seed, we suggest to seed a splitmix64 generator and use its
output to fill s.

Based on code written in 2018 by David Blackman and Sebastiano Vigna (vigna@acm.org)
*/

class Xoshiro256StarStarEngine {
   public:
    using result_type = uint64_t;
    using state_type = std::array<result_type, 4>;
    static constexpr result_type default_seed = 18914u;

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    explicit Xoshiro256StarStarEngine(result_type seed_value = default_seed) { seed(seed_value); }

    template <typename Sseq>
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    explicit Xoshiro256StarStarEngine(Sseq &ss) {
        seed(ss);
    }

    result_type operator()() { return next(); }

    void discard(uint64_t z);

    static constexpr result_type min() { return std::numeric_limits<result_type>::min(); }
    static constexpr result_type max() { return std::numeric_limits<result_type>::max(); }

    friend bool operator==(const Xoshiro256StarStarEngine &left, const Xoshiro256StarStarEngine &right);

    void seed(result_type seed_value = default_seed);

    template <typename Sseq>
    void seed(Sseq &ss);

    const state_type &state() const { return state_; }
    void set_state(const state_type &state) { state_ = state; };

   protected:
    result_type next();

   private:
    state_type state_;
};

// Advance the engine to the next state and return a pseudo-random value
inline Xoshiro256StarStarEngine::result_type Xoshiro256StarStarEngine::next() {
    const uint64_t result_starstar = detail::rotl(state_[1] * 5, 7) * 9;

    const uint64_t t = state_[1] << 17;

    state_[2] ^= state_[0];
    state_[3] ^= state_[1];
    state_[1] ^= state_[2];
    state_[0] ^= state_[3];

    state_[2] ^= t;

    state_[3] = detail::rotl(state_[3], 45);

    return result_starstar;
}

// Seed the state with a single value
inline void Xoshiro256StarStarEngine::seed(result_type s) {
    // start with well mixed bits
    state_ = {UINT64_C(0x5FAF84EE2AA04CFF), UINT64_C(0xB3A2EF3524D89987), UINT64_C(0x5A82B68EF098F79D),
              UINT64_C(0x5D7AA03298486D6E)};
    // add in random bits generated from seed
    for(auto &&a : state_) {
        a += detail::splitmix64(&s);
    }
    // check to see if state is all zeros and fix
    if(state_[0] == 0 && state_[1] == 0 && state_[2] == 0 && state_[3] == 0) {
        state_[1] = UINT64_C(0x1615CA18E55EE70C);
    }
    // burn in 256 values
    discard(256);
};

// Seed the state with multiple values
template <typename Sseq>
inline void Xoshiro256StarStarEngine::seed(Sseq &ss) {
    // start with well mixed bits
    state_ = {UINT64_C(0x5FAF84EE2AA04CFF), UINT64_C(0xB3A2EF3524D89987), UINT64_C(0x5A82B68EF098F79D),
              UINT64_C(0x5D7AA03298486D6E)};
    // add in random bits generated from seeds
    for(result_type s : ss) {
        for(auto &&a : state_) {
            a += detail::splitmix64(&s);
        }
    }
    // check to see if state is all zeros and fix
    if(state_[0] == 0 && state_[1] == 0 && state_[2] == 0 && state_[3] == 0) {
        state_[1] = UINT64_C(0x1615CA18E55EE70C);
    }
    // burn in 256 values
    discard(256);
}

// Read z values from the Engine and discard
inline void Xoshiro256StarStarEngine::discard(uint64_t z) {
    for(; z != UINT64_C(0); --z) {
        next();
    }
}

inline bool operator==(const Xoshiro256StarStarEngine &left, const Xoshiro256StarStarEngine &right) {
    return left.state_ == right.state_;
}

inline bool operator!=(const Xoshiro256StarStarEngine &left, const Xoshiro256StarStarEngine &right) {
    return !(left == right);
}

using RandomEngine = Xoshiro256StarStarEngine;

inline uint32_t random_u32(uint64_t u) { return u >> 32; }

// uniformly distributed between [0,max_value)
// Algorithm 5 from Lemire (2018) https://arxiv.org/pdf/1805.10941.pdf
template <typename callback>
uint64_t random_u64_limited(uint64_t max_value, callback &get) {
    uint64_t x = get();
    __uint128_t m = static_cast<__uint128_t>(x) * static_cast<__uint128_t>(max_value);
    auto l = static_cast<uint64_t>(m);
    if(l < max_value) {
        uint64_t t = -max_value % max_value;
        while(l < t) {
            x = get();
            m = static_cast<__uint128_t>(x) * static_cast<__uint128_t>(max_value);
            l = static_cast<uint64_t>(m);
        }
    }
    return m >> 64;
}

// sanity check
static_assert(__FLOAT_WORD_ORDER == __BYTE_ORDER,
              "random_double52 not implemented if double and uint64_t have different byte orders");

inline double random_f52(uint64_t u) {
    u = (u >> 12) | UINT64_C(0x3FF0000000000000);
    double d;
    std::memcpy(&d, &u, sizeof(d));
    // d - (1.0-(DBL_EPSILON/2.0));
    return d - 0.99999999999999988;
}

inline double random_f53(uint64_t u) {
    auto n = static_cast<int64_t>(u >> 11);
    return n / 9007199254740992.0;
}

inline std::pair<uint32_t, uint32_t> random_u32_pair(uint64_t u) { return {u, u >> 32}; }

}  // namespace detail

// code sanity check
static_assert(std::is_same<uint64_t, detail::RandomEngine::result_type>::value,
              "The result type of RandomEngine is not a uint64_t.");

class Random : public detail::RandomEngine {
   public:
    using detail::RandomEngine::RandomEngine;

    uint64_t bits();
    uint64_t bits(int b);

    uint64_t u64();
    uint64_t u64(uint64_t max_value);

    uint32_t u32();
    std::pair<uint32_t, uint32_t> u32_pair();

    double f52();
    double f53();

   protected:
};

// uniformly distributed between [0,2^64)
inline uint64_t Random::bits() { return detail::RandomEngine::operator()(); }
// uniformly distributed between [0,2^b)
inline uint64_t Random::bits(int b) { return bits() >> (64 - b); }

// uniformly distributed between [0,2^64)
inline uint64_t Random::u64() { return bits(); }

// uniformly distributed between [0,max_value)
inline uint64_t Random::u64(uint64_t max_value) { return detail::random_u64_limited(max_value, *this); }

// uniformly distributed between [0,2^32)
inline uint32_t Random::u32() { return detail::random_u32(bits()); }

// uniformly distributed pair between [0,2^32)
inline std::pair<uint32_t, uint32_t> Random::u32_pair() { return detail::random_u32_pair(bits()); }

// uniformly distributed between (0,1.0)
inline double Random::f52() { return detail::random_f52(bits()); }

// uniformly distributed between [0,1.0)
inline double Random::f53() { return detail::random_f53(bits()); }

// Convert a sequence of values into a 64-bit seed
template <typename Sseq>
uint64_t create_uint64_seed(Sseq &ss) {
    uint64_t seed = UINT64_C(0xFD57D105591C980C);
    for(uint64_t s : ss) {
        seed += detail::splitmix64(&s);
    }
    return seed;
}

inline std::vector<uint64_t> create_seed_seq() {
    std::vector<uint64_t> ret;

    // 1. push some well mixed bits on the sequence
    ret.push_back(UINT64_C(0xC8F978DB0B32F62E));

// 2. add current time
#if __cpluscplus >= 201103L
    ret.push_back(std::chrono::high_resolution_clock::now().time_since_epoch().count());
#else
    ret.push_back(time(nullptr));
#endif

// 3. use current pid
#if defined(_WIN64) || defined(_WIN32)
    ret.push_back(_getpid());
#else
    ret.push_back(getpid());
#endif

// 4. add 64 random bits (if properly implemented)
#if __cpluscplus >= 201103L
    uint64_t u = std::random_device{}();
    u = (u << 32) + std::random_device{}();
    ret.push_back(u);
#endif

    return ret;
}

}  // namespace sparkyrng

// SPARKYRNG_HPP
#endif