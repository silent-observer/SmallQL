#pragma once
#include <cstdint>
#include <string>
#include <assert.h>
#include <memory>

using namespace std;

using NodeId = uint32_t;
using CellId = uint16_t;
using SlotId = uint16_t;
using PageId = uint32_t;
using RecordId = uint64_t;
static const uint64_t NULL64 = 0xFFFFFFFFFFFFFFFFul;
static const uint32_t NULL32 = 0xFFFFFFFFu;
static const uint16_t NULL16 = 0xFFFFu;

template<typename T>
static inline const T& makeConst(const T& x) {
    return x;
}

static inline void makeUpper(string& s) {
    for (auto& c: s) c = toupper(c);
}

template<typename To, typename From>
static inline To* convert(const unique_ptr<From>& x) {
    return dynamic_cast<To*>(x.get());
}

template<typename To, typename From>
static inline bool is(const unique_ptr<From>& x) {
    return dynamic_cast<To*>(x.get()) != nullptr;
}

template<typename To, typename From>
static inline To* convert(const shared_ptr<From>& x) {
    return dynamic_cast<To*>(x.get());
}

template<typename To, typename From>
static inline bool is(const shared_ptr<From>& x) {
    return dynamic_cast<To*>(x.get()) != nullptr;
}

class SQLException : public std::exception {};

enum JoinType {
    Cross,
    Inner,
    Left,
    Right,
    Full
};