#pragma once

#include <kel/lbm/swarm/capability.hpp>
#include <kel/lbm/abstract/data.hpp>

#include <string>
#include <thread>

#if defined(__linux__)
#include <unistd.h>
#endif

namespace kel {
namespace lbm {
namespace swarm {

/**
 * What a native worker can honestly say about itself.
 *
 * Deliberately short. A browser worker has to measure its texture budget
 * because WebGL will not tell it, but a process can simply ask the OS, and
 * there is no point inventing numbers beyond that until there is a scheduler
 * that would use them.
 */
inline data<sch::NativeCapability> probe_native_capability(){
	data<sch::NativeCapability> cap;

	unsigned threads = std::thread::hardware_concurrency();
	if(threads == 0u){
		// hardware_concurrency is allowed to give up; one thread is the
		// honest floor rather than zero, which would read as "no capacity".
		threads = 1u;
	}
	cap.get<"threads">().set(threads);

	uint64_t memory_bytes = 0u;
#if defined(__linux__)
	const long pages = ::sysconf(_SC_PHYS_PAGES);
	const long page_size = ::sysconf(_SC_PAGE_SIZE);
	if(pages > 0 and page_size > 0){
		memory_bytes = static_cast<uint64_t>(pages) * static_cast<uint64_t>(page_size);
	}
#endif
	cap.get<"memory_bytes">().set(memory_bytes);

#if defined(__x86_64__)
	cap.get<"cpu">().set(std::string{"x86_64"});
#elif defined(__aarch64__)
	cap.get<"cpu">().set(std::string{"aarch64"});
#else
	cap.get<"cpu">().set(std::string{"unknown"});
#endif

	// Nothing here links SYCL yet. Reported so the orchestrator can start
	// distinguishing compute capable workers without a protocol change.
	cap.get<"has_sycl">().set(false);

	return cap;
}

} // namespace swarm
} // namespace lbm
} // namespace kel
