#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

namespace saw {

#define KEL_CONCAT_(x, y) x##y
#define KEL_CONCAT(x, y) KEL_CONCAT_(x, y)
#define KEL_UNIQUE_NAME(prefix) KEL_CONCAT(prefix, __LINE__)

#define KEL_FORBID_COPY(classname)                                             \
	classname(const classname &) = delete;                                     \
	classname &operator=(const classname &) = delete

#define KEL_FORBID_MOVE(classname)                                             \
	classname(classname &&) = delete;                                          \
	classname &operator=(classname &&) = delete

#define KEL_DEFAULT_COPY(classname)                                            \
	classname(const classname &) = default;                                    \
	classname &operator=(const classname &) = default

#define KEL_DEFAULT_MOVE(classname)                                            \
	classname(classname &&) = default;                                         \
	classname &operator=(classname &&) = default

// In case of C++20
#define KEL_ASSERT(expression)                                                 \
	assert(expression);                                                        \
	if (!(expression)) [[unlikely]]

template <typename T> using maybe = std::optional<T>;

template <typename T> using own = std::unique_ptr<T>;

template <typename T> using our = std::shared_ptr<T>;

template <typename T> using lent = std::weak_ptr<T>;

template<typename T>
class ptr {
private:
	T* ptr_;

public:
	ptr():
		ptr_{nullptr}
	{}

	ptr(T& ptr__):
		ptr_{&ptr__}
	{}

	KEL_DEFAULT_COPY(ptr);
	KEL_DEFAULT_MOVE(ptr);

	T& operator()(){
		return *ptr_;
	}

	const T& operator()() const {
		return *ptr_;
	}

	bool is_valid() const {
		return ptr_ != nullptr;
	}
};

/**
 * Reference class for easier distinction
 * of references and its referenced types.
 */
template <typename T>
class ref {
private:
	/**
	 * Referenced type
	 */
	T* ref_;

	/**
	 * We don't want to move since the would invalidate the type.
	 */
	KEL_FORBID_MOVE(ref);
public:
	/**
	 * Main constructor.
	 */
	constexpr ref(T& ref__):
		ref_{&ref__}
	{}

	KEL_DEFAULT_COPY(ref);

	/**
	 * Operator retrieving the itself.
	 */
	constexpr T& operator()(){
		return *ref_;
	}

	/**
	 * Operator retrieving the itself.
	 */
	constexpr const T& operator()() const {
		return *ref_;
	}
};

template <typename T, class... Args> own<T> heap(Args &&...args) {
	return own<T>(new T(std::forward<Args>(args)...));
}

template <typename T, class... Args> our<T> share(Args &&...args) {
	return std::make_shared<T>(std::forward<Args>(args)...);
}

template <typename T> T instance() noexcept;

template <typename Func, typename T> struct return_type_helper {
	typedef decltype(instance<Func>()(instance<T>())) Type;
};
template <typename Func> struct return_type_helper<Func, void> {
	typedef decltype(instance<Func>()()) Type;
};

template <typename Func, typename T>
using return_type = typename return_type_helper<Func, T>::Type;

/**
 * Struct describing a void type
 */
struct void_t {};

/**
 * Helper function since changing returns between void_t{} and make_error<...>() is a bit annoying.
 */
void_t make_void();

template <typename T> struct void_fix { typedef T Type; };
template <> struct void_fix<void> { typedef void_t Type; };
template <typename T> using fix_void = typename void_fix<T>::Type;

template <typename T> struct void_unfix { typedef T Type; };
template <> struct void_unfix<void_t> { typedef void Type; };
template <typename T> using unfix_void = typename void_unfix<T>::Type;

template <typename... T> constexpr bool always_false = false;

} // namespace saw
