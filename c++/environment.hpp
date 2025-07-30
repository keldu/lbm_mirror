#pragma once

#include <forstio/error.hpp>
#include <filesystem>
#include <cstdlib>

namespace kel {
namespace lbm {

saw::error_or<std::filesystem::path> output_directory(){
	const char* home_dir = std::getenv("HOME");
	if(not home_dir){
		return saw::make_error<saw::err::not_found>("Couldn't find home dir");
	}

	return std::filesystem::path{home_dir} / ".lbm/";
}
}
}
