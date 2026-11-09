#pragma once

/**
 * Base include for this layer.
 *
 * Headers in c++/ take this rather than reaching into abstract/ themselves, so
 * the data model arrives from one place and the layering stays visible.
 */
#include "abstract/data.hpp"
#include "abstract/data_math.hpp"
#include "abstract/data_ptr.hpp"
#include "abstract/math.hpp"

namespace kel {
namespace lbm {
namespace sch {
}
}
}
