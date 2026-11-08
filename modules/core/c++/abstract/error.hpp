#pragma once

#include <algorithm>
#include <limits>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include <cassert>

#include "common.hpp"

namespace kel {
namespace lbm {
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
	inline error(error::code id, bool is_critical);
	inline error(error::code id, bool is_critical, const std::string_view &msg);
	inline error(error::code id, bool is_critical, std::string&& msg);
	inline error(error &&error);

	KEL_FORBID_COPY(error);

	error &operator=(error &&) = default;

	/**
	 * Returns a string view to the message
	 */
	inline const std::string_view get_message() const;

	/**
	 * Returns a string view which describes which error this is.
	 */
	inline const std::string_view get_category() const;

	/**
	 * Checks if it's an error at all, since 0 is reserved for no error.
	 */
	inline bool failed() const;

	/**
	 * Checks if the error is critical.
	 */
	inline bool is_critical() const;

	/**
	 * Checks if the error is recoverable.
	 */
	inline bool is_recoverable() const;

	/**
	 * Replaces the copy constructor. We need this since we want to explicitly copy and not implicitly
	 */
	inline error copy_error() const;

	/**
	 * Return the id.
	 */
	inline code get_id() const;

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
	inline error_registry();

	KEL_FORBID_COPY(error_registry);
	KEL_FORBID_MOVE(error_registry);

	inline error_or<const std::string_view> search_category(const error::code& id) const;

	inline error_or<error::code> search_id(const std::string_view& desc) const;

	inline error_or<error::code> search_or_register_id(const std::string_view& desc, bool is_critical);

	inline bool is_critical(error::code cd) const;
};

inline error_registry& get_error_registry();

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

inline error make_error(error::code code, const std::string_view &generic);

template<typename T> error make_error(const std::string_view& generic){
	error::code id = impl::get_template_error_id<T>();
	// Keep error, don't use make_error
	return error{id, T::is_critical, generic};
}

/**
 * Overload taking ownership of the message.
 *
 * The string_view overload above stores the view, not a copy, so it is only
 * safe for messages that outlive the error -- string literals, in practice.
 * Use this one whenever the text is built at runtime or borrowed from
 * something about to go away.
 */
template<typename T, typename S>
requires std::is_same_v<std::remove_cvref_t<S>, std::string>
error make_error(S&& generic){
	error::code id = impl::get_template_error_id<T>();
	// Keep error, don't use make_error
	return error{id, T::is_critical, std::string{std::forward<S>(generic)}};
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
		return lbm::make_error<T>(generic);
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
inline error no_error();

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
 * So this acts as a type safe guard in case internal handling fails to treat conversion correctly.
 */
template <typename T> class error_or<error_or<T>> {
private:
	error_or() = delete;
};

template<typename T>
bool error::is_type() const {

	return error_code_ == impl::get_template_error_id<T>();
}

/**
 * Implementation.
 *
 * These live in the header and are inline, because lbm's core is header only:
 * the SConscript globs cpp files directly under c++ only, so a translation
 * unit placed under abstract would never be handed to the compiler.
 */

inline error::error(error::code code_, bool is_critical__)
	: error_code_{static_cast<error::code>(code_)}, is_critical_{is_critical__} {}

inline error::error(error::code code_, bool is_critical__, const std::string_view &msg)
	:
	error_code_{static_cast<error::code>(code_)}
	, is_critical_{is_critical__}, error_message_{msg}{}

inline error::error(error::code code_, bool is_critical__, std::string&& msg):
	error_code_{static_cast<error::code>(code_)},
	is_critical_{is_critical__},
	error_message_{std::move(msg)}
{}

inline error::error(error &&error)
	:
	error_code_{std::move(error.error_code_)}
	, is_critical_{std::move(error.is_critical_)}
	, error_message_{std::move(error.error_message_)}{}

inline const std::string_view error::get_category() const {
	auto& reg = impl::get_error_registry();

	auto eov = reg.search_category(error_code_);
	KEL_ASSERT(eov.is_value()){
		return "Error category not found. Report this error to the lbm maintainer";
	}

	return eov.get_value();
}

inline const std::string_view error::get_message() const {
	return std::visit(
		[](auto &&arg) -> const std::string_view {
			using T = std::decay_t<decltype(arg)>;

			if constexpr (std::is_same_v<T, std::string>) {
				return std::string_view{arg};
			} else if constexpr (std::is_same_v<T, std::string_view>) {
				return arg;
			} else {
				return "Error in class Error. Good luck :)";
			}
		},
		error_message_);
}

inline bool error::failed() const {
	return !this->is_type<err::no_error>();
}

inline bool error::is_critical() const {
	return is_critical_;
}

inline bool error::is_recoverable() const {
	return !is_critical_;
}

inline error error::copy_error() const {
	auto copy_error_code = error_code_;
	error error{copy_error_code, is_critical_};

	try {
		error.error_message_ = error_message_;
	} catch (const std::bad_alloc &) {
		error.error_message_ =
			std::string_view{"Error while copying Error string. Out of memory"};
	}

	return error;
}

inline error::code error::get_id() const { return error_code_; }

namespace impl {
inline error_registry& get_error_registry() {
	static own<error_registry> reg = nullptr;
	if(!reg){
		reg = heap<error_registry>();
	}

	assert(reg);
	return *reg;
}
}

inline error no_error(){
	return make_error<err::no_error>();
}

namespace impl {
inline error_registry::error_registry():
	infos_{
		{
			err::no_error::description,
			err::no_error::is_critical
		},
		{
			err::not_found::description,
			err::not_found::is_critical
		},
		{
			err::out_of_memory::description,
			err::out_of_memory::is_critical
		}
	}
{}

inline error_or<const std::string_view> error_registry::search_category(const error::code& id) const {
	if( id >= infos_.size()){
		return make_error<err::not_found>();
	}

	return infos_.at(id).description;
}

inline error_or<error::code> error_registry::search_id(const std::string_view& desc)const{
	/**
	 * Search the index in the vector
	 */
	size_t i{};
	size_t info_max_size = std::min<std::size_t>(infos_.size(), std::numeric_limits<error::code>::max());
	for(i = 0; i < info_max_size; ++i){
		if(infos_.at(i).description == desc){
			break;
		}
	}

	if(i == info_max_size){
		return make_error<err::not_found>();
	}

	return static_cast<error::code>(i);
}

inline error_or<error::code> error_registry::search_or_register_id(const std::string_view& desc, bool is_critical){
	auto err_or_id = search_id(desc);

	if(err_or_id.is_value()){
		return err_or_id.get_value();
	}

	auto& err = err_or_id.get_error();

	if(err.is_type<err::not_found>()){
		size_t new_index = infos_.size();
		if(new_index == std::numeric_limits<error::code>::max()){
			return make_error<err::out_of_memory>("Error registry ids are exhausted");
		}
		infos_.emplace_back(error_info{desc, is_critical});
		return static_cast<error::code>(new_index);
	}

	return std::move(err);
}

inline bool error_registry::is_critical(error::code cd) const {
	return infos_.at(cd).is_critical;
}
}

inline error make_error(error::code cd, const std::string_view& generic){
	auto& reg = impl::get_error_registry();
	return error{cd, reg.is_critical(cd), generic};
}

} // namespace lbm
} // namespace kel

#define KEL_MAKE_ERROR(ErrType, ErrMsg) \
	return ::kel::lbm::make_error<ErrType>(ErrMsg)
