#pragma once

#include "abstract/data.hpp"
#include "abstract/json.hpp"

#include <fstream>
#include <iterator>
#include <sstream>
#include <string_view>
#include <string>

namespace kel {
namespace lbm {
namespace sch {
template<typename T, typename Desc>
using LbmConfig = Struct<
    Member<T, "delta_x">,
    Member<T, "kinematic_viscosity">,
    Member<T, "delta_t">
>;
}

template<typename T, typename Desc>
error_or<data<sch::LbmConfig<T,Desc>>> load_lbm_config(std::string_view file_name){
    std::ifstream file{std::string{file_name}, std::ios::binary};

    if(!file.is_open()){
        return make_error<err::not_found>("Couldn't open file");
    }

    std::string contents{
        std::istreambuf_iterator<char>{file},
        std::istreambuf_iterator<char>{}
    };

    if(file.bad()){
        return make_error<err::invalid_state>("Couldn't read file");
    }

    data<sch::LbmConfig<T,Desc>, enc::Json> lbm_json_conf{std::move(contents)};

    data<sch::LbmConfig<T,Desc>> lbm_conf;
    codec<sch::LbmConfig<T,Desc>, enc::Json> json_codec;
    {
        auto eov = json_codec.decode(lbm_json_conf, lbm_conf);
        if(eov.is_error()){
            return std::move(eov.get_error());
        }
    }

    return lbm_conf;
}
}
}
