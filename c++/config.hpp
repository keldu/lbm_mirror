#pragma once

#include <forstio/codec/data.hpp>
#include <forstio/codec/json/json.hpp>

#include <fstream>
#include <sstream>
#include <string_view>
#include <string>

namespace kel {
namespace lbm {
namespace sch {
using namespace saw::schema;
template<typename T, typename Desc>
using LbmConfig = Struct<
    Member<T, "delta_x">,
    Member<T, "kinematic_viscosity">,
    Member<T, "delta_t">,
    Member<FixedArray<UInt64,Desc::D>, "dims">
>;
}

template<typename T, typename Desc>
saw::error_or<saw::data<sch::LbmConfig<T,Desc>>> load_lbm_config(std::string_view file_name){
    std::ifstream file{std::string{file_name}};

    if(!file.is_open()){
        return saw::make_error<saw::err::not_found>("Couldn't open file");
    }

    saw::data<sch::LbmConfig<T,Desc>, saw::encode::Json> lbm_json_conf{saw::heap<saw::array_buffer>(1u)};

    uint8_t ele{};
    while(file.readsome(reinterpret_cast<char*>(&ele), 1u) > 0u){
        auto err = lbm_json_conf.get_buffer().push(ele,1u);
        if(err.failed()){
            return err;
        }
    }

    saw::data<sch::LbmConfig<T,Desc>> lbm_conf;
    saw::codec<sch::LbmConfig<T,Desc>, saw::encode::Json> json_codec;
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
