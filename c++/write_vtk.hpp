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

template<typename T, uint64_t D>
struct lbm_vtk_writer<sch::Primitive<T,D>> {
	static saw::error_or<void> apply_header(std::ostream& vtk_file, std::string_view name){
		vtk_file<<"SCALARS "<<name<<" float\n";
		vtk_file<<"LOOKUP_TABLE default\n";
		return saw::make_void();
	}
	static saw::error_or<void> apply(std::ostream& vtk_file, const saw::data<sch::Primitive<T,D>>& field){
		vtk_file<<field.get()<<"\n";
		return saw::make_void();
	}
};

template<typename T, uint64_t D>
struct lbm_vtk_writer<sch::FixedArray<T,D>> {
	static saw::error_or<void> apply_header(std::ostream& vtk_file, std::string_view name){
		vtk_file<<"VECTORS "<<name<<" float\n";
		return saw::make_void();
	}
	static saw::error_or<void> apply(std::ostream& vtk_file, const saw::data<sch::FixedArray<T,D>>& field){
		static_assert(D > 0, "Non-dimensionality is bad for velocity.");
		static_assert(D <= 3, "4th dimension as well. Mostly due to vtk.");

		// vtk_file<<"VECTORS "<<name<<" float\n";
		for(uint64_t i = 0u; i < D; ++i){
			if(i > 0){
				vtk_file<<" ";
			}
			vtk_file<<field.at({i}).get();
		}
		for(uint64_t i = D; i < 3; ++i){
			vtk_file<<" 0";
		}
		vtk_file<<"\n";
		return saw::make_void();
	}
};

template<uint64_t Dim, typename... StructT, saw::string_literal... StructN>
struct lbm_vtk_writer<sch::Array<sch::Struct<sch::Member<StructT,StructN>...> , Dim>> {

	template<uint64_t i, uint64_t Dep>
	static saw::error_or<void> write_i_iterate_d(std::ostream& vtk_file, const saw::data<sch::Array<sch::Struct<sch::Member<StructT,StructN>...>,Dim>>& field, saw::data<sch::FixedArray<sch::UInt64,Dim>>& index){
		constexpr auto Lit = saw::parameter_key_pack_type<i, StructN...>::literal;
		using Type = typename saw::parameter_pack_type<i,StructT...>::type;

		if constexpr (Dep == Dim){
			return lbm_vtk_writer<Type>::apply(vtk_file, field.at(index).template get<Lit>());
		} else {
			// Dep < Dim // I hope
			static_assert(Dep < Dim, "Don't fall into this case");
			for(index.at({Dep}) = 0; index.at({Dep}) < field.get_dims().at({Dep}); ++index.at({Dep})){
				auto eov = write_i_iterate_d<i,Dep+1>(vtk_file, field, index);
				if(eov.is_error()){
					return eov;
				}
			}
		}
		return saw::make_void();
	}

	template<uint64_t i>
	static saw::error_or<void> write_i(std::ostream& vtk_file, const saw::data<sch::Array<sch::Struct<sch::Member<StructT,StructN>...>,Dim>>& field){

		auto meta = field.get_dims();
		saw::data<sch::FixedArray<sch::UInt64,Dim>> index;
		for(saw::data<sch::UInt64> it{0}; it.get() < Dim; ++it){
			index.at({0u}).set(0u);
		}
		// vtk_file write?
		// Data header
		{

			auto eov = lbm_vtk_writer<typename saw::parameter_pack_type<i,StructT...>::type>::apply_header(vtk_file, saw::parameter_key_pack_type<i, StructN...>::literal.view());
			if(eov.is_error()){
				return eov;
			}

		}
		return write_i_iterate_d<i,0u>(vtk_file, field, index);
	}

	template<uint64_t i>
	static saw::error_or<void> iterate_i(std::ostream& vtk_file, const saw::data<sch::Array<sch::Struct<sch::Member<StructT,StructN>...>, Dim>>& field){
		constexpr auto Lit = saw::parameter_key_pack_type<i, StructN...>::literal;
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
		const saw::data<sch::Array<sch::Struct<sch::Member<StructT,StructN>...>, Dim>>& field){

		vtk_file
			<<"# vtk DataFile Version 3.0\n"
			<<"LBM File\n"
			<<"ASCII\n"
			<<"DATASET STRUCTURED_POINTS\n"
			<<"SPACING 1.0 1.0 1.0\n"
			<<"ORIGIN 0.0 0.0 0.0\n"
		;
		
		auto meta = field.get_dims();
		saw::data<sch::UInt64> pd_size{1u};
		// DIMENSIONS

		{
			vtk_file << "DIMENSIONS";
			for(saw::data<sch::UInt64> i{0u}; i.get() < Dim; ++i){
				pd_size = pd_size * meta.at(i);
				vtk_file << " " << meta.at(i).get(); 
			}
			for(saw::data<sch::UInt64> i{Dim}; i.get() < 3u; ++i){
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
