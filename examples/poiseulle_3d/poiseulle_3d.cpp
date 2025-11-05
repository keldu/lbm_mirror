#include <kel/lbm/lbm.hpp>

#include <forstio/remote/sycl/sycl.hpp>

namespace saw {
template<typename Desc, typename CellT, typename Encode>
class data<kel::lbm::sch::CellField<Desc, CellT>, encode::Sycl<Encode>> final {
public:
	using Schema = kel::lbm::sch::CellField<Desc,CellT>;
	using MetaSchema = typename meta_schema<Schema>::MetaSchema;
private:
	data<schema::Array<CellT,Desc::D>, encode::Sycl<Encode>> inner_;
public:
	data() = delete;
	data(const data<MetaSchema,Encode>& inner_meta__, acpp::sycl::queue& q):
		inner_{inner_meta__,q}
	{}

	const data<MetaSchema, Encode> meta() const {
		return inner_.dims();
	}

	template<uint64_t i>
	data<schema::UInt64,Encode> get_dim_size() const {
		static_assert(i < Desc::D, "Not enough dimensions");
		return inner_.template get_dim_size<i>();
	}

	const data<CellT>& operator()(const data<schema::FixedArray<schema::UInt64, Desc::D>, Encode>& index)const{
		return inner_.at(index);
	}

	data<CellT>& operator()(const data<schema::FixedArray<schema::UInt64, Desc::D>, Encode>& index){
		return inner_.at(index);
	}

	const data<CellT>& at(const data<schema::FixedArray<schema::UInt64, Desc::D>, Encode>& index)const{
		return inner_.at(index);
	}

	data<CellT>& at(const data<schema::FixedArray<schema::UInt64, Desc::D>, Encode>& index){
		return inner_.at(index);
	}

	ref<data<schema::Array<CellT,Desc::D>, encode::Sycl<Encode> >> inner_data() {
		return {inner_};
	}

	data<schema::UInt64,Encode> internal_size() const {
		return inner_.internal_size();
	}

	data<CellT,Encode>* internal_data() {
		return inner_.internal_data();
	}
};
/*
template<typename T, uint64_t D, typename Encode>
class data<schema::Ref<schema::Array<T, D>>, encode::Sycl<Encode>> final {
public:
	using Schema = schema::Ref<schema::Array<T, D>>;
private:
	data<schema::Ref<schema::Array<CellT,Desc::D>>, encode::Sycl<Encode>>> inner_ref_;
public:
	data() = delete;
	data(ref<data<kel::lbm::sch::CellField<Desc,CellT>,encode::Sycl<Encode>>> inner_ref__):
		inner_ref_{inner_ref__().inner_data()}
	{}

	const data<MetaSchema, Encode> meta() const {
		return inner_ref_().dims();
	}

	const data<CellT>& operator()(const data<schema::FixedArray<schema::UInt64, Desc::D>, Encode>& index)const{
		return inner_ref_().at(index);
	}

	data<CellT>& operator()(const data<schema::FixedArray<schema::UInt64, Desc::D>, Encode>& index){
		return inner_ref_().at(index);
	}

	const data<CellT>& at(const data<schema::FixedArray<schema::UInt64, Desc::D>, Encode>& index)const{
		return inner_ref_().at(index);
	}

	data<CellT>& at(const data<schema::FixedArray<schema::UInt64, Desc::D>, Encode>& index){
		return inner_ref_().at(index);
	}

	data<schema::UInt64,Encode> internal_size() const {
		return inner_ref_().internal_size();
	}

	data<CellT,Encode>* internal_data() {
		return inner_ref_().internal_data();
	}
};


template<typename Desc, typename CellT, typename Encode>
class data<schema::Ref<kel::lbm::sch::CellField<Desc, CellT>>, encode::Sycl<Encode>> final {
public:
	using Schema = schema::Ref<kel::lbm::sch::CellField<Desc,CellT>>;
private:
	data<schema::Ref<schema::Array<CellT,Desc::D>>, encode::Sycl<Encode>>> inner_ref_;
public:
	data() = delete;
	data(ref<data<kel::lbm::sch::CellField<Desc,CellT>,encode::Sycl<Encode>>> inner_ref__):
		inner_ref_{inner_ref__().inner_data()}
	{}

	const data<MetaSchema, Encode> meta() const {
		return inner_ref_().dims();
	}

	const data<CellT>& operator()(const data<schema::FixedArray<schema::UInt64, Desc::D>, Encode>& index)const{
		return inner_ref_().at(index);
	}

	data<CellT>& operator()(const data<schema::FixedArray<schema::UInt64, Desc::D>, Encode>& index){
		return inner_ref_().at(index);
	}

	const data<CellT>& at(const data<schema::FixedArray<schema::UInt64, Desc::D>, Encode>& index)const{
		return inner_ref_().at(index);
	}

	data<CellT>& at(const data<schema::FixedArray<schema::UInt64, Desc::D>, Encode>& index){
		return inner_ref_().at(index);
	}

	data<schema::UInt64,Encode> internal_size() const {
		return inner_ref_().internal_size();
	}

	data<CellT,Encode>* internal_data() {
		return inner_ref_().internal_data();
	}
};
*/

}

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

template<typename StructFieldSchema, typename Encode>
saw::error_or<void> set_geometry(saw::data<StructFieldSchema, Encode>& latt){
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

template<typename StructFieldSchema, typename Encode>
saw::error_or<void> set_initial_conditions(saw::data<StructFieldSchema, Encode>& latt){

	saw::data<sch::T> rho{1.0};
	saw::data<sch::FixedArray<sch::T,sch::Desc::D>> vel{};
	vel.at({0u}) = {0.1f};
	auto eq = equilibrium<sch::T,sch::Desc>(rho, vel);

	auto meta = latt.meta();

	/**
	 * Set distribution
	 */
	iterator<sch::Desc::D>::apply([&](const saw::data<sch::FixedArray<sch::UInt64,sch::Desc::D>>& index){
		auto& cell = latt.at(index);
		auto& dfs = cell.template get<"dfs">();
		auto& dfs_old = cell.template get<"dfs_old">();

		for(saw::data<sch::UInt64> k = 0; k < saw::data<sch::UInt64>{sch::Desc::Q}; ++k){
			dfs(k) = eq.at(k);
			dfs_old(k) = eq.at(k);
		}
	}, {}, meta);
	return saw::make_void();
}

template<typename StructFieldSchema, typename Encode>
saw::error_or<void> lbm_step(
	saw::data<StructFieldSchema, Encode>& latt,
	saw::data<sch::UInt64> time_step,
	acpp::sycl::queue& sycl_q
){
	using dfi = df_info<sch::T,sch::Desc>;
	auto meta = latt.meta();
	bool even_step = ((time_step.get() % 2u) == 0u);

	component<sch::T, sch::Desc, cmpt::BGK, saw::encode::Sycl<saw::encode::Native>> coll{0.5384};
	component<sch::T, sch::Desc, cmpt::BounceBack, saw::encode::Sycl<saw::encode::Native>> bb;

	/**
	 * Collision
	 */
	iterator<sch::Desc::D>::apply([&](const saw::data<sch::FixedArray<sch::UInt64,sch::Desc::D>>& index){
		auto& cell = latt.at(index);
		auto& info = cell.template get<"info">();

		switch(info({0u}).get()){
			case 1u: {
				bb.apply(latt, index, time_step);
				break;
			}
			case 2u: {
				coll.apply(latt, index, time_step);
				break;
			}
			case 3u: {
				//inlet.apply(latt, index, time_step);
				break;
			}
			case 4u: {
				//outlet.apply(latt, index, time_step);
				break;
			}
			default:
				break;
		}

	}, {{0u,0u}}, meta);

	// Stream
	iterator<sch::Desc::D>::apply([&](const saw::data<sch::FixedArray<sch::UInt64,sch::Desc::D>>& index){
		auto& cell = latt(index);
		auto& df_new = even_step ? cell.template get<"dfs">() : cell.template get<"dfs_old">();
		auto& info_new = cell.template get<"info">();

		if(info_new({0u}).get() > 0u){
			for(uint64_t k = 0u; k < sch::Desc::Q; ++k){
				auto dir = dfi::directions[dfi::opposite_index[k]];
				auto index_dir = index;
				for(uint64_t i = 0; i < sch::Desc::D; ++i){
					index_dir.at({i}) = index_dir.at({i}) + dir[i];
				}
				auto& cell_dir_old = latt.at(index_dir);

				auto& df_old = even_step ? cell_dir_old.template get<"dfs_old">() : cell_dir_old.template get<"dfs">();

				df_new({k}) = df_old({k});
			}
		}
	}, {{0u,0u}}, meta);
	return saw::make_void();
}

saw::error_or<void> real_main(int argc, char** argv){
	using dfi = df_info<sch::T,sch::Desc>;

	converter<sch::T> conv {
		{0.1f},
		{0.1f}
	};

	print_lbm_meta<sch::T,sch::Desc>(conv, {1e-3f});
	acpp::sycl::queue sycl_q;

	/**
	 * Set the meta and initialize the lattice
	 */
	saw::data<sch::FixedArray<sch::UInt64,sch::Desc::D>> meta{{2048u,128u,128u}};
	saw::data<sch::CellField<sch::Desc,sch::CellStruct>, saw::encode::Sycl<saw::encode::Native>> lattice{meta,sycl_q};

	/**
	 * Set the geometry
	 */
	{
		auto eov = set_geometry(lattice);
		if(eov.is_error()){
			return eov;
		}
	}

	/**
	 * Set initial conditions in the simulations
	 */
	{
	auto eov = set_initial_conditions(lattice);
		if(eov.is_error()){
			return eov;
		}
	}

	for(saw::data<sch::UInt64> ts{0u}; ts < saw::data<sch::UInt64>{1024u*32u}; ++ts){
		auto eov = lbm_step(lattice,ts,sycl_q);
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
