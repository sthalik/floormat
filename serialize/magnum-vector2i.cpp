#include "magnum-vector2i.hpp"
#include "compat/exception.hpp"

using namespace floormat;

void floormat::Serialize::throw_failed_to_parse_vector2(const std::string& str)
{
    fm_throw("failed to parse Vector2 '{}'"_cf, str);
}
