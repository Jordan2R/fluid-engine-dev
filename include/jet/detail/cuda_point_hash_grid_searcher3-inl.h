// Copyright (c) 2017 Doyub Kim
//
// I am making my contributions/submissions to this project solely in my
// personal capacity and am not conveying any rights to any intellectual
// property of any third parties.

#ifndef INCLUDE_JET_DETAIL_CUDA_POINT_HASH_GRID_SEARCHER3_INL_H_
#define INCLUDE_JET_DETAIL_CUDA_POINT_HASH_GRID_SEARCHER3_INL_H_

#ifdef JET_USE_CUDA

#include <jet/cuda_point_hash_grid_searcher3.h>
#include <jet/cuda_utils.h>

namespace jet {

namespace experimental {

JET_CUDA_HOST_DEVICE CudaPointHashGridSearcher3::HashUtils::HashUtils() {}

JET_CUDA_HOST_DEVICE CudaPointHashGridSearcher3::HashUtils::HashUtils(
    float gridSpacing, int3 resolution)
    : _gridSpacing(gridSpacing), _resolution(resolution) {}

inline JET_CUDA_HOST_DEVICE void
CudaPointHashGridSearcher3::HashUtils::getNearbyKeys(float4 position,
                                                     size_t* nearbyKeys) const {
    int3 originIndex = getBucketIndex(position), nearbyBucketIndices[8];

    for (int i = 0; i < 8; i++) {
        nearbyBucketIndices[i] = originIndex;
    }

    if ((originIndex.x + 0.5f) * _gridSpacing <= position.x) {
        nearbyBucketIndices[4].x += 1;
        nearbyBucketIndices[5].x += 1;
        nearbyBucketIndices[6].x += 1;
        nearbyBucketIndices[7].x += 1;
    } else {
        nearbyBucketIndices[4].x -= 1;
        nearbyBucketIndices[5].x -= 1;
        nearbyBucketIndices[6].x -= 1;
        nearbyBucketIndices[7].x -= 1;
    }

    if ((originIndex.y + 0.5f) * _gridSpacing <= position.y) {
        nearbyBucketIndices[2].y += 1;
        nearbyBucketIndices[3].y += 1;
        nearbyBucketIndices[6].y += 1;
        nearbyBucketIndices[7].y += 1;
    } else {
        nearbyBucketIndices[2].y -= 1;
        nearbyBucketIndices[3].y -= 1;
        nearbyBucketIndices[6].y -= 1;
        nearbyBucketIndices[7].y -= 1;
    }

    if ((originIndex.z + 0.5f) * _gridSpacing <= position.z) {
        nearbyBucketIndices[1].z += 1;
        nearbyBucketIndices[3].z += 1;
        nearbyBucketIndices[5].z += 1;
        nearbyBucketIndices[7].z += 1;
    } else {
        nearbyBucketIndices[1].z -= 1;
        nearbyBucketIndices[3].z -= 1;
        nearbyBucketIndices[5].z -= 1;
        nearbyBucketIndices[7].z -= 1;
    }

    for (int i = 0; i < 8; i++) {
        nearbyKeys[i] = getHashKeyFromBucketIndex(nearbyBucketIndices[i]);
    }
}

inline JET_CUDA_HOST_DEVICE int3
CudaPointHashGridSearcher3::HashUtils::getBucketIndex(float4 position) const {
    int3 bucketIndex;
    bucketIndex.x = static_cast<ssize_t>(floorf(position.x / _gridSpacing));
    bucketIndex.y = static_cast<ssize_t>(floorf(position.y / _gridSpacing));
    bucketIndex.z = static_cast<ssize_t>(floorf(position.z / _gridSpacing));
    return bucketIndex;
}

inline JET_CUDA_HOST_DEVICE size_t
CudaPointHashGridSearcher3::HashUtils::getHashKeyFromBucketIndex(
    int3 bucketIndex) const {
    int3 wrappedIndex = bucketIndex;
    wrappedIndex.x = bucketIndex.x % _resolution.x;
    wrappedIndex.y = bucketIndex.y % _resolution.y;
    wrappedIndex.z = bucketIndex.z % _resolution.z;
    if (wrappedIndex.x < 0) {
        wrappedIndex.x += _resolution.x;
    }
    if (wrappedIndex.y < 0) {
        wrappedIndex.y += _resolution.y;
    }
    if (wrappedIndex.z < 0) {
        wrappedIndex.z += _resolution.z;
    }
    return static_cast<size_t>(
        (wrappedIndex.z * _resolution.y + wrappedIndex.y) * _resolution.x +
        wrappedIndex.x);
}

inline JET_CUDA_HOST_DEVICE size_t
CudaPointHashGridSearcher3::HashUtils::getHashKeyFromPosition(
    float4 position) const {
    int3 bucketIndex = getBucketIndex(position);
    return getHashKeyFromBucketIndex(bucketIndex);
}

template <typename Callback>
inline JET_CUDA_HOST_DEVICE CudaPointHashGridSearcher3::ForEachNearbyPointFunc<
    Callback>::ForEachNearbyPointFunc(float r, float gridSpacing,
                                      int3 resolution, const size_t* sit,
                                      const size_t* eit, const size_t* si,
                                      const float4* p, const float4* o,
                                      Callback cb)
    : _hashUtils(gridSpacing, resolution),
      _radius(r),
      _startIndexTable(sit),
      _endIndexTable(eit),
      _sortedIndices(si),
      _points(p),
      _origins(o),
      _callback(cb) {}

template <typename Callback>
template <typename Index>
inline JET_CUDA_HOST_DEVICE void
CudaPointHashGridSearcher3::ForEachNearbyPointFunc<Callback>::operator()(
    Index idx) {
    const float4 origin = _origins[idx];

    size_t nearbyKeys[8];
    _hashUtils.getNearbyKeys(origin, nearbyKeys);

    const float queryRadiusSquared = _radius * _radius;

    for (int i = 0; i < 8; i++) {
        size_t nearbyKey = nearbyKeys[i];
        size_t start = _startIndexTable[nearbyKey];
        size_t end = _endIndexTable[nearbyKey];

        // Empty bucket -- continue to next bucket
        if (start == kMaxSize) {
            continue;
        }

        for (size_t j = start; j < end; ++j) {
            float4 direction = _points[j] - origin;
            float distanceSquared = lengthSquared(direction);
            if (distanceSquared <= queryRadiusSquared) {
                float distance = 0.0f;
                if (distanceSquared > 0) {
                    distance = sqrtf(distanceSquared);
                    direction /= distance;
                }

                _callback(_sortedIndices[j], _points[j]);
            }
        }
    }
}

}  // namespace experimental

}  // namespace jet

#endif  // JET_USE_CUDA

#endif  // INCLUDE_JET_DETAIL_CUDA_POINT_HASH_GRID_SEARCHER3_INL_H_
