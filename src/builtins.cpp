#include "builtins.h"

namespace
{

#include "builtins/vera_ttf.c"

}

namespace builtins
{

Blob font()
{
	return Blob(vera_ttf, vera_ttf_len);
}

SDL_RWops* as_rwops(Blob const& builtin)
{
	return SDL_RWFromConstMem(builtin.data(), builtin.size());
}

} // namespace builtins
