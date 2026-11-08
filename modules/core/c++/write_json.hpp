#pragma once

#include "abstract/error.hpp"

#include "abstract/data.hpp"
#include "abstract/data_math.hpp"
#include "abstract/json.hpp"

#include <filesystem>
#include <fstream>

namespace kel {
namespace lbm {

template<typename Schema>
error_or<void> write_json(const std::filesystem::path& file_name, const data<Schema>& field){
	data<Schema, enc::Json> j_data;
	{
		codec<Schema, enc::Json> j_codec;
		j_codec.set_pretty(true);
		auto eov = j_codec.encode(field, j_data);
		if(eov.is_error()){
			return eov;
		}
	}

	std::ofstream json_file{file_name, std::ios::binary | std::ios::trunc};
	if(!json_file.is_open()){
		return make_error<err::not_found>("Couldn't open file for writing");
	}

	const auto& text = j_data.stl_string();
	json_file.write(text.data(), static_cast<std::streamsize>(text.size()));
	if(!json_file){
		return make_error<err::invalid_state>("Couldn't write json file");
	}

	return make_void();
}
}
}
