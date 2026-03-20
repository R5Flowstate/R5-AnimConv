#include <pch.h>
#include <rseq/rseq.h>

using namespace r5;

static temp::file_t LoadFile(const std::string& path) {
    if (!std::filesystem::exists(path))
        PRINTANDTHROW(path.c_str(), "[!] Error: file is missing.");
    
    temp::file_t f{};

	f.path = path;
    f.size = std::filesystem::file_size(path);
	f.buffer.resize(f.size);

    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) PRINTANDTHROW(path.c_str(), "[!] Error: cannot open file for reading.");
    stream.read(f.buffer.data(), f.size);
    if (!stream || stream.gcount() != static_cast<std::streamsize>(f.size)) PRINTANDTHROW(path.c_str(), "[!] Error: failed to read entire file.");
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
            const uint8_t boneFlags = pBoneFlagArray[bone / 2] >> (4 * (bone % 2));

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
        if ((size_t)off >= seq.extn.size) PRINTANDTHROW(seq.extn.path.c_str(), "[!] Error: Passed the end of .rseq_extn");
        return PTR_FROM_IDX(char, seq.extn.buffer.data(), off);
    }
    else {
        if ((size_t)sectionIdx >= std::filesystem::file_size(seq.path)) PRINTANDTHROW(seq.path.c_str(), "[!] Error: Passed the end of .rseq");
        return PTR_FROM_IDX(char, pAnimDescBase, sectionIdx);
    }
}

// ============================================================================
//  ParseRSEQ_v10
// ============================================================================

void ParseRSEQ_v10(std::string in_dir, temp::rig_t& rig) {
    ProgressBar bar(rig.rseqpaths.size());
    std::vector<std::future<void>> tasks;
    std::mutex mutex;

    if (!_enable_verbose && !rig.rseqpaths.empty()) bar.Print();

    for (const auto& file : rig.rseqpaths) {
        tasks.push_back(std::async(std::launch::async, [&, file]() {
            const std::string path          = in_dir + "\\" + file;
            const std::filesystem::path rel = std::filesystem::relative(path, in_dir);

            if (!std::filesystem::is_regular_file(path)) {
                printf("[!] Error: rseq not found for %s\n", rel.string().c_str());
                return;
            }

            const size_t inputFileSize = std::filesystem::file_size(path);
            std::vector<char> buffer(inputFileSize, 0); {
                std::lock_guard<std::mutex> lock(mutex);
                std::ifstream stream(path, std::ios::binary);
                stream.read(buffer.data(), inputFileSize);
            }

            const std::string out_dir = BuildOutputPath(in_dir, rel);

            if (inputFileSize <= sizeof(anim::v10::mstudioseqdesc_t)) {
                printf("[!] Skipping %s (%zu byte)\n",
                       std::filesystem::path(file).stem().string().c_str(), inputFileSize);
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

            // Blends
            auto* pBlends = PTR_FROM_IDX(int, pSeqDesc, pSeqDesc->animindexindex);
            std::vector<int32_t> animIndexes = GetAnimIndexes(pBlends, seq, numanims);

            for (int anim_iter = 0; anim_iter < seq.numuniqueblends; anim_iter++) {
                auto* pAnimDesc = PTR_FROM_IDX(anim::v10::mstudioanimdesc_t, pSeqDesc, animIndexes[anim_iter]);

                temp::animdesc_t anim{};
                anim.name      = STRING_FROM_IDX(pAnimDesc, pAnimDesc->sznameindex);
                anim.fps       = pAnimDesc->fps;
                anim.flags     = pAnimDesc->flags;
                anim.numframes = pAnimDesc->numframes;
                anim.InitData(rig, seq.IsAdditive());

                if (!(anim.flags & ANIM_VALID)) { seq.anims.push_back(anim); continue; }

                uint32_t num_sections = 1;
                anim::mstudioanimsections_t* animsections{};
                if (pAnimDesc->sectionindex) {
                    num_sections = GetSectionCount(*pAnimDesc);
                    animsections = PTR_FROM_IDX(anim::mstudioanimsections_t, pAnimDesc, pAnimDesc->sectionindex);
                }
                const uint32_t bfa_size = ((numbones + 3) / 2) & ~1u;

                uint32_t sectionbaseframe = 0;
                for (uint32_t section = 0; section < num_sections; section++) {
                    const uint32_t sectionframes = GetSectionLength(*pAnimDesc, section, num_sections);

                    char* pBFA = PTR_FROM_IDX(char, pAnimDesc, pAnimDesc->animindex);
                    if (pAnimDesc->sectionindex) {
                        if (animsections[section].isExternal) {
							seq.extn = LoadFile(path + "_extn");
                            if (animsections[section].animidx >= seq.extn.size)
                                PRINTANDTHROW(seq.extn.path.c_str(), "[!] Error: Passed the end of .rseq_extn");
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
                seq.anims.push_back(anim);
            }

            {
                std::lock_guard<std::mutex> lock(mutex);
                rig.sequences.push_back(std::move(seq));
            }
            std::vector<char>().swap(buffer);
        }));
        if (!_enable_verbose) bar.AddAndPrint();
    }
    for (auto& t : tasks) t.get();
    printf("\n");
}


// ============================================================================
//  ParseRSEQ_v11
// ============================================================================

void ParseRSEQ_v11(std::string in_dir, temp::rig_t& rig) {
    ProgressBar bar(rig.rseqpaths.size());
    std::vector<std::future<void>> tasks;
    std::mutex mutex;

    if (!_enable_verbose && !rig.rseqpaths.empty()) bar.Print();

    for (const auto& file : rig.rseqpaths) {
        tasks.push_back(std::async(std::launch::async, [&, file]() {
            const std::string path          = in_dir + "\\" + file;
            const std::filesystem::path rel = std::filesystem::relative(path, in_dir);

            if (!std::filesystem::is_regular_file(path)) {
                printf("[!] Error: rseq not found for %s\n", rel.string().c_str());
                return;
            }

            const size_t inputFileSize = std::filesystem::file_size(path);
            std::vector<char> buffer(inputFileSize, 0); {
                std::lock_guard<std::mutex> lock(mutex);
                std::ifstream stream(path, std::ios::binary);
                stream.read(buffer.data(), inputFileSize);
            }

            const std::string out_dir = BuildOutputPath(in_dir, rel);

            if (inputFileSize <= sizeof(anim::v11::mstudioseqdesc_t)) {
                printf("[!] Skipping %s (%zu byte)\n",
                       std::filesystem::path(file).stem().string().c_str(), inputFileSize);
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

            // Blends
            auto* pBlends = PTR_FROM_IDX(uint16_t, pSeqDesc, OFFSET(pSeqDesc->animindexindex));
            std::vector<int32_t> animIndexes = GetAnimIndexes(pBlends, seq, numanims);

            for (int anim_iter = 0; anim_iter < seq.numuniqueblends; anim_iter++) {
                auto* pAnimDesc = PTR_FROM_IDX(anim::v11::mstudioanimdesc_t, pSeqDesc, animIndexes[anim_iter]);

                temp::animdesc_t anim{};
                anim.name      = STRING_FROM_IDX(pAnimDesc, OFFSET(pAnimDesc->sznameindex));
                anim.fps       = pAnimDesc->fps;
                anim.flags     = pAnimDesc->flags;
                anim.numframes = pAnimDesc->numframes;
                anim.InitData(rig, seq.IsAdditive());

                if (!(anim.flags & ANIM_VALID)) { seq.anims.push_back(anim); continue; }

                // Section setup
                uint32_t num_sections = 1;
                int32_t* animsections{};
                if (pAnimDesc->sectionindex) {
                    num_sections = GetSectionCount(*pAnimDesc);
                    animsections = reinterpret_cast<int32_t*>((char*)pAnimDesc + OFFSET(pAnimDesc->sectionindex));
                }
                const uint32_t bfa_size = ((numbones + 3) / 2) & ~1u;

                uint32_t sectionbaseframe = 0;
                for (uint32_t section = 0; section < num_sections; section++) {
                    const uint32_t sectionframes = GetSectionLength(*pAnimDesc, section, num_sections);

                    char* pBFA = PTR_FROM_IDX(char, pAnimDesc, OFFSET(pAnimDesc->animindex));
                    if (pAnimDesc->sectionindex) {
                        pBFA = ResolveRLESectionBFA(animsections[section], (char*)pAnimDesc, seq);
                    }

                    ParseRLESection(pBFA, numbones, bfa_size, sectionbaseframe, sectionframes, anim);
                    sectionbaseframe += sectionframes;
                }

                RLE::ParseIkrules       (pAnimDesc, anim);
                RLE::ParseFrameMovements(pAnimDesc, anim);
                seq.anims.push_back(anim);
            }

            {
                std::lock_guard<std::mutex> lock(mutex);
                rig.sequences.push_back(std::move(seq));
            }
            std::vector<char>().swap(buffer);
        }));
        if (!_enable_verbose) bar.AddAndPrint();
    }
    for (auto& t : tasks) t.get();
    printf("\n");
}


// ============================================================================
//  ParseRSEQ_v12
// ============================================================================

void ParseRSEQ_v12(std::string in_dir, temp::rig_t& rig) {
    ProgressBar bar(rig.rseqpaths.size());
    std::vector<std::future<void>> tasks;
    std::mutex mutex;

    if (!_enable_verbose && !rig.rseqpaths.empty()) bar.Print();

    for (const auto& file : rig.rseqpaths) {
        tasks.push_back(std::async(std::launch::async, [&, file]() {
            const std::string path = in_dir + "\\" + file;
            const std::filesystem::path rel = std::filesystem::relative(path, in_dir);

            if (!std::filesystem::is_regular_file(path)) {
                printf("[!] Error: rseq not found for %s\n", rel.string().c_str());
                return;
            }

            size_t inputFileSize = 0;
            char* stream_buffer = nullptr; {
                inputFileSize = std::filesystem::file_size(path);
                stream_buffer = new char[inputFileSize];
                std::lock_guard<std::mutex> lock(mutex);
                std::ifstream stream(path, std::ios::binary);
                stream.read(stream_buffer, inputFileSize);
            }

            const std::string out_dir = BuildOutputPath(in_dir, rel);

            if (inputFileSize <= sizeof(anim::v12::mstudioseqdesc_t)) {
                printf("[!] Skipping %s (%zu byte)\n",
                    std::filesystem::path(file).stem().string().c_str(), inputFileSize);
                return;
            }

            auto* pSeqDesc = reinterpret_cast<anim::v12::mstudioseqdesc_t*>(stream_buffer);
            const std::string seqname = STRING_FROM_IDX(pSeqDesc, pSeqDesc->szlabelindex);
            const std::string activityname = STRING_FROM_IDX(pSeqDesc, pSeqDesc->szactivitynameindex);
            const int numanims = pSeqDesc->groupsize[0] * pSeqDesc->groupsize[1];
            const int numbones = (int)rig.bones.size();

            temp::Sequence seq(seqname, numbones);
            seq.anims.reserve(24);
            PopulateSeqCommon(seq, pSeqDesc, path, out_dir, activityname);

            verbose("%s\n", seqname.c_str());

            ParsePoseKey(pSeqDesc, seq);
            ParseEvent(pSeqDesc, seq);
            ParseAutoLayer(pSeqDesc, seq);
            ParseWeightList(pSeqDesc, seq);
            ParseActMod(pSeqDesc, seq);

            // Blends
            auto* pBlends = PTR_FROM_IDX(uint16_t, stream_buffer, pSeqDesc->animindexindex);
            std::vector<int32_t> animIndexes = GetAnimIndexes(pBlends, seq, numanims);

            for (int anim_iter = 0; anim_iter < seq.numuniqueblends; anim_iter++) {
                auto* pAnimDesc = PTR_FROM_IDX(anim::v12::mstudioanimdesc_t, stream_buffer, animIndexes[anim_iter]);

                temp::animdesc_t anim{};
                anim.name = STRING_FROM_IDX(pAnimDesc, pAnimDesc->sznameindex);
                anim.fps = pAnimDesc->fps;
                anim.flags = pAnimDesc->flags;
                anim.numframes = pAnimDesc->numframes;
                anim.InitData(rig, seq.IsAdditive());

                if (!(anim.flags & ANIM_VALID)) { seq.anims.push_back(anim); continue; }

                if (anim.flags & ANIM_DATAPOINT) {
                    std::vector<Quaternion> boneQuats(numbones);
                    std::vector<Vector3>    bonePoses(numbones);
                    for (int i = 0; i < numbones; i++) {
                        boneQuats[i] = rig.bones[i].q;
                        bonePoses[i] = rig.bones[i].pos;
                    }

                    char* primary_dp = PTR_FROM_IDX(char, pAnimDesc, OFFSET(pAnimDesc->animindex));
                    r5::DP::ParseDataPoint(pAnimDesc, rig, seq, anim);

                    RLE::ParseIkrules(pAnimDesc, anim);
                    if (pAnimDesc->flags & ANIM_FRAMEMOVEMENT)
                        r5::DP::ParseFrameMovementsDP(pAnimDesc, anim);
                }
                else {
                    uint32_t num_sections = 1;
                    int32_t* animsections{};
                    if (pAnimDesc->sectionindex) {
                        num_sections = GetSectionCount(*pAnimDesc);
                        animsections = PTR_FROM_IDX(int32_t, pAnimDesc, pAnimDesc->sectionindex);
                    }
                    const uint32_t bfa_size = ((numbones + 3) / 2) & ~1u;

                    uint32_t sectionbaseframe = 0;
                    for (uint32_t section = 0; section < num_sections; section++) {
                        const uint32_t sectionframes = GetSectionLength(*pAnimDesc, section, num_sections);

                        char* pBFA = PTR_FROM_IDX(char, pAnimDesc, OFFSET(pAnimDesc->animindex));
                        if (pAnimDesc->sectionindex) {
                            pBFA = ResolveRLESectionBFA(animsections[section], (char*)pAnimDesc, seq);
                        }

                        ParseRLESection(pBFA, numbones, bfa_size, sectionbaseframe, sectionframes, anim);
                        sectionbaseframe += sectionframes;
                    }

                    RLE::ParseIkrules(pAnimDesc, anim);
                    RLE::ParseFrameMovements(pAnimDesc, anim);
                }
                seq.anims.push_back(anim);
            }

            {
                std::lock_guard<std::mutex> lock(mutex);
                rig.sequences.push_back(std::move(seq));
            }
        }));
        if (!_enable_verbose) bar.AddAndPrint();
    }
    for (auto& t : tasks) t.get();
    printf("\n");
}


// ============================================================================
//  ParseRSEQ_v121
// ============================================================================

void ParseRSEQ_v121(std::string in_dir, temp::rig_t& rig) {
    ProgressBar bar(rig.rseqpaths.size());
    std::vector<std::future<void>> tasks;
    std::mutex mutex;

    if (!_enable_verbose && !rig.rseqpaths.empty()) bar.Print();

    for (const auto& file : rig.rseqpaths) {
        tasks.push_back(std::async(std::launch::async, [&, file]() {
            const std::string path          = in_dir + "\\" + file;
            const std::filesystem::path rel = std::filesystem::relative(path, in_dir);

            if (!std::filesystem::is_regular_file(path)) {
                printf("[!] Error: rseq not found for %s\n", rel.string().c_str());
                return;
            }

            size_t inputFileSize = 0;
            char*  stream_buffer = nullptr; {
                inputFileSize = std::filesystem::file_size(path);
                stream_buffer = new char[inputFileSize];
                std::lock_guard<std::mutex> lock(mutex);
                std::ifstream stream(path, std::ios::binary);
                stream.read(stream_buffer, inputFileSize);
            }

            const std::string out_dir = BuildOutputPath(in_dir, rel);

            if (inputFileSize <= sizeof(anim::v121::mstudioseqdesc_t)) {
                printf("[!] Skipping %s (%zu byte)\n",
                       std::filesystem::path(file).stem().string().c_str(), inputFileSize);
                return;
            }

            auto* pSeqDesc      = reinterpret_cast<anim::v121::mstudioseqdesc_t*>(stream_buffer);
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

            // Blends
            auto* pBlends = PTR_FROM_IDX(uint16_t, stream_buffer, pSeqDesc->animindexindex);
            std::vector<int32_t> animIndexes = GetAnimIndexes(pBlends, seq, numanims);

            for (int anim_iter = 0; anim_iter < seq.numuniqueblends; anim_iter++) {
                auto* pAnimDesc = PTR_FROM_IDX(anim::v121::mstudioanimdesc_t, stream_buffer, animIndexes[anim_iter]);

                temp::animdesc_t anim{};

                if (pAnimDesc->animDataAsset) {
                    anim.asqd = LoadFile(std::format("{}/animseq_data/0x{:X}.asqd", in_dir, pAnimDesc->animDataAsset));
                }

                anim.name      = STRING_FROM_IDX(pAnimDesc, pAnimDesc->sznameindex);
                anim.fps       = pAnimDesc->fps;
                anim.flags     = pAnimDesc->flags;
                anim.numframes = pAnimDesc->numframes;
                anim.InitData(rig, seq.IsAdditive());

                if (!(anim.flags & ANIM_VALID)) { seq.anims.push_back(anim); continue; }

                if (anim.flags & ANIM_DATAPOINT) {
                    if (anim.asqd.buffer.empty())
                        PRINTANDTHROW(seq.name.c_str(), "[!] Error: DataPoint anim has no .asqd buffer.");

                    std::vector<Quaternion> boneQuats(numbones);
                    std::vector<Vector3>    bonePoses(numbones);
                    for (int i = 0; i < numbones; i++) {
                        boneQuats[i] = rig.bones[i].q;
                        bonePoses[i] = rig.bones[i].pos;
                    }

                    r5::DP::ParseDataPoint(pAnimDesc, rig, seq, anim);
                    RLE::ParseIkrules(pAnimDesc, anim);
                    if (pAnimDesc->flags & ANIM_FRAMEMOVEMENT) 
                        r5::DP::ParseFrameMovementsDP(pAnimDesc, anim);
                }
                else {
                    uint32_t num_sections = 1;
                    int32_t* animsections{};
                    if (pAnimDesc->sectionindex) {
                        num_sections = GetSectionCount(*pAnimDesc);
                        animsections = PTR_FROM_IDX(int32_t, pAnimDesc, pAnimDesc->sectionindex);
                    }
                    const uint32_t bfa_size = ((numbones + 3) / 2) & ~1u;

                    uint32_t sectionbaseframe = 0;
                    for (uint32_t section = 0; section < num_sections; section++) {
                        const uint32_t sectionframes = GetSectionLength(*pAnimDesc, section, num_sections);

                        char* pBFA = reinterpret_cast<char*>(anim.asqd.buffer.data());
                        if (pAnimDesc->sectionindex && section) {
                            const int32_t sectionIdx = animsections[section - 1];
                            if (sectionIdx < 0) {
                                const int32_t offset = -1 - sectionIdx;
                                seq.extn = LoadFile(path + "_extn");
                                if ((size_t)offset >= seq.extn.size) PRINTANDTHROW(seq.extn.path.c_str(), "[!] Error: Passed the end .rseq_extn");
                                pBFA = PTR_FROM_IDX(char, seq.extn.buffer.data(), offset);
                            }
                            else {
                                if ((size_t)sectionIdx >= anim.asqd.size) PRINTANDTHROW((seq.name + ":" + anim.asqd.path).c_str(), "[!] Error: Passed the end of .asqd");
                                pBFA = PTR_FROM_IDX(char, anim.asqd.buffer.data(), sectionIdx);
                            }
                        }

                        ParseRLESection(pBFA, numbones, bfa_size, sectionbaseframe, sectionframes, anim);
                        sectionbaseframe += sectionframes;
                    }

                    RLE::ParseIkrules       (pAnimDesc, anim);
                    RLE::ParseFrameMovements(pAnimDesc, anim);
                }
                seq.anims.push_back(anim);
            }

            {
                std::lock_guard<std::mutex> lock(mutex);
                rig.sequences.push_back(std::move(seq));
            }
        }));
        if (!_enable_verbose) bar.AddAndPrint();
    }
    for (auto& t : tasks) t.get();
    printf("\n");
}

// ============================================================================
//  WriteRSEQ_v7
// ============================================================================

void WriteRSEQ_v7(temp::rig_t& rig, bool bSkipEvents) {
    ProgressBar bar(rig.sequences.size());
    std::vector<std::future<void>> tasks;
    std::mutex mutex;

    if (!_enable_verbose && !rig.sequences.empty()) bar.Print();

    for (auto& seq : rig.sequences) {
        tasks.push_back(std::async(std::launch::async, [&]() {
            std::vector<char> buffer(8 * 1024 * 1024, 0);
            char* pBase = buffer.data();
            char* pData = pBase;
            temp::StringTable stringTables{};
            stringTables.Init();

            std::filesystem::create_directories(std::filesystem::path(seq.outpath).parent_path());
            std::ofstream outRseq(seq.outpath, std::ios::out | std::ios::binary);

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
                for (int i = 0; i < (int)seq.posekeys.size(); i++) posekeys[i] = seq.posekeys[i];
                pData += sizeof(float) * seq.posekeys.size();
            }

            // TODO: remove this
            if (bSkipEvents) {
                for (int i = 0; i < (int)seq.events.size(); i++) {
                    const auto& name = seq.events[i].name;
                    if (name == "AE_CL_CREATE_PROP"          || name == "AE_CL_DESTROY_PROP" ||
                        name == "AE_CL_SCRIPT_ANIM_WINDOW_BEGIN" || name == "AE_CL_SCRIPT_ANIM_WINDOW_END") {
                        seq.events.erase(seq.events.begin() + i--);
                    }
                }
            }

            v7RseqDesc->numevents   = static_cast<uint32_t>(seq.events.size());
            v7RseqDesc->eventindex  = static_cast<uint32_t>(pData - pBase);
            if (v7RseqDesc->numevents) {
                auto* v7Events = reinterpret_cast<anim::v7::mstudioevent_t*>(pData);
                for (int i = 0; i < (int)v7RseqDesc->numevents; i++) {
                    stringTables.Add(&v7Events[i], &v7Events[i].szeventindex, seq.events[i].name);
                    v7Events[i].cycle  = seq.events[i].cycle;
                    v7Events[i].event  = seq.events[i].event;
                    v7Events[i].type   = seq.events[i].type;
                    memcpy_s(v7Events[i].options, 256, seq.events[i].options.c_str(), seq.events[i].options.length());
                }
            }
            pData += sizeof(anim::v7::mstudioevent_t) * v7RseqDesc->numevents;

            // Autolayers
            v7RseqDesc->autolayerindex = static_cast<int32_t>(pData - pBase);
            auto* v7Autolayer = reinterpret_cast<anim::v7::mstudioautolayer_t*>(pData);
            for (int i = 0; i < (int)seq.autolayers.size(); i++) {
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

            // Weight list
            v7RseqDesc->weightlistindex = static_cast<uint32_t>(pData - pBase);
            auto* v7WeightList = reinterpret_cast<float*>(pData);
            for (int i = 0; i < (int)rig.bones.size(); i++) v7WeightList[i] = seq.weightlist[i];
            pData += sizeof(float) * rig.bones.size();

            // IK locks / keyvalues (TODO)
            v7RseqDesc->iklockindex   = static_cast<uint32_t>(pData - pBase);
            v7RseqDesc->keyvalueindex = static_cast<uint32_t>(pData - pBase);

            // Activity modifiers
            v7RseqDesc->activitymodifierindex = static_cast<uint32_t>(pData - pBase);
            auto* v7Actmod = reinterpret_cast<anim::v7::mstudioactivitymodifier_t*>(pData);
            for (int i = 0; i < (int)seq.actmods.size(); i++) {
                stringTables.Add(&v7Actmod[i], &v7Actmod[i].sznameindex, seq.actmods[i].name);
                v7Actmod[i].negate = seq.actmods[i].negate;
            }
            pData += sizeof(anim::v7::mstudioactivitymodifier_t) * v7RseqDesc->numactivitymodifiers;

            // Blends
            std::vector<std::pair<int, int>> blends_index_map;
            v7RseqDesc->animindexindex = static_cast<uint32_t>(pData - pBase);
            auto* v7Blends = reinterpret_cast<int*>(pData);
            pData += sizeof(int) * v7RseqDesc->numblends;

            for (int anim_iter = 0; anim_iter < seq.numuniqueblends; anim_iter++) {
                blends_index_map.push_back({ seq.blends[anim_iter], (int)(pData - pBase) });
                temp::animdesc_t anim = seq.anims[anim_iter];
                anim.SubtractBase(rig.bones.size(), rig, seq.IsAdditive());

                constexpr uint32_t targetsectionframes = 61;

                // Animdesc
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

                // Sections
                uint32_t  numsections = 1;
                uint32_t* animsections{};
                if (anim.numframes > targetsectionframes) {
                    animDesc->sectionindex = static_cast<int32_t>(pData - (char*)animDesc);
                    numsections  = GetSectionCount(*animDesc);
                    animsections = PTR_FROM_IDX(uint32_t, pData, 0);
                    pData += sizeof(uint32_t) * numsections;
                }
                animDesc->animindex = static_cast<int32_t>(pData - (char*)animDesc);

                uint32_t startframe = 0;
                for (uint32_t section = 0; section < numsections; section++) {
                    const uint32_t sectionframes  = GetSectionLength(*animDesc, section, numsections);
                    const bool     bInterpframe    = (section + 1 != numsections);
                    const uint32_t endframe        = startframe + sectionframes + bInterpframe;

                    if (numsections > 1)
                        animsections[section] = static_cast<int32_t>(pData - (char*)animDesc);

                    // Bone-flag array
                    const uint32_t bfa_size = ((rig.bones.size() + 3) / 2) & ~1u;
                    static thread_local std::vector<uint8_t> flaggedBones;
                    flaggedBones.clear();
                    flaggedBones.resize(bfa_size * 2);
                    char* boneflagarray = reinterpret_cast<char*>(pData);
                    pData += bfa_size;

                    for (int bone = 0; bone < (int)rig.bones.size(); bone++) {
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

                        // posscale
                        float posscale = 0.003906369f;
                        Vector3 maxpos{}, minpos{};
                        findMinMaxSIMD(animData.pos, startframe, endframe, minpos, maxpos);
                        const float v1max = std::max({ std::fabs(maxpos.Max()), std::fabs(minpos.Min()) });
                        if (v1max > 127.f) posscale = (v1max * 2.f) / 65534.f;

                        // static pos
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

                        // static rot
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

                        // static scl
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
                        flaggedBones.at(bone) = boneFlags;

                        for (int i = 0; i < (int)(flaggedBones.size() / 2); i++) {
                            boneflagarray[i]  = flaggedBones.at(i * 2);
                            boneflagarray[i] |= flaggedBones.at(i * 2 + 1) << 4;
                        }
                    }
                    ALIGN4(pData);
                    startframe += sectionframes;
                }

                // IK rules
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

                        const int32_t sectioncount = static_cast<int32_t>(
                            (float)(anim.numframes - 1) / (float)ikrule.sectionframes) + 1;
                        auto* sectionindices = reinterpret_cast<int32_t*>(pData);
                        pData += sizeof(int32_t) * sectioncount;

                        // Recompute scale
                        Vector3 mn{}, mx{}, mn1{}, mx1{};
                        findMinMaxSIMD(ikrule.ikruledata.pos, 0, anim.numframes, mn, mx);
                        findMinMaxSIMD(ikrule.ikruledata.rot, 0, anim.numframes, mn1, mx1);
                        for (int j = 0; j < 6; j++) {
                            const float v1 = (j < 3)
                                ? std::max({ std::fabs(mx[j]),   std::fabs(mn[j]) })
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

                // Frame movement
                if ((anim.flags & r5::ANIM_FRAMEMOVEMENT) && anim.movement.sectionframes != 0) {
                    animDesc->framemovementindex = static_cast<int32_t>(pData - (char*)animDesc);
                    auto* frameMovement = reinterpret_cast<anim::v7::mstudioframemovement_t*>(pData);
                    frameMovement->scale        = anim.movement.scale;
                    frameMovement->sectionframes = anim.movement.sectionframes;
                    pData += sizeof(anim::v7::mstudioframemovement_t);

                    auto& movementdata = anim.movement.movementdata;
                    const uint32_t sectioncount = static_cast<uint32_t>(
                        (float)(anim.numframes - 1) / (float)anim.movement.sectionframes) + 1;
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
                            if (allEqualVector(movementdata, startframe, endframe, idx, frameMovement->scale[idx]) &&
                                movementdata[startframe][idx] == 0) continue;

                            offsets[idx] = static_cast<int16_t>(pData - (char*)offsets);
                            WriteAnimData(pData, movementdata, startframe, endframe, idx, frameMovement->scale[idx]);
                        }
                        startframe += sectionframes;
                    }
                }
            } // for anim_iter

			// write Blends
            for (int iter = 0; iter < (int)v7RseqDesc->numblends; iter++)
                v7Blends[iter] = blends_index_map[seq.blends[iter]].second;

            v7RseqDesc->weightFixupOffset = static_cast<uint32_t>(pData - pBase);

            // write to file
            pData = stringTables.Write(pData);
            ALIGN4(pData);
            outRseq.write(pBase, pData - pBase);
            stringTables.Init();
            std::vector<char>().swap(buffer);
        }));
        if (!_enable_verbose) bar.AddAndPrint();
    }
    for (auto& t : tasks) t.get();
    printf("\n");
}
