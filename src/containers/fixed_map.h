#include <map>
#include <cstdint>
//these should't be actually needed... right?
//#include <cassert>
//#include <new> 
#include <type_traits>
#include "rpi.h"

//WARNING: THIS MAY RUIN US LATER

// SERIOUSLY, I AM UNSURE OF THE REPERCUSSIONS FOR DOING THIS
// extern "C" {
//   void* _sbrk(ptrdiff_t incr) { return nullptr; }
//   void _exit(int status) { while (1); }
//   int _kill(int pid, int sig) { return -1; }
//   int _getpid() { return 1; }
//   int _read(int fd, char* ptr, int len) { return 0; }
//   int _write(int fd, const char* ptr, int len) { return 0; }
//   int _close(int fd) { return -1; }
//   int _fstat(int fd, void* st) { return 0; }
//   int _isatty(int fd) { return 1; }
//   int _lseek(int fd, int ptr, int dir) { return 0; }
// }


template <std::size_t BlockSize, std::size_t Capacity>
struct Pool {
    //properly aligns T with our memory block
    //alignof is the align requirements of T
    //alignas will align the storage with the requirements of T
    alignas(std::max_align_t) char storage[BlockSize * Capacity];
    bool used[Capacity] = {};

    void* allocate() {
        for (std::size_t i = 0; i < Capacity; ++i) {
            if (!used[i]) {
                used[i] = true;
                return static_cast<void*>(&storage[i * BlockSize]);
            }
        }
        //todo: replace w assert
        uart_printf(CONSOLE, "ERROR: bad alloc");
    }

    void deallocate(void* ptr) {
        auto base = reinterpret_cast<std::uintptr_t>(storage);
        auto p = reinterpret_cast<std::uintptr_t>(ptr);
        std::size_t index = (p - base) / BlockSize;
        //should be replaced w assert
        //  assert(index < Capacity);
        if(index>=Capacity){
            uart_printf(CONSOLE, "ERROR: pool index >= capacity");
        }
        used[index] = false;
    }
};

template <typename T, std::size_t BlockSize, std::size_t Capacity>
struct PoolAllocator {
    using value_type = T;

    Pool<BlockSize, Capacity>* pool;

    PoolAllocator(Pool<BlockSize, Capacity>* p = nullptr) : pool(p) {}

    template <typename U>
    PoolAllocator(const PoolAllocator<U, BlockSize, Capacity>& other) noexcept
        : pool(other.pool) {}

    T* allocate(std::size_t n) {
        if(n != 1){
            uart_printf(CONSOLE, "ERROR: multi-allocation");
        }
        return static_cast<T*>(pool->allocate());
    }

    void deallocate(T* p, std::size_t) {
        pool->deallocate(p);
    }

    template <typename U>
    struct rebind {
        using other = PoolAllocator<U, BlockSize, Capacity>;
    };

    using is_always_equal = std::true_type;
};