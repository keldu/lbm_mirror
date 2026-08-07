#pragma once

namespace kel {
namespace lbm {
namespace sch {
using D3Q19 = Descriptor<3u,19u>;
}

template<typename T>
class df_info<T, sch::Descriptor<3,19>> {
public:
    using Descriptor = sch::Descriptor<3,19>;

    static constexpr uint64_t D = 3u;
    static constexpr uint64_t Q = 19u;

    static constexpr std::array<std::array<int32_t,D>,Q> directions = {{
        { 0,  0,  0}, // 0

        {-1,  0,  0}, // 1
        { 1,  0,  0}, // 2
        { 0, -1,  0}, // 3
        { 0,  1,  0}, // 4
        { 0,  0, -1}, // 5
        { 0,  0,  1}, // 6

        {-1, -1,  0}, // 7
        { 1, -1,  0}, // 8
        {-1,  1,  0}, // 9
        { 1,  1,  0}, //10

        {-1,  0, -1}, //11
        { 1,  0, -1}, //12
        {-1,  0,  1}, //13
        { 1,  0,  1}, //14

        { 0, -1, -1}, //15
        { 0,  1, -1}, //16
        { 0, -1,  1}, //17
        { 0,  1,  1}  //18
    }};

    static constexpr std::array<typename saw::native_data_type<T>::type,Q> weights = {
        1./3.,

        1./18., 1./18.,
        1./18., 1./18.,
        1./18., 1./18.,

        1./36., 1./36.,
        1./36., 1./36.,
        1./36., 1./36.,
        1./36., 1./36.,
        1./36., 1./36.,
        1./36., 1./36.
    };

    static constexpr std::array<uint64_t,Q> opposite_index = {
        0,
        2,1,
        4,3,
        6,5,
        10,9,8,7,
        14,13,12,11,
        18,17,16,15
    };

    static constexpr typename saw::native_data_type<T>::type inv_cs2 = 3.0;
    static constexpr typename saw::native_data_type<T>::type cs2 = 1./3.;
};
}
}
