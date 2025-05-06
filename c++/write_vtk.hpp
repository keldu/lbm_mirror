#pragma once

#include <forstio/error.hpp>

#include <forstio/codec/data.hpp>

#include "descriptor.hpp"

#include <fstream>

namespace kel {
namespace lbm {
namespace impl {

template<typename CellFieldSchema>
struct lbm_vtk_writer {
};

template<typename... MemberTypes, saw::string_literal... MemberNames>
struct lbm_vtk_writer<sch::Struct<sch::Member<MemberTypes,MemberNames>...>> {

	static saw::error_or<void> apply(){
		return saw::make_void();
	}
};

template<typename T, uint64_t D>
struct lbm_vtk_writer<sch::FixedArray<T,D>> {
	static saw::error_or<void> apply(std::ostream& vtk_file, std::string_view name){
		vtk_file<<"VECTORS "<<name<<" float\n";
	}
};

template<typename Desc, typename... StructT, saw::string_literal... StructN>
struct lbm_vtk_writer<sch::CellField<Desc,sch::Struct<sch::Member<StructT,StructN>...>>> {
	template<uint64_t i, uint64_t Dep>
	static saw::error_or<void> write_i_iterate_d(std::ostream& vtk_file, const saw::data<sch::CellField<Desc,sch::Struct<sch::Member<StructT,StructN>...>>>& field, saw::data<sch::FixedArray<sch::UInt64,Desc::D>>& index){
		if constexpr (Dep == Desc::D){

		}
	}

	template<uint64_t i>
	static saw::error_or<void> write_i(std::ostream& vtk_file, const saw::data<sch::CellField<Desc,sch::Struct<sch::Member<StructT,StructN>...>>>& field){

		auto meta = field.meta();
		saw::data<sch::FixedArray<sch::UInt64,Desc::D>> index;
		for(saw::data<sch::UInt64> it{0}; it.get() < Desc::D; ++it){
			index.at({0u}).set(0u);
		}
		// vtk_file write?
		// Data header
		{
			auto eov = lbm_vtk_writer<typename saw::parameter_pack_type<i,StructT...>::type>::apply(vtk_file, saw::parameter_key_pack_type<i, StructN...>::literal.view());
		}
		return write_i_iterate_d<i,0u>(vtk_file, field, index);

		return saw::make_void();
	}

	template<uint64_t i>
	static saw::error_or<void> iterate_i(std::ostream& vtk_file, const saw::data<sch::CellField<Desc,sch::Struct<sch::Member<StructT,StructN>...>>>& field){
		{
			auto eov = write_i<i>(vtk_file, field);
			if(eov.is_error()){
				return eov;
			}
		}
		if constexpr ( (i+1u) < sizeof...(StructT) ){
			return iterate_i<i+1u>(vtk_file, field);
		}
		return saw::make_void();
	}

	static saw::error_or<void> apply(std::ostream& vtk_file,
		const saw::data<sch::CellField<Desc,sch::Struct<sch::Member<StructT,StructN>...>>>& field){

		vtk_file
			<<"# vtk DataFile Version 3.0\n"
			<<"LBM File\n"
			<<"ASCII\n"
			<<"DATASET STRUCTURED POINTS\n"
			<<"SPACING 1.0 1.0 1.0\n"
			<<"ORIGIN 0.0 0.0 0.0\n"
		;
		
		auto meta = field.meta();
		saw::data<sch::UInt64> pd_size{1u};
		// DIMENSIONS
		{
			vtk_file << "DIMENSIONS ";
			for(saw::data<sch::UInt64> i{0u}; i.get() < Desc::D; ++i){
				pd_size = pd_size * meta.at(i);
				vtk_file << " " << meta.at(i).get(); 
			}
			for(saw::data<sch::UInt64> i{Desc::D}; i.get() < 3u; ++i){
				vtk_file << " 1";
			}

			vtk_file << "\n";
		}

		if constexpr (sizeof...(StructT) > 0u){
			// POINT DATA
			{
				vtk_file << "POINT_DATA " << pd_size.get() <<"\n";
			}

			// HEADER TO BODY
			{
				vtk_file << "\n";
			}

			return iterate_i<0u>(vtk_file, field);
		}

		return saw::make_void();
	}
};
}

template<typename Struct>
saw::error_or<void> write_vtk_file(const std::string_view file_name, const saw::data<Struct>& field){

	std::string vtk_file_name{file_name};
	std::ofstream vtk_file{vtk_file_name};

	if(!vtk_file.is_open()){
		return saw::make_error<saw::err::critical>("Could not open file.");
	}


	auto eov = impl::lbm_vtk_writer<Struct>::apply(vtk_file, field);
	return eov;
	/*
	vtk_file <<"# vtk DataFile Version 3.0\n";
	vtk_file <<"Velocity Cavity2D example\n";
	vtk_file <<"ASCII\n";
	vtk_file <<"DATASET STRUCTURED_POINTS\n";
	vtk_file <<"DIMENSIONS "<< dim_x <<" "<<dim_y<<" 1\n";
	vtk_file <<"SPACING 1.0 1.0 1.0\n";
	vtk_file <<"ORIGIN 0.0 0.0 0.0\n";
	vtk_file <<"POINT_DATA "<<(dim_x*dim_y)<<"\n";
	vtk_file <<"VECTORS Velocity float\n";
	*/
}
}
}
