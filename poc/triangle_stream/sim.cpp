#include <kel/lbm/lbm.hpp>

namespace kel {
namespace lbm {

constexpr uint64_t dim_y = 256ul;
constexpr uint64_t dim_x = dim_y * 20ul;

namespace sch {
using namespace saw::schema;

using InfoChunk = Chunk<UInt8, 0u, dim_x, dim_y>;

template<typename T, typename Desc>
using DfChunk = Chunk<FixedArray<T,Desc::Q>, 1u, dim_x, dim_y>;

template<typename T, typename Desc>
using ScalarChunk = Chunk<Scalar<T>, 0u, dim_x, dim_y>;

template<typename T, typename Desc>
using VectorChunk = Chunk<Vector<T,Desc::D>, 0u, dim_x, dim_y>;

template<typename T, typename Desc>
using ChunkStruct = Struct<
	Member<InfoChunk, "info">,
	Member<DfChunk<T,Desc>, "dfs">,
	Member<DfChunk<T,Desc>, "dfs_old">
>;

template<typename T, typename Desc>
using VelChunk = Chunk<Vector<T,Desc::D>, 0u, dim_x, dim_y>;

template<typename T>
using RhoChunk = Chunk<Scalar<T>, 0u, dim_x, dim_y>;

template<typename T, typename Desc>
using MacroStruct = Struct<
	Member<VelChunk<T,Desc>, "velocity">,
	Member<RhoChunk<T>, "density">
>;

//template<typename T, typename Desc>
//using ParticleArray = Array<
//	Particle<T,Desc::D>
//>;
}

template<typename T, typename Desc>
void init(){
}

template<typename T, typename Desc>
saw::error_or<void> kmain(int argc, char** argv){
	using dfi = df_info<T,Desc>;

	(void) argc;
	(void) argv;

	return saw::make_void();
}

}
}

int main(int argc, char** argv){
	using namespace kel::lbm;
	auto eov = kmain<sch::Float32,sch::D2Q9>(argc,argv);
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
