#pragma once

#include <algorithm>
#include <limits>
#include <functional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <cassert>

#include "common.hpp"

namespace saw {
/**
 * Utility class for generating errors. Has a base distinction between
 * critical and recoverable errors. Additional code ids can be provided to the
 * constructor if additional distinctions are necessary.
 */
class error final {
public:
	/**
	 * Type alias
	 */
	using code = uint32_t;
private:
	/**
	 * Type representing the error code used to distinguish the different error types
	 */
	code error_code_;
	/**
	 * Helper variable for ease of checking if it's a critical error.
	 */
	bool is_critical_;
	/**
	 * Member holding the message. It's a variant for edge cases where we are out of memory.
	 */
	std::variant<std::string_view, std::string> error_message_;

public:
	error(error::code id, bool is_critical);
	error(error::code id, bool is_critical, const std::string_view &msg);
	error(error::code id, bool is_critical, std::string&& msg);
	error(error &&error);

	KEL_FORBID_COPY(error);

	error &operator=(error &&) = default;

	/**
	 * Returns a string view to the message
	 */
	const std::string_view get_message() const;

	/**
	 * Returns a string view which describes which error this is.
	 */
	const std::string_view get_category() const;

	/**
	 * Checks if it's an error at all, since 0 is reserved for no error.
	 */
	bool failed() const;

	/**
	 * Checks if the error is critical.
	 */
	bool is_critical() const;

	/**
	 * Checks if the error is recoverable.
	 */
	bool is_recoverable() const;

	/**
	 * Replaces the copy constructor. We need this since we want to explicitly copy and not implicitly
	 */
	error copy_error() const;

	/**
	 * Return the id.
	 */
	code get_id() const;

	/**
	 * Provide an error struct and check if it matches the type
	 */
	template<typename T>
	bool is_type() const;
};

/**
 * Class which is basically a variant. Replaces exceptions
 */
template<typename T>
class error_or;

namespace impl {

/**
 * Internal registry for all error types. Is generated dynamically.
 */
class error_registry final {
private:
	struct error_info {
		error_info() = delete;
		error_info(const std::string_view& d_, bool critical_):description{d_}, is_critical{critical_}{}

		std::string_view description;
		bool is_critical;
	};

	std::vector<error_info> infos_;
public:
	error_registry();

	KEL_FORBID_COPY(error_registry);
	KEL_FORBID_MOVE(error_registry);

	error_or<const std::string_view> search_category(const error::code& id) const;

	error_or<error::code> search_id(const std::string_view& desc) const;

	error_or<error::code> search_or_register_id(const std::string_view& desc, bool is_critical);

	bool is_critical(error::code cd) const;
};

error_registry& get_error_registry();

template<typename T>
error::code get_template_error_id(){
	static error::code id = std::numeric_limits<error::code>::max();

	if(id == std::numeric_limits<error::code>::max()){
		auto& reg = get_error_registry();

		auto err_or_id = reg.search_or_register_id(T::description, T::is_critical);
		if(err_or_id.is_error()){
			return std::numeric_limits<error::code>::max();
		}

		id = err_or_id.get_value();
	}

	return id;
}
}

error make_error(error::code code, const std::string_view &generic);

template<typename T> error make_error(const std::string_view& generic){
	error::code id = impl::get_template_error_id<T>();
	// Keep error, don't use make_error
	return error{id, T::is_critical, generic};
}

template<typename T> error make_error(){
	error::code id = impl::get_template_error_id<T>();
	// Keep error, don't use make_error
	return error{id, T::is_critical};
}

struct error_builder {
private:
	std::function<std::string()> func_;
public:
	template<typename Func>
	error_builder(Func&& func__):
		func_{std::move(func__)}
	{}

	template<typename T>
	error make_error(const std::string_view& generic){
		try{
			std::string err_str = func_();
			error::code id = impl::get_template_error_id<T>();
			func_ = [](){return "";};
			return error{id, T::is_critical, std::move(err_str)};
		}catch(const std::exception&){}
		return make_error<T>(generic);
	}
};

template<typename Func>
error_builder build_error(Func&& func){
	return error_builder{std::move(func)};
}

namespace err {
struct no_error {
	static constexpr std::string_view description = "No error has occured";
	static constexpr bool is_critical = false;
};

struct recoverable {
	static constexpr std::string_view description = "Generic recoverable error";
	static constexpr bool is_critical = false;
};

struct critical {
	static constexpr std::string_view description = "Generic critical error";
	static constexpr bool is_critical = true;
};

struct buffer_exhausted {
	static constexpr std::string_view description = "Buffer Exhausted";
	static constexpr bool is_critical = false;
};

struct not_found {
	static constexpr std::string_view description = "Not found";
	static constexpr bool is_critical = false;
};

struct out_of_memory {
	static constexpr std::string_view description = "Out of memory";
	static constexpr bool is_critical = true;
};

struct invalid_state {
	static constexpr std::string_view description = "Invalid state";
	static constexpr bool is_critical = true;
};

struct not_supported {
	static constexpr std::string_view description = "Not supported";
	static constexpr bool is_critical = false;
};

struct not_implemented {
	static constexpr std::string_view description = "Not implemented";
	static constexpr bool is_critical = true;
};

struct already_exists {
	static constexpr std::string_view description = "Already exists";
	static constexpr bool is_critical = false;
};
}

/**
 * Shorthand for no error happened
 */
error no_error();

/**
 * Exception alternative. Since I code without exceptions this class is
 * essentially a kind of exception replacement.
 */
template <typename T> class error_or;

class error_or_value {
public:
	virtual ~error_or_value() = default;

	template <typename T> error_or<unfix_void<T>> &as() {
		return static_cast<error_or<unfix_void<T>> &>(*this);
	}

	template <typename T> const error_or<unfix_void<T>> &as() const {
		return static_cast<const error_or<unfix_void<T>> &>(*this);
	}
};

template <typename T> class error_or final : public error_or_value {
private:
	std::variant<error, fix_void<T>> value_or_error_;

	static_assert(!std::is_same_v<T, void_t>,
				"Don't use internal private types");

public:
	constexpr error_or():value_or_error_{make_error<err::invalid_state>("Default assignement for error_or constructor.")}{}
	constexpr error_or(const fix_void<T> &value) : value_or_error_{value} {}

	constexpr error_or(fix_void<T> &&value) : value_or_error_{std::move(value)} {}

	constexpr error_or(const error &error) : value_or_error_{error} {}
	constexpr error_or(error &&error) : value_or_error_{std::move(error)} {}

	constexpr bool is_value() const {
		return std::holds_alternative<fix_void<T>>(value_or_error_);
	}

	constexpr bool is_error() const {
		return std::holds_alternative<class error>(value_or_error_);
	}

	constexpr class error &get_error() {
		return std::get<class error>(value_or_error_);
	}

	constexpr const class error &get_error() const {
		return std::get<class error>(value_or_error_);
	}

	constexpr fix_void<T> &get_value() { return std::get<fix_void<T>>(value_or_error_); }

	constexpr const fix_void<T> &get_value() const {
		return std::get<fix_void<T>>(value_or_error_);
	}
};

/**
 * This tries to catch cases where error starts including itself as a type which can happen in more complicated cases.
 * So this acts as a type safe guard.
 */
template <typename T> class error_or<error_or<T>> {
private:
	error_or() = delete;
};

template<typename T>
bool error::is_type() const {

	return error_code_ == impl::get_template_error_id<T>();
}

} // namespace saw

#define KEL_MAKE_ERROR(ErrType, ErrMsg) \
	return saw::make_error<ErrType>(ErrMsg)
