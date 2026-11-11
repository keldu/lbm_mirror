#pragma once

#include <ostream>
#include <string_view>

#include "unit.hpp"

namespace kel {
namespace lbm {
namespace impl {
/**
 * SI prefix symbol for a decimal scale exponent.
 *
 * Returns an empty view for the unprefixed case and for anything outside the
 * table, which the caller renders as an explicit 10^n instead. Printing a
 * scaled unit identically to an unscaled one would be worse than not printing
 * it at all, so there is no silent fallback.
 */
inline constexpr std::string_view unit_prefix_symbol(int64_t scale) {
	switch (scale) {
	case -9:
		return "n";
	case -6:
		return "u";
	case -3:
		return "m";
	case 0:
		return "";
	case 3:
		return "k";
	case 6:
		return "M";
	case 9:
		return "G";
	default:
		return "";
	}
}

inline constexpr bool unit_prefix_known(int64_t scale) {
	return scale == -9 || scale == -6 || scale == -3 || scale == 0 ||
		   scale == 3 || scale == 6 || scale == 9;
}
} // namespace impl
} // namespace lbm
} // namespace kel

namespace std {
/**
 * The scale has to be deduced, not defaulted: writing UnitElement<UT, UE> here
 * would only ever match zero scaled factors, and a prefixed one would fail to
 * bind at all.
 */
template <typename UT, int64_t UE, int64_t US>
inline ostream &operator<<(ostream &o,
						   const ::kel::lbm::sch::UnitElement<UT, UE, US> &) {
	if constexpr (US != 0) {
		if constexpr (::kel::lbm::impl::unit_prefix_known(US)) {
			o << ::kel::lbm::impl::unit_prefix_symbol(US);
		} else {
			o << "10^(" << US << ')' << '*';
		}
	}
	o << UT::short_name;
	if constexpr (UE != 1) {
		o << '^' << '(' << UE << ')';
	}
	return o;
}
} // namespace std

namespace kel {
namespace lbm {
namespace impl {
template <typename... T> struct unit_print {
	static_assert(always_false<T...>, "Template type not supported");
};

template <typename UT, int64_t UE, int64_t US, typename... UTL, int64_t... UEL,
		  int64_t... USL>
struct unit_print<sch::UnitElement<UT, UE, US>,
				  sch::UnitElement<UTL, UEL, USL>...> {
	static std::ostream &print(std::ostream &o) {
		sch::UnitElement<UT, UE, US> element;

		std::ostream &o_ret = o << element;

		if constexpr (sizeof...(UTL) > 0) {
			std::ostream &o_ret_next = o_ret << ' ' << '*' << ' ';
			return unit_print<sch::UnitElement<UTL, UEL, USL>...>::print(
				o_ret_next);
		}

		return o_ret << ']';
	}
};
} // namespace impl
} // namespace lbm
} // namespace kel

namespace std {
template <typename StorageT, typename Encode, typename... UnitT,
		  int64_t... UnitE, int64_t... UnitS>
inline ostream &
operator<<(ostream &o,
		   const ::kel::lbm::data<
			   ::kel::lbm::sch::Unit<
				   StorageT,
				   ::kel::lbm::sch::UnitElement<UnitT, UnitE, UnitS>...>,
			   Encode> &unit) {
	o << unit.handle().get();
	if constexpr (sizeof...(UnitT) > 0) {
		auto &o_ret = o << ' ' << '[';
		return ::kel::lbm::impl::unit_print<
			::kel::lbm::sch::UnitElement<UnitT, UnitE, UnitS>...>::print(o_ret);
	}
	return o;
}
} // namespace std
