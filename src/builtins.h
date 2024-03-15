#include <SDL_rwops.h>
#include <string_view>

namespace builtins
{

using Blob = std::basic_string_view<unsigned char>;

Blob font();

SDL_RWops* as_rwops(Blob const& builtin);

} // namespace builtins
