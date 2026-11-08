#pragma once

#include "string_literal.hpp"

namespace kel {
namespace lbm {
namespace enc {
/**
 * The in-memory layout the data class uses by default: every value is held as
 * the native C++ type the schema maps onto.
 */
struct Native {
	static constexpr string_literal name = "enc::Native";
};

/**
 * Contiguous, directly addressable layout. Kept distinct from Native because
 * an encoding that can hand out a raw pointer is a stronger guarantee than one
 * that merely stores values natively.
 */
struct NativeRaw {
	static constexpr string_literal name = "enc::NativeRaw";
};

struct Json {
	static constexpr string_literal name = "enc::Json";
};
} // namespace enc

#ifndef KEL_LBM_DEFAULT_DATA_ENCODING
#define KEL_LBM_DEFAULT_DATA_ENCODING ::kel::lbm::enc::Native
#endif

} // namespace lbm
} // namespace kel
