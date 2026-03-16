#pragma once
#include <core/math_types.h>

namespace p2 {
	struct mstudioanimsections_t {
		int32_t animblock;
		int32_t animindex;
	};
	struct mstudio_rle_anim_t {
		unsigned char bone;
		char flags;
		int16_t nextoffset;
	};

	struct mstudiohitboxset_t {
		int32_t sznameindex;
		int32_t numhitboxes;
		int32_t hitboxindex;
	};

	struct mstudiobbox_t {
		int32_t bone;
		int32_t group;
		Vector3 bbmin;
		Vector3 bbmax;
		int32_t szhitboxnameindex;
		int32_t unused[8];
	};

	struct mstudioposeparamdesc_t {
		int	sznameindex;
		int32_t flags;
		float start;
		float end;
		float loop;
	};

	struct mstudioiklink_t {
		int32_t bone;
		Vector3	kneeDir;
		Vector3	unused0;
	};

	struct mstudiolinearbone_t {
		int32_t numbones;
		int32_t flagsindex;
		int	parentindex;
		int	posindex;
		int32_t quatindex;
		int32_t rotindex;
		int32_t posetoboneindex;
		int	posscaleindex;
		int	rotscaleindex;
		int	qalignmentindex;
		int32_t unused[6];
	};

	struct mstudioikchain_t {
		int32_t sznameindex;
		int32_t linktype;
		int32_t numlinks;
		int32_t linkindex;
	};

	struct mstudioautolayer_t {
		int16_t iSequence;
		int16_t iPose;
		int32_t flags;
		float start;
		float peak;
		float tail;
		float end;
	};

	struct mstudiobone_t {
		int32_t sznameindex;
		int32_t parent;
		int32_t bonecontroller[6];
		Vector3 pos;
		Quaternion quat;
		Vector3 rot;
		Vector3 posscale;
		Vector3 rotscale;
		Matrix3x4_t poseToBone;
		Quaternion qAlignment;
		int32_t flags;
		int32_t proctype;
		int32_t procindex;
		int32_t physicsbone;
		int32_t surfacepropidx;
		int32_t contents;
		int32_t surfacepropLookup;
		int32_t unused[7];
	};

	struct studioanimvalue_ptr_t {
		int16_t x;
		int16_t y;
		int16_t z;

		inline short& operator[](int32_t i) {
			return ((short*)this)[i];
		}

		inline int16_t operator[](int32_t i) const {
			return ((short*)this)[i];
		}
	};

	struct studiohdr_t {
		int32_t id;
		int32_t version;
		long checksum;
		char name[64];
		int32_t length;
		Vector3 eyeposition;
		Vector3 illumposition;
		Vector3 hull_min;
		Vector3 hull_max;
		Vector3 view_bbmin;
		Vector3 view_bbmax;
		int32_t flags;
		int32_t numbones;
		int32_t boneindex;
		int32_t numbonecontrollers;
		int32_t bonecontrollerindex;
		int32_t numhitboxsets;
		int32_t hitboxsetindex;
		int32_t numlocalanim;
		int32_t localanimindex;
		int32_t numlocalseq;
		int32_t localseqindex;
		int32_t activitylistversion;
		int32_t eventsindexed;
		int32_t numtextures;
		int32_t textureindex;
		int32_t numcdtextures;
		int32_t cdtextureindex;
		int32_t numskinref;
		int32_t numskinfamilies;
		int32_t skinindex;
		int32_t numbodyparts;
		int32_t bodypartindex;
		int32_t numlocalattachments;
		int32_t localattachmentindex;
		int32_t numlocalnodes;
		int32_t localnodeindex;
		int32_t localnodenameindex;
		int32_t numflexdesc;
		int32_t flexdescindex;
		int32_t numflexcontrollers;
		int32_t flexcontrollerindex;
		int32_t numflexrules;
		int32_t flexruleindex;
		int32_t numikchains;
		int32_t ikchainindex;
		int32_t nummouths;
		int32_t mouthindex;
		int32_t numlocalposeparameters;
		int32_t localposeparamindex;
		int32_t surfacepropindex;
		int32_t keyvalueindex;
		int32_t keyvaluesize;
		int32_t numlocalikautoplaylocks;
		int32_t localikautoplaylockindex;
		float mass;
		int32_t contents;
		int32_t numincludemodels;
		int32_t includemodelindex;
		int32_t virtualModel;
		int32_t szanimblocknameindex;
		int32_t numanimblocks;
		int32_t animblockindex;
		int32_t animblockModel;
		int32_t bonetablebynameindex;
		int32_t pVertexBase;
		int32_t pIndexBase;
		char constdirectionallightdot;
		char rootLOD;
		char numAllowedRootLODs;
		char unused[1];
		int32_t unused4;
		int32_t numflexcontrollerui;
		int32_t flexcontrolleruiindex;
		float flVertAnimFixedPointScale;
		mutable int32_t surfacepropLookup;
		int32_t studiohdr2index;
		int32_t unused2[1];

		int32_t numsrcbonetransform;
		int32_t srcbonetransformindex;
		int	illumpositionattachmentindex;
		float flMaxEyeDeflection;
		int32_t linearboneindex;
		int32_t sznameindex;
		int32_t m_nBoneFlexDriverCount;
		int32_t m_nBoneFlexDriverIndex;
		int32_t reserved[56];
	};


	struct mstudioseqdesc_t {
		int32_t baseptr;
		int	sznameindex;
		int32_t szactivitynameindex;
		int32_t flags;
		int32_t activity;
		int32_t actweight;
		int32_t numevents;
		int32_t eventindex;
		Vector3 bbmin;
		Vector3 bbmax;
		int32_t numblends;
		int32_t animindexindex;
		int32_t movementindex;
		int32_t groupsize[2];
		int32_t paramindex[2];
		float paramstart[2];
		float paramend[2];
		int32_t paramparent;
		float fadeintime;
		float fadeouttime;
		int32_t localentrynode;
		int32_t localexitnode;
		int32_t nodeflags;
		float entryphase;
		float exitphase;
		float lastframe;
		int32_t nextseq;
		int32_t pose;
		int32_t numikrules;
		int32_t numautolayers;
		int32_t autolayerindex;
		int32_t weightlistindex;
		int32_t posekeyindex;
		int32_t numiklocks;
		int32_t iklockindex;
		int	keyvalueindex;
		int32_t keyvaluesize;
		int32_t cycleposeindex;
		int32_t activitymodifierindex;
		int32_t numactivitymodifiers;
		int32_t unused[5];
	};

	struct mstudioevent_t {
		float cycle;
		int32_t event;
		int32_t type;
		char options[64];
		int32_t szeventindex;

	};

	struct mstudioanimdesc_t {
		int32_t baseptr;
		int32_t sznameindex;
		float fps;
		int32_t flags;
		int32_t numframes;
		int	nummovements;
		int32_t movementindex;
		int32_t ikrulezeroframeindex;
		int32_t unused1[5];
		int32_t animblock;
		int32_t animindex;
		int32_t numikrules;
		int32_t ikruleindex;
		int32_t animblockikruleindex;
		int32_t numlocalhierarchy;
		int32_t localhierarchyindex;
		int32_t sectionindex;
		int32_t sectionframes;
		int16_t zeroframespan;
		int16_t zeroframecount;
		int32_t zeroframeindex;
		float zeroframestalltime;
	};

	struct mstudioikrule_t {
		int32_t index;
		int32_t type;
		int32_t chain;
		int	bone;
		int32_t slot;
		float height;
		float radius;
		float floor;
		Vector3 pos;
		Quaternion q;
		int32_t compressedikerrorindex;
		int32_t unused2;
		int32_t iStart;
		int32_t ikerrorindex;
		float start;
		float peak;
		float tail;
		float end;
		float unused3;
		float contact;
		float drop;
		float top;
		int32_t unused6;
		int32_t unused7;
		int32_t unused8;
		int32_t szattachmentindex;
		int32_t unused[7];
	};
}
