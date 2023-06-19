#include "../lattice.h"

#include <forstio/codec/data.h>

int main(){
	saw::data<schema::Lattice2D<saw::schema::Float32>, saw::encode::Native> lattice{512, 512};

	return 0;
}
