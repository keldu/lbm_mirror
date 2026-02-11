#pragma once

#include <forstio/codec/data.hpp>
#include <forstio/codec/args.hpp>

namespace kel {
namespace lbm {
namespace sch {
using namespace saw::schema;
}

saw::error_or<void> init_lbm_env(){
	return saw::make_void();
}
}
}
