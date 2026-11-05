#pragma once

#include <forstio/error.hpp>

#include <forstio/codec/data.hpp>
#include <forstio/codec/data_math.hpp>

#include "descriptor.hpp"
#include "flatten.hpp"
#include "chunk.hpp"

#include <fstream>
#include <filesystem>

namespace kel {
namespace lbm {
namespace impl {

template<typename CellFieldSchema>
struct lbm_vtk_writer {
};

template<typename T, uint64_t D>
struct lbm_vtk_writer<sch::Primitive<T,D>> {
	static saw::error_or<void> apply_header(std::ostream& vtk_file, std::string_view name){
		vtk_file<<"SCALARS "<<name<<" float 1\n";
		vtk_file<<"LOOKUP_TABLE default\n";
		return saw::make_void();
	}
	static saw::error_or<void> apply(std::ostream& vtk_file, const saw::data<sch::Primitive<T,D>>& field){
		if constexpr (std::is_same_v<T,sch::UnsignedInteger> and D == 1u) {
			vtk_file<<field.template cast_to<sch::UInt16>().get()<<"\n";
		}else{
			vtk_file<<field.get()<<"\n";
		}
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
		saw::data<sch::FixedArray<sch::UInt64,D>> index;
		for(saw::data<sch::UInt64> it{0}; it.get() < D; ++it){
			index.at(it).set(0u);
		}

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

template<typename T, uint64_t Ghost, uint64_t... D>
struct lbm_vtk_writer<sch::Chunk<T,Ghost,D...>> {

	template<uint64_t d>
	static saw::error_or<void> apply_d(std::ostream& vtk_file, const saw::data<sch::Chunk<T,Ghost,D...>>& field, saw::data<sch::FixedArray<sch::UInt64,sizeof...(D)>>& index){
		// VTK wants to iterate over z,y,x instead of x,y,z
		// We could reorder the dimensions, but eh
		if constexpr ( d > 0u){
			for(index.at({d-1u}) = 0u; index.at({d-1u}) < field.get_dims().at({d-1u}); ++index.at({d-1u})){
				auto eov = apply_d<d-1u>(vtk_file, field, index);
			}
		}else{
			auto eov = lbm_vtk_writer<T>::apply(vtk_file, field.at(index));
			if(eov.is_error()) return eov;
		}
		return saw::make_void();
	}

	static saw::error_or<void> apply(std::ostream& vtk_file, const saw::data<sch::Chunk<T,Ghost,D...>>& field, std::string_view name){
		{
			auto eov = lbm_vtk_writer<T>::apply_header(vtk_file,name);
			if(eov.is_error()){
				return eov;
			}
		}
		saw::data<sch::FixedArray<sch::UInt64,sizeof...(D)>> index;
		for(saw::data<sch::UInt64> it{0}; it.get() < sizeof...(D); ++it){
			index.at({0u}).set(0u);
		}

		{
			auto eov = apply_d<sizeof...(D)>(vtk_file, field, index);
			if(eov.is_error()){
				return eov;
			}
		}

		vtk_file<<"\n";

		return saw::make_void();
	}
};

template<typename T, uint64_t D>
struct lbm_vtk_writer<sch::Vector<T,D>> {
	static saw::error_or<void> apply_header(std::ostream& vtk_file, std::string_view name){
		vtk_file<<"VECTORS "<<name<<" float\n";
		return saw::make_void();
	}
	static saw::error_or<void> apply(std::ostream& vtk_file, const saw::data<sch::Vector<T,D>>& field){
		static_assert(D > 0, "Non-dimensionality is bad for momentum.");
		static_assert(D <= 3, "4th dimension as well. Mostly due to vtk.");

		// vtk_file<<"VECTORS "<<name<<" float\n";
		for(uint64_t i = 0u; i < D; ++i){
			if(i > 0){
				vtk_file<<" ";
			}
			vtk_file<<field.at({{i}}).get();
		}
		for(uint64_t i = D; i < 3; ++i){
			vtk_file<<" 0";
		}
		vtk_file<<"\n";
		return saw::make_void();
	}
};

template<typename T>
struct lbm_vtk_writer<sch::Scalar<T>> {
	static saw::error_or<void> apply_header(std::ostream& vtk_file, std::string_view name){
		vtk_file<<"SCALARS "<<name<<" float 1\n";
		vtk_file<<"LOOKUP_TABLE default\n";
		return saw::make_void();
	}
	static saw::error_or<void> apply(std::ostream& vtk_file, const saw::data<sch::Scalar<T>>& field){
		// vtk_file<<"VECTORS "<<name<<" float\n";
		vtk_file<<field.at({}).get();
		vtk_file<<"\n";
		return saw::make_void();
	}
};

template<typename... MemberT, saw::string_literal... Keys, uint64_t... Ghost, uint64_t... Dims>
struct lbm_vtk_writer<sch::Struct<sch::Member<sch::Chunk<MemberT,Ghost,Dims...>,Keys>...>> final  {
	template<uint64_t i>
	static saw::error_or<void> iterate_i(std::ostream& vtk_file,
		const saw::data<sch::Struct<sch::Member<sch::Chunk<MemberT,Ghost,Dims...>,Keys>...>>& field){

		if constexpr ( i < sizeof...(MemberT) ) {
			using MT = typename saw::parameter_pack_type<i,sch::Member<sch::Chunk<MemberT,Ghost,Dims...>,Keys>...>::type;
			{
				auto eov = lbm_vtk_writer<typename MT::ValueType>::apply(vtk_file,field.template get<MT::KeyLiteral>(), MT::KeyLiteral.view());
				if(eov.is_error()){
					return eov;
				}
			}

			return iterate_i<i+1u>(vtk_file, field);
		}

		return saw::make_void();
	}


	static saw::error_or<void> apply(std::ostream& vtk_file,
		const saw::data<sch::Struct<sch::Member<sch::Chunk<MemberT,Ghost,Dims...>,Keys>...>>& field){
		
		vtk_file
			<<"# vtk DataFile Version 3.0\n"
			<<"LBM File\n"
			<<"ASCII\n"
			<<"DATASET STRUCTURED_POINTS\n"
			<<"SPACING 1.0 1.0 1.0\n"
			<<"ORIGIN 0.0 0.0 0.0\n"
		;

		auto& field_0 = field.template get<saw::parameter_key_pack_type<0u,Keys...>::literal>();	
		auto meta = field_0.get_dims();
		
		saw::data<sch::UInt64> pd_size{1u};
		// DIMENSIONS

		{
			vtk_file << "DIMENSIONS";
			pd_size.set(saw::ct_multiply<uint64_t,Dims...>::value);
			
			static_assert(saw::ct_multiply<uint64_t,Dims...>::value > 0u, "Invalid Dim size resulting in length 0u");

			for(saw::data<sch::UInt64> i{0u}; i.get() < sizeof...(Dims); ++i){
				vtk_file << " " << meta.at(i).get(); 
			}
			for(saw::data<sch::UInt64> i{sizeof...(Dims)}; i.get() < 3u; ++i){
				vtk_file << " 1";
			}

			vtk_file << "\n";
		}
		if constexpr (sizeof...(MemberT) > 0u){
			// POINT DATA
			{
				vtk_file << "\nPOINT_DATA " << pd_size.get() <<"\n";
			}

			// HEADER TO BODY
			{
				vtk_file << "\n";
			}
		}
		
		return iterate_i<0u>(vtk_file, field);
	}
};

}

template<typename Sch>
saw::error_or<void> write_vtk_file(const std::filesystem::path& out_dir, const std::string_view& file_name, uint64_t d_t, const saw::data<Sch>& field){

	auto vtk_dir = out_dir / "vtk";
	{
		std::error_code ec;
		std::filesystem::create_directories(vtk_dir,ec);
		if(ec != std::errc{}){
			return saw::make_error<saw::err::critical>("Could not create directory for write_vtk_file function");
		}
	}
	std::string ft_name{file_name};

	std::stringstream sstr;
	sstr
		<<file_name
		<<"_"
		<<d_t
		<<".vtk"
	;

	std::ofstream vtk_file{vtk_dir / sstr.str() };

	if( not vtk_file.is_open() ){
		return saw::make_error<saw::err::critical>("Could not open file.");
	}

	auto eov = impl::lbm_vtk_writer<Sch>::apply(vtk_file, field);
	return eov;
}

}
}
