#pragma once

#include "schema.hpp"
#include "string_literal.hpp"

namespace kel {
namespace lbm {
namespace impl {
template<class T>
struct schema_to_string_helper {
	static_assert(always_false<T>, "schema_to_string_helper specialization not handled.");

	static constexpr string_literal apply(){
		return {};
	}
};

template<>
struct schema_to_string_helper<sch::SignedInteger> final {
	static constexpr string_literal apply(){
		return "SignedInteger";
	}
};

template<>
struct schema_to_string_helper<sch::UnsignedInteger> final {
	static constexpr string_literal apply(){
		return "UnsignedInteger";
	}
};

template<>
struct schema_to_string_helper<sch::FloatingPoint> final {
	static constexpr string_literal apply(){
		return "FloatingPoint";
	}
};

template<typename T, uint64_t N>
struct schema_to_string_helper<sch::Primitive<T,N>> final {
	static constexpr string_literal apply(){
		return "Primitive<"+schema_to_string_helper<T>::apply()+","+N+">";
	}
};
}

template<class T>
struct schema_to_string{
	static constexpr std::string_view value = impl::schema_to_string_helper<T>::apply().view();
};
}
}
