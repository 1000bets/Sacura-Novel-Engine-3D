#pragma once

#include <SimpleMath.h>

#include <cstdint>
#include <vector>

struct SkinBinding
{
    static constexpr uint32_t MaxInfluencesPerVertex = 4;

    std::vector<int32_t> JointNodeIndices;
    std::vector<DirectX::SimpleMath::Matrix> InverseBindMatrices;
    std::vector<uint16_t> JointIndices;
    std::vector<float> JointWeights;
    uint32_t InfluencesPerVertex = MaxInfluencesPerVertex;
};
