#include <kel/lbm/lbm.hpp>

#include <iostream>

namespace kel {
namespace lbm {
namespace sch {

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
kel::lbm::error_or<void> meta_lbm_info(const kel::lbm::data<kel::lbm::sch::MetaArgsStruct>& args_str){
	using namespace kel::lbm;
	converter<T> conv{
		as_unit<typename unit_division<sch::SiMeter<T>, sch::LbmMeter<T>>::Schema>(
			args_str.template get<"dx">()),
		as_unit<typename unit_division<sch::SiSecond<T>, sch::LbmSecond<T>>::Schema>(
			args_str.template get<"dt">())
	};
	print_lbm_meta<T,Desc>(
		conv,
		as_unit<sch::SiKinematicViscosity<T>>(args_str.template get<"kin_vis">()),
		as_unit<sch::SiVelocity<T>>(args_str.template get<"char_vel">()),
		as_unit<sch::SiMeter<T>>(args_str.template get<"char_len">()));

	return kel::lbm::make_void();
}


kel::lbm::error_or<void> meta_main(int argc, char** argv){
	using namespace kel::lbm;

	kel::lbm::data<sch::MetaArgs> args_dat;
	auto& args_str = args_dat.template get<"args">();

	// Defaults for anything the caller leaves out.
	args_str.template get<"dx">().set(1);
	args_str.template get<"dt">().set(1);

	{
		auto eov = parse_args(args_dat, argc, argv);
		if(eov.is_error()){
			return eov;
		}
	}

	auto& args_pos = args_dat.template get<"positionals">();

	switch(args_pos.template get<0u>().get()){
	case 1u:
		{
			switch(args_pos.template get<1u>().get()){
			case 3u:
				meta_lbm_info<sch::Prec,sch::Descriptor<1u,3u>>(args_str);
				break;
			default:
				return kel::lbm::make_error<kel::lbm::err::critical>("Second positional needs to be a valid momentum set");
			}
		}
		break;
	case 2u:
		{
			switch(args_pos.template get<1u>().get()){
			case 5u:
				meta_lbm_info<sch::Prec,sch::Descriptor<2u,5u>>(args_str);
				break;
			case 9u:
				meta_lbm_info<sch::Prec,sch::Descriptor<2u,9u>>(args_str);
				break;
			default:
				return kel::lbm::make_error<kel::lbm::err::critical>("Second positional needs to be a valid momentum set");
			}
		}
		break;
	case 3u:
		{
			switch(args_pos.template get<1u>().get()){
			case 7u:
				return kel::lbm::make_error<kel::lbm::err::not_implemented>("D3Q7 Velocity Set is not implemented");
				// meta_lbm_info<sch::Prec,sch::Descriptor<3u,19u>>(args_str);
				break;
			case 19u:
				return kel::lbm::make_error<kel::lbm::err::not_implemented>("D3Q17 Velocity Set is not implemented");
				// meta_lbm_info<sch::Prec,sch::Descriptor<3u,19u>>(args_str);
				break;
			case 27u:
				meta_lbm_info<sch::Prec,sch::Descriptor<3u,27u>>(args_str);
				break;
			default:
				return kel::lbm::make_error<kel::lbm::err::critical>("Second positional needs to be a valid momentum set");
			}
		}
		break;
	default:
		return kel::lbm::make_error<kel::lbm::err::critical>("First positional needs to be Dimension 1,2 or 3");
	}

	return kel::lbm::make_void();
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
