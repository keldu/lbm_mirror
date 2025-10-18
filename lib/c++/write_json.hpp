#pragma once

#include <forstio/error.hpp>

#include <forstio/codec/data.hpp>
#include <forstio/codec/data_math.hpp>

#include <forstio/codec/json/json.hpp>


namespace kel {
namespace lbm {

template<typename Schema>
saw::error_or<void> write_json(const std::filesystem::path& file_name, const saw::data<Schema>& field){
	saw::data<Schema, saw::encode::Json> j_data;
	{
		saw::codec<Schema, saw::encode::Json> j_codec;
		auto eov = j_codec.encode(field, j_data);
		if(eov.is_error()){
			return eov;
		}
	}
	return saw::make_void();
}
}
}
