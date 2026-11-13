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

/**
 * A packed little endian binary layout. See abstract/forst.hpp.
 *
 * Every value sits at a computable byte offset, so reading one member costs no
 * decoding of the members before it. Fixed size schemas are inline; variable
 * length ones spend their slot on dimensions plus an offset and put their bytes
 * in a single dynamic layer after the static section.
 */
struct Forst {
	static constexpr string_literal name = "enc::Forst";
};
} // namespace enc

/**
 * Declared here rather than in any one codec header so that an encoding does
 * not have to include an unrelated one to specialise it. json.hpp and
 * forst.hpp are siblings; neither should need the other.
 */
template <typename Schema, typename Encoding> class codec;

#ifndef KEL_LBM_DEFAULT_DATA_ENCODING
#define KEL_LBM_DEFAULT_DATA_ENCODING ::kel::lbm::enc::Native
#endif

} // namespace lbm
} // namespace kel
