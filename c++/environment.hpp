#pragma once

#include <forstio/error.hpp>
#include <filesystem>
#include <cstdlib>

namespace kel {
namespace lbm {

/**
 * Returns the default output directory.
 * Located outside the project dir because dispatching build jobs with output data in the git directory
 * also copies simulated data which takes a long time.
 */
saw::error_or<std::filesystem::path> output_directory(){
	const char* home_dir = std::getenv("HOME");
	if(not home_dir){
		return saw::make_error<saw::err::not_found>("Couldn't find home dir");
	}

	return std::filesystem::path{home_dir} / ".lbm";
}
}
}
