#pragma once

#include <forstio/error.hpp>
#include <filesystem>
#include <cstdlib>

namespace kel {
namespace lbm {

struct environment {
	std::filesystem::path lbm_dir;
	std::filesystem::path data_dir;
};

saw::error_or<environment> lbm_directory(){
	namespace fs = std::filesystem;

	const char* home_dir = std::getenv("HOME");
	if(not home_dir){
		return saw::make_error<saw::err::not_found>("Couldn't find home dir");
	}

	environment env;

	env.lbm_dir = std::filesystem::path{home_dir} / ".lbm";

	{
		env.data_dir = env.lbm_dir / "data";
		// LBM
		auto lbm_dir_config = lbm_dir / "data_dir_location.txt";
		if( fs::exists(lbm_dir_config) && fs::is_regular_file(lbm_dir_config) ){
			std::ifstream file{lbm_dir_config};

			if(file.is_open()){
				std::stringstream buffer;
				buffer <<file.rdbuf();
				env.data_dir = fs::path{buffer.str()};
			}
		}
	}

	return env;
}

/**
 * Returns the default output directory.
 * Located outside the project dir because dispatching build jobs with output data in the git directory
 * also copies simulated data which takes a long time.
 */
saw::error_or<std::filesystem::path> output_directory(){
	namespace fs = std::filesystem;

	const char* home_dir = std::getenv("HOME");
	if(not home_dir){
		return saw::make_error<saw::err::not_found>("Couldn't find home dir");
	}

	auto lbm_dir = std::filesystem::path{home_dir} / ".lbm";

	{
		auto lbm_dir_config = lbm_dir / "data_dir_location.txt";
		if( fs::exists(lbm_dir_config) && fs::is_regular_file(lbm_dir_config) ){
			std::ifstream file{lbm_dir_config};

			if(file.is_open()){
				std::stringstream buffer;
				buffer <<file.rdbuf();
				return fs::path{buffer.str()};
			}
		}
	}

	return lbm_dir / "data";
}

}
}
