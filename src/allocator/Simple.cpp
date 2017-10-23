#include <afina/allocator/Simple.h>
#include <string.h>
#include <afina/allocator/Pointer.h>
#include <iostream>
#include <vector>

#include <afina/allocator/Error.h>

namespace Afina {
namespace Allocator {

size_t Simple::get_value (size_t offset)
{
  return *(size_t*)((char*)_base + offset);
}

void Simple::set_value (size_t offset, size_t value)
{
  *reinterpret_cast<size_t *> ((char*)_base + offset) = value;
}

size_t Simple::get_descriptor_pos ()
{
  return _base_len - fd_pos * size_t_size - get_value (descriptor_table_size_pos);
}

Simple::Simple (void *base, size_t size) : _base(base), _base_len(size)
{
  set_value (descriptor_table_size_pos, 0);
  set_value (first_free_element_pos, 0);
  set_value (first_free_desc_const_pos, first_free_desc_const_pos);
}

/**
 * TODO: semantics
 * @param N size_t
 */

Pointer Simple::alloc (size_t N, bool verbosity)
{
  /// Allocate must use first free descriptor
  size_t first_free_element = get_value (first_free_element_pos);
  size_t place_to_allocate = first_free_element;

  // try to allocate to free space
  size_t free_chunk = free_chunk_pos,
         prev_free_chunk = special_value;
  while (free_chunk != first_free_chunk_const_pos)
    {
      size_t free_space = get_value (free_chunk + size_t_size);
      if (N <= free_space)
        break;
      prev_free_chunk = free_chunk;
      free_chunk = get_value (free_chunk);
    }
  if (free_chunk != first_free_chunk_const_pos)
    {
      // found some free space
      place_to_allocate = free_chunk;
      if (prev_free_chunk != special_value)
        set_value (prev_free_chunk, get_value (free_chunk));
      else
        {
          // if there is no previous free chunk, then free_chunk == free_chunk_pos
          free_chunk_pos = get_value (free_chunk);
        }
    }
  else
    {
      if (_base_len - fd_pos * size_t_size - get_value (descriptor_table_size_pos) < first_free_element)
        throw (AllocError (AllocErrorType::NoMemory, "Descriptors peresekautsya with pointers"));
      if (verbosity)
        std::cout << _base_len - fd_pos * size_t_size - first_free_element - get_value (descriptor_table_size_pos) << std::endl;
      if (N > _base_len - fd_pos * size_t_size - first_free_element - get_value (descriptor_table_size_pos))
        throw (AllocError (AllocErrorType::NoMemory, "Not enough memory"));
    }

  size_t descriptor = 0;
  if (free_descriptor_pos != first_free_desc_const_pos)
    {
      descriptor = free_descriptor_pos;
      free_descriptor_pos = get_value (free_descriptor_pos);
    }
  else
    {
      size_t descriptor_table_size = get_value (descriptor_table_size_pos);
      descriptor_table_size += size_of_descriptor;
      set_value (descriptor_table_size_pos, descriptor_table_size);
      descriptor = get_descriptor_pos ();
    }

  // update current descriptor value and
  set_value (descriptor, (size_t)((char*)_base + place_to_allocate));
  if (verbosity)
    std::cout << "Set to : " << descriptor << " value of " << place_to_allocate << std::endl;

  set_value (descriptor + size_t_size, N);

  // result pointer
  auto res = Pointer ((char*)_base + descriptor);
  if (verbosity)
    std::cout << "Return pointer: " << get_value (descriptor) << std::endl;

  if (place_to_allocate == first_free_element)
    {
      // update first free element
      first_free_element += N;
      set_value (first_free_element_pos, first_free_element);
    }

  num_of_elements++;
  return res;
}

/**
 * TODO: semantics
 * @param p Pointer
 * @param N size_t
 */
void Simple::realloc (Pointer &p, size_t N)
{
  size_t descriptor_pos = special_value;
  if (!p.descriptor)
    {
      p = alloc (N);
      return;
    }
  else
    {
      descriptor_pos = (size_t)p.descriptor - (size_t)_base;
    }
  size_t prev_size = get_value (descriptor_pos + size_t_size);
  size_t place_to_allocate = special_value,
         initial_place = get_value (descriptor_pos) - (size_t)_base;

  // find empty place to realloc
  size_t free_chunk = free_chunk_pos,
         prev_free_chunk = special_value;

  if (N < prev_size)
    {
      // realloc 'inplace'
      place_to_allocate = (size_t)get_value (descriptor_pos) - (size_t)_base;

      memmove ((char*)_base + place_to_allocate, //dest
               (char*)_base + initial_place,     //src
               N);
      set_value (descriptor_pos + size_t_size, N);
      return;
    }

  while (free_chunk != first_free_chunk_const_pos)
    {
      size_t free_space = get_value (free_chunk + size_t_size);
      if (N <= free_space)
        break;
      prev_free_chunk = free_chunk;
      free_chunk = get_value (free_chunk);
    }
  if (free_chunk != first_free_chunk_const_pos)
    {
          // found some free space
      place_to_allocate = free_chunk;
      if (prev_free_chunk != special_value)
        set_value (prev_free_chunk, get_value (free_chunk));
      else
        {
          // if there is no previous free chunk, then free_chunk == free_chunk_pos
          free_chunk_pos = get_value (free_chunk);
        }
    }
  else
    {
      size_t first_free_element = get_value (first_free_element_pos);
      // no free chunks, but maybe enough space
      if (_base_len - fd_pos * size_t_size - get_value (descriptor_table_size_pos) < first_free_element)
        throw (AllocError (AllocErrorType::NoMemory, "Descriptors peresekautsya with pointers"));
      if (N > _base_len - fd_pos * size_t_size - first_free_element - get_value (descriptor_table_size_pos))
        throw (AllocError (AllocErrorType::NoMemory, "Not enough memory"));

      free_chunk = first_free_element;
      if (first_free_element - initial_place == prev_size)
        {
          // first free element goes right after reallocing pointer
          free_chunk = initial_place;
          size_t first_free_element = get_value (first_free_element_pos);
          set_value (first_free_element_pos, first_free_element + N - prev_size);
        }
    }
  place_to_allocate = free_chunk;
  memmove ((char*)_base + place_to_allocate, //dest
           (char*)_base + initial_place,     //src
           N);

  if (place_to_allocate != initial_place)
    {
      // make current descriptor point to other place
      set_value (descriptor_pos, (size_t)((char*)_base + place_to_allocate));

      // make initial_place empty
      set_value (initial_place, free_chunk_pos);
      set_value (initial_place + size_t_size, prev_size);
      free_chunk_pos = initial_place;
    }
  set_value (descriptor_pos + size_t_size, N);
}

/**
 * TODO: semantics
 * @param p Pointer
 */
void Simple::free (Pointer &p, bool verbosity)
{
  size_t descriptor_pos = (size_t)p.descriptor - (size_t)_base;

  if (verbosity)
    std::cout << "desc: " << descriptor_pos << std::endl;
  size_t allocated_mem_size = get_value (descriptor_pos + size_t_size);

  // set free chunk position value
  // free_chunk_const_pos <- free_chunk_pos_1 <- free_chunk_pos_2 <- ...
  size_t free_chunk = (size_t)get_value (descriptor_pos) - (size_t)_base;
  if (verbosity)
    std::cout << "Free chunk: " << free_chunk << std::endl;
  set_value (free_chunk, free_chunk_pos);
  set_value (free_chunk + size_t_size, allocated_mem_size);
  free_chunk_pos = free_chunk;

  // set free descriptor position value
  // free_descriptor_const_pos <- free_desc_pos_1 <- free_desc_pos_2 <- ...
  set_value (descriptor_pos, free_descriptor_pos);
  free_descriptor_pos = descriptor_pos;

  p.descriptor = nullptr;
  num_of_elements--;
}

/**
 * TODO: semantics
 */
void Simple::defrag (bool verbosity)
{
  size_t global_offset = 0,
         free_space_offset = 0,
         filled_space_offset = 0;

  size_t desc_table_size = get_value (descriptor_table_size_pos);
  size_t first_free_chunk_idx = special_value,
         prev_free_chunk_idx  = special_value;

  bool was_free = false;
  size_t elements_num = 0;

  while (   global_offset < _base_len - fd_pos * size_t_size - desc_table_size
         && free_space_offset < _base_len - fd_pos * size_t_size - desc_table_size
         && elements_num < num_of_elements)
    {
      auto curr_pointer = (char*)_base + global_offset;
      if (verbosity)
        std::cout << "global offset: " << global_offset << std::endl;

      // check if current pointer is empty
      size_t local_free_chunk_idx = free_chunk_pos;
      size_t local_prev_free_chunk_idx = special_value;
      while (local_free_chunk_idx != first_free_chunk_const_pos)
        {
          if (curr_pointer == (char*)_base + local_free_chunk_idx)
            break;
          local_prev_free_chunk_idx = local_free_chunk_idx;
          local_free_chunk_idx = get_value (local_free_chunk_idx);
        }

      if (local_free_chunk_idx != first_free_chunk_const_pos)
        {
          // current chunk is actually free
          if (first_free_chunk_idx == special_value)
            {
              first_free_chunk_idx = local_free_chunk_idx;
              prev_free_chunk_idx  = local_prev_free_chunk_idx;
            }
          size_t free_offset_size = get_value (local_free_chunk_idx + size_t_size);
          free_space_offset += free_offset_size;
          global_offset += free_offset_size;
          was_free = true;
        }
      else
        {
          // current chunk is allocated

          // find descriptor
          if (verbosity)
            std::cout << "HERE:" << global_offset << std::endl;
          size_t descriptor_pos = get_descriptor_pos ();
          for (; descriptor_pos <= _base_len - fd_pos * size_t_size; descriptor_pos += size_of_descriptor)
            {
              if ((size_t)curr_pointer == get_value(descriptor_pos))
                break;
            }
          if (descriptor_pos >= _base_len - fd_pos * size_t_size)
            {
              std::cout << descriptor_pos << " >= " << _base_len - fd_pos * size_t_size << std::endl;
              throw (AllocError (AllocErrorType::NoValidPointer, "Cannot find pointer"));
            }

          size_t allocated_mem = get_value (descriptor_pos + size_t_size);
          if (allocated_mem == 0)
            {
              std::cout << "LOLWTF current descriptor is free, wtfwtfwtf" << std::endl;
              throw (AllocError (AllocErrorType::NoValidPointer, ""));
            }
          else
            {
              // move to free position
              elements_num++;
              if (free_space_offset)
                {
                  // update chunk_idx + move it free_space_offset right
                  if (was_free)
                    {
                      size_t next_chunk_idx = get_value (first_free_chunk_idx);
                      if (prev_free_chunk_idx != special_value)
                        set_value (prev_free_chunk_idx, next_chunk_idx);
                      else
                        {
                          // no previous chunk, so this is free_chunk_pos
                          free_chunk_pos = next_chunk_idx;
                        }
                      prev_free_chunk_idx = first_free_chunk_idx = special_value;
                      was_free = false;
                    }

                  memmove ((char*)_base + filled_space_offset, // dest
                           (char*)_base + global_offset,       // src
                           allocated_mem);
                  set_value (descriptor_pos, (size_t)((char*)_base + filled_space_offset));
                }

              global_offset += allocated_mem;
              filled_space_offset += allocated_mem;
            }
        }
    }
  // update first free element
  size_t first_free_el = get_value (first_free_element_pos);
  set_value (first_free_element_pos, first_free_el - free_space_offset);

  if (verbosity)
    std::cout << "After defrag" << std::endl;
  size_t descriptor_pos = get_descriptor_pos ();
  // cycle through descriptors to find where p is allocated
  if (verbosity)
    for (; descriptor_pos < _base_len - fd_pos * size_t_size; descriptor_pos += size_of_descriptor)
      {
        std::cout << descriptor_pos << " Pointer value: " << get_value (descriptor_pos) << " size " << get_value (descriptor_pos + size_t_size)
                  << " on size position : " << descriptor_pos + size_t_size << std::endl;
      }

}

/**
 * TODO: semantics
 */
std::vector<size_t> Simple::dump() const {
  std::vector <size_t> ret;
  size_t len = _base_len / sizeof (size_t);
  for (size_t i = 0; i < len; i++) {
      ret.push_back (*reinterpret_cast <size_t *>((char*)_base + i * sizeof (size_t)) / sizeof (size_t));
    }
  return ret; }

} // namespace Allocator
} // namespace Afina
