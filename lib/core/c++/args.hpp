#pragma once

#include <forstio/codec/data.hpp>
#include <forstio/codec/args.hpp>

namespace kel {
namespace lbm {
namespace sch {
using namespace saw::schema;
}

template<typename ArgSchema>
saw::error_or<saw::data<ArgSchema>> setup_lbm_env(int argc, char** argv){
	saw::data<ArgSchema,saw::encode::Args> args_data{argc,argv};

	saw::codec<ArgSchema,saw::encode::Args> args_codec;

	saw::data<ArgSchema> args_decoded;
	auto eov = args_codec.decode(args_data,args_decoded);
	if(eov.is_error()){
		return eov;
	}

	return args_decoded;
}
}
}
