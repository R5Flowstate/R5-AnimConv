#include <pch.h>
#include <rseq/rseq.h>

using namespace r5;

static char  s_FrameBitCountLUT[4]  { 0, 2, 4, 0 };
static float s_FrameValOffsetLUT[4] { 0.0f, 3.0f, 15.0f, 0.0f };
static char  s_AnimSeekLUT[60] {
    1,  15, 16, 2,  7,  8,  2,  15, 0,  3,  15, 0,  4,  15, 0,  5,
    15, 0,  6,  15, 0,  7,  15, 0,  2,  15, 2,  3,  15, 2,  4,  15,
    2,  5,  15, 2,  6,  15, 2,  7,  15, 2,  2,  15, 4,  3,  15, 4,
    4,  15, 4, 5, 15, 4, 6, 15, 4,  7,  15, 4,
};

extern float HalfToFloat(uint16_t h);

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
        std::is_same_v<TAnimDesc, r5::anim::v10::mstudioanimdesc_t>  ||
        std::is_same_v<TAnimDesc, r5::anim::v11::mstudioanimdesc_t>  ||
        std::is_same_v<TAnimDesc, r5::anim::v12::mstudioanimdesc_t>  ||
        std::is_same_v<TAnimDesc, r5::anim::v121::mstudioanimdesc_t>) {
        if (!animdesc.sectionframes) return animdesc.numframes;

        const int  sectionstallframes = animdesc.sectionstallframes;
        const bool isDP               = (animdesc.flags & r5::ANIM_DATAPOINT) != 0;

        if (sectionstallframes && section == 0) return sectionstallframes;
        if (!isDP && section == (numSections - 1)) return 1;

        const int sectionbase     = section - (bool)sectionstallframes;
        const int frameoffset     = sectionstallframes + sectionbase * animdesc.sectionframes;
        const int remainingframes = animdesc.numframes - frameoffset - (isDP ? 0 : 1);

        return (remainingframes <= animdesc.sectionframes) ? remainingframes : animdesc.sectionframes;
    }
    return 0;
}
template int GetSectionLength<p2::mstudioanimdesc_t>             (const p2::mstudioanimdesc_t&,              const int, const int);
template int GetSectionLength<r2::mstudioanimdesc_t>             (const r2::mstudioanimdesc_t&,              const int, const int);
template int GetSectionLength<r5::anim::v7::mstudioanimdesc_t>  (const r5::anim::v7::mstudioanimdesc_t&,   const int, const int);
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
        std::is_same_v<TAnimDesc, r5::anim::v10::mstudioanimdesc_t> ||
        std::is_same_v<TAnimDesc, r5::anim::v11::mstudioanimdesc_t> ||
        std::is_same_v<TAnimDesc, r5::anim::v12::mstudioanimdesc_t>) {
        const int useTrail = (animdesc.flags & ANIM_DATAPOINT) ? 0 : 1;
        const int useStall = animdesc.sectionstallframes ? 1 : 0;
        const int base     = (animdesc.numframes - animdesc.sectionstallframes - 1) / animdesc.sectionframes;
        return base + useTrail + useStall + 1;
    }
    else if constexpr (
        std::is_same_v<TAnimDesc, r5::anim::v121::mstudioanimdesc_t>) {
        const int useTrail = (animdesc.flags & ANIM_DATAPOINT) ? 0 : 1;
        const int useStall = animdesc.sectionstallframes
                           ? (animdesc.sectionstallframes != animdesc.sectionframes) : 0;
        const int base     = (animdesc.numframes - animdesc.sectionstallframes - 1) / animdesc.sectionframes;
        return base + useTrail + useStall + 1;
    }
    return 0;
}
template int GetSectionCount<p2::mstudioanimdesc_t>             (const p2::mstudioanimdesc_t&);
template int GetSectionCount<r2::mstudioanimdesc_t>             (const r2::mstudioanimdesc_t&);
template int GetSectionCount<r5::anim::v7::mstudioanimdesc_t>  (const r5::anim::v7::mstudioanimdesc_t&);
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

void RLE::ExtractAnimValue(const r5::anim::mstudioanimvalue_t* panimvalue, const int frame,
                           const float scale, float& v1) {
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

void p2::RLE::ExtractAnimValue(int frame, const r5::anim::mstudioanimvalue_t* panimvalue,
                               float scale, float& v1) {
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

void RLE::ExtractAnimValue(int frame, const r5::anim::mstudioanimvalue_t* panimvalue,
                           float scale, float& v1) {
    int k = frame;
    while (panimvalue->meta.total <= k) {
        k -= panimvalue->meta.total;
        panimvalue += RLE::GetAnimValueOffset(panimvalue);
    }
    RLE::ExtractAnimValue(panimvalue, k, scale, v1);
}

void RLE::ExtractAnimValue(int frame, const r5::anim::mstudioanimvalue_t* panimvalue,
                           float scale, float& v1, float& v2) {
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

void RLE::CalcBoneQuaternion(const r5::anim::mstudio_rle_anim_t& pAnim, uint16_t** BoneTrackData,
                             Vector3& trackval, uint32_t localframe) {
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

void r5::DP::CalcBoneQuaternion_DP(const int sectionlength, const uint8_t** panimtrack,
    const int localFrame, Quaternion& q) {
    const uint8_t* ptrack = *panimtrack;

    const uint8_t  valid = ptrack[0];
    const uint8_t  total = ptrack[1];
    const uint8_t* pFrameIndices = ptrack + 2;

    const r5::anim::AnimQuat32* pPackedData = reinterpret_cast<const r5::anim::AnimQuat32*>  (total >= sectionlength ? pFrameIndices : pFrameIndices + total);
    const r5::anim::AxisFixup_t* pAxisFixup = reinterpret_cast<const r5::anim::AxisFixup_t*> (pPackedData + valid);

    const int idx = r5::DP::ResolveFrameIndex(localFrame, total, sectionlength, pFrameIndices);
    r5::DP::UnpackAnimQuat32(q, pPackedData[idx], &pAxisFixup[idx]);

    *panimtrack = reinterpret_cast<const uint8_t*>(pAxisFixup + total);
}

void r5::DP::CalcBonePosition_DP(const int sectionlength, const uint8_t** panimtrack,
    const int localFrame, Vector3& pos) {
    const uint8_t* ptrack = *panimtrack;

    const uint8_t  valid = ptrack[0];
    const uint8_t  total = ptrack[1];
    const uint8_t* pFrameIndices = ptrack + 2;

    *panimtrack = reinterpret_cast<const uint8_t*>(pFrameIndices);

    if (!total) { pos = { 0.0f, 0.0f, 0.0f }; return; }

    const r5::anim::AnimPos64* pPackedData = reinterpret_cast<const r5::anim::AnimPos64*>  (total >= sectionlength ? pFrameIndices : pFrameIndices + total);
    const r5::anim::AxisFixup_t* pAxisFixup = reinterpret_cast<const r5::anim::AxisFixup_t*>(pPackedData + valid);

    const int idx = r5::DP::ResolveFrameIndex(localFrame, total, sectionlength, pFrameIndices);
    r5::DP::UnpackAnimPos64(pos, pPackedData[idx], &pAxisFixup[idx]);

    *panimtrack = reinterpret_cast<const uint8_t*>(pAxisFixup + total);
}

void r5::DP::CalcBonePositionVirtual_DP(const int sectionlength, const uint8_t** panimtrack,
    const int localFrame, Vector3& pos) {
    const uint8_t* ptrack = *panimtrack;

    const uint8_t  total = ptrack[0];
    const float    posscale = HalfToFloat(*reinterpret_cast<const uint16_t*>(ptrack + 1)) / 127.0f;
    const uint8_t* pFrameIndices = ptrack + 3;

    const r5::anim::AxisFixup_t* pAxisFixup = reinterpret_cast<const r5::anim::AxisFixup_t*>(total >= sectionlength ? pFrameIndices : pFrameIndices + total);

    const int idx = r5::DP::ResolveFrameIndex(localFrame, total, sectionlength, pFrameIndices);
    pos.x = static_cast<float>(pAxisFixup[idx].adjustment[0]) * posscale;
    pos.y = static_cast<float>(pAxisFixup[idx].adjustment[1]) * posscale;
    pos.z = static_cast<float>(pAxisFixup[idx].adjustment[2]) * posscale;

    *panimtrack = reinterpret_cast<const uint8_t*>(pAxisFixup + total);
}

void r5::DP::CalcBoneScale_DP(const int sectionlength, const uint8_t** panimtrack,
    const int localFrame, Vector3& scale) {
    const uint8_t* ptrack = *panimtrack;

    const uint8_t  total = ptrack[0];
    const uint8_t* pFrameIndices = ptrack + 1;

    const Vector48* pPackedData = reinterpret_cast<const Vector48*>(total >= sectionlength ? pFrameIndices : pFrameIndices + total);

    const int idx = r5::DP::ResolveFrameIndex(localFrame, total, sectionlength, pFrameIndices);
    scale = { static_cast<float>(pPackedData[idx].x),
              static_cast<float>(pPackedData[idx].y),
              static_cast<float>(pPackedData[idx].z) };

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
            Vector3    scl = anim.animdata[bone].scl[frame];

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
    if (anim.asqd.buffer.empty())
        PRINTANDTHROW(anim.asqd.path.c_str(), "[!] Error: DataPoint buffer is null.");

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
                    if (seq.extn.buffer.empty() || (size_t)off >= seq.extn.size)
                        PRINTANDTHROW(seq.extn.path.c_str(), "[!] Error: DP section past end of .rseq_extn");
                    pBoneFlagArray = seq.extn.buffer.data() + off;
                }
                else {
                    if ((size_t)idx >= anim.asqd.buffer.size())
                        PRINTANDTHROW(anim.asqd.path.c_str(), "[!] Error: DP section past end of .asqd");
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
                    if (seq.extn.buffer.empty() || (size_t)offset >= seq.extn.size)
                        PRINTANDTHROW(seq.extn.path.c_str(), "[!] Error: DP section past end of .rseq_extn");
                    pBoneFlagArray = seq.extn.buffer.data() + offset;
                }
                else {
                    if ((size_t)idx >= anim.asqd.buffer.size())
                        PRINTANDTHROW(seq.path.c_str(), "[!] Error: DP section past end of .rseq");
                    pBoneFlagArray = PTR_FROM_IDX(char, pAnimDesc, idx);
                }
            }
        }

        r5::DP::ParseDataPointSection(reinterpret_cast<const uint8_t*>(pBoneFlagArray), sectionframes, sectionbaseframe, rig, anim);
        sectionbaseframe += static_cast<uint32_t>(sectionframes);
    }
}
template void r5::DP::ParseDataPoint<r5::anim::v12::mstudioanimdesc_t>(const r5::anim::v12::mstudioanimdesc_t*, temp::rig_t&, temp::Sequence&, temp::animdesc_t&);
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
            const r5::anim::AnimPos64* pPackedData = reinterpret_cast<const r5::anim::AnimPos64*>  (total16 >= numframes ? pFrameIdx : pFrameIdx + total16);
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
template void r5::DP::ParseFrameMovementsDP<r5::anim::v12::mstudioanimdesc_t>(const r5::anim::v12::mstudioanimdesc_t*, temp::animdesc_t&);
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

void ParsePoseKey(const r5::anim::v10::mstudioseqdesc_t* pSeqDesc, temp::Sequence& seq) {
    if (!pSeqDesc->posekeyindex) return;

    auto* pPosekeys = reinterpret_cast<float*>((char*)pSeqDesc + pSeqDesc->posekeyindex);
    const int count = pSeqDesc->groupsize[0] + pSeqDesc->groupsize[1];
    for (int i = 0; i < count; i++)
        seq.posekeys.push_back(pPosekeys[i]);
}

template<typename TSeqDesc>
void ParsePoseKey(const TSeqDesc* pSeqDesc, temp::Sequence& seq) {
    if (!pSeqDesc->posekeyindex) return;

    auto* pPosekeys = reinterpret_cast<float*>((char*)pSeqDesc + OFFSET(pSeqDesc->posekeyindex));
    const int count = pSeqDesc->groupsize[0] + pSeqDesc->groupsize[1];
    for (int i = 0; i < count; i++)
        seq.posekeys.push_back(pPosekeys[i]);
}
template void ParsePoseKey<r5::anim::v11::mstudioseqdesc_t> (const r5::anim::v11::mstudioseqdesc_t*,  temp::Sequence&);
template void ParsePoseKey<r5::anim::v12::mstudioseqdesc_t> (const r5::anim::v12::mstudioseqdesc_t*,  temp::Sequence&);
template void ParsePoseKey<r5::anim::v121::mstudioseqdesc_t>(const r5::anim::v121::mstudioseqdesc_t*, temp::Sequence&);

void ParseEvent(const r5::anim::v10::mstudioseqdesc_t* pSeqDesc, temp::Sequence& seq) {
    if (!pSeqDesc->numevents) return;

    bool b2ndEvent = (pSeqDesc->weightlistindex - pSeqDesc->eventindex) % 272 != 0;
    if (!(pSeqDesc->numevents % 68)) {
        printf("[!] Warning: events might be wrong, needed to check it manually.\n");
        b2ndEvent = true;
    }

    for (int i = 0; i < pSeqDesc->numevents; i++) {
        temp::seqevent_t event{};
        if (b2ndEvent) {
            auto* pEvents = reinterpret_cast<r5::anim::v10::mstudioevent_2_t*>((char*)pSeqDesc + pSeqDesc->eventindex);
            event.name    = STRING_FROM_IDX(&pEvents[i], pEvents[i].szeventindex);
            event.cycle   = pEvents[i].cycle;
            event.event   = pEvents[i].event;
            event.type    = pEvents[i].type;
            event.options = std::string(pEvents[i].options);
        }
        else {
            auto* pEvents = reinterpret_cast<r5::anim::v10::mstudioevent_t*>((char*)pSeqDesc + pSeqDesc->eventindex);
            event.name    = STRING_FROM_IDX(&pEvents[i], pEvents[i].szeventindex);
            event.cycle   = pEvents[i].cycle;
            event.event   = pEvents[i].event;
            event.type    = pEvents[i].type;
            event.options = std::string(pEvents[i].options);
        }
        seq.events.push_back(event);
    }
}

template<typename TSeqDesc>
void ParseEvent(const TSeqDesc* pSeqDesc, temp::Sequence& seq) {
    if (!pSeqDesc->numevents) return;

    auto* pEvents = reinterpret_cast<r5::anim::v11::mstudioevent_t*>((char*)pSeqDesc + OFFSET(pSeqDesc->eventindex));
    for (int i = 0; i < pSeqDesc->numevents; i++) {
        if (!pEvents[i].szeventindex) continue;

        temp::seqevent_t event{};
        event.name    = STRING_FROM_IDX(&pEvents[i], OFFSET(pEvents[i].szeventindex));
        event.cycle   = pEvents[i].cycle;
        event.event   = pEvents[i].event;
        event.type    = pEvents[i].type;
        event.options = STRING_FROM_IDX(&pEvents[i], OFFSET(pEvents[i].optionsindex));
        seq.events.push_back(event);
    }
}
template void ParseEvent<r5::anim::v11::mstudioseqdesc_t> (const r5::anim::v11::mstudioseqdesc_t*,  temp::Sequence&);
template void ParseEvent<r5::anim::v12::mstudioseqdesc_t> (const r5::anim::v12::mstudioseqdesc_t*,  temp::Sequence&);
template void ParseEvent<r5::anim::v121::mstudioseqdesc_t>(const r5::anim::v121::mstudioseqdesc_t*, temp::Sequence&);

void ParseAutoLayer(const r5::anim::v10::mstudioseqdesc_t* pSeqDesc, temp::Sequence& seq) {
    auto* pAutolayer = reinterpret_cast<r5::anim::v7::mstudioautolayer_t*>((char*)pSeqDesc + pSeqDesc->autolayerindex);
    for (int i = 0; i < pSeqDesc->numautolayers; i++) {
        temp::autolayer_t autolayer{};
        autolayer.guidSequence = pAutolayer[i].guidSequence;
        autolayer.iSequence    = pAutolayer[i].iSequence;
        autolayer.iPose        = pAutolayer[i].iPose;
        autolayer.flags        = pAutolayer[i].flags;
        autolayer.start        = pAutolayer[i].start;
        autolayer.peak         = pAutolayer[i].peak;
        autolayer.tail         = pAutolayer[i].tail;
        autolayer.end          = pAutolayer[i].end;
        seq.autolayers.push_back(autolayer);
    }
}

template<typename TSeqDesc>
void ParseAutoLayer(const TSeqDesc* pSeqDesc, temp::Sequence& seq) {
    auto* pAutolayer = reinterpret_cast<r5::anim::v11::mstudioautolayer_t*>((char*)pSeqDesc + OFFSET(pSeqDesc->autolayerindex));
    for (int i = 0; i < pSeqDesc->numautolayers; i++) {
        temp::autolayer_t autolayer{};
        autolayer.guidSequence = pAutolayer[i].guidSequence;
        autolayer.iPose        = pAutolayer[i].iPose;
        autolayer.flags        = pAutolayer[i].flags;
        autolayer.start        = pAutolayer[i].start;
        autolayer.peak         = pAutolayer[i].peak;
        autolayer.tail         = pAutolayer[i].tail;
        autolayer.end          = pAutolayer[i].end;
        seq.autolayers.push_back(autolayer);
    }
}
template void ParseAutoLayer<r5::anim::v11::mstudioseqdesc_t> (const r5::anim::v11::mstudioseqdesc_t*,  temp::Sequence&);
template void ParseAutoLayer<r5::anim::v12::mstudioseqdesc_t> (const r5::anim::v12::mstudioseqdesc_t*,  temp::Sequence&);
template void ParseAutoLayer<r5::anim::v121::mstudioseqdesc_t>(const r5::anim::v121::mstudioseqdesc_t*, temp::Sequence&);

void ParseWeightList(const r5::anim::v10::mstudioseqdesc_t* pSeqDesc, temp::Sequence& seq) {
    auto* pWeightList = reinterpret_cast<float*>((char*)pSeqDesc + pSeqDesc->weightlistindex);
    for (int i = 0; i < (int)seq.weightlist.size(); i++)
        seq.weightlist[i] = pWeightList[i];
}

template<typename TSeqDesc>
void ParseWeightList(const TSeqDesc* pSeqDesc, temp::Sequence& seq) {
    const auto idx = pSeqDesc->weightlistindex;
    if (idx && idx != 1 && idx != 3 && idx != 5) {
        auto* pWeightList = reinterpret_cast<float*>((char*)pSeqDesc + OFFSET(idx));
        for (int i = 0; i < (int)seq.weightlist.size(); i++)
            seq.weightlist[i] = pWeightList[i];
    }
    else {
        std::fill(seq.weightlist.begin(), seq.weightlist.end(), 1.0f);
    }
}
template void ParseWeightList<r5::anim::v11::mstudioseqdesc_t> (const r5::anim::v11::mstudioseqdesc_t*,  temp::Sequence&);
template void ParseWeightList<r5::anim::v12::mstudioseqdesc_t> (const r5::anim::v12::mstudioseqdesc_t*,  temp::Sequence&);
template void ParseWeightList<r5::anim::v121::mstudioseqdesc_t>(const r5::anim::v121::mstudioseqdesc_t*, temp::Sequence&);

void ParseActMod(const r5::anim::v10::mstudioseqdesc_t* pSeqDesc, temp::Sequence& seq) {
    if (!pSeqDesc->numactivitymodifiers) return;

    auto* pActMod = reinterpret_cast<r5::anim::v10::mstudioactivitymodifier_t*>(
        (char*)pSeqDesc + pSeqDesc->activitymodifierindex);

    for (int i = 0; i < pSeqDesc->numactivitymodifiers; i++) {
        temp::actmod_t actmod{};
        actmod.name   = STRING_FROM_IDX(&pActMod[i], pActMod[i].sznameindex);
        actmod.negate = pActMod->negate;
        seq.actmods.push_back(actmod);
    }
}

template<typename TSeqDesc>
void ParseActMod(const TSeqDesc* pSeqDesc, temp::Sequence& seq) {
    if (!pSeqDesc->numactivitymodifiers) return;

    auto* pActMod = reinterpret_cast<r5::anim::v11::mstudioactivitymodifier_t*>(
        (char*)pSeqDesc + OFFSET(pSeqDesc->activitymodifierindex));

    for (int i = 0; i < pSeqDesc->numactivitymodifiers; i++) {
        temp::actmod_t actmod{};
        actmod.name   = STRING_FROM_IDX(&pActMod[i], OFFSET(pActMod[i].sznameindex));
        actmod.negate = pActMod->negate;
        seq.actmods.push_back(actmod);
    }
}
template void ParseActMod<r5::anim::v11::mstudioseqdesc_t> (const r5::anim::v11::mstudioseqdesc_t*,  temp::Sequence&);
template void ParseActMod<r5::anim::v12::mstudioseqdesc_t> (const r5::anim::v12::mstudioseqdesc_t*,  temp::Sequence&);
template void ParseActMod<r5::anim::v121::mstudioseqdesc_t>(const r5::anim::v121::mstudioseqdesc_t*, temp::Sequence&);

void RLE::ParseIkrules(const r5::anim::v10::mstudioanimdesc_t* pAnimDesc, temp::animdesc_t& anim) {
    if (!pAnimDesc) PRINTANDTHROW(anim.name.c_str(), "pAnimDesc is null");
    if (!pAnimDesc->numikrules) return;

    auto* ikrules = reinterpret_cast<r5::anim::v10::mstudioikrule_t*>((char*)pAnimDesc + pAnimDesc->ikruleindex);
    for (int i = 0; i < pAnimDesc->numikrules; i++) {
        temp::ikrule_t ikrule{};
        ikrule.index      = ikrules[i].index;
        ikrule.type       = ikrules[i].type;
        ikrule.chain      = ikrules[i].chain;
        ikrule.bone       = ikrules[i].bone;
        ikrule.slot       = ikrules[i].slot;
        ikrule.height     = ikrules[i].height;
        ikrule.radius     = ikrules[i].radius;
        ikrule.floor      = ikrules[i].floor;
        ikrule.pos        = ikrules[i].pos;
        ikrule.q          = ikrules[i].q;
        for (int j = 0; j < 6; j++) ikrule.scale[j] = ikrules[i].scale[j];
        ikrule.sectionframes = ikrules[i].sectionframes;
        ikrule.iStart     = ikrules[i].iStart;
        ikrule.start      = ikrules[i].start;
        ikrule.peak       = ikrules[i].peak;
        ikrule.tail       = ikrules[i].tail;
        ikrule.end        = ikrules[i].end;
        ikrule.contact    = ikrules[i].contact;
        ikrule.drop       = ikrules[i].drop;
        ikrule.top        = ikrules[i].top;
        ikrule.endHeight  = ikrules[i].endHeight;
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

template<typename TAnimDesc>
void RLE::ParseIkrules(const TAnimDesc* pAnimDesc, temp::animdesc_t& anim) {
    if (!pAnimDesc) PRINTANDTHROW(anim.name.c_str(), "pAnimDesc is null");
    if (!pAnimDesc->numikrules || pAnimDesc->ikruleindex == 3 || pAnimDesc->ikruleindex == 5) return;

    auto* ikrules = reinterpret_cast<r5::anim::v11::mstudioikrule_t*>((char*)pAnimDesc + OFFSET(pAnimDesc->ikruleindex));
    for (int i = 0; i < pAnimDesc->numikrules; i++) {
        temp::ikrule_t ikrule{};
        ikrule.type       = ikrules[i].type;
        ikrule.chain      = ikrules[i].chain;
        ikrule.bone       = ikrules[i].bone;
        ikrule.slot       = ikrules[i].slot;
        ikrule.height     = ikrules[i].height;
        ikrule.radius     = ikrules[i].radius;
        ikrule.floor      = ikrules[i].floor;
        ikrule.pos        = ikrules[i].pos;
        ikrule.q          = ikrules[i].q;
        for (int j = 0; j < 6; j++) ikrule.scale[j] = ikrules[i].scale[j];
        ikrule.sectionframes = ikrules[i].sectionframes;
        ikrule.iStart     = ikrules[i].iStart;
        ikrule.start      = ikrules[i].start;
        ikrule.peak       = ikrules[i].peak;
        ikrule.tail       = ikrules[i].tail;
        ikrule.end        = ikrules[i].end;
        ikrule.contact    = ikrules[i].contact;
        ikrule.drop       = ikrules[i].drop;
        ikrule.top        = ikrules[i].top;
        ikrule.endHeight  = ikrules[i].endHeight;
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
template void RLE::ParseIkrules<r5::anim::v11::mstudioanimdesc_t> (const r5::anim::v11::mstudioanimdesc_t*,  temp::animdesc_t&);
template void RLE::ParseIkrules<r5::anim::v12::mstudioanimdesc_t> (const r5::anim::v12::mstudioanimdesc_t*,  temp::animdesc_t&);
template void RLE::ParseIkrules<r5::anim::v121::mstudioanimdesc_t>(const r5::anim::v121::mstudioanimdesc_t*, temp::animdesc_t&);

void RLE::ParseFrameMovements(const r5::anim::v10::mstudioanimdesc_t* pAnimDesc, temp::animdesc_t& anim) {
    if (!pAnimDesc) PRINTANDTHROW(anim.name.c_str(), "pAnimDesc is null");
    if (!(pAnimDesc->flags & r5::ANIM_FRAMEMOVEMENT) || !pAnimDesc->framemovementindex) return;

    auto* pFrameMovement = reinterpret_cast<r5::anim::v7::mstudioframemovement_t*>((char*)pAnimDesc + pAnimDesc->framemovementindex);
    auto* sectionindices = reinterpret_cast<int32_t*>((char*)pFrameMovement + sizeof(r5::anim::v7::mstudioframemovement_t));
    const uint32_t sectioncount = static_cast<uint32_t>((float)(anim.numframes - 1) / (float)pFrameMovement->sectionframes) + 1;

    temp::framemovement_t movement{};
    movement.scale         = pFrameMovement->scale;
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

template<typename TAnimDesc>
void RLE::ParseFrameMovements(const TAnimDesc* pAnimDesc, temp::animdesc_t& anim) {
    if (!pAnimDesc) PRINTANDTHROW(anim.name.c_str(), "pAnimDesc is null");
    if (!(pAnimDesc->flags & r5::ANIM_FRAMEMOVEMENT) || !pAnimDesc->framemovementindex) return;
    if (pAnimDesc->flags & ANIM_DATAPOINT) return;

    auto* pFrameMovement = reinterpret_cast<r5::anim::v7::mstudioframemovement_t*>((char*)pAnimDesc + OFFSET(pAnimDesc->framemovementindex));
    auto* sectionindices = reinterpret_cast<uint16_t*>((char*)pFrameMovement + sizeof(r5::anim::v7::mstudioframemovement_t));
    const uint32_t sectioncount = static_cast<uint32_t>((float)(anim.numframes - 1) / (float)pFrameMovement->sectionframes) + 1;

    temp::framemovement_t movement{};
    movement.scale         = pFrameMovement->scale;
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
template void RLE::ParseFrameMovements<r5::anim::v11::mstudioanimdesc_t> (const r5::anim::v11::mstudioanimdesc_t*,  temp::animdesc_t&);
template void RLE::ParseFrameMovements<r5::anim::v12::mstudioanimdesc_t> (const r5::anim::v12::mstudioanimdesc_t*,  temp::animdesc_t&);
template void RLE::ParseFrameMovements<r5::anim::v121::mstudioanimdesc_t>(const r5::anim::v121::mstudioanimdesc_t*, temp::animdesc_t&);

// ============================================================================
//  Write helpers
// ============================================================================

/* TODO: rewrite to support more than 3 compression types */
template<typename TVecType>
void WriteCompressedAnim(char*& pData, const std::vector<TVecType>& rawdata,
                         temp::animblock_t c, int axis, float scale) {
    const int framerange = c.endframe - c.startframe;

    auto* valueRLE  = reinterpret_cast<r5::anim::mstudioanimvalue_t*>(pData);
    char* cValueRLE = reinterpret_cast<char*>((char*)valueRLE + 4);
    pData += sizeof(r5::anim::mstudioanimvalue_t);

    valueRLE->meta.type  = c.comptype;
    valueRLE->meta.total = framerange;

    const int16_t base = (int16_t)(rawdata[c.startframe][axis] / scale);

    switch (c.comptype) {
    case 0:
        for (int i = 0; i < framerange; i++) {
            valueRLE[i + 1].value = (int16_t)(rawdata[c.startframe + i][axis] / scale);
            pData += sizeof(int16_t);
        }
        break;

    case 1:
        valueRLE[1].value = base;
        pData += sizeof(int16_t);
        for (int i = 0; i < framerange - 1; i++)
            cValueRLE[i] = (int16_t)(rawdata[c.startframe + i + 1][axis] / scale) - base;
        pData += (framerange - 1) * sizeof(int8_t);
        break;

    case 2:
        valueRLE[1].value = base;
        pData += sizeof(int16_t);
        break;

    default:
        std::cerr << "[!] Error: anim data compress type is invalid.\n";
        break;
    }
    ALIGN2(pData);
}
template void WriteCompressedAnim<Vector3>(char*&, const std::vector<Vector3>&, temp::animblock_t, int, float);
template void WriteCompressedAnim<Vector4>(char*&, const std::vector<Vector4>&, temp::animblock_t, int, float);


template<typename TVecType>
static void FindConstantRanges(const std::vector<TVecType>& raw, int axis, float eps,
                               std::vector<temp::animblock_t>& out, uint32_t start, uint32_t end) {
    uint32_t i = start;
    while (i < end) {
        const float ref = raw[i][axis];
        uint32_t j = i + 1;
        while (j < end && raw[j][axis] == ref) j++;

        if (j - i >= 3)
            out.push_back({ 2, i, j });
        i = j;
    }
}

template<typename TVecType>
static void FindDiffRanges(const std::vector<TVecType>& raw, int axis, float threshold,
                           std::vector<temp::animblock_t>& out, uint32_t start, uint32_t end,
                           const std::vector<temp::animblock_t>& existing) {
    auto isCovered = [&](uint32_t f) {
        for (auto& b : existing)
            if (f >= b.startframe && f < b.endframe) return true;
        return false;
    };

    uint32_t i = start;
    while (i < end) {
        if (isCovered(i)) { i++; continue; }

        const float ref = raw[i][axis];
        float vmin = ref, vmax = ref;
        uint32_t j = i + 1;

        while (j < end && !isCovered(j)) {
            const float v = raw[j][axis];
            vmin = std::min(vmin, v);
            vmax = std::max(vmax, v);
            if ((ref - vmin) >= threshold || (vmax - ref) >= threshold) break;
            j++;
        }

        if (j - i >= 3)
            out.push_back({ 1, i, j });
        i = j;
    }
}

template<typename TVecType>
void WriteAnimData(char*& pData, const std::vector<TVecType>& rawdata, uint32_t startframe, uint32_t endframe, int axis, float scale) {
    const float threshold = 127.0f * scale;

    std::vector<temp::animblock_t> blocks;  blocks.reserve(64);
    std::vector<temp::animblock_t> merged;  merged.reserve(64);
    std::vector<temp::animblock_t> final_;  final_.reserve(64);

    FindConstantRanges(rawdata, axis, scale, blocks, startframe, endframe);
    FindDiffRanges    (rawdata, axis, threshold, blocks, startframe, endframe, blocks);

    std::sort(blocks.begin(), blocks.end(), [](auto& a, auto& b) { return a.startframe < b.startframe; });

    uint32_t pos = startframe;
    for (auto& b : blocks) {
        if (pos < b.startframe) merged.push_back({ 0, pos, b.startframe });
        merged.push_back(b);
        pos = b.endframe;
    }
    if (pos < endframe) merged.push_back({ 0, pos, endframe });

    for (auto& b : merged) {
        if (!final_.empty() &&
            final_.back().comptype == 0 && b.comptype == 0 &&
            final_.back().endframe == b.startframe) {
            final_.back().endframe = b.endframe;
        }
        else {
            final_.push_back(b);
        }
    }

    for (auto& c : final_)
        WriteCompressedAnim(pData, rawdata, c, axis, scale);
}
template void WriteAnimData<Vector3>(char*&, const std::vector<Vector3>&, const uint32_t, const uint32_t, int, float);
template void WriteAnimData<Vector4>(char*&, const std::vector<Vector4>&, const uint32_t, const uint32_t, int, float);

void WriteAnim(char*& pData, r5::anim::studioanimvalue_ptr_t* animvalueptr,
               std::vector<Vector3> rawdata, int32_t startframe, int32_t endframe, float scale) {
    animvalueptr->offset = pData - (char*)animvalueptr;

    uint8_t  tmp      = 0;
    uint8_t* offsets[] = { &tmp, &animvalueptr->idx1, &animvalueptr->idx2 };
    char* beginaddress = pData;

    for (int axis = 0; axis < 3; axis++) {
        if (allEqualVector(rawdata, startframe, endframe, axis, scale) && rawdata[startframe][axis] == 0)
            continue;

        animvalueptr->flags |= (4 >> axis);
        *offsets[axis] = (uint8_t)((pData - beginaddress) / 2);
        WriteAnimData(pData, rawdata, startframe, endframe, axis, scale);
    }
}