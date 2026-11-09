#include <forstio/test/suite.hpp>

#include <cmath>
#include <string>

#include "../c++/lbm.hpp"
#include "skip.hpp"

/**
 * Minimal confirmation that the device layer actually works, rather than
 * merely compiles.
 *
 * Everything else in the suite is host side, so a device allocation that never
 * happened, a copy that moved nothing, or a kernel that silently did not run
 * would all look exactly like success. Each test here therefore round trips a
 * value: write it somewhere, read it back from the other side, compare.
 */

namespace {
namespace sch {
using namespace kel::lbm::sch;
using T = Float64;
}

using kel::lbm::data;
using Dev = kel::lbm::encode::Sycl<kel::lbm::enc::Native>;

SAW_TEST("Sycl / device allocation and FixedArray round trip"){
	using namespace kel;

	KEL_REQUIRE_SYCL();

	acpp::sycl::queue q;
	lbm::device dev;

	using Sch = sch::FixedArray<sch::UInt64, 4u>;

	data<Sch> host;
	for(uint64_t i = 0u; i < 4u; ++i){
		host.at({i}).set(i * 10u + 1u);
	}

	data<Sch, Dev> device_data{dev.get_handle()};

	auto eov = dev.copy_to_device(host, device_data);
	SAW_EXPECT(eov.is_value(), "copy to device failed");

	// Clear the host copy so a no-op read back cannot pass by accident.
	data<Sch> back;
	for(uint64_t i = 0u; i < 4u; ++i){
		back.at({i}).set(0u);
	}

	auto eov2 = dev.copy_to_host(device_data, back);
	SAW_EXPECT(eov2.is_value(), "copy to host failed");
	dev.get_handle().wait();

	for(uint64_t i = 0u; i < 4u; ++i){
		SAW_EXPECT(back.at({i}).get() == i * 10u + 1u,
			std::string{"round trip lost element "} + std::to_string(i)
			+ ": " + std::to_string(back.at({i}).get()));
	}
}

SAW_TEST("Sycl / a kernel actually runs and writes"){
	using namespace kel;

	KEL_REQUIRE_SYCL();

	/**
	 * The one that catches a kernel which never launched: the host never
	 * writes these values, so they can only come from the device.
	 */
	lbm::device dev;
	auto& q = dev.get_handle();

	using Sch = sch::FixedArray<sch::UInt64, 8u>;

	data<Sch> host;
	for(uint64_t i = 0u; i < 8u; ++i){
		host.at({i}).set(0u);
	}

	data<Sch, Dev> device_data{q};
	auto eov = dev.copy_to_device(host, device_data);
	SAW_EXPECT(eov.is_value(), "copy to device failed");
	q.wait();

	data<sch::Ptr<Sch>, Dev> view{device_data};

	q.submit([&](acpp::sycl::handler& h){
		h.parallel_for(acpp::sycl::range<1u>{8u}, [=](acpp::sycl::id<1u> idx){
			data<sch::FixedArray<sch::UInt64,1u>> index;
			index.at({{0u}}).set(idx[0]);
			view.at(index).set(idx[0] * 7u + 3u);
		});
	}).wait();

	auto eov2 = dev.copy_to_host(device_data, host);
	SAW_EXPECT(eov2.is_value(), "copy to host failed");
	q.wait();

	for(uint64_t i = 0u; i < 8u; ++i){
		SAW_EXPECT(host.at({i}).get() == i * 7u + 3u,
			std::string{"kernel did not write element "} + std::to_string(i)
			+ ": got " + std::to_string(host.at({i}).get()));
	}
}

SAW_TEST("Sycl / dynamic Array round trips with its extent"){
	using namespace kel;

	KEL_REQUIRE_SYCL();

	// Array carries a runtime extent, so the device side has to pick it up
	// rather than assume one.
	lbm::device dev;
	auto& q = dev.get_handle();

	using Sch = sch::Array<sch::Float64, 1u>;

	data<Sch> host{data<sch::FixedArray<sch::UInt64,1u>>{{6u}}};
	SAW_EXPECT(host.size().get() == 6u, "host array should be sized from its meta");
	for(uint64_t i = 0u; i < 6u; ++i){
		host.at({i}).set(static_cast<double>(i) * 0.5);
	}

	data<Sch, Dev> device_data{q};
	auto eov = dev.copy_to_device(host, device_data);
	SAW_EXPECT(eov.is_value(), "copy to device failed");
	q.wait();

	data<Sch> back{data<sch::FixedArray<sch::UInt64,1u>>{{6u}}};
	for(uint64_t i = 0u; i < 6u; ++i){
		back.at({i}).set(-1.0);
	}

	auto eov2 = dev.copy_to_host(device_data, back);
	SAW_EXPECT(eov2.is_value(), "copy to host failed");
	q.wait();

	for(uint64_t i = 0u; i < 6u; ++i){
		SAW_EXPECT(back.at({i}).get() == static_cast<double>(i) * 0.5,
			std::string{"array round trip lost element "} + std::to_string(i));
	}
}

SAW_TEST("Sycl / Struct members round trip independently"){
	using namespace kel;

	KEL_REQUIRE_SYCL();

	/**
	 * A struct copy walks member by member. If it reused one member's pointer
	 * or skipped one, the others would still look right -- so check each.
	 */
	lbm::device dev;
	auto& q = dev.get_handle();

	using Sch = sch::Struct<
		sch::Member<sch::FixedArray<sch::UInt64,3u>, "counts">,
		sch::Member<sch::FixedArray<sch::Float64,3u>, "weights">
	>;

	data<Sch> host;
	for(uint64_t i = 0u; i < 3u; ++i){
		host.template get<"counts">().at({i}).set(i + 100u);
		host.template get<"weights">().at({i}).set(static_cast<double>(i) + 0.25);
	}

	data<Sch, Dev> device_data{q};
	auto eov = dev.copy_to_device(host, device_data);
	SAW_EXPECT(eov.is_value(), "copy to device failed");
	q.wait();

	data<Sch> back;
	for(uint64_t i = 0u; i < 3u; ++i){
		back.template get<"counts">().at({i}).set(0u);
		back.template get<"weights">().at({i}).set(0.0);
	}

	auto eov2 = dev.copy_to_host(device_data, back);
	SAW_EXPECT(eov2.is_value(), "copy to host failed");
	q.wait();

	for(uint64_t i = 0u; i < 3u; ++i){
		SAW_EXPECT(back.template get<"counts">().at({i}).get() == i + 100u,
			std::string{"counts lost element "} + std::to_string(i));
		SAW_EXPECT(back.template get<"weights">().at({i}).get()
				== static_cast<double>(i) + 0.25,
			std::string{"weights lost element "} + std::to_string(i));
	}
}

SAW_TEST("Sycl / Chunk round trips and is addressable in a kernel"){
	using namespace kel;

	KEL_REQUIRE_SYCL();

	// Chunk is the type the whole fluid solver runs on, so it is the one that
	// matters most.
	lbm::device dev;
	auto& q = dev.get_handle();

	constexpr uint64_t nx = 4u;
	constexpr uint64_t ny = 3u;
	using Sch = sch::Chunk<sch::UInt64, 0u, nx, ny>;

	data<Sch> host;
	lbm::iterator<2u>::apply([&](const auto& index){
		host.at(index).set(0u);
	},{},host.get_dims());

	data<Sch, Dev> device_data{q};
	auto eov = dev.copy_to_device(host, device_data);
	SAW_EXPECT(eov.is_value(), "copy to device failed");
	q.wait();

	data<sch::Ptr<Sch>, Dev> view{device_data};

	// Write a position dependent value so a transposed or flattened index
	// shows up as a mismatch rather than a plausible constant.
	q.submit([&](acpp::sycl::handler& h){
		h.parallel_for(acpp::sycl::range<2u>{nx,ny}, [=](acpp::sycl::id<2u> idx){
			data<sch::FixedArray<sch::UInt64,2u>> index;
			index.at({0u}).set(idx[0]);
			index.at({1u}).set(idx[1]);
			view.at(index).set(idx[0] * 1000u + idx[1]);
		});
	}).wait();

	auto eov2 = dev.copy_to_host(device_data, host);
	SAW_EXPECT(eov2.is_value(), "copy to host failed");
	q.wait();

	uint64_t checked = 0u;
	lbm::iterator<2u>::apply([&](const auto& index){
		auto x = index.at({0u}).get();
		auto y = index.at({1u}).get();
		SAW_EXPECT(host.at(index).get() == x * 1000u + y,
			std::string{"chunk cell ("} + std::to_string(x) + "," + std::to_string(y)
			+ ") = " + std::to_string(host.at(index).get()));
		++checked;
	},{},host.get_dims());

	SAW_EXPECT(checked == nx * ny, "expected to visit every cell");
}

SAW_TEST("Sycl / make_view reaches struct members on device"){
	using namespace kel;

	KEL_REQUIRE_SYCL();

	/**
	 * make_view recurses into a struct, building a borrowed view per member.
	 * Confirm a kernel reached through it writes the member it was aimed at.
	 */
	lbm::device dev;
	auto& q = dev.get_handle();

	using Sch = sch::Struct<
		sch::Member<sch::FixedArray<sch::UInt64,4u>, "a">,
		sch::Member<sch::FixedArray<sch::UInt64,4u>, "b">
	>;

	data<Sch> host;
	for(uint64_t i = 0u; i < 4u; ++i){
		host.template get<"a">().at({i}).set(0u);
		host.template get<"b">().at({i}).set(9u);
	}

	data<Sch, Dev> device_data{q};
	SAW_EXPECT(dev.copy_to_device(host, device_data).is_value(), "copy to device failed");
	q.wait();

	auto view = lbm::make_view(device_data);

	q.submit([&](acpp::sycl::handler& h){
		h.parallel_for(acpp::sycl::range<1u>{4u}, [=](acpp::sycl::id<1u> idx){
			data<sch::FixedArray<sch::UInt64,1u>> index;
			index.at({{0u}}).set(idx[0]);
			view.template get<"a">().at(index).set(idx[0] + 50u);
		});
	}).wait();

	SAW_EXPECT(dev.copy_to_host(device_data, host).is_value(), "copy to host failed");
	q.wait();

	for(uint64_t i = 0u; i < 4u; ++i){
		SAW_EXPECT(host.template get<"a">().at({i}).get() == i + 50u,
			std::string{"member a not written at "} + std::to_string(i));
		// The untouched member must be exactly as it was.
		SAW_EXPECT(host.template get<"b">().at({i}).get() == 9u,
			std::string{"member b was clobbered at "} + std::to_string(i));
	}
}

SAW_TEST("Sycl / arithmetic on data inside a kernel"){
	using namespace kel;

	KEL_REQUIRE_SYCL();

	/**
	 * The data class carries operators; confirm they behave on device rather
	 * than only in host tests.
	 */
	lbm::device dev;
	auto& q = dev.get_handle();

	using Sch = sch::FixedArray<sch::Float64, 5u>;

	data<Sch> host;
	for(uint64_t i = 0u; i < 5u; ++i){
		host.at({i}).set(static_cast<double>(i));
	}

	data<Sch, Dev> device_data{q};
	SAW_EXPECT(dev.copy_to_device(host, device_data).is_value(), "copy to device failed");
	q.wait();

	data<sch::Ptr<Sch>, Dev> view{device_data};

	q.submit([&](acpp::sycl::handler& h){
		h.parallel_for(acpp::sycl::range<1u>{5u}, [=](acpp::sycl::id<1u> idx){
			data<sch::FixedArray<sch::UInt64,1u>> index;
			index.at({{0u}}).set(idx[0]);

			auto& v = view.at(index);
			data<sch::Float64> two;
			two.set(2.0);
			data<sch::Float64> one;
			one.set(1.0);

			v = v * two + one;
		});
	}).wait();

	SAW_EXPECT(dev.copy_to_host(device_data, host).is_value(), "copy to host failed");
	q.wait();

	for(uint64_t i = 0u; i < 5u; ++i){
		auto expect = static_cast<double>(i) * 2.0 + 1.0;
		SAW_EXPECT(host.at({i}).get() == expect,
			std::string{"arithmetic wrong at "} + std::to_string(i)
			+ ": " + std::to_string(host.at({i}).get())
			+ " expected " + std::to_string(expect));
	}
}
}
