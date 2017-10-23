#include <afina/allocator/Pointer.h>

namespace Afina {
namespace Allocator {

  Pointer::Pointer (void *desc) : descriptor (desc)
  {

  }

  Pointer::Pointer(const Pointer &p)
  {
    descriptor = p.descriptor;
  }
Pointer::Pointer (Pointer &&p)
{
  descriptor = p.descriptor;
}

Pointer &Pointer::operator=(const Pointer &p)
{
  descriptor = p.descriptor;
  return *this;
}

Pointer &Pointer::operator=(Pointer &&p)
{
  descriptor = p.descriptor;
  return *this;
}

} // namespace Allocator
} // namespace Afina
