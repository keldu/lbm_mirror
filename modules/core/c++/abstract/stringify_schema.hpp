#pragma once

#include "schema.hpp"
#include "string_literal.hpp"
#include "templates.hpp"

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
struct schema_to_string_helper<sch::Void> final {
	static constexpr auto apply(){
		return string_literal{"Void"};
	}
};

template<>
struct schema_to_string_helper<sch::Bool> final {
	static constexpr auto apply(){
		return string_literal{"Bool"};
	}
};

template<>
struct schema_to_string_helper<sch::String> final {
	static constexpr auto apply(){
		return string_literal{"String"};
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

/**
 * Renders a compile time number as its decimal digits. string_literal has no
 * operator+ for integers, so the number has to be turned into a literal first.
 */
template<uint64_t N>
constexpr auto number_literal(){
	return ct_convert_to_digits<N, 10u>::literal;
}

/**
 * Joins a parameter pack of literals with ',' separators.
 */
template<typename... T>
struct schema_list_to_string_helper;

template<>
struct schema_list_to_string_helper<> final {
	static constexpr auto apply(){
		return string_literal{""};
	}
};

template<typename T>
struct schema_list_to_string_helper<T> final {
	static constexpr auto apply(){
		return schema_to_string_helper<T>::apply();
	}
};

template<typename T, typename U, typename... R>
struct schema_list_to_string_helper<T,U,R...> final {
	static constexpr auto apply(){
		return schema_to_string_helper<T>::apply()
			+ string_literal{","}
			+ schema_list_to_string_helper<U,R...>::apply();
	}
};

/**
 * Joins a pack of compile time numbers with ',' separators.
 */
template<uint64_t... N>
struct number_list_to_string_helper;

template<>
struct number_list_to_string_helper<> final {
	static constexpr auto apply(){
		return string_literal{""};
	}
};

template<uint64_t N>
struct number_list_to_string_helper<N> final {
	static constexpr auto apply(){
		return number_literal<N>();
	}
};

template<uint64_t N, uint64_t M, uint64_t... R>
struct number_list_to_string_helper<N,M,R...> final {
	static constexpr auto apply(){
		return number_literal<N>()
			+ string_literal{","}
			+ number_list_to_string_helper<M,R...>::apply();
	}
};

template<typename T, uint64_t N>
struct schema_to_string_helper<sch::Primitive<T,N>> final {
	static constexpr auto apply(){
		return string_literal{"Primitive<"}
			+ schema_to_string_helper<T>::apply()
			+ string_literal{","}
			+ number_literal<N>()
			+ string_literal{">"};
	}
};

template<typename S, typename I>
struct schema_to_string_helper<sch::MixedPrecision<S,I>> final {
	static constexpr auto apply(){
		return string_literal{"MixedPrecision<"}
			+ schema_to_string_helper<S>::apply()
			+ string_literal{","}
			+ schema_to_string_helper<I>::apply()
			+ string_literal{">"};
	}
};

template<typename T, uint64_t... D>
struct schema_to_string_helper<sch::FixedArray<T,D...>> final {
	static constexpr auto apply(){
		return string_literal{"FixedArray<"}
			+ schema_to_string_helper<T>::apply()
			+ string_literal{","}
			+ number_list_to_string_helper<D...>::apply()
			+ string_literal{">"};
	}
};

template<typename T, uint64_t D>
struct schema_to_string_helper<sch::Array<T,D>> final {
	static constexpr auto apply(){
		return string_literal{"Array<"}
			+ schema_to_string_helper<T>::apply()
			+ string_literal{","}
			+ number_literal<D>()
			+ string_literal{">"};
	}
};

template<typename T, uint64_t... D>
struct schema_to_string_helper<sch::Tensor<T,D...>> final {
	static constexpr auto apply(){
		return string_literal{"Tensor<"}
			+ schema_to_string_helper<T>::apply()
			+ string_literal{","}
			+ number_list_to_string_helper<D...>::apply()
			+ string_literal{">"};
	}
};

template<typename... T>
struct schema_to_string_helper<sch::Tuple<T...>> final {
	static constexpr auto apply(){
		return string_literal{"Tuple<"}
			+ schema_list_to_string_helper<T...>::apply()
			+ string_literal{">"};
	}
};

template<typename T, string_literal K>
struct schema_to_string_helper<sch::Member<T,K>> final {
	static constexpr auto apply(){
		return string_literal{"Member<"}
			+ schema_to_string_helper<T>::apply()
			+ string_literal{","}
			+ K
			+ string_literal{">"};
	}
};

template<typename... T, string_literal... K>
struct schema_to_string_helper<sch::Struct<sch::Member<T,K>...>> final {
	static constexpr auto apply(){
		return string_literal{"Struct<"}
			+ schema_list_to_string_helper<sch::Member<T,K>...>::apply()
			+ string_literal{">"};
	}
};

template<typename... T, string_literal... K>
struct schema_to_string_helper<sch::Union<sch::Member<T,K>...>> final {
	static constexpr auto apply(){
		return string_literal{"Union<"}
			+ schema_list_to_string_helper<sch::Member<T,K>...>::apply()
			+ string_literal{">"};
	}
};

template<typename T>
struct schema_to_string_helper<sch::Ptr<T>> final {
	static constexpr auto apply(){
		return string_literal{"Ptr<"}
			+ schema_to_string_helper<T>::apply()
			+ string_literal{">"};
	}
};

template<typename T>
struct schema_to_string_helper<sch::Ref<T>> final {
	static constexpr auto apply(){
		return string_literal{"Ref<"}
			+ schema_to_string_helper<T>::apply()
			+ string_literal{">"};
	}
};
}

template<class T>
struct schema_to_string {
	static constexpr auto literal = impl::schema_to_string_helper<T>::apply();
	static constexpr std::string_view value = literal.view();
};
}
}
