#pragma once

#include <exception>
#include <iostream>
#include <string>

/**
 * Skip support for device tests.
 *
 * forstio's suite only has pass and fail: SAW_EXPECT throws and the case is
 * reported as a failure. That is the wrong verdict for a device test on a
 * machine with no such device -- "this backend is absent" is not the same
 * finding as "this backend is broken", and conflating them means a red suite
 * that everyone learns to ignore.
 *
 * These let a case attempt the work and bow out cleanly instead. A skip is
 * printed so it stays visible rather than looking like a silent pass.
 */

#define KEL_SKIP(reason)                                                       \
	do {                                                                       \
		std::cout << "[ SKIP ] " << __FILE__ << ":" << __LINE__ << ": "        \
			<< (reason) << std::endl;                                          \
		return;                                                                \
	} while(false)

#define KEL_SKIP_IF(cond, reason)                                              \
	do {                                                                       \
		if((cond)){                                                            \
			KEL_SKIP(reason);                                                   \
		}                                                                      \
	} while(false)

/**
 * Runs `expr`, skipping the case if it throws.
 *
 * Device construction is the usual culprit: a missing runtime or an
 * unavailable backend surfaces as an exception out of the queue constructor,
 * long before any of the behaviour under test could be wrong.
 */
#define KEL_SKIP_ON_THROW(expr, reason)                                        \
	do {                                                                       \
		try {                                                                  \
			(void)(expr);                                                      \
		} catch(const std::exception& e){                                      \
			std::cout << "[ SKIP ] " << __FILE__ << ":" << __LINE__ << ": "    \
				<< (reason) << " (" << e.what() << ")" << std::endl;           \
			return;                                                            \
		} catch(...){                                                          \
			std::cout << "[ SKIP ] " << __FILE__ << ":" << __LINE__ << ": "    \
				<< (reason) << std::endl;                                      \
			return;                                                            \
		}                                                                      \
	} while(false)

namespace kel {
namespace lbm {
namespace test {

/**
 * True when a default queue can be constructed, i.e. there is some backend to
 * run on at all. Cached, because probing is not free and the answer cannot
 * change within a run.
 */
inline bool sycl_available(){
	static const bool ok = [](){
		try {
			acpp::sycl::queue q;
			// Touching the device is what actually proves a backend resolved;
			// constructing the queue alone can succeed lazily.
			(void) q.get_device();
			return true;
		} catch(...){
			return false;
		}
	}();
	return ok;
}

}
}
}

/**
 * Guard for a whole device test case.
 */
#define KEL_REQUIRE_SYCL()                                                     \
	KEL_SKIP_IF(!::kel::lbm::test::sycl_available(),                           \
		"no usable SYCL device on this machine")
