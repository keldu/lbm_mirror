#pragma once

#include <array>
#include <string_view>

namespace kel {
namespace lbm {
/**
 * Helper object which creates a templated string from the provided string
 * literal. It guarantees compile time uniqueness and thus allows using strings
 * in template parameters.
 */
template <class CharT, size_t N> class string_literal {
public:
    static_assert(N > 0, "string_literal needs a null terminator");

	constexpr string_literal(const CharT (&input)[N]) noexcept {
		for (size_t i = 0; i < N; ++i) {
			data[i] = input[i];
		}
	}

	std::array<CharT, N> data{};

	constexpr std::string_view view() const noexcept {
		return std::string_view{data.data()};
	}

	constexpr bool
	operator==(const string_literal<CharT, N> &) const noexcept = default;

	template <class CharTR, size_t NR>
	constexpr bool
	operator==(const string_literal<CharTR, NR> &) const noexcept {
		return false;
	}

	template<size_t NR>
    constexpr string_literal<CharT, N+NR-1> operator+(const string_literal<CharT, NR>& rhs) const noexcept {
		CharT sum[N+NR-1];

		// The weird i+1 happens due to needing to skip the '\0' terminator
		for(size_t i = 0; (i+1) < N; ++i){
				sum[i] = data[i];
		}
		for(size_t i = 0; i < NR; ++i){
				sum[i+N-1] = rhs.data[i];
		}

		return string_literal<CharT, N+NR-1>{sum};
	}


};

template <typename T, T... Chars>
constexpr string_literal<T, sizeof...(Chars) + 1u> operator""_sl() {
	return string_literal<T, sizeof...(Chars) + 1u>{{Chars..., '\0'}};
}
}
}
