#pragma once

#include "Hash.hpp"
#include "Types.hpp"

#include <format>
#include <string>
/**
 * @brief Generic string class. Allocated with a set amount of stack memory, allocates to heap when there is no space
 * remaining.
 */
class String
{
    static constexpr uint32 scStackAllocSize = 32;

public:
    static String NoCopy(char* ptr, uint32 length);

    String() = default;
    String(uint32 allocation_size);
    String(const char* str, uint32 length);
    String(const std::string& str);
    String(const char* str);

    FORCE_INLINE bool IsHeapAllocated() const { return (mpHeapStr != nullptr); }

    FORCE_INLINE uint32 GetLength() const { return Length; }
    FORCE_INLINE const char* CStr() const
    {
        if (IsHeapAllocated()) {
            return mpHeapStr;
        }

        return mpStackStr;
    }

    Hash32 GetHash();
    FORCE_INLINE void InvalidateHash() { mHash = HashNull32; }

    String& operator=(const char* str);

    String operator+(const String& other) const;
    String operator+(const char* other) const;

    const char operator[](size_t index) const;
    char& operator[](size_t index);


    ~String();

private:
    FORCE_INLINE char* GetInternalPtr()
    {
        if (IsHeapAllocated()) {
            return mpHeapStr;
        }

        return mpStackStr;
    }

public:
    uint32 Length = 0;

private:
    char mpStackStr[scStackAllocSize];
    char* mpHeapStr = nullptr;

    Hash32 mHash = HashNull32;
};


template <>
struct std::formatter<String>
{
    auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(const String& str, std::format_context& ctx) const
    {
        return std::format_to(ctx.out(), "{}", str.CStr());
    }
};
