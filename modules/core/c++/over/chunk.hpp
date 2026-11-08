#pragma once

#include "common.hpp"

/*
 * The idea is to split chunks into overarching chunk which takes separable computation blocks into
 * account
 */
namespace kel {
namespace lbm {
namespace sch {
template<typename Chunk, uint64_t Dim>
using OverChunk = Array<Chunk,Dim>;
}
}
}

namespace kel {
namespace lbm {
template<template Chunk, uint64_t Dim>
class data<kel::lbm::sch::OverChunk<Chunk,Dim>> final {
public:
	using Schema = kel::lbm::sch::OverChunk<Chunk,Dim>;
private:
	std::vector<data<Schema>> data_;
public:
};
}
}
