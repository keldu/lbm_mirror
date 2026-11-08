#pragma once

#include <ostream>

#include "unit.hpp"

namespace std {
template <typename UT, int64_t UE>
inline ostream &operator<<(ostream &o,
						   const ::kel::lbm::sch::UnitElement<UT, UE> &) {
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

template <typename UT, int64_t UE, typename... UTL, int64_t... UEL>
struct unit_print<sch::UnitElement<UT, UE>, sch::UnitElement<UTL, UEL>...> {
	static std::ostream &print(std::ostream &o) {
		sch::UnitElement<UT, UE> element;

		std::ostream &o_ret = o << element;

		if constexpr (sizeof...(UTL) > 0) {
			std::ostream &o_ret_next = o_ret << ' ' << '*' << ' ';
			return unit_print<sch::UnitElement<UTL, UEL>...>::print(o_ret_next);
		}

		return o_ret << ']';
	}
};
} // namespace impl
} // namespace lbm
} // namespace kel

namespace std {
template <typename StorageT, typename Encode, typename... UnitT,
		  int64_t... UnitE>
inline ostream &
operator<<(ostream &o,
		   const ::kel::lbm::data<
			   ::kel::lbm::sch::Unit<
				   StorageT, ::kel::lbm::sch::UnitElement<UnitT, UnitE>...>,
			   Encode> &unit) {
	o << unit.handle().get();
	if constexpr (sizeof...(UnitT) > 0) {
		auto &o_ret = o << ' ' << '[';
		return ::kel::lbm::impl::unit_print<
			::kel::lbm::sch::UnitElement<UnitT, UnitE>...>::print(o_ret);
	}
	return o;
}
} // namespace std
