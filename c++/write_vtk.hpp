#pragma once

#include <forstio/error.hpp>

namespace kel {
namespace lbm {
namespace impl {

template<typename... MemberTypes, saw::string_literal... MemberNames>
struct lbm_vtk_writer<sch::Struct<MemberTypes,MemberNames>...> {

	saw::error_or<void> apply(){
		return saw::make_void();
	}
};

template<typename CellFieldSchema>
struct lbm_vtk_writer {
};

template<typename Desc, typename... StructT, saw::string_literal... StructN>
struct lbm_vtk_writer<CellField<Desc,Struct<Member<StructT,StructN>...>>> {
	template<uint64_t i>
	saw::error_or<void> write_i(std::ofstream& vtk_file, const saw::data<CellField<Desc,Struct<Member<StructT,StructN>...>>>& field){

		// vtk_file<<

		return saw::make_void();
	}

	template<uint64_t i>
	saw::error_or<void> iterate_i(std::ofstream& vtk_file, const saw::data<CellField<Desc,Struct<Member<StructT,StructN>...>>>& field){
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

	saw::error_or<void> apply(std::ofstream& vtk_file, 
		const saw::data<CellField<Desc,Struct<Member<StructT,StructN>...>>>& field){
		
		auto meta = field.meta();
		// DIMENSIONS
		{
			vtk_file << "DIMENSIONS ";
			for(saw::data<sch::UInt64> i{0u}; i < Desc::D; ++i){
				vtk_file << " " << meta.at(i).get(); 
			}
			for(saw::data<sch::UInt64> i{Desc::D}; i < 3u; ++i){
				vtk_file << " 1";
			}

			vtk_file << "\n";
		}

		// HEADER TO BODY 
		{
			vtk_file << "\n";
		}

		if constexpr (sizeof...(StructT) > 0u){
			return iterate_i<0u>(vtk_file, field);
		}

		return saw::make_void();
	}
};
}

template<typename Struct>
saw::error_or<void> write_vtk(const std::string_view file_name, const saw::data<Struct>& field){

	std::string vtk_file_name{file_name};
	std::ofstream vtk_file{vtk_file_name};

	if(!vtk_file.is_open()){
		return saw::make_error<saw::err::critical>("Could not open file.");
	}

	vtk_file
		<<"# vtk DataFile Version 3.0\n"
		<<"LBM File\n"
		<<"ASCII\n"
		<<"DATASET STRUCTURED POINTS\n"
		<<"SPACING 1.0 1.0 1.0\n"
		<<"ORIGIN 0.0 0.0 0.0\n"
	;


	auto eov = lbm_vtk_writer::apply(vtk_file, field);
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
:w
}
}
}
