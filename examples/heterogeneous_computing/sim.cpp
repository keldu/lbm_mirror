#include <forstio/error.hpp>
#include <iostream>

#include <kel/lbm/lbm.hpp>

namespace kel {
namespace lbm {
namespace sch {
using namespace saw::schema;

/**
 * struct lbm_data {
 *   std::array<std::array<float,9>, 64u*64u> dfs;
 *   std::array<uint8_t, 64u*64u> info;
 * };
 *
 * which leads to the form
 *
 * template<uint64_t Dim, uint64_t Size>
 * struct lbm_data {
 *   std::array<std::array<float,9>, Size*Size> dfs;
 *   std::array<uint8_t, Size*Size> info;
 * };
 *
 * which transferred into sycl requires us to go to
 *
 * template<uint64_t Dim, uint64_t Size>
 * struct lbm_sycl_data {
 *   std::array<float,9>* dfs;
 *   uint8_t* info;
 * };
 *
 * in data form on host
 *
 * template<uint64_t Dim, uint64_t Size>
 * using LbmData = Struct<
 *  Member<FixedArray<FixedArray<Float32,9u>, Size*Size>, "dfs">,
 *  Member<FixedArray<UInt8, Size*Size>, "info">
 * >;
 *
 * If we specialize the encode::Sycl<Encoding> data type, then we get
 * With a helper class we can copy single values back and forth and the whole block is guaranteed
 * to be allocated. And this one can be dynamic while the host definition might be compile time.
 *
 * template<...>
 * class data<LbmData<...>,encode::Sycl<Encoding>> final {
 *   saw::data<sch::FixedArray<sch::UInt64,Dim>> meta;
 *   saw::data<FixedArray<Float32u,9>>* dfs;
 *   saw::data<UInt8>* info;
 * };
 */

template<typename T, typename Desc>
using CellStruct = Struct<
	Member<Chunk<FixedArray<T,Desc::Q>,Desc::D, "dfs">,
	Member<FixedArray<T,Desc::Q>, "dfs_old">,
	Member<UInt8, "info">
>;

}
template<typename T, typename Desc>
saw::error_or<void> simulate(int argc, char** argv,
	const saw::data<sch::FixedArray<sch::UInt64,Desc::D>>& meta
{
	constexpr auto cell_size = sizeof(saw::data<sch::CellStruct<T,Desc>>);

	auto lbm_data = saw::heap<saw::data<sch::Chunk<CellStruct<>,Desc::D, 64u>>>{meta};

	return saw::make_void();
}

}

saw::error_or<void> lbm_main(int argc, char** argv){
	using namespace lbm;
	return simulate<sch::Float64,sch::D2Q9>(argc, argv);
}
}

int main(int argc, char** argv){
	auto eov = kel::lbm_main(argc, argv);
	if(eov.is_error()){
		auto& err = eov.get_error();
		auto err_msg = err.get_message();
		std::cerr<<"[Error]: "<<err.get_category();

		if(not err_msg.empty()){
			std::cerr<<" - "<<err_msg;
		}
		std::cerr<<std::endl;

		return err.get_id();
	}

	return 0;
}
