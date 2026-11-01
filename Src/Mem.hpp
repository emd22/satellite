#pragma once

#include "Types.hpp"

#include <cstddef>
#include <cstdlib>

// Tired of new/free shenanigans, this is kindof a temp fill-in for a memory pool.

namespace Mem {

template <typename TType>
TType* Alloc(std::size_t size)
{
    return reinterpret_cast<TType*>(std::malloc(size));
}

template <typename TType>
TType* Realloc(TType* ptr, std::size_t size)
{
    return reinterpret_cast<TType*>(std::realloc(ptr, size));
}

template <typename TType>
void Free(TType* ptr)
{
    if (ptr == nullptr) {
        return;
    }

    std::free(reinterpret_cast<void*>(ptr));
}

} // namespace Mem
