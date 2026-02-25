#include <kel/lbm/lbm.hpp>

#include <iostream>

namespace sch {
using namespace saw::schema;

using MetaArgsStruct = Struct<
	Member<Float64, "dx">,
	Member<Float64, "dt">,
	Member<Float64, "kin_vis">,
	Member<Float64, "char_vel">,
	Member<Float64, "char_len">
>;

using MetaArgs = Args<
	MetaArgsStruct,
	Tuple<UInt64,UInt64>
>;
}

template<

saw::error_or<void> meta_main(int argc, char** argv){
	using namespace kel::lbm;

	std::string_view cfg_file_name = "config.json";
	if(argc > 1){
		cfg_file_name = argv[1];
	}

	saw::data<MetaArgs> args_dat;
	{
		saw::data<MetaArgs, saw::encode::Args> args_enc{argc,argv};
		saw::codec<MetaArgs, saw::encode::Args> args_codec;

		auto eov = args_codec.decode(args_enc,args_dat);
		if(eov.is_error()){
			return eov;
	}

	auto& args_str = args_dat.template get<"args">();
	auto& args_pos = args_dat.template get<"positionals">();

	converter<sch::Float64> conv{
		{args_str.template get<"delta_x">()},
		{args_str.template get<"delta_t">()}
	};

	switch(args_pos.get<0u>().get()){
	case 1u:
		{
		}
		break;
	case 2u:
		{
			switch(args_pos.get<1u>().get()){
			case 5u:
				break;
			case 9u:
				break;
			default:
				return saw::make_error<saw::err::critical>("Second positional needs to be a valid velocity set");
			}
		}
		break;
	case 3u:
		{

		}
		break;
	default:
		return saw::make_error<saw::err::critical>("First positional needs to be Dimension 1,2 or 3");
	}
	print_lbm_meta<sch::Float64,sch::Descriptor<2,9>>(conv, {args_str.template get<"kinematic_viscosity">()},{args_str.template get<"char_vel">()},{args_str.template get<"char_len">()});

	return saw::make_void();
}

int main(int argc, char** argv){
	auto eov = meta_main(argc,argv);
	if(eov.is_error()){
		auto& err = eov.get_error();

		std::cerr<<"[Error]: "<<err.get_category();

		auto err_msg = err.get_message();
		if(not err_msg.empty()){
			std::cerr<<" - "<<err_msg;
		}

		std::cerr<<std::endl;
	}

	return 0;
}
