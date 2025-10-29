#include <kel/lbm/lbm.hpp>

#include <forstio/remote/sycl/sycl.hpp>

namespace kel {
namespace lbm {
namespace sch {
using namespace saw::schema;
using T = Float32;
using Desc = Descriptor<3u,27u>;

using DfCell = Cell<T, Desc, 0u, 0u, 1u>;

using CellInfo = Cell<UInt8, Desc, 1u, 0u, 0u>;

using CellStruct = Struct<
	Member<DfCell, "dfs">,
	Member<DfCell, "dfs_old">,
	//Member<CellForceField<Desc>, "force">,
	Member<CellInfo, "info">
>;
}

template<typename StructFieldSchema>
saw::error_or<void> set_geometry(saw::data<StructFieldSchema>& latt){
	using namespace kel::lbm;

	auto meta = latt.meta();

	/**
	 * Set ghost
	 */
	iterator<sch::Desc::D>::apply([&](const saw::data<sch::FixedArray<sch::UInt64,sch::Desc::D>>& index){
		auto& cell = latt.at(index);
		auto& info = cell.template get<"info">();

		info({0u}).set(0u);
	}, {{0u,0u}}, meta);

	saw::data<sch::FixedArray<sch::UInt64,sch::Desc::D>> distance;

	/**
	 * Set wall
	 */
	iterator<1u>::apply([&](const saw::data<sch::FixedArray<sch::UInt64,1u>>& index){
		distance.at(index.at({0u})) = {1u};
	},{{0u}},{{sch::Desc::D}});
	/*
	 */
	iterator<sch::Desc::D>::apply([&](const saw::data<sch::FixedArray<sch::UInt64,sch::Desc::D>>& index){
		auto& cell = latt.at(index);
		auto& info = cell.template get<"info">();

		info({0u}).set(2u);
	}, {}, meta, distance);

	/**
	 * Set fluid
	 */
	iterator<1u>::apply([&](const saw::data<sch::FixedArray<sch::UInt64,1u>>& index){
		distance.at(index.at({0u})) = {2u};
	},{{0u}},{{sch::Desc::D}});
	/*
	 */
	iterator<sch::Desc::D>::apply([&](const saw::data<sch::FixedArray<sch::UInt64,sch::Desc::D>>& index){
		auto& cell = latt.at(index);
		auto& info = cell.template get<"info">();

		info({0u}).set(1u);

	}, {}, meta, distance);

	/**
	 * Set inflow
	 */

	saw::data<sch::FixedArray<sch::UInt64,sch::Desc::D>> shifted_point;
	iterator<1u>::apply([&](const saw::data<sch::FixedArray<sch::UInt64,1u>>& index){
		distance.at(index.at({0u})) = (index.at({0u}).get() == 0u)?1u:2u;
		shifted_point.at(index.at({0u})) = (index.at({0u}).get() == 0u)?3u:meta.at({index.at({0u})});
	},{{0u}},{{sch::Desc::D}});
	/*
	 */
	iterator<sch::Desc::D>::apply([&](const saw::data<sch::FixedArray<sch::UInt64,sch::Desc::D>>& index){
		auto& cell = latt.at(index);
		auto& info = cell.template get<"info">();

		info({0u}).set(3u);

	}, {}, shifted_point, distance);

	/**
	 * Set outflow
	 */
	iterator<1u>::apply([&](const saw::data<sch::FixedArray<sch::UInt64,1u>>& index){
		distance.at(index.at({0u})) = (index.at({0u}).get() == 0u)?1u:2u;
		shifted_point.at(index.at({0u})) = (index.at({0u}).get() == 0u)?(meta.at({0u})-3u):0u;
	},{{0u}},{{sch::Desc::D}});
	/*
	 */
	iterator<sch::Desc::D>::apply([&](const saw::data<sch::FixedArray<sch::UInt64,sch::Desc::D>>& index){
		auto& cell = latt.at(index);
		auto& info = cell.template get<"info">();

		info({0u}).set(4u);

	}, shifted_point, meta, distance);

	return saw::make_void();
}

saw::error_or<void> set_initial_conditions(saw::data<StructFieldSchema>& latt){
	return saw::make_void();
}

saw::error_or<void> lbm_step(saw::data<StructFieldSchema>& latt){
	return saw::make_void();
}

saw::error_or<void> real_main(int argc, char** argv){
	using dfi = df_info<sch::T,sch::Desc>;

	converter<sch::T> conv {
		{0.1f},
		{0.1f}
	};

	print_lbm_meta<sch::T,sch::Desc>(conv, {1e-3f});

	/**
	 * Set the meta and initialize the lattice
	 */
	saw::data<sch::FixedArray<sch::UInt64,sch::Desc::D>> meta{{2048u,128u,128u}};
	saw::data<sch::CellField<sch::Desc,sch::CellStruct>, encode::Sycl<encode::Native>> lattice{meta};

	/**
	 * Set the geometry
	 */
	auto eov = set_geometry(lattice);
	if(eov.is_error()){
		return eov;
	}

	/**
	 * Set initial conditions in the simulations
	 */
	auto eov = set_initial_conditions(lattice);
	if(eov.is_error()){
		return eov;
	}

	for(uint64_t i = 0u; i < 1024u*32; ++i){
		auto eov = lbm_step(lattice);
		if(eov.is_error()){
			return eov;
		}
	}

	return saw::make_void();
}

} // lbm
} // kel

/**
 * main, but I don't like the error handling
 */
int main(int argc, char** argv){
	auto eov = kel::lbm::real_main(argc, argv);
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
