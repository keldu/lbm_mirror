#pragma once

#include "common.hpp"

namespace kel {
namespace lbm {

/**
 * I'm mixing up the geometry values regarding inflow etc. And a bit of logic
 */
template<typename InfoSchema>
class domain_registry final {
private:
	kel::lbm::data<sch::Array<InfoSchema>> infos_;
public:
	template<typename T>
	kel::lbm::data<InfoSchema> get_template_id() {
		static kel::lbm::data<InfoSchema> id{std::numeric_limits<typename kel::lbm::native_data_type<InfoSchema>::type>::max()};

		if( id == std::numeric_limits<typename kel::lbm::native_data_type<InfoSchema>::type>::max() ){
			auto err_or_id = search_or_register_id(T::name);
			if(err_or_id.is_error()){
				// Unsure about recovery from this
				exit(-1);
			}
		}
	}
};

template<typename Schema>
void clean_grid(kel::lbm::data<Schema>& info_field){
}
}
}
