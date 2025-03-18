#pragma once

#include <forstio/codec/data.hpp>

namespace kel {
namespace lbm {
namespace sch {
using namespace saw::schema;

template<uint64_t DV, uint64_t QV>
struct Descriptor {
	static constexpr uint64_t D = DV;
	static constexpr uint64_t Q = QV;
};

template<typename Sch, typename Desc, uint64_t SC_V, uint64_t DC_V, uint64_t QC_V>
struct Cell {
	using Descriptor = Desc;
	static constexpr uint64_t SC = SC_V;
	static constexpr uint64_t DC = DC_V;
	static constexpr uint64_t QC = QC_V;
	static constexpr uint64_t Size = SC + Desc::D * DC + Desc::Q * QC;
};

template<typename Desc, typename CellStruct>
struct Field;

template<typename Desc, typename... CellMembers>
struct Field<
	Desc,
	Struct<
		CellMembers...
	>
>;

}

template<typename T, typename Desc>
class df_info{};

template<typename T>
class df_info<T,sch::Descriptor<2, 5>> {
	static constexpr std::array<std::array<int32_t, 2>, 5> directions = {{
		{ 0, 0},
		{-1, 0},
		{ 0,-1},
		{ 0, 1},
		{ 1, 0}
	}};

	static constexpr std::array<typename saw::native_data_type<T>::type,5> weights = {
		1./3.,
		1./6.,
		1./6.,
		1./6.,
		1./6.
	};

	static constexpr typename saw::native_data_type<T>::type cs2 = 1./3.;
};
}
}

namespace saw {
template<typename T, typename Desc, uint64_t S, uint64_t D, uint64_t Q>
struct meta_schema<kel::lbm::sch::Cell<T,Desc,S,D,Q>> {
  using MetaSchema = schema::Void;
	using Schema = kel::lbm::sch::Cell<T,Desc,S,D,Q>;
};

template<typename Desc, typename CellT>
struct meta_schema<kel::lbm::sch::Field<Desc, CellT>> {
  using MetaSchema = schema::FixedArray<schema::UInt64,CellT::Descriptor::D>;
	using Schema = kel::lbm::sch::Field<Desc, CellT>;
};

template<typename Sch, typename Desc, uint64_t S, uint64_t D, uint64_t Q, typename Encode>
class data<kel::lbm::sch::Cell<Sch, Desc, S, D, Q>, Encode> final {
public:
	using Schema = kel::lbm::sch::Cell<Sch,Desc,S,D,Q>;
private:
	data<schema::FixedArray<Sch, Schema::Size>, Encode> inner_;
public:
	data() = default;

	data<Sch, Encode>& operator()(const data<schema::UInt64>& index){
		return inner_(index);
	}
};

template<typename Desc, typename CellT, typename Encode>
class data<kel::lbm::sch::Field<Desc, CellT>, Encode> final {
private:
	data<schema::Array<CellT,Desc::D>, Encode> inner_;
public:
	data(const data<schema::FixedArray<schema::UInt64, Desc::D>>& inner_meta__):
		inner_{inner_meta__}
	{}

	template<uint64_t i>
	data<schema::UInt64,Encode> get_dim_size() const {
		static_assert(i < Desc::D, "Not enough dimensions");
		return inner_.template get_dim_size<i>();
	}

	data<CellT>& operator()(const data<schema::FixedArray<schema::UInt64, Desc::D>, Encode>& index){
		return inner_(index);
	}
};
}
