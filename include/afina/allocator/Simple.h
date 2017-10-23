#ifndef AFINA_ALLOCATOR_SIMPLE_H
#define AFINA_ALLOCATOR_SIMPLE_H

#include <string>
#include <cstddef>

#include <vector>

namespace Afina {
namespace Allocator {

// Forward declaration. Do not include real class definition
// to avoid expensive macros calculations and increase compile speed
class Pointer;

constexpr size_t size_t_size = sizeof (size_t);
constexpr size_t size_of_descriptor = 2 * size_t_size;
constexpr size_t special_value = -1;

/**
 * Wraps given memory area and provides defagmentation allocator interface on
 * the top of it.
 *
 * Allocator instance doesn't take ownership of wrapped memmory and do not delete it
 * on destruction. So caller must take care of resource cleaup after allocator stop
 * being needs
 */
// TODO: Implements interface to allow usage as C++ allocators
class Simple {
public:
    Simple(void *base, const size_t size);

    /**
     * TODO: semantics
     * @param N size_t
     */
    Pointer alloc(size_t N, bool verbosity = false);

    /**
     * TODO: semantics
     * @param p Pointer
     * @param N size_t
     */
    void realloc(Pointer &p, size_t N);

    /**
     * TODO: semantics
     * @param p Pointer
     */
    void free(Pointer &p, bool verbosity = false);

    /**
     * TODO: semantics
     */
    void defrag(bool verbosity = false);

    /**
     * TODO: semantics
     */
    std::vector<size_t> dump() const;

    size_t get_descriptor_pos ();

private:
    size_t get_value (size_t offset);
    void set_value  (size_t offset, size_t value);

private:
    void *_base;
    const size_t _base_len;

    // Descriptor table contains pointer to allocated memory and size
    const size_t descriptor_table_size_pos  = _base_len - 1 * size_t_size;
    const size_t first_free_element_pos     = _base_len - 2 * size_t_size;
    const size_t first_free_desc_const_pos  = _base_len - 3 * size_t_size;
    const size_t first_free_chunk_const_pos = _base_len - 5 * size_t_size; // pointer to next free piece + size
    const size_t fd_pos                     = 5; // free pointer of descriptors

    size_t free_descriptor_pos = first_free_desc_const_pos;
    size_t free_chunk_pos = first_free_chunk_const_pos;

    size_t num_of_elements = 0;
};

} // namespace Allocator
} // namespace Afina
#endif // AFINA_ALLOCATOR_SIMPLE_H
