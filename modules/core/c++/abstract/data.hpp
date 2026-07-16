#pragma once

#include "schema.hpp"
#include "encode.hpp"

namespace kel {
namespace lbm {
template<typename Schema, typename Encode = enc::Native>
class data final {};

template<typename T, uint64_t N>
class data<sch::Primitive<T,N>,Encode> final {
private:
public:
};


}
}
