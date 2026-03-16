#pragma once
#include <core/math_types.h>

namespace r2 {
	struct studiohdr_t {
		int32_t magic;
		int32_t version;
		int32_t checksum;
		int32_t sznameindex;
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
		int	localseqindex;
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
		int32_t localNodeUnk;
		int32_t deprecated_flexdescindex;
		int32_t deprecated_numflexcontrollers;
		int32_t deprecated_flexcontrollerindex;
		int32_t deprecated_numflexrules;
		int32_t deprecated_flexruleindex;
		int32_t numikchains;
		int32_t ikchainindex;
		int32_t uiPanelCount;
		int32_t uiPanelOffset;
		int32_t numlocalposeparameters;
		int32_t localposeparamindex;
		int32_t surfacepropindex;
		int32_t keyvalueindex;
		int32_t keyvaluesize;
		int32_t numlocalikautoplaylocks;
		int32_t localikautoplaylockindex;
		float mass;
		int32_t contents;
		int32_t numincludemodels;//
		int32_t includemodelindex;
		int32_t virtualModel;
		int32_t bonetablebynameindex;
		char constdirectionallightdot;
		char rootLOD;
		char numAllowedRootLODs;
		char unused;
		float fadeDistance;
		int32_t deprecated_numflexcontrollerui;
		int32_t deprecated_flexcontrolleruiindex;
		float flVertAnimFixedPointScale;
		int32_t surfacepropLookup;
		int32_t sourceFilenameOffset;
		int32_t numsrcbonetransform;
		int32_t srcbonetransformindex;
		int	illumpositionattachmentindex;
		int32_t linearboneindex;
		int32_t m_nBoneFlexDriverCount;
		int32_t m_nBoneFlexDriverIndex;
		int32_t m_nPerTriAABBIndex;
		int32_t m_nPerTriAABBNodeCount;
		int32_t m_nPerTriAABBLeafCount;
		int32_t m_nPerTriAABBVertCount;
		int32_t unkStringOffset;
		int32_t vtxOffset;
		int32_t vvdOffset;
		int32_t vvcOffset;
		int32_t phyOffset;
		int32_t vtxSize;
		int32_t vvdSize;
		int32_t vvcSize;
		int32_t phySize;
		int32_t collisionOffset;
		int32_t staticCollisionCount;
		int32_t boneFollowerCount;
		int32_t boneFollowerOffset;
		int32_t unused1[60];

	};

	struct mstudiobone_t {
		int32_t sznameindex;
		int32_t parent;
		int32_t bonecontroller[6];
		Vector3 pos;
		Quaternion quat;
		Vector3 rot;
		Vector3 scale;
		Vector3 posscale;
		Vector3 rotscale;
		Vector3 scalescale;
		Matrix3x4_t poseToBone;
		Quaternion qAlignment;
		int32_t flags;
		int32_t proctype;
		int32_t procindex;
		int32_t physicsbone;
		int32_t surfacepropidx;
		int32_t contents;
		int32_t surfacepropLookup;
		uint16_t collisionIndex;
		uint16_t collisionCount;
		int32_t unused[7];
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
		int32_t critShotOverride;
		int32_t hitdataGroupOffset;
		int32_t unused[6];
	};

	struct mstudioiklink_t {
		int		bone;
		Vector3	kneeDir;
		Vector3	unused0;
	};

	struct mstudioikchain_t {
		int	sznameindex;
		int	linktype;
		int	numlinks;
		int	linkindex;
		float unk;
		int	unused[3];
	};
	struct mstudioposeparamdesc_t {
		int	sznameindex;
		int32_t flags;
		float start;
		float end;
		float loop;
	};

	struct mstudioactivitymodifier_t {
		int32_t sznameindex;
		bool negate;
	};

	struct mstudioseqdesc_t {
		int32_t baseptr;
		int	szlabelindex;
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
		int32_t keyvalueindex;
		int32_t keyvaluesize;
		int32_t cycleposeindex;
		int32_t activitymodifierindex;
		int32_t numactivitymodifiers;
		int32_t ikResetMask;
		int32_t unk_C4;
		int32_t unused[8];
	};

	struct mstudioanimdesc_t {
		int32_t baseptr;
		int32_t sznameindex;
		float fps;
		int32_t flags;
		int32_t numframes;
		int32_t nummovements;
		int32_t movementindex;
		int32_t framemovementindex;
		int32_t animindex;
		int32_t numikrules;
		int32_t ikruleindex;
		int32_t numlocalhierarchy;
		int32_t localhierarchyindex;
		int32_t sectionindex;
		int32_t sectionframes;
		int32_t unused[8];
	};

	struct mstudio_rle_anim_t {
		float posscale;
		uint8_t bone;
		uint8_t flags;
		char pad[2];
		Quaternion64 rot;
		Vector48 pos;
		Vector48 scale;
		int32_t nextoffset;
	};

	struct mstudioikrule_t {
		int32_t index;
		int32_t type;
		int32_t chain;
		int32_t bone;
		int32_t slot;
		float height;
		float radius;
		float floor;
		Vector3 pos;
		Quaternion q;
		int32_t compressedikerrorindex;
		int32_t iStart;
		int32_t ikerrorindex;
		float start;
		float peak;
		float tail;
		float end;
		float contact;
		float drop;
		float top;
		int32_t szattachmentindex;
		float endHeight;
		int32_t unused[8];
	};

	struct animvalue_ptr_t {
		int16_t x;
		int16_t y;
		int16_t z;
		int16_t pad;
	};

	struct mstudiocompressedikerror_t {
		Vector3 posscale;
		Vector3 rotscale;
		int16_t offset[6];
	};

	struct mstudioframemovement_t {
		Vector4 scale;
		int16_t offset[4];
	};
}
