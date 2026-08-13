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
struct lbm_csv_writer {
};

template<typename T, uint64_t D>
struct lbm_csv_writer<sch::Primitive<T,D>> {
	static saw::error_or<void> apply(std::ostream& csv_file, const saw::data<sch::Primitive<T,D>>& field){
		if constexpr (std::is_same_v<T,sch::UnsignedInteger> and D == 1u) {
			csv_file<<field.template cast_to<sch::UInt16>().get();
		}else{
			csv_file<<field.get();
		}
		return saw::make_void();
	}
};

template<typename T, uint64_t D>
struct lbm_csv_writer<sch::FixedArray<T,D>> {
	static saw::error_or<void> apply(std::ostream& csv_file, const saw::data<sch::FixedArray<T,D>>& field){
		saw::data<sch::FixedArray<sch::UInt64,D>> index;
		for(saw::data<sch::UInt64> it{0}; it.get() < D; ++it){
			index.at({0u}).set(0u);
		}

		// csv_file<<"VECTORS "<<name<<" float\n";
		for(uint64_t i = 0u; i < D; ++i){
			if(i > 0){
				csv_file<<",";
			}
			csv_file<<field.at({i}).get();
		}
		return saw::make_void();
	}
};

template<typename T, uint64_t Ghost, uint64_t... D>
struct lbm_csv_writer<sch::Chunk<T,Ghost,D...>> {

	template<uint64_t d>
	static saw::error_or<void> apply_d(std::ostream& csv_file, const saw::data<sch::Chunk<T,Ghost,D...>>& field, saw::data<sch::FixedArray<sch::UInt64,sizeof...(D)>>& index){
		// VTK wants to iterate over z,y,x instead of x,y,z
		// So we do the same with CSV to stay consistent for now
		// We could reorder the dimensions, but eh
		if constexpr ( d > 0u){
			for(index.at({d-1u}) = 0u; index.at({d-1u}) < field.get_dims().at({d-1u}); ++index.at({d-1u})){
				auto eov = apply_d<d-1u>(csv_file, field, index);
			}
		}else{
			auto eov = lbm_csv_writer<T>::apply(csv_file, field.at(index));
			csv_file<<"\n";
			if(eov.is_error()) return eov;
		}
		return saw::make_void();
	}

	static saw::error_or<void> apply(std::ostream& csv_file, const saw::data<sch::Chunk<T,Ghost,D...>>& field, std::string_view name){
		saw::data<sch::FixedArray<sch::UInt64,sizeof...(D)>> index;
		for(saw::data<sch::UInt64> it{0}; it.get() < sizeof...(D); ++it){
			index.at({0u}).set(0u);
		}

		{
			auto eov = apply_d<sizeof...(D)>(csv_file, field, index);
			if(eov.is_error()){
				return eov;
			}
		}

		return saw::make_void();
	}
};

template<typename T, uint64_t D>
struct lbm_csv_writer<sch::Vector<T,D>> {
	static saw::error_or<void> apply(std::ostream& csv_file, const saw::data<sch::Vector<T,D>>& field){
		static_assert(D > 0, "Non-dimensionality is bad for momentum.");

		// csv_file<<"VECTORS "<<name<<" float\n";
		for(uint64_t i = 0u; i < D; ++i){
			if(i > 0){
				csv_file<<",";
			}
			{
				auto eov = lbm_csv_writer<T>::apply(csv_file,field.at({{i}}));
				if(eov.is_error()) return eov;
			}
		}
		return saw::make_void();
	}
};

template<typename T>
struct lbm_csv_writer<sch::Scalar<T>> {
	static saw::error_or<void> apply(std::ostream& csv_file, const saw::data<sch::Scalar<T>>& field){
		return lbm_csv_writer<T>::apply(csv_file,field.at({}));
	}
};

template<typename... MemberT, saw::string_literal... Keys, uint64_t... Ghost, uint64_t... Dims>
struct lbm_csv_writer<sch::Struct<sch::Member<sch::Chunk<MemberT,Ghost,Dims...>,Keys>...>> final  {
	template<uint64_t i>
	static saw::error_or<void> iterate_i(
			const std::filesystem::path& csv_dir, const std::string_view& file_base_name, uint64_t d_t, 
			const saw::data<sch::Struct<sch::Member<sch::Chunk<MemberT,Ghost,Dims...>,Keys>...>>& field){

		if constexpr ( i < sizeof...(MemberT) ) {
			using MT = typename saw::parameter_pack_type<i,sch::Member<sch::Chunk<MemberT,Ghost,Dims...>,Keys>...>::type;
			{
				std::stringstream sstr;
				sstr
					<<file_base_name
					<<"_"
					<<MT::Key.view()
					<<"_"
					<<d_t
					<<".csv"
				;
				std::ofstream csv_file{csv_dir / sstr.str() };

				if( not csv_file.is_open() ){
					return saw::make_error<saw::err::critical>("Could not open file.");
				}
				// 
				auto eov = lbm_csv_writer<typename MT::ValueType>::apply(csv_file,field.template get<MT::KeyLiteral>(), MT::KeyLiteral.view());
				if(eov.is_error()){
					return eov;
				}
			}

			return iterate_i<i+1u>(csv_dir, file_base_name, d_t,field);
		}

		return saw::make_void();
	}


	static saw::error_or<void> apply(
			const std::filesystem::path& csv_dir, const std::string_view& file_base_name, uint64_t d_t, 
			const saw::data<sch::Struct<sch::Member<sch::Chunk<MemberT,Ghost,Dims...>,Keys>...>>& field){
		
		auto& field_0 = field.template get<saw::parameter_key_pack_type<0u,Keys...>::literal>();	
		auto meta = field_0.get_dims();
		
		return iterate_i<0u>(csv_dir,file_base_name, d_t, field);
	}
};

}

template<typename Sch>
saw::error_or<void> write_csv_file(const std::filesystem::path& out_dir, const std::string_view& file_name, uint64_t d_t, const saw::data<Sch>& field){

	auto csv_dir = out_dir / "csv";
	{
		std::error_code ec;
		std::filesystem::create_directories(csv_dir,ec);
		if(ec != std::errc{}){
			return saw::make_error<saw::err::critical>("Could not create directory for write_csv_file function");
		}
	}
	auto eov = impl::lbm_csv_writer<Sch>::apply(csv_dir, file_name, d_t, field);
	return eov;
}

}
}
