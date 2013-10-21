#include "signature.h++"

Signature::Signature(std::string r_hex, std::string s_hex, int size)
    : _r(r_hex, size),
      _s(s_hex, size)
{
}

std::string Signature::r(void) const
{
    return this->_r.hex();
}

std::string Signature::s(void) const
{
    return this->_s.hex();
}
