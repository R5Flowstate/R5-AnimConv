#include <pch.h>
#include <rseq/rseq.h>

using namespace r5;

static temp::file_t LoadFile(const std::string& path) {
    if (!std::filesystem::exists(path))
        Error("file is missing: '%s'", path.c_str());

    temp::file_t f{};
    f.path = path;
    f.size = std::filesystem::file_size(path);
    f.buffer.resize(f.size);

    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) Error("cannot open file for reading: '%s'", path.c_str());
    stream.read(f.buffer.data(), f.size);
    if (!stream || stream.gcount() != static_cast<std::streamsize>(f.size))
        Error("failed to read entire file: '%s'", path.c_str());
    return f;
}

static std::string BuildOutputPath(const std::string& in_dir, const std::filesystem::path& relative_path) {
    auto it = relative_path.begin();
    std::filesystem::path newPath = it->string();
    for (++it; it != relative_path.end(); ++it) newPath /= *it;
    return in_dir + "\\conv\\" + newPath.string();
}

template<typename TSeqDesc>
static void PopulateSeqCommon(temp::Sequence& seq, const TSeqDesc* pSeqDesc, const std::string& path, const std::string& out_dir, const std::string& activityname) {
    seq.path           = path;
    seq.outpath        = out_dir;
    seq.activityname   = activityname;
    seq.flags          = pSeqDesc->flags;
    seq.activity       = pSeqDesc->activity;
    seq.actweight      = pSeqDesc->actweight;
    seq.bbmin          = pSeqDesc->bbmin;
    seq.bbmax          = pSeqDesc->bbmax;
    seq.groupsize[0]   = pSeqDesc->groupsize[0];
    seq.groupsize[1]   = pSeqDesc->groupsize[1];
    seq.paramindex[0]  = pSeqDesc->paramindex[0];
    seq.paramindex[1]  = pSeqDesc->paramindex[1];
    seq.paramstart[0]  = pSeqDesc->paramstart[0];
    seq.paramstart[1]  = pSeqDesc->paramstart[1];
    seq.paramend[0]    = pSeqDesc->paramend[0];
    seq.paramend[1]    = pSeqDesc->paramend[1];
    seq.fadeintime     = pSeqDesc->fadeintime;
    seq.fadeouttime    = pSeqDesc->fadeouttime;
    seq.localentrynode = pSeqDesc->localentrynode;
    seq.localexitnode  = pSeqDesc->localexitnode;
    seq.ikResetMask    = pSeqDesc->ikResetMask;
    seq.unk1           = pSeqDesc->unk1;
}

static void ParseRLESection(const char* pBoneFlagArray, int numbones, uint32_t bfa_size, uint32_t sectionbaseframe, uint32_t sectionframes, temp::animdesc_t& anim) {
    for (uint32_t localframe = 0; localframe < sectionframes; localframe++) {
        const uint32_t frame = sectionbaseframe + localframe;
        auto* pTrack = PTR_FROM_IDX(anim::mstudio_rle_anim_t, pBoneFlagArray, bfa_size);

        for (int bone = 0; bone < numbones; bone++) {
            const uint8_t boneFlags = pBoneFlagArray[bone / 2] >> (4 * (bone % 2)) & 0xF;
            AssertMsg(boneFlags < 8, "BoneFlagArray is out of range.");

            Vector3& trackpos = anim.animdata[bone].pos[frame];
            Vector3& trackrot = anim.animdata[bone].rot[frame];
            Vector3& trackscl = anim.animdata[bone].scl[frame];

            if (boneFlags & RLE::BONEDATA) {
                auto* pTrackData = PTR_FROM_IDX(uint16_t, pTrack, sizeof(anim::mstudio_rle_anim_t));
                if (boneFlags & RLE::BONEPOS)   RLE::CalcBonePosition  (*pTrack, &pTrackData, trackpos, localframe);
                if (boneFlags & RLE::BONEROT)   RLE::CalcBoneQuaternion(*pTrack, &pTrackData, trackrot, localframe);
                if (boneFlags & RLE::BONESCALE) RLE::CalcBoneScale     (*pTrack, &pTrackData, trackscl, localframe);
                pTrack = (anim::mstudio_rle_anim_t*)((char*)pTrack + pTrack->size);
            }
        }
    }
}

static char* ResolveRLESectionBFA(int32_t sectionIdx, char* pAnimDescBase, temp::Sequence& seq) {
    if (sectionIdx < 0) {
        const int32_t off = -1 - sectionIdx;
        seq.extn = LoadFile(seq.path + "_extn");
        if ((size_t)off >= seq.extn.size) Error("passed the end of .rseq_extn '%s'", seq.extn.path.c_str());
        return PTR_FROM_IDX(char, seq.extn.buffer.data(), off);
    }
    if ((size_t)sectionIdx >= std::filesystem::file_size(seq.path)) Error("passed the end of .rseq '%s'", seq.path.c_str());
    return PTR_FROM_IDX(char, pAnimDescBase, sectionIdx);
}

static std::vector<char> ReadFileDirect(const std::string& path, size_t& outSize) {
    outSize = std::filesystem::file_size(path);
    std::vector<char> buffer(outSize);
    std::ifstream stream(path, std::ios::binary);
    if (stream.is_open())
        stream.read(buffer.data(), outSize);
    return buffer;
}

// ============================================================================
//  ParseRSEQ_v7.1
// ============================================================================

void ParseRSEQ_v71(std::string in_dir, temp::rig_t& rig) {
    ProgressBar bar(rig.rseqpaths.size());
    std::vector<std::future<void>> tasks;
    std::mutex mutex;

    if ((g_VerboseLevel == 1) && !rig.rseqpaths.empty()) bar.Print();

    tasks.reserve(rig.rseqpaths.size());
    for (const auto& file : rig.rseqpaths) {
        tasks.push_back(std::async(std::launch::async, [&, file]() {
            const std::string path = in_dir + "\\" + file;
            const std::filesystem::path rel = std::filesystem::relative(path, in_dir);

            if (!std::filesystem::is_regular_file(path)) {
                if (file != "df5") {
                    print("[!] Error: rseq not found for %s\n", rel.string().c_str());
                }
                return;
            }

            size_t inputFileSize;
            std::vector<char> buffer = ReadFileDirect(path, inputFileSize);
            const std::string out_dir = BuildOutputPath(in_dir, rel);

            if (inputFileSize <= sizeof(anim::v7::mstudioseqdesc_t)) {
                print("[!] Skipping %s (%zu byte)\n", std::filesystem::path(file).stem().string().c_str(), inputFileSize);
                return;
            }

            auto* pSeqDesc = reinterpret_cast<anim::v7::mstudioseqdesc_t*>(buffer.data());
            const std::string seqname = STRING_FROM_IDX(pSeqDesc, pSeqDesc->szlabelindex);
            const std::string activityname = STRING_FROM_IDX(pSeqDesc, pSeqDesc->szactivitynameindex);
            const int numanims = pSeqDesc->groupsize[0] * pSeqDesc->groupsize[1];
            const int numbones = (int)rig.bones.size();

            temp::Sequence seq(seqname, numbones);
            seq.anims.reserve(24);
            PopulateSeqCommon(seq, pSeqDesc, path, out_dir, activityname);

            seq.paramparent = pSeqDesc->paramparent;
            seq.nodeflags = pSeqDesc->nodeflags;
            seq.entryphase = pSeqDesc->entryphase;
            seq.exitphase = pSeqDesc->exitphase;
            seq.lastframe = pSeqDesc->lastframe;
            seq.nextseq = pSeqDesc->nextseq;
            seq.pose = pSeqDesc->pose;

            verbose("%s\n", seqname.c_str());

            ParsePoseKey(pSeqDesc, seq);
            ParseEvent(pSeqDesc, seq);
            ParseAutoLayer(pSeqDesc, seq);
            ParseWeightList(pSeqDesc, seq);
            ParseActMod(pSeqDesc, seq);

            auto* pBlends = PTR_FROM_IDX(int, pSeqDesc, pSeqDesc->animindexindex);
            std::vector<int32_t> animIndexes = GetAnimIndexes(pBlends, seq, numanims);

            const uint32_t bfa_size = ((numbones + 3) / 2) & ~1u;

            for (int anim_iter = 0; anim_iter < seq.numuniqueblends; anim_iter++) {
                auto* pAnimDesc = PTR_FROM_IDX(anim::v71::mstudioanimdesc_t, pSeqDesc, animIndexes[anim_iter]);

                temp::animdesc_t anim{};
                anim.name = STRING_FROM_IDX(pAnimDesc, pAnimDesc->sznameindex);
                anim.fps = pAnimDesc->fps;
                anim.flags = pAnimDesc->flags;
                anim.numframes = pAnimDesc->numframes;
                anim.InitData(rig, seq.IsAdditive());

                if (!(anim.flags & ANIM_VALID)) { seq.anims.push_back(std::move(anim)); continue; }

                anim.numsections = 1;
                anim::mstudioanimsections_t* animsections{};
                if (pAnimDesc->sectionindex) {
                    anim.numsections = GetSectionCount(*pAnimDesc);
                    animsections = PTR_FROM_IDX(anim::mstudioanimsections_t, pAnimDesc, pAnimDesc->sectionindex);
                }

                uint32_t sectionbaseframe = 0;
                for (uint32_t section = 0; section < anim.numsections; section++) {
                    const uint32_t sectionframes = GetSectionLength(*pAnimDesc, section, anim.numsections);

                    char* pBFA = PTR_FROM_IDX(char, pAnimDesc, pAnimDesc->animindex);
                    if (pAnimDesc->sectionindex) {
                        if (animsections[section].isExternal) {
                            seq.extn = LoadFile(path + "_extn");
                            AssertMsg(animsections[section].animidx < seq.extn.size, "passed the end of .rseq_extn '%s'", seq.extn.path.c_str());
                            pBFA = PTR_FROM_IDX(char, seq.extn.buffer.data(), animsections[section].animidx);
                        }
                        else {
                            pBFA = PTR_FROM_IDX(char, pAnimDesc, animsections[section].animidx);
                        }
                    }

                    ParseRLESection(pBFA, numbones, bfa_size, sectionbaseframe, sectionframes, anim);
                    sectionbaseframe += sectionframes;
                }

                RLE::ParseIkrules(pAnimDesc, anim);
                RLE::ParseFrameMovements(pAnimDesc, anim);
                seq.anims.push_back(std::move(anim));
            }

            {
                std::lock_guard<std::mutex> lock(mutex);
                rig.sequences.push_back(std::move(seq));
            }
            }));
        if (g_VerboseLevel == 1) bar.AddAndPrint();
    }
    for (auto& t : tasks) t.get();
    print("\n");
}

// ============================================================================
//  ParseRSEQ_v10
// ============================================================================

void ParseRSEQ_v10(std::string in_dir, temp::rig_t& rig) {
    ProgressBar bar(rig.rseqpaths.size());
    std::vector<std::future<void>> tasks;
    std::mutex mutex;

    if ((g_VerboseLevel == 1) && !rig.rseqpaths.empty()) bar.Print();

    tasks.reserve(rig.rseqpaths.size());
    for (const auto& file : rig.rseqpaths) {
        tasks.push_back(std::async(std::launch::async, [&, file]() {
            const std::string path          = in_dir + "\\" + file;
            const std::filesystem::path rel = std::filesystem::relative(path, in_dir);

            if (!std::filesystem::is_regular_file(path)) {
                print("[!] Error: rseq not found for %s\n", rel.string().c_str());
                return;
            }

            size_t inputFileSize;
            std::vector<char> buffer = ReadFileDirect(path, inputFileSize);
            const std::string out_dir = BuildOutputPath(in_dir, rel);

            if (inputFileSize <= sizeof(anim::v10::mstudioseqdesc_t)) {
                print("[!] Skipping %s (%zu byte)\n", std::filesystem::path(file).stem().string().c_str(), inputFileSize);
                return;
            }

            auto* pSeqDesc      = reinterpret_cast<anim::v10::mstudioseqdesc_t*>(buffer.data());
            const std::string seqname      = STRING_FROM_IDX(pSeqDesc, pSeqDesc->szlabelindex);
            const std::string activityname = STRING_FROM_IDX(pSeqDesc, pSeqDesc->szactivitynameindex);
            const int numanims  = pSeqDesc->groupsize[0] * pSeqDesc->groupsize[1];
            const int numbones  = (int)rig.bones.size();

            temp::Sequence seq(seqname, numbones);
            seq.anims.reserve(24);
            PopulateSeqCommon(seq, pSeqDesc, path, out_dir, activityname);

            seq.paramparent    = pSeqDesc->paramparent;
            seq.nodeflags      = pSeqDesc->nodeflags;
            seq.entryphase     = pSeqDesc->entryphase;
            seq.exitphase      = pSeqDesc->exitphase;
            seq.lastframe      = pSeqDesc->lastframe;
            seq.nextseq        = pSeqDesc->nextseq;
            seq.pose           = pSeqDesc->pose;

            verbose("%s\n", seqname.c_str());

            ParsePoseKey   (pSeqDesc, seq);
            ParseEvent     (pSeqDesc, seq);
            ParseAutoLayer (pSeqDesc, seq);
            ParseWeightList(pSeqDesc, seq);
            ParseActMod    (pSeqDesc, seq);

            auto* pBlends = PTR_FROM_IDX(int, pSeqDesc, pSeqDesc->animindexindex);
            std::vector<int32_t> animIndexes = GetAnimIndexes(pBlends, seq, numanims);

            const uint32_t bfa_size = ((numbones + 3) / 2) & ~1u;

            for (int anim_iter = 0; anim_iter < seq.numuniqueblends; anim_iter++) {
                auto* pAnimDesc = PTR_FROM_IDX(anim::v10::mstudioanimdesc_t, pSeqDesc, animIndexes[anim_iter]);

                temp::animdesc_t anim{};
                anim.name      = STRING_FROM_IDX(pAnimDesc, pAnimDesc->sznameindex);
                anim.fps       = pAnimDesc->fps;
                anim.flags     = pAnimDesc->flags;
                anim.numframes = pAnimDesc->numframes;
                anim.InitData(rig, seq.IsAdditive());

                if (!(anim.flags & ANIM_VALID)) { seq.anims.push_back(std::move(anim)); continue; }

                anim.numsections = 1;
                anim::mstudioanimsections_t* animsections{};
                if (pAnimDesc->sectionindex) {
                    anim.numsections = GetSectionCount(*pAnimDesc);
                    animsections = PTR_FROM_IDX(anim::mstudioanimsections_t, pAnimDesc, pAnimDesc->sectionindex);
                }

                uint32_t sectionbaseframe = 0;
                for (uint32_t section = 0; section < anim.numsections; section++) {
                    const uint32_t sectionframes = GetSectionLength(*pAnimDesc, section, anim.numsections);

                    char* pBFA = PTR_FROM_IDX(char, pAnimDesc, pAnimDesc->animindex);
                    if (pAnimDesc->sectionindex) {
                        if (animsections[section].isExternal) {
                            seq.extn = LoadFile(path + "_extn");
                            AssertMsg(animsections[section].animidx < seq.extn.size, "passed the end of .rseq_extn '%s'", seq.extn.path.c_str());
                            pBFA = PTR_FROM_IDX(char, seq.extn.buffer.data(), animsections[section].animidx);
                        }
                        else {
                            pBFA = PTR_FROM_IDX(char, pAnimDesc, animsections[section].animidx);
                        }
                    }

                    ParseRLESection(pBFA, numbones, bfa_size, sectionbaseframe, sectionframes, anim);
                    sectionbaseframe += sectionframes;
                }

                RLE::ParseIkrules       (pAnimDesc, anim);
                RLE::ParseFrameMovements(pAnimDesc, anim);
                seq.anims.push_back(std::move(anim));
            }

            {
                std::lock_guard<std::mutex> lock(mutex);
                rig.sequences.push_back(std::move(seq));
            }
        }));
        if (g_VerboseLevel == 1) bar.AddAndPrint();
    }
    for (auto& t : tasks) t.get();
    print("\n");
}

// ============================================================================
//  ParseRSEQ_v11
// ============================================================================

void ParseRSEQ_v11(std::string in_dir, temp::rig_t& rig) {
    ProgressBar bar(rig.rseqpaths.size());
    std::vector<std::future<void>> tasks;
    std::mutex mutex;

    if ((g_VerboseLevel == 1) && !rig.rseqpaths.empty()) bar.Print();

    tasks.reserve(rig.rseqpaths.size());
    for (const auto& file : rig.rseqpaths) {
        tasks.push_back(std::async(std::launch::async, [&, file]() {
            const std::string path          = in_dir + "\\" + file;
            const std::filesystem::path rel = std::filesystem::relative(path, in_dir);

            if (!std::filesystem::is_regular_file(path)) {
                print("[!] Error: rseq not found for %s\n", rel.string().c_str());
                return;
            }

            size_t inputFileSize;
            std::vector<char> buffer = ReadFileDirect(path, inputFileSize);
            const std::string out_dir = BuildOutputPath(in_dir, rel);

            if (inputFileSize <= sizeof(anim::v11::mstudioseqdesc_t)) {
                print("[!] Skipping %s (%zu byte)\n", std::filesystem::path(file).stem().string().c_str(), inputFileSize);
                return;
            }

            auto* pSeqDesc      = reinterpret_cast<anim::v11::mstudioseqdesc_t*>(buffer.data());
            const std::string seqname      = STRING_FROM_IDX(pSeqDesc, OFFSET(pSeqDesc->szlabelindex));
            const std::string activityname = STRING_FROM_IDX(pSeqDesc, OFFSET(pSeqDesc->szactivitynameindex));
            const int numanims  = pSeqDesc->groupsize[0] * pSeqDesc->groupsize[1];
            const int numbones  = (int)rig.bones.size();

            temp::Sequence seq(seqname, numbones);
            seq.anims.reserve(24);
            PopulateSeqCommon(seq, pSeqDesc, path, out_dir, activityname);

            verbose("%s\n", seqname.c_str());

            ParsePoseKey   (pSeqDesc, seq);
            ParseEvent     (pSeqDesc, seq);
            ParseAutoLayer (pSeqDesc, seq);
            ParseWeightList(pSeqDesc, seq);
            ParseActMod    (pSeqDesc, seq);

            auto* pBlends = PTR_FROM_IDX(uint16_t, pSeqDesc, OFFSET(pSeqDesc->animindexindex));
            std::vector<int32_t> animIndexes = GetAnimIndexes(pBlends, seq, numanims);

            const uint32_t bfa_size = ((numbones + 3) / 2) & ~1u;

            for (int anim_iter = 0; anim_iter < seq.numuniqueblends; anim_iter++) {
                auto* pAnimDesc = PTR_FROM_IDX(anim::v11::mstudioanimdesc_t, pSeqDesc, animIndexes[anim_iter]);

                if (uintptr_t(pAnimDesc) >= uintptr_t(pSeqDesc + OFFSET(pSeqDesc->szlabelindex))) continue;

                temp::animdesc_t anim{};
                anim.name      = STRING_FROM_IDX(pAnimDesc, OFFSET(pAnimDesc->sznameindex));
                anim.fps       = pAnimDesc->fps;
                anim.flags     = pAnimDesc->flags;
                anim.numframes = pAnimDesc->numframes;
                anim.sectionstallframes = pAnimDesc->sectionstallframes;
                anim.InitData(rig, seq.IsAdditive());

                if (!(anim.flags & ANIM_VALID)) { seq.anims.push_back(std::move(anim)); continue; }

                anim.numsections = 1;
                int32_t* animsections{};
                if (pAnimDesc->sectionindex) {
                    anim.numsections = GetSectionCount(*pAnimDesc);
                    animsections = reinterpret_cast<int32_t*>((char*)pAnimDesc + OFFSET(pAnimDesc->sectionindex));
                }

                uint32_t sectionbaseframe = 0;
                for (uint32_t section = 0; section < anim.numsections; section++) {
                    const uint32_t sectionframes = GetSectionLength(*pAnimDesc, section, anim.numsections);

                    char* pBFA = PTR_FROM_IDX(char, pAnimDesc, pAnimDesc->animindex);
                    if (pAnimDesc->sectionindex)
                        pBFA = ResolveRLESectionBFA(animsections[section], (char*)pAnimDesc, seq);

                    ParseRLESection(pBFA, numbones, bfa_size, sectionbaseframe, sectionframes, anim);
                    sectionbaseframe += sectionframes;
                }

                RLE::ParseIkrules       (pAnimDesc, anim);
                RLE::ParseFrameMovements(pAnimDesc, anim);
                seq.anims.push_back(std::move(anim));
            }

            {
                std::lock_guard<std::mutex> lock(mutex);
                rig.sequences.push_back(std::move(seq));
            }
        }));
        if (g_VerboseLevel == 1) bar.AddAndPrint();
    }
    for (auto& t : tasks) t.get();
    print("\n");
}


// ============================================================================
//  ParseRSEQ_v12
// ============================================================================

void ParseRSEQ_v12(std::string in_dir, temp::rig_t& rig) {
    ProgressBar bar(rig.rseqpaths.size());
    std::vector<std::future<void>> tasks;
    std::mutex mutex;

    if ((g_VerboseLevel == 1) && !rig.rseqpaths.empty()) bar.Print();

    tasks.reserve(rig.rseqpaths.size());
    for (const auto& file : rig.rseqpaths) {
        tasks.push_back(std::async(std::launch::async, [&, file]() {
            const std::string path = in_dir + "\\" + file;
            const std::filesystem::path rel = std::filesystem::relative(path, in_dir);

            if (!std::filesystem::is_regular_file(path)) {
                print("[!] Error: rseq not found for %s\n", rel.string().c_str());
                return;
            }

            size_t inputFileSize;
            std::vector<char> buffer = ReadFileDirect(path, inputFileSize);
            const char* stream_buffer = buffer.data();
            const std::string out_dir = BuildOutputPath(in_dir, rel);

            if (inputFileSize <= sizeof(anim::v12::mstudioseqdesc_t)) {
                print("[!] Skipping %s (%zu byte)\n", std::filesystem::path(file).stem().string().c_str(), inputFileSize);
                return;
            }

            auto* pSeqDesc = reinterpret_cast<const anim::v12::mstudioseqdesc_t*>(stream_buffer);
            const std::string seqname      = STRING_FROM_IDX(pSeqDesc, pSeqDesc->szlabelindex);
            const std::string activityname = STRING_FROM_IDX(pSeqDesc, pSeqDesc->szactivitynameindex);
            const int numanims = pSeqDesc->groupsize[0] * pSeqDesc->groupsize[1];
            const int numbones = (int)rig.bones.size();

            temp::Sequence seq(seqname, numbones);
            seq.anims.reserve(24);
            PopulateSeqCommon(seq, pSeqDesc, path, out_dir, activityname);

            verbose("%s\n", seqname.c_str());

            ParsePoseKey   (pSeqDesc, seq);
            ParseEvent     (pSeqDesc, seq);
            ParseAutoLayer (pSeqDesc, seq);
            ParseWeightList(pSeqDesc, seq);
            ParseActMod    (pSeqDesc, seq);

            auto* pBlends = PTR_FROM_IDX(uint16_t, stream_buffer, pSeqDesc->animindexindex);
            std::vector<int32_t> animIndexes = GetAnimIndexes(pBlends, seq, numanims);

            const uint32_t bfa_size = ((numbones + 3) / 2) & ~1u;

            for (int anim_iter = 0; anim_iter < seq.numuniqueblends; anim_iter++) {
                auto* pAnimDesc = PTR_FROM_IDX(anim::v12::mstudioanimdesc_t, stream_buffer, animIndexes[anim_iter]);

                temp::animdesc_t anim{};
                anim.name      = STRING_FROM_IDX(pAnimDesc, pAnimDesc->sznameindex);
                anim.fps       = pAnimDesc->fps;
                anim.flags     = pAnimDesc->flags;
                anim.numframes = pAnimDesc->numframes;
                anim.sectionstallframes = pAnimDesc->sectionstallframes;
                anim.InitData(rig, seq.IsAdditive());

                if (!(anim.flags & ANIM_VALID)) { seq.anims.push_back(std::move(anim)); continue; }

                if (anim.flags & ANIM_DATAPOINT) {
                    r5::DP::ParseDataPoint(pAnimDesc, rig, seq, anim);
                    RLE::ParseIkrules(pAnimDesc, anim);
                    if (pAnimDesc->flags & ANIM_FRAMEMOVEMENT)
                        r5::DP::ParseFrameMovementsDP(pAnimDesc, anim);
                }
                else {
                    anim.numsections = 1;
                    int32_t* animsections{};
                    if (pAnimDesc->sectionindex) {
                        anim.numsections = GetSectionCount(*pAnimDesc);
                        animsections = PTR_FROM_IDX(int32_t, pAnimDesc, pAnimDesc->sectionindex);
                    }

                    uint32_t sectionbaseframe = 0;
                    for (uint32_t section = 0; section < anim.numsections; section++) {
                        const uint32_t sectionframes = GetSectionLength(*pAnimDesc, section, anim.numsections);

                        char* pBFA = PTR_FROM_IDX(char, pAnimDesc, pAnimDesc->animindex);
                        if (pAnimDesc->sectionindex)
                            pBFA = ResolveRLESectionBFA(animsections[section], (char*)pAnimDesc, seq);

                        ParseRLESection(pBFA, numbones, bfa_size, sectionbaseframe, sectionframes, anim);
                        sectionbaseframe += sectionframes;
                    }

                    RLE::ParseIkrules       (pAnimDesc, anim);
                    RLE::ParseFrameMovements(pAnimDesc, anim);
                }
                seq.anims.push_back(std::move(anim));
            }

            {
                std::lock_guard<std::mutex> lock(mutex);
                rig.sequences.push_back(std::move(seq));
            }
        }));
        if (g_VerboseLevel == 1) bar.AddAndPrint();
    }
    for (auto& t : tasks) t.get();
    print("\n");
}


// ============================================================================
//  ParseRSEQ_v121
// ============================================================================

void ParseRSEQ_v121(std::string in_dir, temp::rig_t& rig) {
    ProgressBar bar(rig.rseqpaths.size());
    std::vector<std::future<void>> tasks;
    std::mutex mutex;

    if ((g_VerboseLevel == 1) && !rig.rseqpaths.empty()) bar.Print();

    tasks.reserve(rig.rseqpaths.size());
    for (const auto& file : rig.rseqpaths) {
        tasks.push_back(std::async(std::launch::async, [&, file]() {
            const std::string path          = in_dir + "\\" + file;
            const std::filesystem::path rel = std::filesystem::relative(path, in_dir);

            if (!std::filesystem::is_regular_file(path)) {
                print("[!] Error: rseq not found for %s\n", rel.string().c_str());
                return;
            }

            size_t inputFileSize;
            std::vector<char> buffer = ReadFileDirect(path, inputFileSize);
            const char* stream_buffer = buffer.data();
            const std::string out_dir = BuildOutputPath(in_dir, rel);

            if (inputFileSize <= sizeof(anim::v121::mstudioseqdesc_t)) {
                print("[!] Skipping %s (%zu byte)\n", std::filesystem::path(file).stem().string().c_str(), inputFileSize);
                return;
            }

            auto* pSeqDesc      = reinterpret_cast<const anim::v121::mstudioseqdesc_t*>(stream_buffer);
            const std::string seqname      = STRING_FROM_IDX(pSeqDesc, pSeqDesc->szlabelindex);
            const std::string activityname = STRING_FROM_IDX(pSeqDesc, pSeqDesc->szactivitynameindex);
            const int numanims  = pSeqDesc->groupsize[0] * pSeqDesc->groupsize[1];
            const int numbones  = (int)rig.bones.size();

            temp::Sequence seq(seqname, numbones);
            seq.anims.reserve(24);
            PopulateSeqCommon(seq, pSeqDesc, path, out_dir, activityname);

            verbose("%s\n", seqname.c_str());

            ParsePoseKey   (pSeqDesc, seq);
            ParseEvent     (pSeqDesc, seq);
            ParseAutoLayer (pSeqDesc, seq);
            ParseWeightList(pSeqDesc, seq);
            ParseActMod    (pSeqDesc, seq);

            auto* pBlends = PTR_FROM_IDX(uint16_t, stream_buffer, pSeqDesc->animindexindex);
            std::vector<int32_t> animIndexes = GetAnimIndexes(pBlends, seq, numanims);

            const uint32_t bfa_size = ((numbones + 3) / 2) & ~1u;

            for (int anim_iter = 0; anim_iter < seq.numuniqueblends; anim_iter++) {
                auto* pAnimDesc = PTR_FROM_IDX(anim::v121::mstudioanimdesc_t, stream_buffer, animIndexes[anim_iter]);

                temp::animdesc_t anim{};

                if (pAnimDesc->animDataAsset)
                    anim.asqd = LoadFile(std::format("{}/animseq_data/0x{:X}.asqd", in_dir, pAnimDesc->animDataAsset));

                anim.name      = STRING_FROM_IDX(pAnimDesc, pAnimDesc->sznameindex);
                anim.fps       = pAnimDesc->fps;
                anim.flags     = pAnimDesc->flags;
                anim.numframes = pAnimDesc->numframes;
                anim.sectionstallframes = pAnimDesc->sectionstallframes;
                anim.InitData(rig, seq.IsAdditive());

                if (!(anim.flags & ANIM_VALID)) { seq.anims.push_back(std::move(anim)); continue; }

                if (anim.flags & ANIM_DATAPOINT) {
                    AssertMsg(!anim.asqd.buffer.empty(), "DataPoint anim has no .asqd buffer for '%s'", seq.name.c_str());

                    r5::DP::ParseDataPoint(pAnimDesc, rig, seq, anim);
                    RLE::ParseIkrules(pAnimDesc, anim);
                    if (pAnimDesc->flags & ANIM_FRAMEMOVEMENT)
                        r5::DP::ParseFrameMovementsDP(pAnimDesc, anim);
                }
                else {
                    anim.numsections = 1;
                    int32_t* animsections{};
                    if (pAnimDesc->sectionindex) {
                        anim.numsections = GetSectionCount(*pAnimDesc);
                        animsections = PTR_FROM_IDX(int32_t, pAnimDesc, pAnimDesc->sectionindex);
                    }

                    uint32_t sectionbaseframe = 0;
                    for (uint32_t section = 0; section < anim.numsections; section++) {
                        const uint32_t sectionframes = GetSectionLength(*pAnimDesc, section, anim.numsections);

                        char* pBFA = reinterpret_cast<char*>(anim.asqd.buffer.data());
                        if (pAnimDesc->sectionindex && section) {
                            const int32_t sectionIdx = animsections[section - 1];
                            if (sectionIdx < 0) {
                                const int32_t offset = -1 - sectionIdx;
                                seq.extn = LoadFile(path + "_extn");
                                AssertMsg((size_t)offset < seq.extn.size, "passed the end of .rseq_extn '%s'", seq.extn.path.c_str());
                                pBFA = PTR_FROM_IDX(char, seq.extn.buffer.data(), offset);
                            }
                            else {
                                AssertMsg((size_t)sectionIdx < anim.asqd.size, "passed the end of .asqd '%s:%s'", seq.name.c_str(), anim.asqd.path.c_str());
                                pBFA = PTR_FROM_IDX(char, anim.asqd.buffer.data(), sectionIdx);
                            }
                        }

                        ParseRLESection(pBFA, numbones, bfa_size, sectionbaseframe, sectionframes, anim);
                        sectionbaseframe += sectionframes;
                    }

                    RLE::ParseIkrules       (pAnimDesc, anim);
                    RLE::ParseFrameMovements(pAnimDesc, anim);
                }
                seq.anims.push_back(std::move(anim));
            }

            {
                std::lock_guard<std::mutex> lock(mutex);
                rig.sequences.push_back(std::move(seq));
            }
        }));
        if (g_VerboseLevel == 1) bar.AddAndPrint();
    }
    for (auto& t : tasks) t.get();
    print("\n");
}

// ============================================================================
//  WriteRSEQ_v7
// ============================================================================

void WriteRSEQ_v7(temp::rig_t& rig) {
    ProgressBar bar(rig.sequences.size());
    std::vector<std::future<void>> tasks;
    std::mutex mutex;

    if ((g_VerboseLevel == 1) && !rig.sequences.empty()) bar.Print();

    tasks.reserve(rig.sequences.size());
    for (auto& seq : rig.sequences) {
        tasks.push_back(std::async(std::launch::async, [&]() {
            std::vector<char> buffer(8 * 1024 * 1024, 0);
            char* pBase = buffer.data();
            char* pData = pBase;
            temp::StringTable<int32_t> stringTables{};
            stringTables.Init();

            auto* v7RseqDesc = reinterpret_cast<anim::v7::mstudioseqdesc_t*>(pData);
            stringTables.Add(v7RseqDesc, &v7RseqDesc->szlabelindex,       seq.name);
            stringTables.Add(v7RseqDesc, &v7RseqDesc->szactivitynameindex, seq.activityname);
            v7RseqDesc->flags              = seq.flags;
            v7RseqDesc->activity           = seq.activity;
            v7RseqDesc->actweight          = seq.actweight;
            v7RseqDesc->bbmin              = seq.bbmin;
            v7RseqDesc->bbmax              = seq.bbmax;
            v7RseqDesc->numblends          = static_cast<uint32_t>(seq.blends.size());
            v7RseqDesc->groupsize[0]       = seq.groupsize[0];
            v7RseqDesc->groupsize[1]       = seq.groupsize[1];
            v7RseqDesc->paramindex[0]      = seq.paramindex[0];
            v7RseqDesc->paramindex[1]      = seq.paramindex[1];
            v7RseqDesc->paramstart[0]      = seq.paramstart[0];
            v7RseqDesc->paramstart[1]      = seq.paramstart[1];
            v7RseqDesc->paramend[0]        = seq.paramend[0];
            v7RseqDesc->paramend[1]        = seq.paramend[1];
            v7RseqDesc->paramparent        = seq.paramparent;
            v7RseqDesc->fadeintime         = seq.fadeintime;
            v7RseqDesc->fadeouttime        = seq.fadeouttime;
            v7RseqDesc->localentrynode     = (seq.localentrynode == rig.ignorenode) ? 0 : seq.localentrynode;
            v7RseqDesc->localexitnode      = (seq.localexitnode  == rig.ignorenode) ? 0 : seq.localexitnode;
            v7RseqDesc->nodeflags          = seq.nodeflags;
            v7RseqDesc->entryphase         = seq.entryphase;
            v7RseqDesc->exitphase          = seq.exitphase;
            v7RseqDesc->lastframe          = seq.lastframe;
            v7RseqDesc->nextseq            = seq.nextseq;
            v7RseqDesc->pose               = seq.pose;
            v7RseqDesc->numautolayers      = static_cast<uint32_t>(seq.autolayers.size());
            v7RseqDesc->numikrules         = 0;
            v7RseqDesc->numiklocks         = 0;
            v7RseqDesc->keyvaluesize       = 0;
            v7RseqDesc->cycleposeindex     = 0;
            v7RseqDesc->numactivitymodifiers = static_cast<uint32_t>(seq.actmods.size());
            v7RseqDesc->ikResetMask        = seq.ikResetMask;
            v7RseqDesc->unk1               = seq.unk1;
            v7RseqDesc->weightFixupCount   = 0;
            pData += sizeof(anim::v7::mstudioseqdesc_t);

            verbose("%s\n", seq.name.c_str());

            if (!seq.posekeys.empty()) {
                v7RseqDesc->posekeyindex = static_cast<uint32_t>(pData - pBase);
                auto* posekeys = reinterpret_cast<float*>(pData);
                const int pkcount = (int)seq.posekeys.size();
                for (int i = 0; i < pkcount; i++) posekeys[i] = seq.posekeys[i];
                pData += sizeof(float) * pkcount;
            }

            if (g_SkipEvents) {
                auto& evts = seq.events;
                evts.erase(std::remove_if(evts.begin(), evts.end(), [](const temp::seqevent_t& e) {
                    return e.name == "AE_CL_CREATE_PROP"              ||
                           e.name == "AE_CL_DESTROY_PROP"             ||
                           e.name == "AE_CL_SCRIPT_ANIM_WINDOW_BEGIN" ||
                           e.name == "AE_CL_SCRIPT_ANIM_WINDOW_END";
                }), evts.end());
            }

            v7RseqDesc->numevents  = static_cast<uint32_t>(seq.events.size());
            v7RseqDesc->eventindex = static_cast<uint32_t>(pData - pBase);
            if (v7RseqDesc->numevents) {
                auto* v7Events = reinterpret_cast<anim::v7::mstudioevent_t*>(pData);
                const int nevents = (int)v7RseqDesc->numevents;
                for (int i = 0; i < nevents; i++) {
                    stringTables.Add(&v7Events[i], &v7Events[i].szeventindex, seq.events[i].name);
                    v7Events[i].cycle  = seq.events[i].cycle;
                    v7Events[i].event  = seq.events[i].event;
                    v7Events[i].type   = seq.events[i].type;
                    memcpy_s(v7Events[i].options, 256, seq.events[i].options.c_str(), seq.events[i].options.length());
                }
            }
            pData += sizeof(anim::v7::mstudioevent_t) * v7RseqDesc->numevents;

            v7RseqDesc->autolayerindex = static_cast<int32_t>(pData - pBase);
            auto* v7Autolayer = reinterpret_cast<anim::v7::mstudioautolayer_t*>(pData);
            const int nautolayers = (int)seq.autolayers.size();
            for (int i = 0; i < nautolayers; i++) {
                v7Autolayer[i].guidSequence = seq.autolayers[i].guidSequence;
                v7Autolayer[i].flags        = seq.autolayers[i].flags;
                v7Autolayer[i].iPose        = seq.autolayers[i].iPose;
                v7Autolayer[i].iSequence    = seq.autolayers[i].iSequence;
                v7Autolayer[i].start        = seq.autolayers[i].start;
                v7Autolayer[i].end          = seq.autolayers[i].end;
                v7Autolayer[i].peak         = seq.autolayers[i].peak;
                v7Autolayer[i].tail         = seq.autolayers[i].tail;
            }
            pData += v7RseqDesc->numautolayers * sizeof(anim::v7::mstudioautolayer_t);

            v7RseqDesc->weightlistindex = static_cast<uint32_t>(pData - pBase);
            auto* v7WeightList = reinterpret_cast<float*>(pData);
            const int nbones = (int)rig.bones.size();
            for (int i = 0; i < nbones; i++) v7WeightList[i] = seq.weightlist[i];
            pData += sizeof(float) * nbones;

            v7RseqDesc->iklockindex   = static_cast<uint32_t>(pData - pBase);
            v7RseqDesc->keyvalueindex = static_cast<uint32_t>(pData - pBase);

            v7RseqDesc->activitymodifierindex = static_cast<uint32_t>(pData - pBase);
            auto* v7Actmod = reinterpret_cast<anim::v7::mstudioactivitymodifier_t*>(pData);
            const int nactmods = (int)seq.actmods.size();
            for (int i = 0; i < nactmods; i++) {
                stringTables.Add(&v7Actmod[i], &v7Actmod[i].sznameindex, seq.actmods[i].name);
                v7Actmod[i].negate = seq.actmods[i].negate;
            }
            pData += sizeof(anim::v7::mstudioactivitymodifier_t) * v7RseqDesc->numactivitymodifiers;

            std::vector<std::pair<int, int>> blends_index_map;
            v7RseqDesc->animindexindex = static_cast<uint32_t>(pData - pBase);
            auto* v7Blends = reinterpret_cast<int*>(pData);
            pData += sizeof(int) * v7RseqDesc->numblends;

            for (int anim_iter = 0; anim_iter < seq.numuniqueblends; anim_iter++) {
                blends_index_map.push_back({ seq.blends[anim_iter], (int)(pData - pBase) });
                temp::animdesc_t anim = seq.anims[anim_iter];
                anim.SubtractBase(rig.bones.size(), rig, seq.IsAdditive());

                constexpr uint32_t targetsectionframes = 61;

                auto* animDesc = reinterpret_cast<anim::v7::mstudioanimdesc_t*>(pData);
                stringTables.Add(animDesc, &animDesc->sznameindex, anim.name);
                animDesc->fps           = anim.fps;
                animDesc->flags         = anim.flags & ~r5::ANIM_DATAPOINT;
                animDesc->numframes     = anim.numframes;
                animDesc->nummovements  = 0;
                animDesc->framemovementindex = 0;
                animDesc->numikrules    = static_cast<uint32_t>(anim.ikrules.size());
                animDesc->sectionframes = (anim.numframes > targetsectionframes) ? targetsectionframes : 0;

                pData += sizeof(anim::v7::mstudioanimdesc_t);
                animDesc->animindex = static_cast<int32_t>(pData - (char*)animDesc);

                if (!(anim.flags & ANIM_VALID)) continue;

                anim.numsections = 1;
                uint32_t* animsections{};
                if (anim.numframes > targetsectionframes) {
                    animDesc->sectionindex = static_cast<int32_t>(pData - (char*)animDesc);
                    anim.numsections = GetSectionCount(*animDesc);
                    animsections = PTR_FROM_IDX(uint32_t, pData, 0);
                    pData += sizeof(uint32_t) * anim.numsections;
                }
                animDesc->animindex = static_cast<int32_t>(pData - (char*)animDesc);

                uint32_t startframe = 0;
                for (uint32_t section = 0; section < anim.numsections; section++) {
                    const uint32_t sectionframes  = GetSectionLength(*animDesc, section, anim.numsections);
                    const bool     bInterpframe    = (section + 1 != anim.numsections);
                    const uint32_t endframe        = startframe + sectionframes + bInterpframe;

                    if (anim.numsections > 1)
                        animsections[section] = static_cast<int32_t>(pData - (char*)animDesc);

                    const uint32_t bfa_size = ((rig.bones.size() + 3) / 2) & ~1u;
                    static thread_local std::vector<uint8_t> flaggedBones;
                    flaggedBones.assign(bfa_size * 2, 0);
                    char* boneflagarray = reinterpret_cast<char*>(pData);
                    pData += bfa_size;

                    for (int bone = 0; bone < nbones; bone++) {
                        auto& animData = anim.animdata[bone];
                        uint8_t boneFlags = 0;

                        const bool bRawpos = allEqualVector(animData.pos, startframe, endframe);
                        const bool bRawrot = allEqualVector(animData.rot, startframe, endframe);
                        const bool bRawscl = allEqualVector(animData.scl, startframe, endframe);

                        const bool bHasPosData = !bRawpos || !animData.pos[startframe].approx_equal({0,0,0});
                        const bool bHasRotData = !bRawrot || !animData.rot[startframe].approx_equal(rig.bones[bone].rot);
                        const bool bHasSclData = !bRawscl || !animData.scl[startframe].approx_equal({0,0,0});

                        if (!bHasPosData && !bHasRotData && !bHasSclData) continue;

                        auto* animRLE    = reinterpret_cast<anim::mstudio_rle_anim_t*>(pData);
                        pData += sizeof(anim::mstudio_rle_anim_t);

                        anim::studioanimvalue_ptr_t* animposptr{};
                        anim::studioanimvalue_ptr_t* animrotptr{};
                        anim::studioanimvalue_ptr_t* animsclptr{};

                        float posscale = 0.003906369f;
                        Vector3 maxpos{}, minpos{};
                        findMinMaxSIMD(animData.pos, startframe, endframe, minpos, maxpos);
                        const float v1max = std::max({ std::fabs(maxpos.Max()), std::fabs(minpos.Min()) });
                        if (v1max > 127.f) posscale = (v1max * 2.f) / 65534.f;

                        if (bHasPosData) {
                            boneFlags |= 0x1;
                            if (bRawpos) {
                                Vector3 val = animData.pos[startframe];
                                if (!seq.IsAdditive()) val += rig.bones[bone].pos;
                                *(Vector48*)pData = Pack48(val);
                                pData += sizeof(Vector48);
                            }
                            else {
                                animRLE->bAnimPosition = true;
                                *(float*)pData = posscale;
                                pData += sizeof(float);
                                animposptr = reinterpret_cast<anim::studioanimvalue_ptr_t*>(pData);
                                pData += sizeof(anim::studioanimvalue_ptr_t);
                            }
                        }

                        if (bHasRotData) {
                            boneFlags |= 0x2;
                            if (bRawrot) {
                                Quaternion q{};
                                AngleQuaternion(animData.rot[startframe], q);
                                *(Quaternion64*)pData = PackQuat64(q);
                                pData += sizeof(Quaternion64);
                            }
                            else {
                                animRLE->bAnimRotation = true;
                                animrotptr = reinterpret_cast<anim::studioanimvalue_ptr_t*>(pData);
                                pData += sizeof(anim::studioanimvalue_ptr_t);
                            }
                        }

                        if (bHasSclData) {
                            boneFlags |= 0x4;
                            if (bRawscl) {
                                Vector3 val = animData.scl[startframe];
                                if (!seq.IsAdditive()) val += rig.bones[bone].scl;
                                *(Vector48*)pData = Pack48(val);
                                pData += sizeof(Vector48);
                            }
                            else {
                                animRLE->bAnimScale = true;
                                animsclptr = reinterpret_cast<anim::studioanimvalue_ptr_t*>(pData);
                                pData += sizeof(anim::studioanimvalue_ptr_t);
                            }
                        }

                        if (bHasPosData && !bRawpos) WriteAnim(pData, animposptr, animData.pos, startframe, endframe, posscale);
                        if (bHasRotData && !bRawrot) WriteAnim(pData, animrotptr, animData.rot, startframe, endframe, 0.00019175345f);
                        if (bHasSclData && !bRawscl) WriteAnim(pData, animsclptr, animData.scl, startframe, endframe, 0.0030518509f);

                        animRLE->size = (int)(pData - (char*)animRLE);
                        flaggedBones[bone] = boneFlags;

                        const int bfahalf = (int)(flaggedBones.size() / 2);
                        for (int i = 0; i < bfahalf; i++) {
                            boneflagarray[i]  = flaggedBones[i * 2];
                            boneflagarray[i] |= flaggedBones[i * 2 + 1] << 4;
                        }
                    }
                    ALIGN4(pData);
                    startframe += sectionframes;
                }

                if (!anim.ikrules.empty()) {
                    v7RseqDesc->numikrules = std::max(v7RseqDesc->numikrules, (int)anim.ikrules.size());
                    animDesc->numikrules   = static_cast<uint32_t>(anim.ikrules.size());
                    animDesc->ikruleindex  = static_cast<int32_t>(pData - (char*)animDesc);
                    auto* v7Ikrule         = reinterpret_cast<anim::v7::mstudioikrule_t*>(pData);

                    for (int i = 0; i < (int)anim.ikrules.size(); i++) {
                        temp::ikrule_t& ikrule = anim.ikrules[i];
                        v7Ikrule[i].index        = ikrule.index;
                        v7Ikrule[i].type         = ikrule.type;
                        v7Ikrule[i].chain        = ikrule.chain;
                        v7Ikrule[i].bone         = ikrule.bone;
                        v7Ikrule[i].slot         = ikrule.slot;
                        v7Ikrule[i].height       = ikrule.height;
                        v7Ikrule[i].radius       = ikrule.radius;
                        v7Ikrule[i].pos          = ikrule.pos;
                        v7Ikrule[i].q            = ikrule.q;
                        for (int j = 0; j < 6; j++) v7Ikrule[i].scale[j] = ikrule.scale[j];
                        v7Ikrule[i].sectionframes = ikrule.sectionframes;
                        v7Ikrule[i].iStart       = ikrule.iStart;
                        v7Ikrule[i].start        = ikrule.start;
                        v7Ikrule[i].peak         = ikrule.peak;
                        v7Ikrule[i].tail         = ikrule.tail;
                        v7Ikrule[i].end          = ikrule.end;
                        v7Ikrule[i].contact      = ikrule.contact;
                        v7Ikrule[i].drop         = ikrule.drop;
                        v7Ikrule[i].top          = ikrule.top;
                        v7Ikrule[i].endHeight    = ikrule.endHeight;
                        if (!ikrule.attachmentname.empty())
                            stringTables.Add(&v7Ikrule[i], &v7Ikrule[i].szattachmentindex, ikrule.attachmentname);
                        pData += sizeof(anim::v7::mstudioikrule_t);
                    }

                    for (int i = 0; i < (int)anim.ikrules.size(); i++) {
                        v7Ikrule[i].compressedikerrorindex = static_cast<int32_t>(pData - (char*)&v7Ikrule[i]);
                        temp::ikrule_t& ikrule = anim.ikrules[i];
                        if (!ikrule.sectionframes) continue;

                        const int32_t sectioncount = static_cast<int32_t>((float)(anim.numframes - 1) / (float)ikrule.sectionframes) + 1;
                        auto* sectionindices = reinterpret_cast<int32_t*>(pData);
                        pData += sizeof(int32_t) * sectioncount;

                        Vector3 mn{}, mx{}, mn1{}, mx1{};
                        findMinMaxSIMD(ikrule.ikruledata.pos, 0, anim.numframes, mn, mx);
                        findMinMaxSIMD(ikrule.ikruledata.rot, 0, anim.numframes, mn1, mx1);
                        for (int j = 0; j < 6; j++) {
                            const float v1 = (j < 3)
                                ? std::max({ std::fabs(mx[j]),    std::fabs(mn[j]) })
                                : std::max({ std::fabs(mx1[j-3]), std::fabs(mn1[j-3]) });
                            if (v1 > 127.f) v7Ikrule[i].scale[j] = (v1 * 2.f) / 65534.f;
                        }

                        startframe = 0;
                        for (int section = 0; section < sectioncount; section++) {
                            sectionindices[section] = static_cast<int32_t>(pData - (char*)&v7Ikrule[i]);
                            const int sectionframes = GetSectionLength(anim.numframes, ikrule.sectionframes, section);
                            const uint32_t framerange = sectionframes + !(section + 1 == sectioncount);
                            const uint32_t endframe   = startframe + framerange;

                            auto* offsets = reinterpret_cast<int16_t*>(pData);
                            pData += 6 * sizeof(int16_t);

                            for (int idx = 0; idx < 3; idx++) {
                                offsets[idx] = static_cast<int16_t>(pData - (char*)offsets);
                                WriteAnimData(pData, ikrule.ikruledata.pos, startframe, endframe, idx, ikrule.scale[idx]);
                            }
                            for (int idx = 0; idx < 3; idx++) {
                                offsets[idx + 3] = static_cast<int16_t>(pData - (char*)offsets);
                                WriteAnimData(pData, ikrule.ikruledata.rot, startframe, endframe, idx, ikrule.scale[idx + 3]);
                            }
                            startframe += sectionframes;
                        }
                    }
                }

                if ((anim.flags & r5::ANIM_FRAMEMOVEMENT) && anim.movement.sectionframes != 0) {
                    animDesc->framemovementindex = static_cast<int32_t>(pData - (char*)animDesc);
                    auto* frameMovement = reinterpret_cast<anim::v7::mstudioframemovement_t*>(pData);
                    frameMovement->scale         = anim.movement.scale;
                    frameMovement->sectionframes = anim.movement.sectionframes;
                    pData += sizeof(anim::v7::mstudioframemovement_t);

                    auto& movementdata = anim.movement.movementdata;
                    const uint32_t sectioncount = static_cast<uint32_t>((float)(anim.numframes - 1) / (float)anim.movement.sectionframes) + 1;
                    auto* sectionindices = reinterpret_cast<int32_t*>(pData);
                    pData += sizeof(uint32_t) * sectioncount;

                    Vector4 mn{}, mx{};
                    findMinMaxSIMD(movementdata, 0, (int)movementdata.size(), mn, mx);
                    for (int idx = 0; idx < 4; idx++) {
                        const float v1 = std::max({ std::fabs(mx[idx]), std::fabs(mn[idx]) });
                        if (v1 > 127.f) frameMovement->scale[idx] = (v1 * 2.f) / 65534.f;
                    }

                    startframe = 0;
                    for (uint32_t section = 0; section < sectioncount; section++) {
                        sectionindices[section] = static_cast<int32_t>(pData - (char*)frameMovement);
                        const int      sectionframes = GetSectionLength(anim.numframes, anim.movement.sectionframes, section);
                        const uint32_t framerange    = sectionframes + !(section + 1 == sectioncount);
                        const uint32_t endframe      = startframe + framerange;

                        auto* offsets = reinterpret_cast<int16_t*>(pData);
                        pData += 4 * sizeof(int16_t);

                        for (int idx = 0; idx < 4; idx++) {
                            if (allEqualVector(movementdata, startframe, endframe, idx, frameMovement->scale[idx]) && movementdata[startframe][idx] == 0) continue;

                            offsets[idx] = static_cast<int16_t>(pData - (char*)offsets);
                            WriteAnimData(pData, movementdata, startframe, endframe, idx, frameMovement->scale[idx]);
                        }
                        startframe += sectionframes;
                    }
                }
            }

            for (int iter = 0; iter < (int)v7RseqDesc->numblends; iter++)
                v7Blends[iter] = blends_index_map[seq.blends[iter]].second;

            v7RseqDesc->weightFixupOffset = static_cast<uint32_t>(pData - pBase);

            pData = stringTables.Write(pData);
            ALIGN4(pData);

            std::filesystem::create_directories(std::filesystem::path(seq.outpath).parent_path());
            std::ofstream outRseq(seq.outpath, std::ios::out | std::ios::binary);
            outRseq.write(pBase, pData - pBase);

            {
                std::lock_guard<std::mutex> lock(mutex);
                seq.anims.clear();
            }

        }));
        if (g_VerboseLevel == 1) bar.AddAndPrint();
    }
    for (auto& t : tasks) t.get();
    print("\n");
}


// ============================================================================
//  WriteRSEQ_v11
// ============================================================================

void WriteRSEQ_v11(temp::rig_t& rig) {
    ProgressBar bar(rig.sequences.size());
    std::vector<std::future<void>> tasks;
    std::mutex mutex;

    if ((g_VerboseLevel == 1) && !rig.sequences.empty()) bar.Print();

    tasks.reserve(rig.sequences.size());
    for (auto& seq : rig.sequences) {
        tasks.push_back(std::async(std::launch::async, [&]() {
            std::vector<char> buffer(8 * 1024 * 1024, 0);
            char* pBase = buffer.data();
            char* pData = pBase;
            temp::StringTable<uint16_t> stringTables{};
            stringTables.Init();

            auto* v11SeqDesc = reinterpret_cast<anim::v11::mstudioseqdesc_t*>(pData);
            stringTables.Add(v11SeqDesc, &v11SeqDesc->szlabelindex,        seq.name);
            stringTables.Add(v11SeqDesc, &v11SeqDesc->szactivitynameindex, seq.activityname);
            v11SeqDesc->flags               = seq.flags;
            v11SeqDesc->activity            = static_cast<uint16_t>(seq.activity);
            v11SeqDesc->actweight           = static_cast<uint16_t>(seq.actweight);
            v11SeqDesc->bbmin               = seq.bbmin;
            v11SeqDesc->bbmax               = seq.bbmax;
            v11SeqDesc->numblends           = static_cast<uint16_t>(seq.blends.size());
            v11SeqDesc->groupsize[0]        = static_cast<uint8_t>(seq.groupsize[0]);
            v11SeqDesc->groupsize[1]        = static_cast<uint8_t>(seq.groupsize[1]);
            v11SeqDesc->paramindex[0]       = static_cast<short>(seq.paramindex[0]);
            v11SeqDesc->paramindex[1]       = static_cast<short>(seq.paramindex[1]);
            v11SeqDesc->paramstart[0]       = seq.paramstart[0];
            v11SeqDesc->paramstart[1]       = seq.paramstart[1];
            v11SeqDesc->paramend[0]         = seq.paramend[0];
            v11SeqDesc->paramend[1]         = seq.paramend[1];
            v11SeqDesc->fadeintime          = seq.fadeintime;
            v11SeqDesc->fadeouttime         = seq.fadeouttime;
            v11SeqDesc->localentrynode      = (seq.localentrynode == rig.ignorenode) ? 0 : static_cast<uint16_t>(seq.localentrynode);
            v11SeqDesc->localexitnode       = (seq.localexitnode  == rig.ignorenode) ? 0 : static_cast<uint16_t>(seq.localexitnode);
            v11SeqDesc->numikrules          = 0;
            v11SeqDesc->numiklocks          = 0;
            v11SeqDesc->unk_5C              = 0;
            v11SeqDesc->cycleposeindex      = 0;
            v11SeqDesc->numautolayers       = static_cast<uint16_t>(seq.autolayers.size());
            v11SeqDesc->numactivitymodifiers = static_cast<uint16_t>(seq.actmods.size());
            v11SeqDesc->ikResetMask         = seq.ikResetMask;
            v11SeqDesc->unk1                = seq.unk1;
            v11SeqDesc->weightFixupCount    = 0;
            pData += sizeof(anim::v11::mstudioseqdesc_t);

            verbose("%s\n", seq.name.c_str());

            ALIGN2(pData);
            if (!seq.posekeys.empty()) {
                v11SeqDesc->posekeyindex = SHORTOFFSET(pBase, pData);
                auto* posekeys = reinterpret_cast<float*>(pData);
                const int pkcount = (int)seq.posekeys.size();
                for (int i = 0; i < pkcount; i++) posekeys[i] = seq.posekeys[i];
                pData += sizeof(float) * pkcount;
            }

            if (g_SkipEvents) {
                auto& evts = seq.events;
                evts.erase(std::remove_if(evts.begin(), evts.end(), [](const temp::seqevent_t& e) {
                    return e.name == "AE_CL_CREATE_PROP"              ||
                           e.name == "AE_CL_DESTROY_PROP"             ||
                           e.name == "AE_CL_SCRIPT_ANIM_WINDOW_BEGIN" ||
                           e.name == "AE_CL_SCRIPT_ANIM_WINDOW_END";
                }), evts.end());
            }

            ALIGN2(pData);
            v11SeqDesc->numevents  = static_cast<uint16_t>(seq.events.size());
            v11SeqDesc->eventindex = SHORTOFFSET(pBase, pData);
            if (v11SeqDesc->numevents) {
                auto* v11Events = reinterpret_cast<anim::v11::mstudioevent_t*>(pData);
                const int nevents = (int)v11SeqDesc->numevents;
                for (int i = 0; i < nevents; i++) {
                    v11Events[i].cycle = seq.events[i].cycle;
                    v11Events[i].event = seq.events[i].event;
                    v11Events[i].type  = seq.events[i].type;
                    v11Events[i].unk   = 0;
                    stringTables.Add(&v11Events[i], reinterpret_cast<uint16_t*>(&v11Events[i].szeventindex), seq.events[i].name);
                    stringTables.Add(&v11Events[i], reinterpret_cast<uint16_t*>(&v11Events[i].optionsindex), seq.events[i].options);
                }
            }
            pData += sizeof(anim::v11::mstudioevent_t) * v11SeqDesc->numevents;

            ALIGN2(pData);
            v11SeqDesc->autolayerindex = SHORTOFFSET(pBase, pData);
            auto* v11Autolayer = reinterpret_cast<anim::v11::mstudioautolayer_t*>(pData);
            const int nautolayers = (int)seq.autolayers.size();
            for (int i = 0; i < nautolayers; i++) {
                v11Autolayer[i].guidSequence = seq.autolayers[i].guidSequence;
                v11Autolayer[i].iPose        = seq.autolayers[i].iPose;
                v11Autolayer[i].flags        = seq.autolayers[i].flags;
                v11Autolayer[i].start        = seq.autolayers[i].start;
                v11Autolayer[i].peak         = seq.autolayers[i].peak;
                v11Autolayer[i].tail         = seq.autolayers[i].tail;
                v11Autolayer[i].end          = seq.autolayers[i].end;
            }
            pData += v11SeqDesc->numautolayers * sizeof(anim::v11::mstudioautolayer_t);

            ALIGN2(pData);
            v11SeqDesc->weightlistindex = SHORTOFFSET(pBase, pData);
            auto* v11WeightList = reinterpret_cast<float*>(pData);
            const int nbones = (int)rig.bones.size();
            for (int i = 0; i < nbones; i++) v11WeightList[i] = seq.weightlist[i];
            pData += sizeof(float) * nbones;

            ALIGN2(pData);
            v11SeqDesc->iklockindex = SHORTOFFSET(pBase, pData);

            ALIGN2(pData);
            v11SeqDesc->activitymodifierindex = SHORTOFFSET(pBase, pData);
            auto* v11Actmod = reinterpret_cast<anim::v11::mstudioactivitymodifier_t*>(pData);
            const int nactmods = (int)seq.actmods.size();
            for (int i = 0; i < nactmods; i++) {
                stringTables.Add(&v11Actmod[i], &v11Actmod[i].sznameindex, seq.actmods[i].name);
                v11Actmod[i].negate = seq.actmods[i].negate;
                v11Actmod[i].pad    = 0;
            }
            pData += sizeof(anim::v11::mstudioactivitymodifier_t) * v11SeqDesc->numactivitymodifiers;

            ALIGN2(pData);
            std::vector<std::pair<int, uint16_t>> blends_index_map;
            v11SeqDesc->animindexindex = SHORTOFFSET(pBase, pData);
            auto* v11Blends = reinterpret_cast<uint16_t*>(pData);
            pData += sizeof(uint16_t) * v11SeqDesc->numblends;

            std::vector<char> extn_buffer(8 * 1024 * 1024, 0);
            char* pBaseExtn = extn_buffer.data();
            char* pDataExtn = pBaseExtn;

            constexpr uint32_t targetsectionframes = 61;

            ALIGN4(pData);
            for (int anim_iter = 0; anim_iter < seq.numuniqueblends; anim_iter++) {
                uint16_t blends_short = SHORTOFFSET(pBase, pData);
                blends_index_map.push_back({ seq.blends[anim_iter], blends_short });
                auto& anim = seq.anims[anim_iter];

                auto* animDesc = reinterpret_cast<anim::v11::mstudioanimdesc_t*>(pData);
                stringTables.Add(animDesc, &animDesc->sznameindex, anim.name);
                animDesc->fps                 = anim.fps;
                animDesc->flags               = anim.flags & ~r5::ANIM_DATAPOINT;
                animDesc->numframes           = anim.numframes;
                animDesc->framemovementindex  = 0;
                animDesc->numikrules          = static_cast<uint16_t>(anim.ikrules.size());
                animDesc->sectionDataExternal = 0;
                animDesc->unk1                = 0;
                animDesc->sectionstallframes  = anim.sectionstallframes;
                animDesc->sectionframes       = (anim.numframes > (int)targetsectionframes) ? static_cast<uint16_t>(targetsectionframes) : 0;
                animDesc->sectionindex        = 0;
                animDesc->ikruleindex         = 0;

                pData += sizeof(anim::v11::mstudioanimdesc_t);
                animDesc->animindex = static_cast<int32_t>(pData - (char*)animDesc);

                anim.pAnimDesc    = animDesc;
                anim.pAnimSections = nullptr;
                anim.numsections  = 1;

                if (anim.flags & ANIM_VALID) {
                    if (anim.numframes > (int)targetsectionframes) {
                        animDesc->sectionindex = SHORTOFFSET(animDesc, pData);
                        anim.numsections = GetSectionCount(*animDesc);
                        anim.pAnimSections = reinterpret_cast<int32_t*>(pData);
                        pData += sizeof(int32_t) * anim.numsections;
                    }
                    animDesc->animindex = static_cast<int32_t>(pData - (char*)animDesc);
                }
            }

            ALIGN2(pData);
            pData = stringTables.Write(pData);
            ALIGN4(pData);

            for (int anim_iter = 0; anim_iter < seq.numuniqueblends; anim_iter++) {
                temp::animdesc_t anim = seq.anims[anim_iter];
                anim.SubtractBase(rig.bones.size(), rig, seq.IsAdditive());
                auto* animDesc = reinterpret_cast<anim::v11::mstudioanimdesc_t*>(anim.pAnimDesc);

                if (!(anim.flags & ANIM_VALID)) continue;

                animDesc->animindex = static_cast<int32_t>(pData - (char*)animDesc);

                uint32_t startframe = 0;
                for (uint32_t section = 0; section < anim.numsections; section++) {
                    const uint32_t sectionframes = GetSectionLength(*animDesc, section, anim.numsections);
                    const bool     bInterpframe  = (section + 1 != anim.numsections);
                    const uint32_t endframe      = startframe + sectionframes + bInterpframe;

                    const bool bIsExtnSection = (anim.numframes >= 100) && (anim.numsections > 2) && (section != 0) && (section != (anim.numsections - 1));
                    char*& pOut = bIsExtnSection ? pDataExtn : pData;

                    if (anim.numsections > 1) {
                        if (bIsExtnSection)
                            anim.pAnimSections[section] = -1 - static_cast<int32_t>(pDataExtn - pBaseExtn);
                        else
                            anim.pAnimSections[section] = static_cast<int32_t>(pData - (char*)animDesc);
                    }

                    const uint32_t bfa_size = ((rig.bones.size() + 3) / 2) & ~1u;
                    static thread_local std::vector<uint8_t> flaggedBones;
                    flaggedBones.assign(bfa_size * 2, 0);
                    char* boneflagarray = reinterpret_cast<char*>(pOut);
                    pOut += bfa_size;

                    for (int bone = 0; bone < nbones; bone++) {
                        auto& animData = anim.animdata[bone];
                        uint8_t boneFlags = 0;

                        const bool bRawpos = allEqualVector(animData.pos, startframe, endframe);
                        const bool bRawrot = allEqualVector(animData.rot, startframe, endframe);
                        const bool bRawscl = allEqualVector(animData.scl, startframe, endframe);

                        const bool bHasPosData = !bRawpos || !animData.pos[startframe].approx_equal({0,0,0});
                        const bool bHasRotData = !bRawrot || !animData.rot[startframe].approx_equal(rig.bones[bone].rot);
                        const bool bHasSclData = !bRawscl || !animData.scl[startframe].approx_equal({0,0,0});

                        if (!bHasPosData && !bHasRotData && !bHasSclData) continue;

                        auto* animRLE = reinterpret_cast<anim::mstudio_rle_anim_t*>(pOut);
                        pOut += sizeof(anim::mstudio_rle_anim_t);

                        anim::studioanimvalue_ptr_t* animposptr{};
                        anim::studioanimvalue_ptr_t* animrotptr{};
                        anim::studioanimvalue_ptr_t* animsclptr{};

                        float posscale = 0.003906369f;
                        Vector3 maxpos{}, minpos{};
                        findMinMaxSIMD(animData.pos, startframe, endframe, minpos, maxpos);
                        const float v1max = std::max({ std::fabs(maxpos.Max()), std::fabs(minpos.Min()) });
                        if (v1max > 127.f) posscale = (v1max * 2.f) / 65534.f;

                        if (bHasPosData) {
                            boneFlags |= 0x1;
                            if (bRawpos) {
                                Vector3 val = animData.pos[startframe];
                                if (!seq.IsAdditive()) val += rig.bones[bone].pos;
                                *(Vector48*)pOut = Pack48(val);
                                pOut += sizeof(Vector48);
                            }
                            else {
                                animRLE->bAnimPosition = true;
                                *(float*)pOut = posscale;
                                pOut += sizeof(float);
                                animposptr = reinterpret_cast<anim::studioanimvalue_ptr_t*>(pOut);
                                pOut += sizeof(anim::studioanimvalue_ptr_t);
                            }
                        }

                        if (bHasRotData) {
                            boneFlags |= 0x2;
                            if (bRawrot) {
                                Quaternion q{};
                                AngleQuaternion(animData.rot[startframe], q);
                                *(Quaternion64*)pOut = PackQuat64(q);
                                pOut += sizeof(Quaternion64);
                            }
                            else {
                                animRLE->bAnimRotation = true;
                                animrotptr = reinterpret_cast<anim::studioanimvalue_ptr_t*>(pOut);
                                pOut += sizeof(anim::studioanimvalue_ptr_t);
                            }
                        }

                        if (bHasSclData) {
                            boneFlags |= 0x4;
                            if (bRawscl) {
                                Vector3 val = animData.scl[startframe];
                                if (!seq.IsAdditive()) val += rig.bones[bone].scl;
                                *(Vector48*)pOut = Pack48(val);
                                pOut += sizeof(Vector48);
                            }
                            else {
                                animRLE->bAnimScale = true;
                                animsclptr = reinterpret_cast<anim::studioanimvalue_ptr_t*>(pOut);
                                pOut += sizeof(anim::studioanimvalue_ptr_t);
                            }
                        }

                        if (bHasPosData && !bRawpos) WriteAnim(pOut, animposptr, animData.pos, startframe, endframe, posscale);
                        if (bHasRotData && !bRawrot) WriteAnim(pOut, animrotptr, animData.rot, startframe, endframe, 0.00019175345f);
                        if (bHasSclData && !bRawscl) WriteAnim(pOut, animsclptr, animData.scl, startframe, endframe, 0.0030518509f);

                        animRLE->size = (int)(pOut - (char*)animRLE);
                        flaggedBones[bone] = boneFlags;

                        const int bfahalf = (int)(flaggedBones.size() / 2);
                        for (int i = 0; i < bfahalf; i++) {
                            boneflagarray[i]  = flaggedBones[i * 2];
                            boneflagarray[i] |= flaggedBones[i * 2 + 1] << 4;
                        }
                    }
                    ALIGN4(pOut);
                    startframe += sectionframes;
                }

                ALIGN2(pData);
                if (!anim.ikrules.empty()) {
                    v11SeqDesc->numikrules = std::max((int)v11SeqDesc->numikrules, (int)anim.ikrules.size());
                    animDesc->numikrules   = static_cast<uint16_t>(anim.ikrules.size());
                    animDesc->ikruleindex  = SHORTOFFSET(animDesc, pData);
                    auto* v11Ikrule        = reinterpret_cast<anim::v11::mstudioikrule_t*>(pData);

                    for (int i = 0; i < (int)anim.ikrules.size(); i++) {
                        const temp::ikrule_t& ikrule = anim.ikrules[i];
                        v11Ikrule[i].chain        = static_cast<short>(ikrule.chain);
                        v11Ikrule[i].bone         = static_cast<short>(ikrule.bone);
                        v11Ikrule[i].type         = static_cast<char>(ikrule.type);
                        v11Ikrule[i].slot         = static_cast<char>(ikrule.slot);
                        v11Ikrule[i].sectionframes = static_cast<uint16_t>(ikrule.sectionframes);
                        for (int j = 0; j < 6; j++) v11Ikrule[i].scale[j] = ikrule.scale[j];
                        v11Ikrule[i].compressedikerrorindex = 0;
                        v11Ikrule[i].iStart        = static_cast<short>(ikrule.iStart);
                        v11Ikrule[i].ikerrorindex  = 0;
                        v11Ikrule[i].szattachmentindex = 0;
                        v11Ikrule[i].start         = ikrule.start;
                        v11Ikrule[i].peak          = ikrule.peak;
                        v11Ikrule[i].tail          = ikrule.tail;
                        v11Ikrule[i].end           = ikrule.end;
                        v11Ikrule[i].contact       = ikrule.contact;
                        v11Ikrule[i].drop          = ikrule.drop;
                        v11Ikrule[i].top           = ikrule.top;
                        v11Ikrule[i].height        = ikrule.height;
                        v11Ikrule[i].endHeight     = ikrule.endHeight;
                        v11Ikrule[i].radius        = ikrule.radius;
                        v11Ikrule[i].floor         = ikrule.floor;
                        v11Ikrule[i].pos           = ikrule.pos;
                        v11Ikrule[i].q             = ikrule.q;
                        if (!ikrule.attachmentname.empty())
                            stringTables.Add(&v11Ikrule[i], &v11Ikrule[i].szattachmentindex, ikrule.attachmentname);
                        pData += sizeof(anim::v11::mstudioikrule_t);
                    }

                    for (int i = 0; i < (int)anim.ikrules.size(); i++) {
                        const temp::ikrule_t& ikrule = anim.ikrules[i];
                        if (!ikrule.sectionframes) continue;

                        v11Ikrule[i].compressedikerrorindex = SHORTOFFSET(&v11Ikrule[i], pData);

                        const int32_t sectioncount = static_cast<int32_t>((float)(anim.numframes - 1) / (float)ikrule.sectionframes) + 1;
                        auto* sectionindices = reinterpret_cast<uint16_t*>(pData);
                        pData += sizeof(uint16_t) * sectioncount;

                        Vector3 mn{}, mx{}, mn1{}, mx1{};
                        findMinMaxSIMD(ikrule.ikruledata.pos, 0, anim.numframes, mn, mx);
                        findMinMaxSIMD(ikrule.ikruledata.rot, 0, anim.numframes, mn1, mx1);
                        for (int j = 0; j < 6; j++) {
                            const float v1 = (j < 3)
                                ? std::max({ std::fabs(mx[j]),    std::fabs(mn[j]) })
                                : std::max({ std::fabs(mx1[j-3]), std::fabs(mn1[j-3]) });
                            if (v1 > 127.f) v11Ikrule[i].scale[j] = (v1 * 2.f) / 65534.f;
                        }

                        uint32_t ikstartframe = 0;
                        for (int section = 0; section < sectioncount; section++) {
                            sectionindices[section] = SHORTOFFSET(&v11Ikrule[i], pData);
                            const int sectionframes = GetSectionLength(anim.numframes, ikrule.sectionframes, section);
                            const uint32_t framerange = sectionframes + !(section + 1 == sectioncount);
                            const uint32_t endframe   = ikstartframe + framerange;

                            auto* offsets = reinterpret_cast<int16_t*>(pData);
                            pData += 6 * sizeof(int16_t);

                            for (int idx = 0; idx < 3; idx++) {
                                offsets[idx] = static_cast<int16_t>(pData - (char*)offsets);
                                WriteAnimData(pData, ikrule.ikruledata.pos, ikstartframe, endframe, idx, ikrule.scale[idx]);
                            }
                            for (int idx = 0; idx < 3; idx++) {
                                offsets[idx + 3] = static_cast<int16_t>(pData - (char*)offsets);
                                WriteAnimData(pData, ikrule.ikruledata.rot, ikstartframe, endframe, idx, ikrule.scale[idx + 3]);
                            }
                            ikstartframe += sectionframes;
                        }
                    }
                }

                ALIGN2(pData);
                if ((anim.flags & r5::ANIM_FRAMEMOVEMENT) && anim.movement.sectionframes != 0) {
                    animDesc->framemovementindex = SHORTOFFSET(animDesc, pData);
                    auto* frameMovement = reinterpret_cast<anim::v7::mstudioframemovement_t*>(pData);
                    frameMovement->scale         = anim.movement.scale;
                    frameMovement->sectionframes = anim.movement.sectionframes;
                    pData += sizeof(anim::v7::mstudioframemovement_t);

                    auto& movementdata = anim.movement.movementdata;
                    const uint32_t sectioncount = static_cast<uint32_t>((float)(anim.numframes - 1) / (float)anim.movement.sectionframes) + 1;
                    auto* sectionindices = reinterpret_cast<uint16_t*>(pData);
                    pData += sizeof(uint16_t) * sectioncount;

                    Vector4 mn{}, mx{};
                    findMinMaxSIMD(movementdata, 0, (int)movementdata.size(), mn, mx);
                    for (int idx = 0; idx < 4; idx++) {
                        const float v1 = std::max({ std::fabs(mx[idx]), std::fabs(mn[idx]) });
                        if (v1 > 127.f) frameMovement->scale[idx] = (v1 * 2.f) / 65534.f;
                    }

                    uint32_t fmstartframe = 0;
                    for (uint32_t section = 0; section < sectioncount; section++) {
                        sectionindices[section] = SHORTOFFSET(frameMovement, pData);
                        const int      sectionframes = GetSectionLength(anim.numframes, anim.movement.sectionframes, section);
                        const uint32_t framerange    = sectionframes + !(section + 1 == sectioncount);
                        const uint32_t endframe      = fmstartframe + framerange;

                        auto* offsets = reinterpret_cast<int16_t*>(pData);
                        pData += 4 * sizeof(int16_t);

                        for (int idx = 0; idx < 4; idx++) {
                            if (allEqualVector(movementdata, fmstartframe, endframe, idx, frameMovement->scale[idx]) && movementdata[fmstartframe][idx] == 0) continue;
                            offsets[idx] = static_cast<int16_t>(pData - (char*)offsets);
                            WriteAnimData(pData, movementdata, fmstartframe, endframe, idx, frameMovement->scale[idx]);
                        }
                        fmstartframe += sectionframes;
                    }
                }
            }

            for (int iter = 0; iter < (int)v11SeqDesc->numblends; iter++)
                v11Blends[iter] = blends_index_map[seq.blends[iter]].second;

            ALIGN2(pData);
            v11SeqDesc->weightFixupOffset = SHORTOFFSET(pBase, pData);
            ALIGN4(pData);

            // write rseq
            std::filesystem::create_directories(std::filesystem::path(seq.outpath).parent_path());
            std::ofstream outRseq(seq.outpath, std::ios::out | std::ios::binary);
            outRseq.write(pBase, pData - pBase);

            // write extn
            if(pDataExtn - pBaseExtn) {
                std::ofstream outRseqExtn(seq.outpath + "_extn", std::ios::out | std::ios::binary);
                outRseqExtn.write(pBaseExtn, pDataExtn - pBaseExtn);
            }

            {
                std::lock_guard<std::mutex> lock(mutex);
                seq.anims.clear();
            }
        }));
        if (g_VerboseLevel == 1) bar.AddAndPrint();
    }
    for (auto& t : tasks) t.get();
    print("\n");
}
