#include <pch.h>
#include <rseq/rseq.h>

using namespace r5;

std::vector<unsigned char> comptypes(8);
float g_AnimCompressError = 1.f;

static char  s_FrameBitCountLUT[4]  { 0, 2, 4, 0 };
static float s_FrameValOffsetLUT[4] { 0.0f, 3.0f, 15.0f, 0.0f };
static char  s_AnimSeekLUT[60] {
    1,  15, 16, 2,  7,  8,  2,  15, 0,  3,  15, 0,  4,  15, 0,  5,
    15, 0,  6,  15, 0,  7,  15, 0,  2,  15, 2,  3,  15, 2,  4,  15,
    2,  5,  15, 2,  6,  15, 2,  7,  15, 2,  2,  15, 4,  3,  15, 4,
    4,  15, 4, 5, 15, 4, 6, 15, 4,  7,  15, 4,
};

int GetSectionLength(const int numframes, const int sectionframes, const int section) {
    if (!sectionframes)
        return numframes;

    const int frameoffset      = section * sectionframes;
    const int remainingframes  = numframes - frameoffset;

    return (remainingframes <= sectionframes) ? remainingframes : sectionframes;
}

template<typename TAnimDesc>
int GetSectionLength(const TAnimDesc& animdesc, const int section, const int numSections) {
    if constexpr (
        std::is_same_v<TAnimDesc, p2::mstudioanimdesc_t>           ||
        std::is_same_v<TAnimDesc, r2::mstudioanimdesc_t>           ||
        std::is_same_v<TAnimDesc, r5::anim::v7::mstudioanimdesc_t>) {
        if (!animdesc.sectionframes) return animdesc.numframes;
        if (section == (numSections - 1)) return 1;

        const int frameoffset     = section * animdesc.sectionframes;
        const int remainingframes = animdesc.numframes - frameoffset - 1;

        return (remainingframes <= animdesc.sectionframes) ? remainingframes : animdesc.sectionframes;
    }
    else if constexpr (
        std::is_same_v<TAnimDesc, r5::anim::v71::mstudioanimdesc_t>  ||
        std::is_same_v<TAnimDesc, r5::anim::v10::mstudioanimdesc_t>  ||
        std::is_same_v<TAnimDesc, r5::anim::v11::mstudioanimdesc_t>  ||
        std::is_same_v<TAnimDesc, r5::anim::v12::mstudioanimdesc_t>) {
        if (!animdesc.sectionframes) return animdesc.numframes;

        const int  sectionstallframes = animdesc.sectionstallframes;
        const bool isDP               = (animdesc.flags & r5::ANIM_DATAPOINT) != 0;

        if (sectionstallframes && section == 0) return sectionstallframes;
        if (!isDP && section == (numSections - 1)) return 1;

        const int sectionbase     = section - ((bool)sectionstallframes | isDP);
        const int frameoffset     = sectionstallframes + sectionbase * animdesc.sectionframes;
        const int remainingframes = animdesc.numframes - frameoffset - (isDP ? 0 : 1);

        return (remainingframes <= animdesc.sectionframes) ? remainingframes : animdesc.sectionframes;
    }
    else if constexpr ( std::is_same_v<TAnimDesc, r5::anim::v121::mstudioanimdesc_t>) {
        if (!animdesc.sectionframes) return animdesc.numframes;

        const int  sectionstallframes = animdesc.sectionstallframes;
        const bool isDP = (animdesc.flags & r5::ANIM_DATAPOINT) != 0;

        if (sectionstallframes && section == 0) return sectionstallframes;

        const int sectionbase = section - ((bool)sectionstallframes | isDP);
        const int frameoffset = sectionstallframes + sectionbase * animdesc.sectionframes;
        const int remainingframes = animdesc.numframes - frameoffset ;

        return (remainingframes <= animdesc.sectionframes) ? remainingframes : animdesc.sectionframes;
    }
    return 0;
}
template int GetSectionLength<p2::mstudioanimdesc_t>            (const p2::mstudioanimdesc_t&,             const int, const int);
template int GetSectionLength<r2::mstudioanimdesc_t>            (const r2::mstudioanimdesc_t&,             const int, const int);
template int GetSectionLength<r5::anim::v7::mstudioanimdesc_t>  (const r5::anim::v7::mstudioanimdesc_t&,   const int, const int);
template int GetSectionLength<r5::anim::v71::mstudioanimdesc_t> (const r5::anim::v71::mstudioanimdesc_t&,  const int, const int);
template int GetSectionLength<r5::anim::v10::mstudioanimdesc_t> (const r5::anim::v10::mstudioanimdesc_t&,  const int, const int);
template int GetSectionLength<r5::anim::v11::mstudioanimdesc_t> (const r5::anim::v11::mstudioanimdesc_t&,  const int, const int);
template int GetSectionLength<r5::anim::v12::mstudioanimdesc_t> (const r5::anim::v12::mstudioanimdesc_t&,  const int, const int);
template int GetSectionLength<r5::anim::v121::mstudioanimdesc_t>(const r5::anim::v121::mstudioanimdesc_t&, const int, const int);

template<typename TAnimDesc>
int GetSectionCount(const TAnimDesc& animdesc) {
    if constexpr (
        std::is_same_v<TAnimDesc, p2::mstudioanimdesc_t>           ||
        std::is_same_v<TAnimDesc, r2::mstudioanimdesc_t>           ||
        std::is_same_v<TAnimDesc, r5::anim::v7::mstudioanimdesc_t>) {
        return (animdesc.numframes - 1) / animdesc.sectionframes + 2;
    }
    else if constexpr (
        std::is_same_v<TAnimDesc, r5::anim::v71::mstudioanimdesc_t> ||
        std::is_same_v<TAnimDesc, r5::anim::v10::mstudioanimdesc_t> ||
        std::is_same_v<TAnimDesc, r5::anim::v11::mstudioanimdesc_t> ||
        std::is_same_v<TAnimDesc, r5::anim::v12::mstudioanimdesc_t> ||
        std::is_same_v<TAnimDesc, r5::anim::v121::mstudioanimdesc_t>) {
        const int useTrail = (animdesc.flags & ANIM_DATAPOINT) ? 0 : 1;
        const int useStall = animdesc.sectionstallframes ? 1 : 0;
        const int base     = (animdesc.numframes - animdesc.sectionstallframes - 1) / animdesc.sectionframes;

        if constexpr(std::is_same_v<TAnimDesc, r5::anim::v121::mstudioanimdesc_t>) return base + useTrail + useStall;
        return base + useTrail + useStall + 1;
    }
    return 0;
}
template int GetSectionCount<p2::mstudioanimdesc_t>            (const p2::mstudioanimdesc_t&);
template int GetSectionCount<r2::mstudioanimdesc_t>            (const r2::mstudioanimdesc_t&);
template int GetSectionCount<r5::anim::v7::mstudioanimdesc_t>  (const r5::anim::v7::mstudioanimdesc_t&);
template int GetSectionCount<r5::anim::v71::mstudioanimdesc_t> (const r5::anim::v71::mstudioanimdesc_t&);
template int GetSectionCount<r5::anim::v10::mstudioanimdesc_t> (const r5::anim::v10::mstudioanimdesc_t&);
template int GetSectionCount<r5::anim::v11::mstudioanimdesc_t> (const r5::anim::v11::mstudioanimdesc_t&);
template int GetSectionCount<r5::anim::v12::mstudioanimdesc_t> (const r5::anim::v12::mstudioanimdesc_t&);
template int GetSectionCount<r5::anim::v121::mstudioanimdesc_t>(const r5::anim::v121::mstudioanimdesc_t&);

// ============================================================================
//  RLE
// ============================================================================

int RLE::GetAnimValueOffset(const r5::anim::mstudioanimvalue_t* const panimvalue) {
    const int lutBaseIdx = panimvalue->meta.type * 3;
    return s_AnimSeekLUT[lutBaseIdx]
         + ((s_AnimSeekLUT[lutBaseIdx + 1] + panimvalue->meta.total * s_AnimSeekLUT[lutBaseIdx + 2]) >> 4);
}

void RLE::ExtractAnimValue(const r5::anim::mstudioanimvalue_t* panimvalue, const int frame, const float scale, float& v1) {
    switch (panimvalue->meta.type) {
    case 0:
        v1 = static_cast<float>(panimvalue[frame + 1].value) * scale;
        return;

    case 1: {
        int16_t value = panimvalue[1].value;
        if (frame > 0)
            value += reinterpret_cast<const char*>(&panimvalue[2])[frame - 1];
        v1 = static_cast<float>(value) * scale;
        return;
    }

    default: {
        const int   bitLUTIndex  = (panimvalue->meta.type - 2) / 6;
        const int   numUnkValue  = (panimvalue->meta.type - 2) % 6;
        const float cycle        = static_cast<float>(frame) / static_cast<float>(panimvalue->meta.total - 1);

        float v7    = 1.0f;
        float value = static_cast<float>(panimvalue[1].value);

        for (int i = 0; i < numUnkValue; i++) {
            v7 *= cycle;
            value += static_cast<float>(panimvalue[i + 2].value) * v7;
        }

        if (bitLUTIndex) {
            const int16_t* pbits         = &panimvalue[numUnkValue + 2].value;
            const uint8_t  maskBitCount  = s_FrameBitCountLUT[bitLUTIndex];
            const uint8_t  mask          = (1 << maskBitCount) - 1;
            const int      bitOffset     = (frame * maskBitCount) & (8 * sizeof(r5::anim::mstudioanimvalue_t) - 1);

            constexpr int animValueBitSize = 8 * sizeof(r5::anim::mstudioanimvalue_t);
            static_assert(animValueBitSize == 16);

            value -= s_FrameValOffsetLUT[bitLUTIndex];
            value += 2.0f * (mask & (pbits[(frame * maskBitCount) >> 4] >> bitOffset));
        }

        v1 = value * scale;
        return;
    }
    }
}

void p2::RLE::ExtractAnimValue(int frame, const r5::anim::mstudioanimvalue_t* panimvalue, float scale, float& v1) {
    if (!panimvalue) { v1 = 0; return; }

    int k = frame;
    while (panimvalue->meta.total <= k) {
        k -= panimvalue->meta.total;
        panimvalue += panimvalue->meta.type + 1;
        if (panimvalue->meta.total == 0) { v1 = 0; return; }
    }

    v1 = (panimvalue->meta.type > k)
       ? panimvalue[k + 1].value * scale
       : panimvalue[panimvalue->meta.type].value * scale;
}

void RLE::ExtractAnimValue(int frame, const r5::anim::mstudioanimvalue_t* panimvalue, float scale, float& v1) {
    int k = frame;
    while (panimvalue->meta.total <= k) {
        k -= panimvalue->meta.total;
        panimvalue += RLE::GetAnimValueOffset(panimvalue);
    }
    RLE::ExtractAnimValue(panimvalue, k, scale, v1);
}

void RLE::ExtractAnimValue(int frame, const r5::anim::mstudioanimvalue_t* panimvalue, float scale, float& v1, float& v2) {
    int k = frame;
    while (panimvalue->meta.total <= k) {
        k -= panimvalue->meta.total;
        panimvalue += RLE::GetAnimValueOffset(panimvalue);
    }

    if (k >= panimvalue->meta.total - 1) {
        RLE::ExtractAnimValue(panimvalue, k, scale, v1);
        RLE::ExtractAnimValue(panimvalue + RLE::GetAnimValueOffset(panimvalue), 0, scale, v2);
    }
    else {
        RLE::ExtractAnimValue(panimvalue, k,     scale, v1);
        RLE::ExtractAnimValue(panimvalue, k + 1, scale, v2);
    }
}

void RLE::CalcBonePosition(const r5::anim::mstudio_rle_anim_t& pAnim, uint16_t** BoneTrackData, Vector3& trackval, uint32_t localframe) {
    if (!pAnim.bAnimPosition) {
        Vector48* packedpos = reinterpret_cast<Vector48*>(*BoneTrackData);
        trackval = Unpack48(*packedpos);
        *BoneTrackData += 3;
        return;
    }

    auto* valueptr = reinterpret_cast<r5::anim::studioanimvalue_ptr_t*>((char*)*BoneTrackData + 4);

    r5::anim::mstudioanimvalue_t* track_x  = PTR_FROM_IDX(r5::anim::mstudioanimvalue_t, valueptr, valueptr->offset);
    r5::anim::mstudioanimvalue_t* track_y  = track_x + valueptr->idx1;
    r5::anim::mstudioanimvalue_t* track_z  = track_x + valueptr->idx2;
    r5::anim::mstudioanimvalue_t* tracks[] = { track_x, track_y, track_z };

    const long ptrflags = valueptr->flags;
    Vector3 result(0, 0, 0);

    for (int axis = 0; axis < 3; axis++) {
        if (_bittest(&ptrflags, 0x2 - axis))
            ExtractAnimValue((int)localframe, tracks[axis], *(float*)*BoneTrackData, result[axis]);
    }

    trackval += result;
    *BoneTrackData += 4;
}

void RLE::CalcBoneQuaternion(const r5::anim::mstudio_rle_anim_t& pAnim, uint16_t** BoneTrackData, Vector3& trackval, uint32_t localframe) {
    if (!pAnim.bAnimRotation) {
        Quaternion64* q64 = reinterpret_cast<Quaternion64*>(*BoneTrackData);
        trackval = UnpackQuat64(*q64).ToRad();
        *BoneTrackData += 4;
        return;
    }

    auto* valueptr = reinterpret_cast<r5::anim::studioanimvalue_ptr_t*>((char*)*BoneTrackData);

    r5::anim::mstudioanimvalue_t* track_x  = PTR_FROM_IDX(r5::anim::mstudioanimvalue_t, valueptr, valueptr->offset);
    r5::anim::mstudioanimvalue_t* track_y  = track_x + valueptr->idx1;
    r5::anim::mstudioanimvalue_t* track_z  = track_x + valueptr->idx2;
    r5::anim::mstudioanimvalue_t* tracks[] = { track_x, track_y, track_z };

    uint16_t ptrflags = valueptr->flags;
    Vector3 rad(0, 0, 0);

    for (int axis = 0; axis < 3; axis++) {
        if (_bittest((const long*)&ptrflags, 0x2 - axis)) {
            ExtractAnimValue((int)localframe, tracks[axis], 0.00019175345f, rad[axis]);
            trackval[axis] = rad[axis];
        }
    }

    *BoneTrackData += 2;
}

void RLE::CalcBoneScale(const r5::anim::mstudio_rle_anim_t& pAnim, uint16_t** BoneTrackData, Vector3& trackval, uint32_t localframe) {
    if (!pAnim.bAnimScale) {
        Vector48* packedscl = reinterpret_cast<Vector48*>(*BoneTrackData);
        trackval = Unpack48(*packedscl);
        *BoneTrackData += 3;
        return;
    }

    auto* valueptr = reinterpret_cast<r5::anim::studioanimvalue_ptr_t*>((char*)*BoneTrackData);

    r5::anim::mstudioanimvalue_t* track_x  = PTR_FROM_IDX(r5::anim::mstudioanimvalue_t, valueptr, valueptr->offset);
    r5::anim::mstudioanimvalue_t* track_y  = track_x + valueptr->idx1;
    r5::anim::mstudioanimvalue_t* track_z  = track_x + valueptr->idx2;
    r5::anim::mstudioanimvalue_t* tracks[] = { track_x, track_y, track_z };

    const long ptrflags = valueptr->flags;
    Vector3 result(0, 0, 0);

    for (int axis = 0; axis < 3; axis++) {
        if (_bittest(&ptrflags, 0x2 - axis))
            ExtractAnimValue((int)localframe, tracks[axis], 0.0030518509f, result[axis]);
    }

    trackval += result;
    *BoneTrackData += 4;
}

// ============================================================================
//  DP
// ============================================================================

template<typename TIndexType>
int r5::DP::ResolveFrameIndex(const int localFrame, const TIndexType total, const int sectionlength, const TIndexType* pFrameIndices) {
    if (total >= static_cast<TIndexType>(sectionlength))
        return localFrame < static_cast<int>(total) ? localFrame : static_cast<int>(total) - 1;

    int lo = 0, hi = static_cast<int>(total) - 1;
    while (lo < hi) {
        const int mid = (lo + hi + 1) >> 1;
        if (static_cast<int>(pFrameIndices[mid]) <= localFrame) 
            lo = mid;
        else                                                    
            hi = mid - 1;
    }
    return lo;
}

template<typename TIndexType, typename TDataType>
static void ResolveInterpFrames_DP(int& prevFrame, int& nextFrame, float& s,
    const int localFrame, const TIndexType total, const int sectionlength,
    const TIndexType* pFrameIndices, const TDataType** pDataOut)
{
    if (total >= static_cast<TIndexType>(sectionlength)) {
        prevFrame = localFrame < static_cast<int>(total) ? localFrame : static_cast<int>(total) - 1;
        nextFrame = prevFrame;
        s = 0.0f;
        *pDataOut = reinterpret_cast<const TDataType*>(pFrameIndices);
        return;
    }

    int lo = 0, hi = static_cast<int>(total) - 1;
    while (lo < hi) {
        const int mid = (lo + hi + 1) >> 1;
        if (static_cast<int>(pFrameIndices[mid]) <= localFrame)
            lo = mid;
        else
            hi = mid - 1;
    }
    prevFrame = lo;

    const int prevIdx = static_cast<int>(pFrameIndices[prevFrame]);

    if (prevIdx >= localFrame || prevFrame >= static_cast<int>(total) - 1) {
        nextFrame = prevFrame;
        s = 0.0f;
    }
    else {
        nextFrame = prevFrame + 1;
        s = static_cast<float>(localFrame - prevIdx) / static_cast<float>(static_cast<int>(pFrameIndices[nextFrame]) - prevIdx);
    }

    *pDataOut = reinterpret_cast<const TDataType*>(pFrameIndices + total);
}

template<typename TPackedType>
static void CalcBoneSeek_DP(const TPackedType* pPackedData, int& validIdx, uint32_t& remainingFrames, const uint32_t targetFrame)
{
    remainingFrames = targetFrame;
    if (pPackedData[validIdx].numInterpFrames < targetFrame) {
        uint32_t numInterpFrames = pPackedData[validIdx].numInterpFrames;
        while (numInterpFrames < remainingFrames) {
            validIdx++;
            remainingFrames -= (numInterpFrames + 1);
            numInterpFrames = pPackedData[validIdx].numInterpFrames;
        }
    }
}

static void QuatSlerp(const Quaternion& p, const Quaternion& q, float t, Quaternion& qt)
{
    Quaternion q2;
    float a = (p.x-q.x)*(p.x-q.x) + (p.y-q.y)*(p.y-q.y) + (p.z-q.z)*(p.z-q.z) + (p.w-q.w)*(p.w-q.w);
    float b = (p.x+q.x)*(p.x+q.x) + (p.y+q.y)*(p.y+q.y) + (p.z+q.z)*(p.z+q.z) + (p.w+q.w)*(p.w+q.w);
    if (a > b) { q2.x = -q.x; q2.y = -q.y; q2.z = -q.z; q2.w = -q.w; }
    else       { q2.x =  q.x; q2.y =  q.y; q2.z =  q.z; q2.w =  q.w; }

    float cosom = p.x*q2.x + p.y*q2.y + p.z*q2.z + p.w*q2.w;

    if ((1.0f + cosom) > 0.000001f) {
        float sclp, sclq;
        if ((1.0f - cosom) > 0.000001f) {
            float omega = acosf(cosom);
            float sinom = sinf(omega);
            sclp = sinf((1.0f - t) * omega) / sinom;
            sclq = sinf(t * omega) / sinom;
        }
        else {
            sclp = 1.0f - t;
            sclq = t;
        }
        qt.x = sclp * p.x + sclq * q2.x;
        qt.y = sclp * p.y + sclq * q2.y;
        qt.z = sclp * p.z + sclq * q2.z;
        qt.w = sclp * p.w + sclq * q2.w;
    }
    else {
        qt.x = -q2.y; qt.y = q2.x; qt.z = -q2.w; qt.w = q2.z;
        float sclp = sinf((1.0f - t) * M_PI2);
        float sclq = sinf(t * M_PI2);
        qt.x = sclp * p.x + sclq * qt.x;
        qt.y = sclp * p.y + sclq * qt.y;
        qt.z = sclp * p.z + sclq * qt.z;
    }
}

void r5::DP::CalcBoneQuaternion_DP(const int sectionlength, const uint8_t** panimtrack, const int localFrame, Quaternion& q) {
    const uint8_t* ptrack = *panimtrack;

    const uint8_t  valid = ptrack[0];
    const uint8_t  total = ptrack[1];
    const uint8_t* pFrameIndices = ptrack + 2;

    int prevFrame, nextFrame;
    float s;
    const r5::anim::AnimQuat32* pPackedData;

    ResolveInterpFrames_DP(prevFrame, nextFrame, s, localFrame, total, sectionlength, pFrameIndices, &pPackedData);
    const auto* pAxisFixup = reinterpret_cast<const r5::anim::AxisFixup_t*>(pPackedData + valid);

    int validIdx = 0;
    uint32_t remainingFrames;
    CalcBoneSeek_DP(pPackedData, validIdx, remainingFrames, static_cast<uint32_t>(prevFrame));

    Quaternion q1;
    r5::DP::UnpackAnimQuat32(q1, pPackedData[validIdx], &pAxisFixup[prevFrame]);

    if (prevFrame == nextFrame) {
        q = q1;
    }
    else {
        CalcBoneSeek_DP(pPackedData, validIdx, remainingFrames, static_cast<uint32_t>(nextFrame - prevFrame) + remainingFrames);

        Quaternion q2;
        r5::DP::UnpackAnimQuat32(q2, pPackedData[validIdx], &pAxisFixup[nextFrame]);
        QuatSlerp(q1, q2, s, q);
    }

    *panimtrack = reinterpret_cast<const uint8_t*>(pAxisFixup + total);
}

void r5::DP::CalcBonePosition_DP(const int sectionlength, const uint8_t** panimtrack, const int localFrame, Vector3& pos) {
    const uint8_t* ptrack = *panimtrack;

    const uint8_t  valid = ptrack[0];
    const uint8_t  total = ptrack[1];
    const uint8_t* pFrameIndices = ptrack + 2;

    *panimtrack = reinterpret_cast<const uint8_t*>(pFrameIndices);

    if (!total) { pos = { 0.0f, 0.0f, 0.0f }; return; }

    int prevFrame, nextFrame;
    float s;
    const r5::anim::AnimPos64* pPackedData;

    ResolveInterpFrames_DP(prevFrame, nextFrame, s, localFrame, total, sectionlength, pFrameIndices, &pPackedData);
    const auto* pAxisFixup = reinterpret_cast<const r5::anim::AxisFixup_t*>(pPackedData + valid);

    int validIdx = 0;
    uint32_t remainingFrames;
    CalcBoneSeek_DP(pPackedData, validIdx, remainingFrames, static_cast<uint32_t>(prevFrame));

    Vector3 pos1;
    r5::DP::UnpackAnimPos64(pos1, pPackedData[validIdx], &pAxisFixup[prevFrame]);

    if (prevFrame == nextFrame) {
        pos = pos1;
    }
    else {
        CalcBoneSeek_DP(pPackedData, validIdx, remainingFrames, static_cast<uint32_t>(nextFrame - prevFrame) + remainingFrames);

        Vector3 pos2;
        r5::DP::UnpackAnimPos64(pos2, pPackedData[validIdx], &pAxisFixup[nextFrame]);

        pos.x = pos1.x + (pos2.x - pos1.x) * s;
        pos.y = pos1.y + (pos2.y - pos1.y) * s;
        pos.z = pos1.z + (pos2.z - pos1.z) * s;
    }

    *panimtrack = reinterpret_cast<const uint8_t*>(pAxisFixup + total);
}

void r5::DP::CalcBonePositionVirtual_DP(const int sectionlength, const uint8_t** panimtrack, const int localFrame, Vector3& pos) {
    const uint8_t* ptrack = *panimtrack;

    const uint8_t  total = ptrack[0];
    const float    posscale = HalfToFloat(*reinterpret_cast<const uint16_t*>(ptrack + 1)) / 127.0f;
    const uint8_t* pFrameIndices = ptrack + 3;

    int prevFrame, nextFrame;
    float s;
    const r5::anim::AxisFixup_t* pAxisFixup;

    ResolveInterpFrames_DP(prevFrame, nextFrame, s, localFrame, total, sectionlength, pFrameIndices, &pAxisFixup);

    Vector3 pos1;
    pos1.x = static_cast<float>(pAxisFixup[prevFrame].adjustment[0]) * posscale;
    pos1.y = static_cast<float>(pAxisFixup[prevFrame].adjustment[1]) * posscale;
    pos1.z = static_cast<float>(pAxisFixup[prevFrame].adjustment[2]) * posscale;

    if (prevFrame == nextFrame) {
        pos = pos1;
    }
    else {
        Vector3 pos2;
        pos2.x = static_cast<float>(pAxisFixup[nextFrame].adjustment[0]) * posscale;
        pos2.y = static_cast<float>(pAxisFixup[nextFrame].adjustment[1]) * posscale;
        pos2.z = static_cast<float>(pAxisFixup[nextFrame].adjustment[2]) * posscale;

        pos.x = pos1.x + (pos2.x - pos1.x) * s;
        pos.y = pos1.y + (pos2.y - pos1.y) * s;
        pos.z = pos1.z + (pos2.z - pos1.z) * s;
    }

    *panimtrack = reinterpret_cast<const uint8_t*>(pAxisFixup + total);
}

void r5::DP::CalcBoneScale_DP(const int sectionlength, const uint8_t** panimtrack, const int localFrame, Vector3& scale) {
    const uint8_t* ptrack = *panimtrack;

    const uint8_t  total = ptrack[0];
    const uint8_t* pFrameIndices = ptrack + 1;

    int prevFrame, nextFrame;
    float s;
    const Vector48* pPackedData;

    ResolveInterpFrames_DP(prevFrame, nextFrame, s, localFrame, total, sectionlength, pFrameIndices, &pPackedData);

    Vector3 s1 = { static_cast<float>(pPackedData[prevFrame].x),
                   static_cast<float>(pPackedData[prevFrame].y),
                   static_cast<float>(pPackedData[prevFrame].z) };

    if (prevFrame == nextFrame) {
        scale = s1;
    }
    else {
        Vector3 s2 = { static_cast<float>(pPackedData[nextFrame].x),
                       static_cast<float>(pPackedData[nextFrame].y),
                       static_cast<float>(pPackedData[nextFrame].z) };

        scale.x = s1.x + (s2.x - s1.x) * s;
        scale.y = s1.y + (s2.y - s1.y) * s;
        scale.z = s1.z + (s2.z - s1.z) * s;
    }

    *panimtrack = reinterpret_cast<const uint8_t*>(pPackedData + total);
}

void r5::DP::ParseDataPointSection(const uint8_t* pBoneFlagArray, int sectionlength, uint32_t sectionbaseframe, temp::rig_t& rig, temp::animdesc_t& anim) {
    const int numbones = static_cast<int>(rig.bones.size());
    const bool isNonDelta = !(anim.flags & ANIM_DELTA);
    const uint32_t bfa_size = (static_cast<uint32_t>(numbones * 4 + 7) / 8 + 1) & ~1u;
    const r5::anim::mstudio_rle_anim_t* panim = reinterpret_cast<const r5::anim::mstudio_rle_anim_t*>(pBoneFlagArray + bfa_size);

    for (int bone = 0; bone < numbones; bone++) {
        const uint8_t boneFlags = static_cast<uint8_t>(pBoneFlagArray[bone / 2] >> (4 * (bone % 2))) & 0xF;

        if (!(boneFlags & r5::DP::BONEDATA)) continue;

        for (int localframe = 0; localframe < sectionlength; localframe++) {
            const uint32_t frame = sectionbaseframe + static_cast<uint32_t>(localframe);
            if (frame >= static_cast<uint32_t>(anim.numframes)) break;

            const uint8_t* cur = reinterpret_cast<const uint8_t*>(panim + 1);

            Quaternion q = rig.bones[bone].q;
            Vector3    pos = rig.bones[bone].pos;
            Vector3    scl = rig.bones[bone].scl;

            if (boneFlags & r5::DP::BONEROT) {
                Quaternion dq{ 0, 0, 0, 1 };
                r5::DP::CalcBoneQuaternion_DP(sectionlength, &cur, localframe, dq);

                if (isNonDelta) {
                    const Quaternion& bq = rig.bones[bone].q;
                    const float dot = bq.x * dq.x + bq.y * dq.y + bq.z * dq.z + bq.w * dq.w;
                    const float sign = (dot < 0.0f) ? -1.0f : 1.0f;
                    q.x = bq.x * (dq.w * sign) + bq.y * (dq.z * sign) - bq.z * (dq.y * sign) + bq.w * (dq.x * sign);
                    q.y = -bq.x * (dq.z * sign) + bq.y * (dq.w * sign) + bq.z * (dq.x * sign) + bq.w * (dq.y * sign);
                    q.z = bq.x * (dq.y * sign) - bq.y * (dq.x * sign) + bq.z * (dq.w * sign) + bq.w * (dq.z * sign);
                    q.w = -bq.x * (dq.x * sign) - bq.y * (dq.y * sign) - bq.z * (dq.z * sign) + bq.w * (dq.w * sign);
                }
                else {
                    q = dq;
                }
            }

            if (boneFlags & r5::DP::BONEPOS) {
                Vector3 dpos{ 0, 0, 0 };
                if (boneFlags & r5::DP::BONEUNK8)
                    r5::DP::CalcBonePositionVirtual_DP(sectionlength, &cur, localframe, dpos);
                else
                    r5::DP::CalcBonePosition_DP(sectionlength, &cur, localframe, dpos);

                if (isNonDelta) {
                    const Quaternion& bq = rig.bones[bone].q;

                    Quaternion posQ;
                    posQ.x = bq.x * 0.0f + bq.y * dpos.z - bq.z * dpos.y + bq.w * dpos.x;
                    posQ.y = -bq.x * dpos.z + bq.y * 0.0f + bq.z * dpos.x + bq.w * dpos.y;
                    posQ.z = bq.x * dpos.y - bq.y * dpos.x + bq.z * 0.0f + bq.w * dpos.z;
                    posQ.w = -bq.x * dpos.x - bq.y * dpos.y - bq.z * dpos.z + bq.w * 0.0f;

                    const float qix = -bq.x, qiy = -bq.y, qiz = -bq.z, qiw = bq.w;
                    const float dot2 = posQ.x * qix + posQ.y * qiy + posQ.z * qiz + posQ.w * qiw;
                    const float sign2 = (dot2 < 0.0f) ? -1.0f : 1.0f;

                    Quaternion result;
                    result.x = posQ.x * (qiw * sign2) + posQ.y * (qiz * sign2) - posQ.z * (qiy * sign2) + posQ.w * (qix * sign2);
                    result.y = -posQ.x * (qiz * sign2) + posQ.y * (qiw * sign2) + posQ.z * (qix * sign2) + posQ.w * (qiy * sign2);
                    result.z = posQ.x * (qiy * sign2) - posQ.y * (qix * sign2) + posQ.z * (qiw * sign2) + posQ.w * (qiz * sign2);
                    result.w = -posQ.x * (qix * sign2) - posQ.y * (qiy * sign2) - posQ.z * (qiz * sign2) + posQ.w * (qiw * sign2);

                    pos.x = result.x + rig.bones[bone].pos.x;
                    pos.y = result.y + rig.bones[bone].pos.y;
                    pos.z = result.z + rig.bones[bone].pos.z;
                }
                else {
                    pos = dpos;
                }
            }

            if (boneFlags & r5::DP::BONESCALE)
                r5::DP::CalcBoneScale_DP(sectionlength, &cur, localframe, scl);

            anim.animdata[bone].pos[frame] = pos;
            anim.animdata[bone].rot[frame] = q.ToRad();
            anim.animdata[bone].scl[frame] = scl;
        }

        panim = reinterpret_cast<const r5::anim::mstudio_rle_anim_t*>(reinterpret_cast<const char*>(panim) + panim->size);
    }
}

template<typename TAnimDesc>
void r5::DP::ParseDataPoint(const TAnimDesc* pAnimDesc, temp::rig_t& rig, temp::Sequence& seq, temp::animdesc_t& anim) {
    AssertMsg(!anim.asqd.buffer.empty(), "DataPoint buffer is null for '%s'", anim.asqd.path.c_str());

    constexpr bool isV121 = std::is_same_v<TAnimDesc, r5::anim::v121::mstudioanimdesc_t>;

    uint32_t  num_sections = 1;
    int32_t* animsections = nullptr;
    if (pAnimDesc->sectionindex) {
        num_sections = GetSectionCount(*pAnimDesc);
        animsections = PTR_FROM_IDX(int32_t, pAnimDesc, pAnimDesc->sectionindex);
    }

    uint32_t sectionbaseframe = 0;
    for (uint32_t section = 0; section < num_sections; section++) {
        const int sectionframes = GetSectionLength(*pAnimDesc, (int)section, (int)num_sections);

        char* pBoneFlagArray;

        if constexpr (isV121) {
            // v12.1
            pBoneFlagArray = anim.asqd.buffer.data();
            if (pAnimDesc->sectionindex && section > 0) {
                const int32_t idx = animsections[section - 1];
                if (idx < 0) {
                    const int32_t off = -1 - idx;
                    AssertMsg(!seq.extn.buffer.empty() && (size_t)off < seq.extn.size, "DP section past end of .rseq_extn '%s'", seq.extn.path.c_str());
                    pBoneFlagArray = seq.extn.buffer.data() + off;
                }
                else {
                    AssertMsg((size_t)idx < anim.asqd.buffer.size(), "DP section past end of .asqd '%s'", anim.asqd.path.c_str());
                    pBoneFlagArray = anim.asqd.buffer.data() + idx;
                }
            }
        }
        else {
            // v12
            pBoneFlagArray = PTR_FROM_IDX(char, pAnimDesc, OFFSET(pAnimDesc->animindex));
            if (pAnimDesc->sectionindex) {
                const int32_t idx = animsections[section];
                if (idx < 0) {
                    const int32_t offset = -1 - idx;
                    AssertMsg(!seq.extn.buffer.empty() && (size_t)offset < seq.extn.size, "DP section past end of .rseq_extn '%s'", seq.extn.path.c_str());
                    pBoneFlagArray = seq.extn.buffer.data() + offset;
                }
                else {
                    AssertMsg((size_t)idx < anim.asqd.buffer.size(), "DP section past end of .rseq '%s'", seq.path.c_str());
                    pBoneFlagArray = PTR_FROM_IDX(char, pAnimDesc, idx);
                }
            }
        }

        r5::DP::ParseDataPointSection(reinterpret_cast<const uint8_t*>(pBoneFlagArray), sectionframes, sectionbaseframe, rig, anim);
        sectionbaseframe += static_cast<uint32_t>(sectionframes);
    }
    
    if (!(anim.flags & ANIM_DELTA)) {
        for (int frame = 0; frame < anim.numframes; frame++) {
            Vector3& pos = anim.animdata[0].pos[frame];
            Vector3& rot = anim.animdata[0].rot[frame];

            rot.z -= M_PI2;
            const float x = pos.x;
            pos.x = pos.y;
            pos.y = -x;
        }
    }
}
template void r5::DP::ParseDataPoint<r5::anim::v12::mstudioanimdesc_t> (const r5::anim::v12::mstudioanimdesc_t*,  temp::rig_t&, temp::Sequence&, temp::animdesc_t&);
template void r5::DP::ParseDataPoint<r5::anim::v121::mstudioanimdesc_t>(const r5::anim::v121::mstudioanimdesc_t*, temp::rig_t&, temp::Sequence&, temp::animdesc_t&);

template<typename TAnimDesc>
void r5::DP::ParseFrameMovementsDP(const TAnimDesc* pAnimDesc, temp::animdesc_t& anim) {
    if (!(pAnimDesc->flags & r5::ANIM_FRAMEMOVEMENT) || !pAnimDesc->framemovementindex)
        return;

    auto* pFrameMovement = reinterpret_cast<const r5::anim::v7::mstudioframemovement_t*>(
        (const char*)pAnimDesc + OFFSET(pAnimDesc->framemovementindex));

    constexpr int baseOffset = sizeof(r5::anim::v7::mstudioframemovement_t) + sizeof(uint16_t);
    const uint8_t* panimtrack = reinterpret_cast<const uint8_t*>(pFrameMovement) + baseOffset;

    const int numframes = anim.numframes;

    temp::framemovement_t movement{};
    movement.scale = pFrameMovement->scale;
    movement.sectionframes = pFrameMovement->sectionframes;
    movement.movementdata.resize(numframes);

    // dp_pos_2x2
    {
        const uint16_t* ptrack16 = reinterpret_cast<const uint16_t*>(panimtrack);
        const uint16_t  valid16 = ptrack16[0];
        const uint16_t  total16 = ptrack16[1];
        const uint16_t* pFrameIdx = ptrack16 + 2;

        if (total16) {
            const r5::anim::AnimPos64* pPackedData  = reinterpret_cast<const r5::anim::AnimPos64*>  (total16 >= numframes ? pFrameIdx : pFrameIdx + total16);
            const r5::anim::AxisFixup_t* pAxisFixup = reinterpret_cast<const r5::anim::AxisFixup_t*>(pPackedData + valid16);

            for (int frame = 0; frame < numframes; frame++) {
                const int idx = r5::DP::ResolveFrameIndex(frame, total16, numframes, pFrameIdx);
                Vector3 pos;
                r5::DP::UnpackAnimPos64(pos, pPackedData[idx], &pAxisFixup[idx]);
                movement.movementdata[frame].x = pos.x;
                movement.movementdata[frame].y = pos.y;
                movement.movementdata[frame].z = pos.z;
            }
            panimtrack = reinterpret_cast<const uint8_t*>(pAxisFixup + total16);
        }
    }

    // dp_single_1x2
    {
        const uint16_t* ptrack16 = reinterpret_cast<const uint16_t*>(panimtrack);
        const uint16_t  total16 = ptrack16[0];
        const uint16_t* pFrameIdx = ptrack16 + 1;

        if (total16) {
            const uint16_t* pPackedData = reinterpret_cast<const uint16_t*>(total16 >= numframes ? pFrameIdx : pFrameIdx + total16);
            for (int frame = 0; frame < numframes; frame++) {
                const int idx = r5::DP::ResolveFrameIndex(frame, total16, numframes, pFrameIdx);
                movement.movementdata[frame].w = HalfToFloat(pPackedData[idx]);
            }
        }
    }

    anim.movement = movement;
}
template void r5::DP::ParseFrameMovementsDP<r5::anim::v12::mstudioanimdesc_t> (const r5::anim::v12::mstudioanimdesc_t*, temp::animdesc_t&);
template void r5::DP::ParseFrameMovementsDP<r5::anim::v121::mstudioanimdesc_t>(const r5::anim::v121::mstudioanimdesc_t*, temp::animdesc_t&);

std::vector<int32_t> GetAnimIndexes(const int32_t* pBlends, temp::Sequence& seq, int32_t numanims) {
    std::vector<int32_t> blends_index_map;
    std::unordered_map<int32_t, uint8_t> blendIndex;

    for (uint8_t i = 0; i < numanims; ++i) {
        int32_t val = pBlends[i];
        auto [it, inserted] = blendIndex.emplace(val, (uint8_t)blends_index_map.size());
        if (inserted) blends_index_map.push_back(val);
        seq.blends.push_back(it->second);
    }
    seq.numuniqueblends = static_cast<int32_t>(blends_index_map.size());
    return blends_index_map;
}

std::vector<int32_t> GetAnimIndexes(const uint16_t* pBlends, temp::Sequence& seq, int32_t numanims) {
    std::vector<int32_t> blends_index_map;
    std::unordered_map<int32_t, uint8_t> blendIndex;

    for (uint8_t i = 0; i < numanims; ++i) {
        int32_t val = OFFSET(pBlends[i]);
        auto [it, inserted] = blendIndex.emplace(val, (uint8_t)blends_index_map.size());
        if (inserted) blends_index_map.push_back(val);
        seq.blends.push_back(it->second);
    }
    seq.numuniqueblends = static_cast<int32_t>(blends_index_map.size());
    return blends_index_map;
}

template<typename TSeqDesc>
void ParsePoseKey(const TSeqDesc* pSeqDesc, temp::Sequence& seq) {
    if (!pSeqDesc->posekeyindex) return;

    const float* pPosekeys = nullptr;
    if constexpr (
        std::is_same_v<TSeqDesc, r5::anim::v7::mstudioseqdesc_t> ||
        std::is_same_v<TSeqDesc, r5::anim::v10::mstudioseqdesc_t>)
        pPosekeys = reinterpret_cast<const float*>((const char*)pSeqDesc + pSeqDesc->posekeyindex);
    else if constexpr (
        std::is_same_v<TSeqDesc, r5::anim::v11::mstudioseqdesc_t> ||
        std::is_same_v<TSeqDesc, r5::anim::v12::mstudioseqdesc_t> ||
        std::is_same_v<TSeqDesc, r5::anim::v121::mstudioseqdesc_t>)
        pPosekeys = reinterpret_cast<const float*>((const char*)pSeqDesc + OFFSET(pSeqDesc->posekeyindex));

    const int count = pSeqDesc->groupsize[0] + pSeqDesc->groupsize[1];
    seq.posekeys.insert(seq.posekeys.end(), pPosekeys, pPosekeys + count);
}
template void ParsePoseKey<r5::anim::v7::mstudioseqdesc_t>  (const r5::anim::v7::mstudioseqdesc_t*,   temp::Sequence&);
template void ParsePoseKey<r5::anim::v10::mstudioseqdesc_t> (const r5::anim::v10::mstudioseqdesc_t*,  temp::Sequence&);
template void ParsePoseKey<r5::anim::v11::mstudioseqdesc_t> (const r5::anim::v11::mstudioseqdesc_t*,  temp::Sequence&);
template void ParsePoseKey<r5::anim::v12::mstudioseqdesc_t> (const r5::anim::v12::mstudioseqdesc_t*,  temp::Sequence&);
template void ParsePoseKey<r5::anim::v121::mstudioseqdesc_t>(const r5::anim::v121::mstudioseqdesc_t*, temp::Sequence&);


template<typename TSeqDesc>
void ParseEvent(const TSeqDesc* pSeqDesc, temp::Sequence& seq) {
    if (!pSeqDesc->numevents) return;

    if constexpr (std::is_same_v<TSeqDesc, r5::anim::v7::mstudioseqdesc_t>) {
        auto* pEvents = reinterpret_cast<r5::anim::v7::mstudioevent_t*>((char*)pSeqDesc + pSeqDesc->eventindex);
        for (int i = 0; i < pSeqDesc->numevents; i++) {
            if (!pEvents[i].szeventindex) continue;
            temp::seqevent_t event{};
            event.name = STRING_FROM_IDX(&pEvents[i], pEvents[i].szeventindex);
            event.cycle = pEvents[i].cycle;
            event.event = pEvents[i].event;
            event.type = pEvents[i].type;
            event.options = std::string(pEvents[i].options);
            seq.events.push_back(event);
        }
    }
    else if constexpr (std::is_same_v<TSeqDesc, r5::anim::v10::mstudioseqdesc_t>) {
        bool b2ndEvent = (pSeqDesc->weightlistindex - pSeqDesc->eventindex) % 272 != 0;
        if (!(pSeqDesc->numevents % 68)) {
            printf("[!] Warning: events might be wrong, needed to check it manually.\n");
            b2ndEvent = true;
        }
        for (int i = 0; i < pSeqDesc->numevents; i++) {
            temp::seqevent_t event{};
            if (b2ndEvent) {
                auto* pEvents = reinterpret_cast<r5::anim::v10::mstudioevent_2_t*>((char*)pSeqDesc + pSeqDesc->eventindex);
                event.name = STRING_FROM_IDX(&pEvents[i], pEvents[i].szeventindex);
                event.cycle = pEvents[i].cycle;
                event.event = pEvents[i].event;
                event.type = pEvents[i].type;
                event.options = std::string(pEvents[i].options);
            }
            else {
                auto* pEvents = reinterpret_cast<r5::anim::v10::mstudioevent_t*>((char*)pSeqDesc + pSeqDesc->eventindex);
                event.name = STRING_FROM_IDX(&pEvents[i], pEvents[i].szeventindex);
                event.cycle = pEvents[i].cycle;
                event.event = pEvents[i].event;
                event.type = pEvents[i].type;
                event.options = std::string(pEvents[i].options);
            }
            seq.events.push_back(event);
        }
    }
    else if constexpr (
        std::is_same_v<TSeqDesc, r5::anim::v11::mstudioseqdesc_t> ||
        std::is_same_v<TSeqDesc, r5::anim::v12::mstudioseqdesc_t> ||
        std::is_same_v<TSeqDesc, r5::anim::v121::mstudioseqdesc_t>) {
        auto* pEvents = reinterpret_cast<r5::anim::v11::mstudioevent_t*>((char*)pSeqDesc + OFFSET(pSeqDesc->eventindex));
        for (int i = 0; i < pSeqDesc->numevents; i++) {
            if (!pEvents[i].szeventindex) continue;
            temp::seqevent_t event{};
            event.name = STRING_FROM_IDX(&pEvents[i], OFFSET(pEvents[i].szeventindex));
            event.cycle = pEvents[i].cycle;
            event.event = pEvents[i].event;
            event.type = pEvents[i].type;
            event.options = STRING_FROM_IDX(&pEvents[i], OFFSET(pEvents[i].optionsindex));
            seq.events.push_back(event);
        }
    }
}
template void ParseEvent<r5::anim::v7::mstudioseqdesc_t>  (const r5::anim::v7::mstudioseqdesc_t*,   temp::Sequence&);
template void ParseEvent<r5::anim::v10::mstudioseqdesc_t> (const r5::anim::v10::mstudioseqdesc_t*,  temp::Sequence&);
template void ParseEvent<r5::anim::v11::mstudioseqdesc_t> (const r5::anim::v11::mstudioseqdesc_t*,  temp::Sequence&);
template void ParseEvent<r5::anim::v12::mstudioseqdesc_t> (const r5::anim::v12::mstudioseqdesc_t*,  temp::Sequence&);
template void ParseEvent<r5::anim::v121::mstudioseqdesc_t>(const r5::anim::v121::mstudioseqdesc_t*, temp::Sequence&);


template<typename TSeqDesc>
void ParseAutoLayer(const TSeqDesc* pSeqDesc, temp::Sequence& seq) {
    if constexpr (
        std::is_same_v<TSeqDesc, r5::anim::v7::mstudioseqdesc_t> ||
        std::is_same_v<TSeqDesc, r5::anim::v10::mstudioseqdesc_t>) {
        auto* pAutolayer = reinterpret_cast<r5::anim::v7::mstudioautolayer_t*>((char*)pSeqDesc + pSeqDesc->autolayerindex);
        for (int i = 0; i < pSeqDesc->numautolayers; i++) {
            temp::autolayer_t autolayer{};
            autolayer.guidSequence = pAutolayer[i].guidSequence;
            autolayer.iSequence = pAutolayer[i].iSequence;
            autolayer.iPose = pAutolayer[i].iPose;
            autolayer.flags = pAutolayer[i].flags;
            autolayer.start = pAutolayer[i].start;
            autolayer.peak = pAutolayer[i].peak;
            autolayer.tail = pAutolayer[i].tail;
            autolayer.end = pAutolayer[i].end;
            seq.autolayers.push_back(autolayer);
        }
    }
    else if constexpr (
        std::is_same_v<TSeqDesc, r5::anim::v11::mstudioseqdesc_t> ||
        std::is_same_v<TSeqDesc, r5::anim::v12::mstudioseqdesc_t> ||
        std::is_same_v<TSeqDesc, r5::anim::v121::mstudioseqdesc_t>) {
        auto* pAutolayer = reinterpret_cast<r5::anim::v11::mstudioautolayer_t*>((char*)pSeqDesc + OFFSET(pSeqDesc->autolayerindex));
        for (int i = 0; i < pSeqDesc->numautolayers; i++) {
            temp::autolayer_t autolayer{};
            autolayer.guidSequence = pAutolayer[i].guidSequence;
            autolayer.iPose = pAutolayer[i].iPose;
            autolayer.flags = pAutolayer[i].flags;
            autolayer.start = pAutolayer[i].start;
            autolayer.peak = pAutolayer[i].peak;
            autolayer.tail = pAutolayer[i].tail;
            autolayer.end = pAutolayer[i].end;
            seq.autolayers.push_back(autolayer);
        }
    }
}
template void ParseAutoLayer<r5::anim::v7::mstudioseqdesc_t>  (const r5::anim::v7::mstudioseqdesc_t*,   temp::Sequence&);
template void ParseAutoLayer<r5::anim::v10::mstudioseqdesc_t> (const r5::anim::v10::mstudioseqdesc_t*,  temp::Sequence&);
template void ParseAutoLayer<r5::anim::v11::mstudioseqdesc_t> (const r5::anim::v11::mstudioseqdesc_t*,  temp::Sequence&);
template void ParseAutoLayer<r5::anim::v12::mstudioseqdesc_t> (const r5::anim::v12::mstudioseqdesc_t*,  temp::Sequence&);
template void ParseAutoLayer<r5::anim::v121::mstudioseqdesc_t>(const r5::anim::v121::mstudioseqdesc_t*, temp::Sequence&);


template<typename TSeqDesc>
void ParseWeightList(const TSeqDesc* pSeqDesc, temp::Sequence& seq) {
    if constexpr (
        std::is_same_v<TSeqDesc, r5::anim::v7::mstudioseqdesc_t> ||
        std::is_same_v<TSeqDesc, r5::anim::v10::mstudioseqdesc_t>) {
        const auto* pWeightList = reinterpret_cast<const float*>((const char*)pSeqDesc + pSeqDesc->weightlistindex);
        std::memcpy(seq.weightlist.data(), pWeightList, seq.weightlist.size() * sizeof(float));
    }
    else if constexpr (
        std::is_same_v<TSeqDesc, r5::anim::v11::mstudioseqdesc_t> ||
        std::is_same_v<TSeqDesc, r5::anim::v12::mstudioseqdesc_t> ||
        std::is_same_v<TSeqDesc, r5::anim::v121::mstudioseqdesc_t>) {
        const auto idx = pSeqDesc->weightlistindex;
        if (idx && idx != 1 && idx != 3 && idx != 5) {
            const auto* pWeightList = reinterpret_cast<const float*>((const char*)pSeqDesc + OFFSET(idx));
            std::memcpy(seq.weightlist.data(), pWeightList, seq.weightlist.size() * sizeof(float));
        }
        else {
            std::fill(seq.weightlist.begin(), seq.weightlist.end(), 1.0f);
        }
    }
}
template void ParseWeightList<r5::anim::v7::mstudioseqdesc_t>  (const r5::anim::v7::mstudioseqdesc_t*,   temp::Sequence&);
template void ParseWeightList<r5::anim::v10::mstudioseqdesc_t> (const r5::anim::v10::mstudioseqdesc_t*,  temp::Sequence&);
template void ParseWeightList<r5::anim::v11::mstudioseqdesc_t> (const r5::anim::v11::mstudioseqdesc_t*,  temp::Sequence&);
template void ParseWeightList<r5::anim::v12::mstudioseqdesc_t> (const r5::anim::v12::mstudioseqdesc_t*,  temp::Sequence&);
template void ParseWeightList<r5::anim::v121::mstudioseqdesc_t>(const r5::anim::v121::mstudioseqdesc_t*, temp::Sequence&);


template<typename TSeqDesc>
void ParseActMod(const TSeqDesc* pSeqDesc, temp::Sequence& seq) {
    if (!pSeqDesc->numactivitymodifiers) return;

    if constexpr (
        std::is_same_v<TSeqDesc, r5::anim::v7::mstudioseqdesc_t> ||
        std::is_same_v<TSeqDesc, r5::anim::v10::mstudioseqdesc_t>) {
        auto* pActMod = reinterpret_cast<r5::anim::v7::mstudioactivitymodifier_t*>(
            (char*)pSeqDesc + pSeqDesc->activitymodifierindex);
        for (int i = 0; i < pSeqDesc->numactivitymodifiers; i++) {
            temp::actmod_t actmod{};
            actmod.name = STRING_FROM_IDX(&pActMod[i], pActMod[i].sznameindex);
            actmod.negate = pActMod->negate;
            seq.actmods.push_back(actmod);
        }
    }
    else if constexpr (
        std::is_same_v<TSeqDesc, r5::anim::v11::mstudioseqdesc_t> ||
        std::is_same_v<TSeqDesc, r5::anim::v12::mstudioseqdesc_t> ||
        std::is_same_v<TSeqDesc, r5::anim::v121::mstudioseqdesc_t>) {
        auto* pActMod = reinterpret_cast<r5::anim::v11::mstudioactivitymodifier_t*>(
            (char*)pSeqDesc + OFFSET(pSeqDesc->activitymodifierindex));
        for (int i = 0; i < pSeqDesc->numactivitymodifiers; i++) {
            temp::actmod_t actmod{};
            actmod.name = STRING_FROM_IDX(&pActMod[i], OFFSET(pActMod[i].sznameindex));
            actmod.negate = pActMod->negate;
            seq.actmods.push_back(actmod);
        }
    }
}
template void ParseActMod<r5::anim::v7::mstudioseqdesc_t>  (const r5::anim::v7::mstudioseqdesc_t*,   temp::Sequence&);
template void ParseActMod<r5::anim::v10::mstudioseqdesc_t> (const r5::anim::v10::mstudioseqdesc_t*,  temp::Sequence&);
template void ParseActMod<r5::anim::v11::mstudioseqdesc_t> (const r5::anim::v11::mstudioseqdesc_t*,  temp::Sequence&);
template void ParseActMod<r5::anim::v12::mstudioseqdesc_t> (const r5::anim::v12::mstudioseqdesc_t*,  temp::Sequence&);
template void ParseActMod<r5::anim::v121::mstudioseqdesc_t>(const r5::anim::v121::mstudioseqdesc_t*, temp::Sequence&);


template<typename TAnimDesc>
void RLE::ParseIkrules(const TAnimDesc* pAnimDesc, temp::animdesc_t& anim) {
    AssertMsg(pAnimDesc, "pAnimDesc is null for '%s'", anim.name.c_str());
    if (!pAnimDesc->numikrules) return;

    if constexpr (
        std::is_same_v<TAnimDesc, r5::anim::v71::mstudioanimdesc_t> ||
        std::is_same_v<TAnimDesc, r5::anim::v10::mstudioanimdesc_t>) {
        auto* ikrules = reinterpret_cast<r5::anim::v7::mstudioikrule_t*>((char*)pAnimDesc + pAnimDesc->ikruleindex);
        for (int i = 0; i < pAnimDesc->numikrules; i++) {
            temp::ikrule_t ikrule{};
            ikrule.index = ikrules[i].index;
            ikrule.type = ikrules[i].type;
            ikrule.chain = ikrules[i].chain;
            ikrule.bone = ikrules[i].bone;
            ikrule.slot = ikrules[i].slot;
            ikrule.height = ikrules[i].height;
            ikrule.radius = ikrules[i].radius;
            ikrule.floor = ikrules[i].floor;
            ikrule.pos = ikrules[i].pos;
            ikrule.q = ikrules[i].q;
            for (int j = 0; j < 6; j++) ikrule.scale[j] = ikrules[i].scale[j];
            ikrule.sectionframes = ikrules[i].sectionframes;
            ikrule.iStart = ikrules[i].iStart;
            ikrule.start = ikrules[i].start;
            ikrule.peak = ikrules[i].peak;
            ikrule.tail = ikrules[i].tail;
            ikrule.end = ikrules[i].end;
            ikrule.contact = ikrules[i].contact;
            ikrule.drop = ikrules[i].drop;
            ikrule.top = ikrules[i].top;
            ikrule.endHeight = ikrules[i].endHeight;
            if (ikrules[i].szattachmentindex)
                ikrule.attachmentname = STRING_FROM_IDX(&ikrules[i], ikrules[i].szattachmentindex);

            if (ikrules[i].sectionframes) {
                ikrule.ikruledata.resize(anim.numframes);
                const uint32_t sectioncount = static_cast<uint32_t>((float)(anim.numframes - 1) / (float)ikrules[i].sectionframes) + 1;
                auto* sectionindices = reinterpret_cast<int32_t*>((char*)&ikrules[i] + ikrules[i].compressedikerrorindex);

                uint32_t sectionbaseframe = 0;
                for (uint32_t section = 0; section < sectioncount; section++) {
                    const uint32_t sectionframes = GetSectionLength(anim.numframes, ikrules[i].sectionframes, section);
                    auto* offsets = reinterpret_cast<int16_t*>((char*)&ikrules[i] + sectionindices[section]);

                    for (uint32_t localframe = 0; localframe < sectionframes; localframe++) {
                        const uint32_t frame = sectionbaseframe + localframe;
                        for (int idx = 0; idx < 3; idx++) {
                            auto* panimvalue = reinterpret_cast<r5::anim::mstudioanimvalue_t*>((char*)offsets + offsets[idx]);
                            RLE::ExtractAnimValue(localframe, panimvalue, ikrules[i].scale[idx], ikrule.ikruledata.pos[frame][idx]);
                        }
                    }
                    sectionbaseframe += sectionframes;
                }
            }
            anim.ikrules.push_back(ikrule);
        }
    }
    else if constexpr (
        std::is_same_v<TAnimDesc, r5::anim::v11::mstudioanimdesc_t> ||
        std::is_same_v<TAnimDesc, r5::anim::v12::mstudioanimdesc_t> ||
        std::is_same_v<TAnimDesc, r5::anim::v121::mstudioanimdesc_t>) {
        if (pAnimDesc->ikruleindex == 3 || pAnimDesc->ikruleindex == 5) return;

        auto* ikrules = reinterpret_cast<r5::anim::v11::mstudioikrule_t*>((char*)pAnimDesc + OFFSET(pAnimDesc->ikruleindex));
        for (int i = 0; i < pAnimDesc->numikrules; i++) {
            temp::ikrule_t ikrule{};
            ikrule.type = ikrules[i].type;
            ikrule.chain = ikrules[i].chain;
            ikrule.bone = ikrules[i].bone;
            ikrule.slot = ikrules[i].slot;
            ikrule.height = ikrules[i].height;
            ikrule.radius = ikrules[i].radius;
            ikrule.floor = ikrules[i].floor;
            ikrule.pos = ikrules[i].pos;
            ikrule.q = ikrules[i].q;
            for (int j = 0; j < 6; j++) ikrule.scale[j] = ikrules[i].scale[j];
            ikrule.sectionframes = ikrules[i].sectionframes;
            ikrule.iStart = ikrules[i].iStart;
            ikrule.start = ikrules[i].start;
            ikrule.peak = ikrules[i].peak;
            ikrule.tail = ikrules[i].tail;
            ikrule.end = ikrules[i].end;
            ikrule.contact = ikrules[i].contact;
            ikrule.drop = ikrules[i].drop;
            ikrule.top = ikrules[i].top;
            ikrule.endHeight = ikrules[i].endHeight;
            if (ikrules[i].szattachmentindex)
                ikrule.attachmentname = STRING_FROM_IDX(&ikrules[i], OFFSET(ikrules[i].szattachmentindex));

            if (ikrules[i].sectionframes) {
                ikrule.ikruledata.resize(anim.numframes);
                const uint32_t sectioncount = static_cast<uint32_t>((float)(anim.numframes - 1) / (float)ikrules[i].sectionframes) + 1;
                auto* sectionindices = reinterpret_cast<uint16_t*>((char*)&ikrules[i] + OFFSET(ikrules[i].compressedikerrorindex));

                uint32_t sectionbaseframe = 0;
                for (uint32_t section = 0; section < sectioncount; section++) {
                    const uint32_t sectionframes = GetSectionLength(anim.numframes, ikrules[i].sectionframes, section);
                    auto* offsets = reinterpret_cast<int16_t*>((char*)&ikrules[i] + OFFSET(sectionindices[section]));

                    for (uint32_t localframe = 0; localframe < sectionframes; localframe++) {
                        const uint32_t frame = sectionbaseframe + localframe;
                        for (int idx = 0; idx < 3; idx++) {
                            auto* panimvalue = reinterpret_cast<r5::anim::mstudioanimvalue_t*>((char*)offsets + offsets[idx]);
                            RLE::ExtractAnimValue(localframe, panimvalue, ikrules[i].scale[idx], ikrule.ikruledata.pos[frame][idx]);
                        }
                    }
                    sectionbaseframe += sectionframes;
                }
            }
            anim.ikrules.push_back(ikrule);
        }
    }
}
template void RLE::ParseIkrules<r5::anim::v71::mstudioanimdesc_t> (const r5::anim::v71::mstudioanimdesc_t*,  temp::animdesc_t&);
template void RLE::ParseIkrules<r5::anim::v10::mstudioanimdesc_t> (const r5::anim::v10::mstudioanimdesc_t*,  temp::animdesc_t&);
template void RLE::ParseIkrules<r5::anim::v11::mstudioanimdesc_t> (const r5::anim::v11::mstudioanimdesc_t*,  temp::animdesc_t&);
template void RLE::ParseIkrules<r5::anim::v12::mstudioanimdesc_t> (const r5::anim::v12::mstudioanimdesc_t*,  temp::animdesc_t&);
template void RLE::ParseIkrules<r5::anim::v121::mstudioanimdesc_t>(const r5::anim::v121::mstudioanimdesc_t*, temp::animdesc_t&);


template<typename TAnimDesc>
void RLE::ParseFrameMovements(const TAnimDesc* pAnimDesc, temp::animdesc_t& anim) {
    AssertMsg(pAnimDesc, "pAnimDesc is null for '%s'", anim.name.c_str());
    if (!(pAnimDesc->flags & r5::ANIM_FRAMEMOVEMENT) || !pAnimDesc->framemovementindex) return;

    if constexpr (
        std::is_same_v<TAnimDesc, r5::anim::v71::mstudioanimdesc_t> ||
        std::is_same_v<TAnimDesc, r5::anim::v10::mstudioanimdesc_t>) {
        auto* pFrameMovement = reinterpret_cast<r5::anim::v7::mstudioframemovement_t*>((char*)pAnimDesc + pAnimDesc->framemovementindex);
        auto* sectionindices = reinterpret_cast<int32_t*>((char*)pFrameMovement + sizeof(r5::anim::v7::mstudioframemovement_t));
        const uint32_t sectioncount = static_cast<uint32_t>((float)(anim.numframes - 1) / (float)pFrameMovement->sectionframes) + 1;

        temp::framemovement_t movement{};
        movement.scale = pFrameMovement->scale;
        movement.sectionframes = pFrameMovement->sectionframes;
        movement.movementdata.resize(anim.numframes);

        uint32_t sectionbaseframe = 0;
        for (uint32_t section = 0; section < sectioncount; section++) {
            const uint32_t sectionframes = GetSectionLength(anim.numframes, pFrameMovement->sectionframes, section);
            auto* offsets = reinterpret_cast<int16_t*>((char*)pFrameMovement + sectionindices[section]);

            for (uint32_t localframe = 0; localframe < sectionframes; localframe++) {
                const uint32_t frame = sectionbaseframe + localframe;
                for (int idx = 0; idx < 4; idx++) {
                    if (!offsets[idx]) continue;
                    auto* panimvalue = reinterpret_cast<r5::anim::mstudioanimvalue_t*>((char*)offsets + offsets[idx]);
                    RLE::ExtractAnimValue(localframe, panimvalue, pFrameMovement->scale[idx], movement.movementdata[frame][idx]);
                }
            }
            sectionbaseframe += sectionframes;
        }
        anim.movement = movement;
    }
    else if constexpr (
        std::is_same_v<TAnimDesc, r5::anim::v11::mstudioanimdesc_t> ||
        std::is_same_v<TAnimDesc, r5::anim::v12::mstudioanimdesc_t> ||
        std::is_same_v<TAnimDesc, r5::anim::v121::mstudioanimdesc_t>) {
        if (pAnimDesc->flags & ANIM_DATAPOINT) return;

        auto* pFrameMovement = reinterpret_cast<r5::anim::v7::mstudioframemovement_t*>((char*)pAnimDesc + OFFSET(pAnimDesc->framemovementindex));
        auto* sectionindices = reinterpret_cast<uint16_t*>((char*)pFrameMovement + sizeof(r5::anim::v7::mstudioframemovement_t));
        const uint32_t sectioncount = static_cast<uint32_t>((float)(anim.numframes - 1) / (float)pFrameMovement->sectionframes) + 1;

        temp::framemovement_t movement{};
        movement.scale = pFrameMovement->scale;
        movement.sectionframes = pFrameMovement->sectionframes;
        movement.movementdata.resize(anim.numframes);

        uint32_t sectionbaseframe = 0;
        for (uint32_t section = 0; section < sectioncount; section++) {
            const uint32_t sectionframes = GetSectionLength(anim.numframes, pFrameMovement->sectionframes, section);
            auto* offsets = reinterpret_cast<int16_t*>((char*)pFrameMovement + OFFSET(sectionindices[section]));

            for (uint32_t localframe = 0; localframe < sectionframes; localframe++) {
                const uint32_t frame = sectionbaseframe + localframe;
                for (int idx = 0; idx < 4; idx++) {
                    if (!offsets[idx]) continue;
                    auto* panimvalue = reinterpret_cast<r5::anim::mstudioanimvalue_t*>((char*)offsets + offsets[idx]);
                    RLE::ExtractAnimValue(localframe, panimvalue, pFrameMovement->scale[idx], movement.movementdata[frame][idx]);
                }
            }
            sectionbaseframe += sectionframes;
        }
        anim.movement = movement;
    }
}
template void RLE::ParseFrameMovements<r5::anim::v71::mstudioanimdesc_t> (const r5::anim::v71::mstudioanimdesc_t*,  temp::animdesc_t&);
template void RLE::ParseFrameMovements<r5::anim::v10::mstudioanimdesc_t> (const r5::anim::v10::mstudioanimdesc_t*,  temp::animdesc_t&);
template void RLE::ParseFrameMovements<r5::anim::v11::mstudioanimdesc_t> (const r5::anim::v11::mstudioanimdesc_t*,  temp::animdesc_t&);
template void RLE::ParseFrameMovements<r5::anim::v12::mstudioanimdesc_t> (const r5::anim::v12::mstudioanimdesc_t*,  temp::animdesc_t&);
template void RLE::ParseFrameMovements<r5::anim::v121::mstudioanimdesc_t>(const r5::anim::v121::mstudioanimdesc_t*, temp::animdesc_t&);


// ============================================================================
//  Write helpers
// ============================================================================

static void FitPoly(const int16_t* vals, int N, int deg, int16_t* coeffs_out, int& ncoeffs_out, float& max_err_out) {
    const int nc = deg + 1;
    ncoeffs_out = nc;
    double ATA[36] = {}, ATb[6] = {};
    for (int f = 0; f < N; f++) {
        const double t = (N > 1) ? (double)f / (N - 1) : 0.0;
        double row[6]; row[0] = 1.0;
        for (int d = 1; d < nc; d++) row[d] = row[d - 1] * t;
        for (int r = 0; r < nc; r++) {
            ATb[r] += row[r] * vals[f];
            for (int c = 0; c < nc; c++) ATA[r * nc + c] += row[r] * row[c];
        }
    }

    double aug[42] = {};
    for (int r = 0; r < nc; r++) {
        for (int c = 0; c < nc; c++) aug[r * (nc + 1) + c] = ATA[r * nc + c];
        aug[r * (nc + 1) + nc] = ATb[r];
    }
    for (int col = 0; col < nc; col++) {
        int pivot = col;
        for (int r = col + 1; r < nc; r++)
            if (std::abs(aug[r * (nc + 1) + col]) > std::abs(aug[pivot * (nc + 1) + col])) pivot = r;
        if (pivot != col)
            for (int c = 0; c <= nc; c++) std::swap(aug[col * (nc + 1) + c], aug[pivot * (nc + 1) + c]);
        const double den = aug[col * (nc + 1) + col];
        if (std::abs(den) < 1e-12) continue;
        for (int r = col + 1; r < nc; r++) {
            const double f = aug[r * (nc + 1) + col] / den;
            for (int c = col; c <= nc; c++) aug[r * (nc + 1) + c] -= f * aug[col * (nc + 1) + c];
        }
    }
    double x[6] = {};
    for (int r = nc - 1; r >= 0; r--) {
        double v = aug[r * (nc + 1) + nc];
        for (int c = r + 1; c < nc; c++) v -= aug[r * (nc + 1) + c] * x[c];
        x[r] = (std::abs(aug[r * (nc + 1) + r]) > 1e-12) ? v / aug[r * (nc + 1) + r] : 0.0;
    }
    for (int i = 0; i < nc; i++) coeffs_out[i] = (int16_t)std::round(x[i]);

    max_err_out = 0.0f;
    for (int f = 0; f < N; f++) {
        const double t = (N > 1) ? (double)f / (N - 1) : 0.0;
        double tpow = 1.0, v = coeffs_out[0];
        for (int i = 1; i < nc; i++) { tpow *= t; v += coeffs_out[i] * tpow; }
        max_err_out = std::max(max_err_out, (float)std::abs(v - vals[f]));
    }
}

void WriteCompressedAnim(char*& pData, uint8_t type, int N, const int16_t* qv_block, const int16_t* poly_coeffs, int ncoeffs) {
    Assert(N > 0 && N <= 255);

    comptypes[type]++; // store statistics

    auto* hdr = reinterpret_cast<r5::anim::mstudioanimvalue_t*>(pData);
    pData += 2;
    hdr->meta.type = type;
    hdr->meta.total = (uint8_t)N;

    switch (type) {
    case 0: {
        for (int i = 0; i < N; i++) {
            reinterpret_cast<r5::anim::mstudioanimvalue_t*>(pData)->value = qv_block[i];
            pData += 2;
        }
        break;
    }
    case 1: {
        const int16_t base = qv_block[0];
        reinterpret_cast<r5::anim::mstudioanimvalue_t*>(pData)->value = base;
        pData += 2;
        for (int i = 1; i < N; i++) {
            *reinterpret_cast<int8_t*>(pData) = (int8_t)std::clamp((int)qv_block[i] - (int)base, -128, 127);
            pData += 1;
        }
        break;
    }
    case 2: {
        reinterpret_cast<r5::anim::mstudioanimvalue_t*>(pData)->value = qv_block[0];
        pData += 2;
        break;
    }
    default: {
        for (int i = 0; i < ncoeffs; i++) {
            reinterpret_cast<r5::anim::mstudioanimvalue_t*>(pData)->value = poly_coeffs[i];
            pData += 2;
        }
        break;
    }
    }
    
    ALIGN2(pData);
}

static int BlockByteCost(uint8_t type, int N) {
    switch (type) {
    case 0: return 2 + N * 2;
    case 1: return 2 + 2 + ((N - 1 + 1) & ~1);
    case 2: return 2 + 2;
    default: return 2 + (type - 1) * 2;
    }
}

template<typename TVecType>
void WriteAnimData(char*& pData, const std::vector<TVecType>& rawdata, uint32_t startframe, uint32_t endframe, int axis, float scale) {
    if (startframe >= endframe) return;

    const int total_frames = (int)(endframe - startframe);
    std::vector<int16_t> qv(total_frames);
    for (int i = 0; i < total_frames; i++)
        qv[i] = (int16_t)std::round(rawdata[startframe + i][axis] / scale);

    const bool bUseCompression = g_AnimCompressError > 0.0f;

    auto emit_block = [&](const temp::animblock_t& blk) {
        const int n = blk.end - blk.start;
        if (n <= 255) {
            WriteCompressedAnim(pData, blk.type, n, qv.data() + blk.start, blk.coeffs, blk.ncoeffs);
        } else {
            const uint8_t chunk_type = (blk.type == 1 || blk.type >= 3) ? (uint8_t)0 : blk.type;
            for (int off = 0; off < n; ) {
                const int chunk = std::min(255, n - off);
                WriteCompressedAnim(pData, chunk_type, chunk, qv.data() + blk.start + off, nullptr, 0);
                off += chunk;
            }
        }
        };

    std::vector<temp::animblock_t> blocks;
    blocks.reserve(64);

    int pos = 0;
    while (pos < total_frames) {
        if (pos + 2 < total_frames) {
            int cend = pos + 1;
            while (cend < total_frames && qv[cend] == qv[pos]) cend++;
            if (cend - pos >= 3) {
                blocks.push_back({ 2, pos, cend });
                pos = cend;
                continue;
            }
        }

        int t1end = pos + 1;
        while (t1end < total_frames && (t1end - pos) < 255) {
            int d = (int)qv[t1end] - (int)qv[pos];
            if (d < -128 || d > 127) break;
            t1end++;
        }

        if (t1end >= total_frames && t1end - pos >= 3) {
            blocks.push_back({ 1, pos, t1end });
            pos = t1end;
            continue;
        }

        if (t1end - pos >= 3 && t1end < total_frames) {
            int scan = t1end;
            while (scan < total_frames) {
                if (scan + 2 < total_frames && qv[scan] == qv[scan + 1] && qv[scan] == qv[scan + 2]) break;
                if (scan + 2 < total_frames) {
                    int d1 = (int)qv[scan + 1] - (int)qv[scan];
                    int d2 = (int)qv[scan + 2] - (int)qv[scan];
                    if (d1 >= -128 && d1 <= 127 && d2 >= -128 && d2 <= 127) break;
                }
                scan++;
            }

            bool found_poly = false;
            if (scan - pos > 2) {
                const int pN = scan - pos;
                const int t1_cost = BlockByteCost(1, t1end - pos);
                const int rest_cost = BlockByteCost(0, scan - t1end);
                const int separate_cost = t1_cost + rest_cost;

                if (bUseCompression) {
                    for (int deg = 1; deg <= std::min(5, pN - 1); deg++) {
                        int16_t c[6]; int nc; float err;
                        FitPoly(qv.data() + pos, pN, deg, c, nc, err);
                        if (err < g_AnimCompressError) {
                            const int poly_sz = BlockByteCost((uint8_t)(2 + deg), pN);
                            if (poly_sz < separate_cost) {
                                temp::animblock_t ab{ (uint8_t)(2 + deg), pos, scan };
                                memcpy(ab.coeffs, c, nc * sizeof(int16_t));
                                ab.ncoeffs = (uint8_t)nc;
                                blocks.push_back(ab);
                                pos = scan;
                                found_poly = true;
                            }
                            break;
                        }
                    }
                }
            }
            if (found_poly) continue;

            {
                const int t1N = t1end - pos;
                const int t1_cost = BlockByteCost(1, t1N);
                if (bUseCompression) {
                    for (int deg = 1; deg <= std::min(5, t1N - 1); deg++) {
                        int16_t c[6]; int nc; float err;
                        FitPoly(qv.data() + pos, t1N, deg, c, nc, err);
                        if (err < g_AnimCompressError) {
                            const int poly_sz = BlockByteCost((uint8_t)(2 + deg), t1N);
                            if (poly_sz < t1_cost) {
                                temp::animblock_t ab{ (uint8_t)(2 + deg), pos, t1end };
                                memcpy(ab.coeffs, c, nc * sizeof(int16_t));
                                ab.ncoeffs = (uint8_t)nc;
                                blocks.push_back(ab);
                                pos = t1end;
                                found_poly = true;
                            }
                            break;
                        }
                    }
                }
            }
            if (found_poly) continue;

            blocks.push_back({ 1, pos, t1end });
            pos = t1end;
            continue;
        }

        int rend = pos + 1;
        while (rend < total_frames) {
            if (rend + 2 < total_frames && qv[rend] == qv[rend + 1] && qv[rend] == qv[rend + 2]) break;
            if (rend + 2 < total_frames) {
                int d1 = (int)qv[rend + 1] - (int)qv[rend];
                int d2 = (int)qv[rend + 2] - (int)qv[rend];
                if (d1 >= -128 && d1 <= 127 && d2 >= -128 && d2 <= 127) break;
            }
            rend++;
        }

        const int rN = rend - pos;
        if (rN > 2) {
            bool found_poly = false;
            const int raw_cost = BlockByteCost(0, rN);
            if (bUseCompression) {
                for (int deg = 1; deg <= std::min(5, rN - 1); deg++) {
                    int16_t c[6]; int nc; float err;
                    FitPoly(qv.data() + pos, rN, deg, c, nc, err);
                    if (err < g_AnimCompressError) {
                        const int poly_sz = BlockByteCost((uint8_t)(2 + deg), rN);
                        if (poly_sz < raw_cost) {
                            temp::animblock_t ab{ (uint8_t)(2 + deg), pos, rend };
                            memcpy(ab.coeffs, c, nc * sizeof(int16_t));
                            ab.ncoeffs = (uint8_t)nc;
                            blocks.push_back(ab);
                            pos = rend;
                            found_poly = true;
                        }
                        break;
                    }
                }
            }
            if (found_poly) continue;
        }

        blocks.push_back({ 0, pos, rend });
        pos = rend;
    }

    std::vector<temp::animblock_t> processed;
    processed.reserve(blocks.size() + 16);

    for (int bi2 = 0; bi2 < (int)blocks.size(); bi2++) {
        auto& rb = blocks[bi2];

        if (rb.type >= 3) {
            processed.push_back(rb);
            continue;
        }

        if (rb.type == 1) {
            const int a = rb.start, b = rb.end;
            const int N = b - a;
            const int nDiffs = N - 1;

            if (N >= 8) {
                int best_pi = -1, best_plen = 0, best_save = 0;
                const int orig_cost = BlockByteCost(1, N);
                int i = 0;
                while (i < nDiffs) {
                    const int16_t d = (int16_t)((int)qv[a + i + 1] - (int)qv[a]);
                    int plen = 1;
                    while (i + plen < nDiffs && (int16_t)((int)qv[a + i + plen + 1] - (int)qv[a]) == d)
                        plen++;

                    int split_cost = BlockByteCost(2, plen);
                    if (i > 0)            split_cost += BlockByteCost(1, i + 1);
                    if (i + plen < nDiffs) split_cost += BlockByteCost(1, nDiffs - i - plen + 1);
                    const int net = orig_cost - split_cost;

                    if (net > best_save) { best_save = net; best_pi = i; best_plen = plen; }
                    i += plen;
                }

                if (best_pi >= 0) {
                    const int16_t pval = (int16_t)((int)qv[a] + (int)((int16_t)((int)qv[a + best_pi + 1] - (int)qv[a])));
                    int ps = a + best_pi + 1;
                    int pe = a + best_pi + best_plen + 1;
                    if (best_pi == 0 && qv[a] == pval) ps = a;

                    bool split_valid = true;
                    if (pe < b) {
                        const int16_t tail_base = qv[pe];
                        for (int vi = pe + 1; vi < b; vi++) {
                            int d = (int)qv[vi] - (int)tail_base;
                            if (d < -128 || d > 127) { split_valid = false; break; }
                        }
                    }

                    if (split_valid) {
                        if (ps > a) processed.push_back({ 1, a, ps });
                        processed.push_back({ 2, ps, pe });
                        if (pe < b) processed.push_back({ 1, pe, b });
                        continue;
                    }
                }
            }
        }

        if (rb.type == 0 && !processed.empty() && processed.back().type == 0 && processed.back().end == rb.start)
            processed.back().end = rb.end;
        else
            processed.push_back(rb);
    }

    const int nblocks = (int)processed.size();
    int bi = 0;
    while (bi < nblocks) {
        auto& blk = processed[bi];

        if (blk.type >= 3 || blk.type == 2) {
            emit_block(blk);
            bi++;
            continue;
        }

        int merge_end = bi + 1;
        while (merge_end < nblocks && processed[merge_end].type < 3 && processed[merge_end].type != 2 &&
               processed[merge_end].start == processed[merge_end - 1].end)
            merge_end++;

        const int ms = processed[bi].start;
        const int me = processed[merge_end - 1].end;
        const int mN = me - ms;

        if (mN > 2 && merge_end - bi > 1) {
            int merge_cost = 0;
            for (int j = bi; j < merge_end; j++)
                merge_cost += BlockByteCost(processed[j].type, processed[j].end - processed[j].start);

            if (bUseCompression) {
                for (int deg = 1; deg <= std::min(5, mN - 1); deg++) {
                    int16_t c[6]; int nc; float err;
                    FitPoly(qv.data() + ms, mN, deg, c, nc, err);
                    if (err < g_AnimCompressError) {
                        const int poly_sz = BlockByteCost((uint8_t)(2 + deg), mN);
                        if (poly_sz < merge_cost) {
                            temp::animblock_t ab{ (uint8_t)(2 + deg), ms, me };
                            memcpy(ab.coeffs, c, nc * sizeof(int16_t));
                            ab.ncoeffs = (uint8_t)nc;
                            emit_block(ab);
                            bi = merge_end;
                            goto next_block;
                        }
                        break;
                    }
                }
            }
        }

        for (int j = bi; j < merge_end; j++) {
            auto& rb = processed[j];
            const int N = rb.end - rb.start;
            if (N > 2 && rb.type < 3 && bUseCompression) {
                int cur_cost = BlockByteCost(rb.type, N);
                for (int deg = 1; deg <= std::min(5, N - 1); deg++) {
                    int16_t c[6]; int nc; float err;
                    FitPoly(qv.data() + rb.start, N, deg, c, nc, err);
                    if (err < g_AnimCompressError) {
                        const int poly_sz = BlockByteCost((uint8_t)(2 + deg), N);
                        if (poly_sz < cur_cost) {
                            temp::animblock_t ab{ (uint8_t)(2 + deg), rb.start, rb.end };
                            memcpy(ab.coeffs, c, nc * sizeof(int16_t));
                            ab.ncoeffs = (uint8_t)nc;
                            emit_block(ab);
                            goto next_inner;
                        }
                        break;
                    }
                }
            }
            emit_block(rb);
            next_inner:;
        }
        bi = merge_end;
        next_block:;
    }
}
template void WriteAnimData<Vector3>(char*&, const std::vector<Vector3>&, const uint32_t, const uint32_t, int, float);
template void WriteAnimData<Vector4>(char*&, const std::vector<Vector4>&, const uint32_t, const uint32_t, int, float);

void WriteAnim(char*& pData, r5::anim::studioanimvalue_ptr_t* animvalueptr, const std::vector<Vector3>& rawdata, int32_t startframe, int32_t endframe, float scale) {
    const ptrdiff_t off = pData - (char*)animvalueptr;
    AssertMsg(off >= 0 && off < 8192, "animvalue_ptr offset overflow: %td (13-bit max)", off);
    animvalueptr->offset = (uint16_t)off;

    uint8_t  tmp = 0;
    uint8_t* offsets[] = { &tmp, &animvalueptr->idx1, &animvalueptr->idx2 };
    char* beginaddress = pData;

    for (int axis = 0; axis < 3; axis++) {
        if (allEqualVector(rawdata, startframe, endframe, axis, scale) && (int16_t)std::round(rawdata[startframe][axis] / scale) == 0)
            continue;

        const ptrdiff_t axis_off = (pData - beginaddress) / 2;
        AssertMsg(axis_off >= 0 && axis_off <= 255, "axis index overflow: %td (8-bit max)", axis_off);

        animvalueptr->flags |= (4 >> axis);
        *offsets[axis] = (uint8_t)axis_off;
        WriteAnimData(pData, rawdata, startframe, endframe, axis, scale);
    }
}