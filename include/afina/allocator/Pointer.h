#ifndef AFINA_ALLOCATOR_POINTER_H
#define AFINA_ALLOCATOR_POINTER_H

namespace Afina {
namespace Allocator {
// Forward declaration. Do not include real class definition
// to avoid expensive macros calculations and increase compile speed
class Simple;

class Pointer {
public:
  Pointer() {}

    Pointer(void *);
    Pointer(const Pointer &);
    Pointer(Pointer &&p);

    Pointer &operator=(const Pointer &);
    Pointer &operator=(Pointer &&);

    void *get() const
    {
      if (descriptor == nullptr)
        return nullptr;
      return *(void**)descriptor;
    }

public:
    void *descriptor = nullptr;
};

} // namespace Allocator
} // namespace Afina

#endif // AFINA_ALLOCATOR_POINTER_H
