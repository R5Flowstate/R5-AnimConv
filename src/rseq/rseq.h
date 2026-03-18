#pragma once
#include <pch.h>

namespace p2 {
    namespace RLE {
        void ExtractAnimValue(int frame, const r5::anim::mstudioanimvalue_t* panimvalue, float scale, float& v1);
    }
}

namespace r5 {
    constexpr uint32_t ANIM_LOOPING       = 0x1;
    constexpr uint32_t ANIM_DELTA         = 0x4;
    constexpr uint32_t ANIM_ALLZEROS      = 0x20;
    constexpr uint32_t ANIM_VALID         = 0x20000;
    constexpr uint32_t ANIM_FRAMEMOVEMENT = 0x40000;
    constexpr uint32_t ANIM_DATAPOINT     = 0x200000;

    namespace DP {
        constexpr uint8_t BONEPOS   = 0x1;
        constexpr uint8_t BONEROT   = 0x2;
        constexpr uint8_t BONESCALE = 0x4;
        constexpr uint8_t BONEUNK8  = 0x8;
        constexpr uint8_t BONEDATA  = (BONEPOS | BONEROT | BONESCALE | BONEUNK8);

        template<typename TIndexType> int ResolveFrameIndex(const int localFrame, const TIndexType total, const int sectionlength, const TIndexType* pFrameIndices);
        
        inline void UnpackAnimQuat32(Quaternion& q, const r5::anim::AnimQuat32 packed, const r5::anim::AxisFixup_t* axisFixup) {
            const int   sf  = packed.scaleFactor;
            const float sc  = sf ? 0.011048543f : 0.0055242716f;
            const float sfx = static_cast<float>(1 << sf) * 0.000021924432f;
            const float a0  = (static_cast<float>(packed.value0) + 0.5f) * sc + static_cast<float>(axisFixup->adjustment[0]) * sfx;
            const float a1  = (static_cast<float>(packed.value1) + 0.5f) * sc + static_cast<float>(axisFixup->adjustment[1]) * sfx;
            const float a2  = (static_cast<float>(packed.value2) + 0.5f) * sc + static_cast<float>(axisFixup->adjustment[2]) * sfx;
            const float dp   = a0*a0 + a1*a1 + a2*a2;
            const float drop = (dp < 1.0f) ? sqrtf(1.0f - dp) : 0.0f;
            switch (packed.droppedAxis) {
            case 0: q.x = drop; q.y = a0;   q.z = a1;   q.w = a2;   break;
            case 1: q.x = a0;   q.y = drop; q.z = a1;   q.w = a2;   break;
            case 2: q.x = a0;   q.y = a1;   q.z = drop; q.w = a2;   break;
            case 3: q.x = a0;   q.y = a1;   q.z = a2;   q.w = drop; break;
            }
        }

        inline void UnpackAnimPos64(Vector3& pos, const r5::anim::AnimPos64 packed, const r5::anim::AxisFixup_t* axisFixup) {
            const float sf = static_cast<float>(packed.scaleFactor + 1);
            pos.x = (static_cast<float>(axisFixup->adjustment[0]) / 100.0f + static_cast<float>(packed.values[0])) * sf;
            pos.y = (static_cast<float>(axisFixup->adjustment[1]) / 100.0f + static_cast<float>(packed.values[1])) * sf;
            pos.z = (static_cast<float>(axisFixup->adjustment[2]) / 100.0f + static_cast<float>(packed.values[2])) * sf;
        }

        void CalcBoneQuaternion_DP     (int sectionlength, const uint8_t** panimtrack, int localFrame, Quaternion& q);
        void CalcBonePosition_DP       (int sectionlength, const uint8_t** panimtrack, int localFrame, Vector3& pos);
        void CalcBonePositionVirtual_DP(int sectionlength, const uint8_t** panimtrack, int localFrame, Vector3& pos);
        void CalcBoneScale_DP          (int sectionlength, const uint8_t** panimtrack, int localFrame, Vector3& scale);

        void ParseDataPointSection(const uint8_t* pBoneFlagArray, int sectionlength, uint32_t sectionbaseframe, temp::rig_t& rig, temp::animdesc_t& anim);
        template<typename TAnimDesc> void ParseDataPoint(const TAnimDesc* pAnimDesc, temp::rig_t& rig, temp::Sequence& seq, temp::animdesc_t& anim);
        template<typename TAnimDesc>  void ParseFrameMovementsDP(const TAnimDesc* pAnimDesc, temp::animdesc_t& anim);
    }

    namespace RLE {
        constexpr uint8_t BONEPOS   = 0x1;
        constexpr uint8_t BONEROT   = 0x2;
        constexpr uint8_t BONESCALE = 0x4;
        constexpr uint8_t BONEDATA  = (BONEPOS | BONEROT | BONESCALE);

        int  GetAnimValueOffset(const r5::anim::mstudioanimvalue_t* const panimvalue);

        void ExtractAnimValue(const r5::anim::mstudioanimvalue_t* panimvalue, int frame, float scale, float& v1);
        void ExtractAnimValue(int frame, const r5::anim::mstudioanimvalue_t* panimvalue, float scale, float& v1);
        void ExtractAnimValue(int frame, const r5::anim::mstudioanimvalue_t* panimvalue, float scale, float& v1, float& v2);

        void CalcBonePosition  (const r5::anim::mstudio_rle_anim_t& pAnim, uint16_t** BoneTrackData, Vector3& trackval, uint32_t localframe);
        void CalcBoneQuaternion(const r5::anim::mstudio_rle_anim_t& pAnim, uint16_t** BoneTrackData, Vector3& trackval, uint32_t localframe);
        void CalcBoneScale     (const r5::anim::mstudio_rle_anim_t& pAnim, uint16_t** BoneTrackData, Vector3& trackval, uint32_t localframe);

        void ParseIkrules       (const r5::anim::v10::mstudioanimdesc_t* pAnimDesc, temp::animdesc_t& anim);
        void ParseFrameMovements(const r5::anim::v10::mstudioanimdesc_t* pAnimDesc, temp::animdesc_t& anim);
        template<typename TAnimDesc> void ParseIkrules       (const TAnimDesc* pAnimDesc, temp::animdesc_t& anim);
        template<typename TAnimDesc> void ParseFrameMovements(const TAnimDesc* pAnimDesc, temp::animdesc_t& anim);
    }
}

int GetSectionLength(int numframes, int sectionframes, int section);
template<typename TAnimDesc> int GetSectionLength(const TAnimDesc& animdesc, int section, int numSections);
template<typename TAnimDesc> int GetSectionCount (const TAnimDesc& animdesc);

std::vector<int32_t> GetAnimIndexes(const int32_t*  pBlends, temp::Sequence& seq, int32_t numanims);
std::vector<int32_t> GetAnimIndexes(const uint16_t* pBlends, temp::Sequence& seq, int32_t numanims);

void ParsePoseKey   (const r5::anim::v10::mstudioseqdesc_t* pSeqDesc, temp::Sequence& seq);
void ParseEvent     (const r5::anim::v10::mstudioseqdesc_t* pSeqDesc, temp::Sequence& seq);
void ParseAutoLayer (const r5::anim::v10::mstudioseqdesc_t* pSeqDesc, temp::Sequence& seq);
void ParseWeightList(const r5::anim::v10::mstudioseqdesc_t* pSeqDesc, temp::Sequence& seq);
void ParseActMod    (const r5::anim::v10::mstudioseqdesc_t* pSeqDesc, temp::Sequence& seq);

template<typename TSeqDesc> void ParsePoseKey   (const TSeqDesc* pSeqDesc, temp::Sequence& seq);
template<typename TSeqDesc> void ParseEvent     (const TSeqDesc* pSeqDesc, temp::Sequence& seq);
template<typename TSeqDesc> void ParseAutoLayer (const TSeqDesc* pSeqDesc, temp::Sequence& seq);
template<typename TSeqDesc> void ParseWeightList(const TSeqDesc* pSeqDesc, temp::Sequence& seq);
template<typename TSeqDesc> void ParseActMod    (const TSeqDesc* pSeqDesc, temp::Sequence& seq);

void ParseRSEQ_v10 (std::string in_dir, temp::rig_t& rig);
void ParseRSEQ_v11 (std::string in_dir, temp::rig_t& rig);
void ParseRSEQ_v12 (std::string in_dir, temp::rig_t& rig);
void ParseRSEQ_v121(std::string in_dir, temp::rig_t& rig);

void WriteAnim(char*& pData, r5::anim::studioanimvalue_ptr_t* animvalueptr, std::vector<Vector3> rawdata, int32_t startframe, int32_t endframe, float scale);
template<typename TVecType> void WriteAnimData(char*& pData, const std::vector<TVecType>& rawdata, uint32_t startframe, uint32_t endframe, int axis, float scale);
template<typename TVecType> void WriteCompressedAnim(char*& pData, const std::vector<TVecType>& rawdata, temp::animblock_t c, int axis, float scale);

void WriteRSEQ_v7(temp::rig_t& rig, bool bSkipEvents = false);
