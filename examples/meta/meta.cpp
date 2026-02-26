#include <kel/lbm/lbm.hpp>

#include <forstio/codec/args.hpp>

#include <iostream>

namespace kel {
namespace lbm {
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

using Prec = Float64;
}
}
}

template<typename T, typename Desc>
saw::error_or<void> meta_lbm_info(const saw::data<kel::lbm::sch::MetaArgsStruct>& args_str){
	using namespace kel::lbm;
	converter<T> conv{
		{args_str.template get<"dx">()},
		{args_str.template get<"dt">()}
	};
	print_lbm_meta<T,Desc>(conv, {args_str.template get<"kin_vis">()},{args_str.template get<"char_vel">()},{args_str.template get<"char_len">()});
	
	return saw::make_void();
}


saw::error_or<void> meta_main(int argc, char** argv){
	using namespace kel::lbm;

	std::string_view cfg_file_name = "config.json";
	if(argc > 1){
		cfg_file_name = argv[1];
	}

	saw::data<sch::MetaArgs> args_dat;
	{
		{
			
		}
		saw::data<sch::MetaArgs, saw::encode::Args> args_enc{argc,argv};
		saw::codec<sch::MetaArgs, saw::encode::Args> args_codec;

		auto eov = args_codec.decode(args_enc,args_dat);
		if(eov.is_error()){
			return eov;
		}
	}

	auto& args_str = args_dat.template get<"args">();
	auto& args_pos = args_dat.template get<"positionals">();


	switch(args_pos.get<0u>().get()){
	case 1u:
		{
			switch(args_pos.get<1u>().get()){
			case 3u:
				meta_lbm_info<sch::Prec,sch::Descriptor<1u,3u>>(args_str);
				break;
			default:
				return saw::make_error<saw::err::critical>("Second positional needs to be a valid velocity set");
			}
		}
		break;
	case 2u:
		{
			switch(args_pos.get<1u>().get()){
			case 5u:
				meta_lbm_info<sch::Prec,sch::Descriptor<2u,5u>>(args_str);
				break;
			case 9u:
				meta_lbm_info<sch::Prec,sch::Descriptor<2u,9u>>(args_str);
				break;
			default:
				return saw::make_error<saw::err::critical>("Second positional needs to be a valid velocity set");
			}
		}
		break;
	case 3u:
		{
			switch(args_pos.get<1u>().get()){
			case 7u:
				return saw::make_error<saw::err::not_implemented>("D3Q7 Velocity Set is not implemented");
				// meta_lbm_info<sch::Prec,sch::Descriptor<3u,19u>>(args_str);
				break;
			case 19u:
				return saw::make_error<saw::err::not_implemented>("D3Q17 Velocity Set is not implemented");
				// meta_lbm_info<sch::Prec,sch::Descriptor<3u,19u>>(args_str);
				break;
			case 27u:
				meta_lbm_info<sch::Prec,sch::Descriptor<3u,27u>>(args_str);
				break;
			default:
				return saw::make_error<saw::err::critical>("Second positional needs to be a valid velocity set");
			}
		}
		break;
	default:
		return saw::make_error<saw::err::critical>("First positional needs to be Dimension 1,2 or 3");
	}

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
