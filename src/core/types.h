#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <core/math_types.h>

namespace temp {
	struct stringentry_t {
		char* base;
		char* addr;
		int* ptr;
		std::string string;
		int dupindex;
	};

	class StringTable {
	public:
		std::vector<stringentry_t> stringTable;

		void Init() {
			stringTable.clear();
			stringTable.emplace_back(stringentry_t{ NULL, NULL, NULL, "", -1 });
		}

		template<typename T>
		void Add(T* base, int* ptr, std::string str) {
			if (str.empty()) str = "";
			stringentry_t newString{};

			int i = 0;
			for (auto& it : stringTable) {
				if ((str == it.string)) {
					newString.base = (char*)base;
					newString.ptr = ptr;
					newString.string = str;
					newString.dupindex = i;
					stringTable.emplace_back(newString);
					return;
				}
				i++;
			}

			newString.base = (char*)base;
			newString.ptr = ptr;
			newString.string = str;
			newString.dupindex = -1;

			stringTable.emplace_back(newString);
		}

		template<typename T>
		void Add(T* base, int* ptr, const char* str) {
			if (!str) str = "";
			stringentry_t newString{};

			int i = 0;
			for (auto& it : stringTable) {
				if (!strcmp(str, it.string.c_str())) {
					newString.base = (char*)base;
					newString.ptr = ptr;
					newString.string = str;
					newString.dupindex = i;
					stringTable.emplace_back(newString);
					return;
				}
				i++;
			}

			newString.base = (char*)base;
			newString.ptr = ptr;
			newString.string = str;
			newString.dupindex = -1;

			stringTable.emplace_back(newString);
		}

		char* Write(char* pData) {
			for (auto& it : stringTable) {
				if (it.dupindex == -1) {
					it.addr = pData;
					if (it.ptr) {
						*it.ptr = int(pData - it.base);
						size_t length = it.string.length();
						strcpy_s(pData, length + 1, it.string.c_str());

						pData += length;
					}
					*pData = '\0';
					pData++;
				}
				else {
					*it.ptr = int(stringTable[it.dupindex].addr - it.base);
				}
			}
			return pData;
		}
	};

	struct animdata_t {
		std::vector<Vector3> pos;
		std::vector<Vector3> rot;
		std::vector<Vector3> scl;
		void resize(size_t size) {
			pos.resize(size);
			rot.resize(size);
			scl.resize(size);
		}
	};

	struct ikrule_t {
		int index = 0;
		int type = 4;
		int chain = 0;
		int bone = 0;
		int slot = 0;
		float height = 50.f;
		float radius = 15.f;
		float floor = 0.f;
		Vector3 pos{};
		Quaternion q{};
		float scale[6] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
		uint32_t sectionframes = 0;
		int iStart = 0;
		float start = 0.f;
		float peak = 0.f;
		float tail = 1.f;
		float end = 1.f;
		float contact = 0.f;
		float drop = 0.f;
		float top = 0.f;
		std::string attachmentname = "";
		float endHeight = -100.f;
		temp::animdata_t ikruledata{};
	};

	struct framemovement_t {
		Vector4 scale{};
		int sectionframes = 0;
		std::vector<Vector4> movementdata{};
	};

	struct rig_t;
	struct animdesc_t {
		std::string name = "";
		float fps = 0;
		int32_t flags = 0;
		int32_t numframes = 0;
		std::vector <animdata_t> animdata;
		std::vector<temp::ikrule_t> ikrules{};
		framemovement_t movement{};
		int32_t sectionstallframes = 0;
		int32_t sectionframes = 0;

		void InitData(const temp::rig_t& rig, bool badditive);
		void SubtractBase(int32_t numbones, const temp::rig_t& rig, bool badditive);
		bool IsAdditive() const { return flags & 0x4; }
	};

	struct seqevent_t {
		std::string name;
		float cycle;
		int32_t event;
		int32_t type;
		std::string options;

	};

	struct autolayer_t {
		uint64_t guidSequence;
		int16_t iSequence;
		int16_t iPose;
		int flags;
		float start;
		float peak;
		float tail;
		float end;
	};

	struct actmod_t {
		std::string name;
		bool negate;
	};

	class Sequence {
	public:
		std::string path;
		std::string name;
		int32_t numbones = 0;
		uint32_t flags = 0u;

		std::string activityname;
		int32_t activity = 0u;
		int32_t actweight = 0u;

		std::vector<float> posekeys;
		std::vector<temp::seqevent_t> events;
		std::vector<temp::autolayer_t> autolayers;
		std::vector<temp::actmod_t> actmods;
		Vector3 bbmin{};
		Vector3 bbmax{};
		int32_t groupsize[2] = { 0, 0 };
		int32_t paramindex[2] = { 0, 0 };
		float paramstart[2] = { 0, 0 };
		float paramend[2] = { 0, 0 };
		int32_t paramparent = 0;
		float fadeintime = 0.f;
		float fadeouttime = 0.f;
		int32_t localentrynode = 0;
		int32_t localexitnode = 0;
		int32_t nodeflags = 0;
		float entryphase = 0.f;
		float exitphase = 0.f;
		float lastframe = 0.f;
		int32_t nextseq = 0;
		int32_t pose = 0;
		// iklocks
		// keyvalue
		int32_t ikResetMask = 0;
		int32_t unk1 = 0;
		// weightfixup

		std::vector<float> weightlist;
		int32_t numuniqueblends = 0u;
		std::vector<uint32_t> blends;
		std::vector<temp::animdesc_t> anims;

		Sequence(const std::string name, const int32_t numbones) : name(name), numbones(numbones) {
			weightlist.resize(numbones);
		}

		inline bool IsAdditive() const { return flags & 0x4; }
	};

	struct rigdesc_t {
		Vector3 eyeposition{};
		Vector3 illumposition{};
		Vector3 hull_min{};
		Vector3 hull_max{};
		Vector3 view_bbmin{};
		Vector3 view_bbmax{};
		int flags;
		int numbones;
		int activitylistversion;
		float mass;
		int contents;
		float defaultFadeDist;
		float gathersize;
		std::string surfaceprop;
		Vector3 mins{};
		Vector3 maxs{};
	};

	struct bone_t {
		std::string name;
		int32_t parent = -1;
		uint32_t flags = 0u;
		int32_t bonecontroller[6] = { -1, -1, -1, -1, -1, -1 };
		int32_t proctype = 0;
		int32_t procindex = 0;
		int32_t physicsbone = 0;
		std::string surfaceprop;
		int32_t contents = 0;
		int32_t surfacepropLookup = 0;

		Vector3 pos{};
		Quaternion q{};
		Vector3 rot{};
		Vector3 scl{};
		Matrix3x4_t poseToBone{};
		Quaternion qAlignment{};
	};

	struct bbox_t {
		std::string name{};
		int bone;
		int group;
		Vector3 bbmin{};
		Vector3 bbmax{};
		int critShotOverride;
		std::string hitdataGroupOffset;
	};

	struct hitboxsets_t {
		std::string name;
		std::vector<temp::bbox_t> hitboxes;
	};

	struct nodedata_t {
		uint16_t tonode = 0;
		uint16_t seq = 0;
	};

	struct node_t {
		std::string name;
		std::vector<temp::nodedata_t> nodedatas;
	};

	struct poseparam_t {
		std::string name;
		int flags;
		float start;
		float end;
		float loop;
	};

	struct iklink_t {
		int bone;
		Vector3	kneeDir;
	};

	struct ikchain_t {
		std::string name;
		int linktype;
		std::vector<temp::iklink_t> iklinks;
		float unk;
	};


	struct rig_t {
		std::string rrigpath;
		std::string rsonpath;
		std::vector<std::string> rseqpaths;
		std::vector<std::string> rigpaths;
		//std::vector<std::string> materialpaths;

		temp::rigdesc_t hdr{};
		std::string name = "CANNOT LOAD RRIG NAME";
		std::vector<temp::bone_t> bones;
		std::vector<temp::hitboxsets_t> hitboxsets;
		std::vector<uint8_t> bonebyname;
		uint16_t ignorenode = 0;
		std::vector<temp::node_t> nodes;
		std::vector<temp::poseparam_t> poseparams;
		std::vector<temp::ikchain_t> ikchains;
		std::vector<temp::Sequence> sequences;
	};

	struct animblock_t {
		uint8_t comptype;
		uint32_t startframe;
		uint32_t endframe;
	};

}

inline void temp::animdesc_t::InitData(const temp::rig_t& rig, bool badditive) {
	const Vector3 zero(0, 0, 0);
	const Vector3 one(1, 1, 1);
	const uint16_t numbones = rig.bones.size();
	animdata.resize(numbones);

	for (int i = 0; i < numbones; ++i) {
		auto& ad = animdata[i];
		ad.resize(numframes);

		if (badditive) {
			std::fill_n(ad.pos.begin(), numframes, zero);
			std::fill_n(ad.rot.begin(), numframes, zero);
			std::fill_n(ad.scl.begin(), numframes, one);
		}
		else {
			std::fill_n(ad.pos.begin(), numframes, rig.bones[i].pos);
			std::fill_n(ad.rot.begin(), numframes, rig.bones[i].rot);
			std::fill_n(ad.scl.begin(), numframes, rig.bones[i].scl);
		}
	}
}

inline void temp::animdesc_t::SubtractBase(int32_t numbones, const temp::rig_t& rig, bool badditive) {
	for (int i = 0; i < numbones; ++i) {
		auto& ad = animdata[i];

		const Vector3& basePos = rig.bones[i].pos;
		const Vector3& baseRot = rig.bones[i].rot;
		const Vector3& baseScl = rig.bones[i].scl;
		if (!badditive)
			for (auto& v : ad.pos) v -= basePos;
		for (auto& v : ad.scl) v -= baseScl;
	}
}
