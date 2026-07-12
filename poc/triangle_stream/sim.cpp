void init(){
}

saw::error_or<void> kmain(int argc, char** argv){
	using namespace kel;

	return saw::make_void();
}

int main(int argc, char** argv){
	auto eov = kmain(argc,argv);
	if(eov.is_error()){
		auto& err = eov.get_error();

		std::cerr<<"[Error]: "<<err.get_category();
		auto err_msg = err.get_message();
		if(not err_msg.empty()){
			std::cerr<<" - "<<err_msg;
		}
		std::cerr<<std::endl;
		return err.code();
	}
	return 0;
}
