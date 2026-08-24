#pragma once

#include "schema.hpp"
#include "string_literal.hpp"

namespace kel {
namespace lbm {
namespace impl {
template<class T>
struct schema_to_string_helper {
	static_assert(always_false<T>, "schema_to_string_helper specialization not handled.");

	static constexpr auto apply(){
		return string_literal{""};
	}
};

template<>
struct schema_to_string_helper<sch::SignedInteger> final {
	static constexpr auto apply(){
		return string_literal{"SignedInteger"};
	}
};

template<>
struct schema_to_string_helper<sch::UnsignedInteger> final {
	static constexpr auto apply(){
		return string_literal{"UnsignedInteger"};
	}
};

template<>
struct schema_to_string_helper<sch::FloatingPoint> final {
	static constexpr auto apply(){
		return string_literal{"FloatingPoint"};
	}
};

template<typename T, uint64_t N>
struct schema_to_string_helper<sch::Primitive<T,N>> final {
	static constexpr auto apply(){
		return string_literal{"Primitive<"}+schema_to_string_helper<T>::apply()+string_literal{","}+N+string_literal{">"};
	}
};
}

template<class T>
struct schema_to_string{
	static constexpr std::string_view value = impl::schema_to_string_helper<T>::apply().view();
};
}
}
