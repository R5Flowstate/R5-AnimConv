#include <pch.h>

#include <rrig/rrig.h>
#include <rseq/rseq.h>
#include <mdl/mdl.h>
#include <utils/rson_parser.h>
#include <utils/misc.h>
#include <core/cli.h>

std::string g_in_season = "28";
std::string g_out_season = "3";
std::string g_Outpath;
std::string g_DumpTracks;
std::string g_TargetRigDir;
std::string g_DumpRig;
bool g_SnapConstPos = false;

// -collapsepose <param>=<value>[@<sequence substring>]: a blend dimension driven
// by a pose parameter the target engine never sets is fixed at the authored
// key nearest <value>; later entries with a sequence filter take precedence.
struct CollapsePose_t {
	std::string param;
	float value = 0.f;
	std::string seqFilter;
};
std::vector<CollapsePose_t> g_CollapsePose;

// One JSON per rig: bone name, parent, base pos/rot/scale in rig order.
static void DumpRig(const temp::rig_t& rig, const std::string& outdir) {
	std::filesystem::create_directories(outdir);
	std::string name = rig.name;
	for (char& c : name) if (c == '/' || c == '\\' || c == ':') c = '_';
	std::ofstream f(outdir + "/" + name + ".json", std::ios::out | std::ios::binary);
	f << "{\"rig\":\"" << rig.name << "\",\"bones\":[";
	for (size_t i = 0; i < rig.bones.size(); i++) {
		const auto& b = rig.bones[i];
		if (i) f << ",";
		f << "{\"name\":\"" << b.name << "\",\"parent\":" << b.parent
		  << ",\"pos\":[" << b.pos.x << "," << b.pos.y << "," << b.pos.z << "]"
		  << ",\"rot\":[" << b.rot.x << "," << b.rot.y << "," << b.rot.z << "]"
		  << ",\"scl\":[" << b.scl.x << "," << b.scl.y << "," << b.scl.z << "]}";
	}
	f << "]}";
}

struct TargetRig_t {
	temp::rig_t rig;
	std::vector<int> map;   // source bone index -> target bone index, -1 when absent
	bool loaded = false;
	bool identity = true;
};

static bool LoadTargetRig(const temp::rig_t& rig, const std::string& in_dir, TargetRig_t& out) {
	const std::filesystem::path rel = std::filesystem::relative(rig.rrigpath, in_dir);
	const std::filesystem::path targetPath = std::filesystem::path(g_TargetRigDir) / rel;
	if (!std::filesystem::is_regular_file(targetPath)) {
		print("[!] Warning: no target rig for %s, keeping source bone order\n", rig.name.c_str());
		return false;
	}

	const std::string targetSeason = g_out_season == "3" ? "21" : g_out_season;
	auto targetParser = Parsers.find(targetSeason);
	if (targetParser == Parsers.end()) {
		print("[!] Error: no rig parser for target season %s\n", targetSeason.c_str());
		return false;
	}

	std::vector<char> buffer(std::filesystem::file_size(targetPath));
	std::ifstream stream(targetPath, std::ios::binary);
	stream.read(buffer.data(), buffer.size());
	stream.close();

	out.rig.rrigpath = targetPath.string();
	targetParser->second.rrig(buffer.data(), out.rig);

	std::unordered_map<std::string, int> targetIndex;
	for (int i = 0; i < (int)out.rig.bones.size(); i++) targetIndex[out.rig.bones[i].name] = i;

	const int srcCount = (int)rig.bones.size();
	out.map.assign(srcCount, -1);
	int dropped = 0;
	for (int i = 0; i < srcCount; i++) {
		auto it = targetIndex.find(rig.bones[i].name);
		if (it == targetIndex.end()) { dropped++; continue; }
		out.map[i] = it->second;
	}
	out.identity = srcCount == (int)out.rig.bones.size() && dropped == 0;
	for (int i = 0; out.identity && i < srcCount; i++) out.identity = out.map[i] == i;
	out.loaded = true;
	print("[+] %s: target rig %d bones (%d dropped, %d target-only, order %s)\n", rig.name.c_str(),
		(int)out.rig.bones.size(), dropped, (int)out.rig.bones.size() - (srcCount - dropped), out.identity ? "same" : "differs");
	return true;
}

// Re-index every bone-indexed table of the parsed rig onto the target rig's bone
// order. Later builds reorder the same skeleton, so a clip converted in source
// order would drive the wrong bones on the target rig. Decoded values are absolute
// (the RLE decoder accumulates onto the source base), so the writer must subtract
// the target base: the rig always takes the target bone table, even when the
// order already matches.
static void RemapRigToTarget(temp::rig_t& rig, const TargetRig_t& target) {
	// Source payloads encode against the source base pose, so every clip is
	// re-encoded against the target base even when the bone order already matches.
	for (auto& seq : rig.sequences)
		for (auto& anim : seq.anims)
			anim.reencode = true;

	if (target.identity) {
		rig.bones      = target.rig.bones;
		rig.bonebyname = target.rig.bonebyname;
		rig.hitboxsets = target.rig.hitboxsets;
		return;
	}
	const int srcCount = (int)rig.bones.size();
	const int dstCount = (int)target.rig.bones.size();
	const std::vector<int>& map = target.map;

	for (auto& seq : rig.sequences) {
		std::vector<float> weights(dstCount, 1.f);
		for (int i = 0; i < srcCount && i < (int)seq.weightlist.size(); i++)
			if (map[i] >= 0) weights[map[i]] = seq.weightlist[i];
		seq.weightlist = std::move(weights);
		seq.numbones = dstCount;

		for (auto& anim : seq.anims) {
			std::vector<temp::animdata_t> data(dstCount);
			const Vector3 zero(0, 0, 0), one(1, 1, 1);
			for (int i = 0; i < dstCount; i++) {
				auto& ad = data[i];
				ad.resize(anim.numframes);
				if (seq.IsAdditive()) {
					std::fill_n(ad.pos.begin(), anim.numframes, zero);
					std::fill_n(ad.rot.begin(), anim.numframes, zero);
					std::fill_n(ad.scl.begin(), anim.numframes, one);
				}
				else {
					std::fill_n(ad.pos.begin(), anim.numframes, target.rig.bones[i].pos);
					std::fill_n(ad.rot.begin(), anim.numframes, target.rig.bones[i].rot);
					std::fill_n(ad.scl.begin(), anim.numframes, target.rig.bones[i].scl);
				}
			}
			for (int i = 0; i < srcCount && i < (int)anim.animdata.size(); i++)
				if (map[i] >= 0) data[map[i]] = std::move(anim.animdata[i]);
			anim.animdata = std::move(data);

			for (auto& ik : anim.ikrules)
				if (ik.bone >= 0 && ik.bone < srcCount && map[ik.bone] >= 0) ik.bone = map[ik.bone];
			anim.reencode = true;
		}
	}

	for (auto& chain : rig.ikchains)
		for (auto& link : chain.iklinks)
			if (link.bone >= 0 && link.bone < srcCount && map[link.bone] >= 0) link.bone = map[link.bone];

	rig.bones      = target.rig.bones;
	rig.bonebyname = target.rig.bonebyname;
	rig.hitboxsets = target.rig.hitboxsets;
	print("[+] %s: sequences re-indexed onto the target rig order\n", rig.name.c_str());
}

// A clip authored on another class and re-exported under this rig carries that
// class's bone lengths as constant position tracks. Skeleton bones whose position
// never moves in the clip take the target rig's own proportions; attachment and
// camera helpers keep their authored offsets.
static bool IsProportionBone(const std::string& name) {
	return name.rfind("def_", 0) == 0 || name == "jx_c_pov" || name == "jx_c_start" || name.rfind("jx_c_neck", 0) == 0;
}

static void SnapConstantPositions(temp::rig_t& rig) {
	int snapped = 0;
	for (auto& seq : rig.sequences) {
		if (seq.IsAdditive()) continue;
		for (auto& anim : seq.anims) {
			for (int b = 0; b < (int)anim.animdata.size() && b < (int)rig.bones.size(); b++) {
				if (!IsProportionBone(rig.bones[b].name)) continue;
				auto& pos = anim.animdata[b].pos;
				if (pos.empty()) continue;
				// Root motion rides the framemovement block; a translating start bone would
				// move the whole skeleton a second time.
				const bool rootMotionBone = rig.bones[b].name == "jx_c_start";
				bool constant = true;
				for (size_t f = 1; f < pos.size() && constant; f++) constant = pos[f].approx_equal(pos[0]);
				if (!rootMotionBone && (!constant || pos[0].approx_equal(rig.bones[b].pos))) continue;
				if (rootMotionBone && constant && pos[0].approx_equal(rig.bones[b].pos)) continue;
				std::fill(pos.begin(), pos.end(), rig.bones[b].pos);
				anim.reencode = true;
				snapped++;
			}
		}
	}
	if (snapped) print("[+] %s: %d constant skeleton position tracks snapped to the rig base\n", rig.name.c_str(), snapped);
}

static bool ParseCollapsePose(const std::string& spec) {
	const size_t eq = spec.find('=');
	if (eq == std::string::npos || eq == 0) return false;
	CollapsePose_t cp;
	cp.param = spec.substr(0, eq);
	std::string rest = spec.substr(eq + 1);
	const size_t at = rest.find('@');
	if (at != std::string::npos) {
		cp.seqFilter = rest.substr(at + 1);
		rest = rest.substr(0, at);
	}
	if (rest.empty()) return false;
	cp.value = static_cast<float>(atof(rest.c_str()));
	g_CollapsePose.push_back(cp);
	return true;
}

static const CollapsePose_t* FindCollapsePose(const std::string& param, const std::string& seqname) {
	const CollapsePose_t* found = nullptr;
	for (const auto& cp : g_CollapsePose) {
		if (cp.param != param) continue;
		if (!cp.seqFilter.empty()) {
			if (seqname.find(cp.seqFilter) == std::string::npos) continue;
			return &cp;
		}
		if (!found) found = &cp;
	}
	return found;
}

static void CollapsePoseParams(temp::rig_t& rig) {
	if (g_CollapsePose.empty()) return;
	int collapsed = 0;
	for (auto& seq : rig.sequences) {
		for (int d = 0; d < 2; d++) {
			const int pi = seq.paramindex[d];
			if (pi < 0 || pi >= (int)rig.poseparams.size() || seq.groupsize[d] <= 1) continue;
			const CollapsePose_t* cp = FindCollapsePose(rig.poseparams[pi].name, seq.name);
			if (!cp) continue;

			const int gs0 = seq.groupsize[0], gs1 = seq.groupsize[1];
			const int n = seq.groupsize[d];
			std::vector<float> keys(n);
			for (int k = 0; k < n; k++) {
				if (!seq.posekeys.empty()) keys[k] = seq.posekeys[(d == 0 ? 0 : gs0) + k];
				else keys[k] = seq.paramstart[d] + (seq.paramend[d] - seq.paramstart[d]) * (n > 1 ? (float)k / (n - 1) : 0.f);
			}
			int pick = 0;
			for (int k = 1; k < n; k++)
				if (fabsf(keys[k] - cp->value) < fabsf(keys[pick] - cp->value)) pick = k;

			std::vector<uint32_t> blends;
			for (int y = 0; y < gs1; y++)
				for (int x = 0; x < gs0; x++)
					if ((d == 0 ? x : y) == pick) blends.push_back(seq.blends[y * gs0 + x]);

			// Pose keys stay groupsize[0] + groupsize[1] floats; the collapsed dimension keeps one.
			std::vector<float> posekeys;
			if (!seq.posekeys.empty()) {
				if (d == 0) posekeys.push_back(keys[pick]);
				else for (int k = 0; k < gs0; k++) posekeys.push_back(seq.posekeys[k]);
				if (d == 1) posekeys.push_back(keys[pick]);
				else for (int k = 0; k < gs1; k++) posekeys.push_back(seq.posekeys[gs0 + k]);
			}

			seq.blends = std::move(blends);
			seq.groupsize[d] = 1;
			seq.paramindex[d] = -1;
			seq.paramstart[d] = 0.f;
			seq.paramend[d] = 1.f;
			seq.posekeys = (seq.groupsize[0] > 1 || seq.groupsize[1] > 1) ? std::move(posekeys) : std::vector<float>{};

			// Drop unique anims no surviving blend slot references and renumber.
			std::vector<int> remap(seq.anims.size(), -1);
			std::vector<temp::animdesc_t> anims;
			for (auto& b : seq.blends) {
				if (remap[b] < 0) {
					remap[b] = (int)anims.size();
					anims.push_back(std::move(seq.anims[b]));
				}
				b = remap[b];
			}
			seq.anims = std::move(anims);
			seq.numuniqueblends = (int)seq.anims.size();

			print("[+] %s: %s fixed at %g (key %g), now %dx%d\n", seq.name.c_str(), cp->param.c_str(), cp->value, keys[pick], seq.groupsize[0], seq.groupsize[1]);
			collapsed++;
		}
	}
	if (collapsed) print("[+] %s: %d blend dimensions collapsed\n", rig.name.c_str(), collapsed);
}

static int RunRseqMode(const std::string& input_path) {
	auto parser = Parsers.find(g_in_season);
	auto writer = Writers.find(g_out_season);
	if (parser == Parsers.end() || writer == Writers.end()) {
		printf("[!] Error: Unsupported assets version.\n");
		return 1;
	}

	std::vector<temp::rig_t> rigs;
	std::string in_dir = std::filesystem::is_regular_file(input_path) ? std::filesystem::path(input_path).parent_path().string() : input_path;
	std::filesystem::directory_entry entry = std::filesystem::directory_entry(in_dir);

	/* GATHER PATHS */
	GatherRigPaths(in_dir, entry, rigs);
	for (auto& rig : rigs) {
		std::filesystem::path rigpath = rig.rrigpath;
		std::filesystem::path rsonpath = rigpath.parent_path().string() + "\\" + rigpath.stem().string() + ".rson";
		if (std::filesystem::is_regular_file(rsonpath)) {
			auto data = parse_rson(rsonpath.string());
			rig.rsonpath = rsonpath.string();
			rig.rseqpaths = data["seqs"];
			rig.rigpaths = data["rigs"];
			//rig.materialpaths = data["matl"];
		}
	}

	if (rigs.empty()) {
		printf("[!] Error: No rrig files found in the specified directory.\n");
		return 1;
	}

	for (auto& rig : rigs) {
		/* PARSE */ {
			/* PARSE RRIG */ {
				uint32_t rigFileSize = (uint32_t)std::filesystem::file_size(rig.rrigpath);
				std::ifstream rrig_stream(rig.rrigpath, std::ios::binary);
				rrig_stream.seekg(0, std::ios::beg);
				std::vector<char> buffer(rigFileSize);
				rrig_stream.read(buffer.data(), rigFileSize);
				rrig_stream.close();

				parser->second.rrig(buffer.data(), rig);
				std::replace(rig.name.begin(), rig.name.end(), '\\', '/');

				print("\Converting %s...\n", rig.name.c_str());
			}
			rig.sequences.reserve(rig.rseqpaths.size());

		TargetRig_t target;
		if (!g_TargetRigDir.empty()) LoadTargetRig(rig, in_dir, target);

		/* PARSE RSEQ */ {
			parser->second.rseq(in_dir, rig);
		}

		if (target.loaded) RemapRigToTarget(rig, target);
		if (target.loaded && g_SnapConstPos) SnapConstantPositions(rig);
		CollapsePoseParams(rig);

		if (!g_DumpRig.empty()) DumpRig(rig, g_DumpRig);
		if (!g_DumpTracks.empty()) {
			DumpTracks(rig, g_DumpTracks);
			continue;
		}
	}

		/* WRITE */ {
			/* WRITE RRIG */ {
				if (std::filesystem::path(rig.name).extension() != ".rmdl") writer->second.rrig(g_Outpath.empty() ? in_dir + "/conv" : g_Outpath, rig);
				if (rig.rseqpaths.empty()) continue;
			}

			/* WRITE RSEQ */ {
				writer->second.rseq(rig);
			}
		}
		rig.sequences.clear();
	}
	print("\n");

	/* PRINT REPAK ENTRIES */
	if (!g_NoEntries) print("\n\nRePak Entries:\n");
	for (auto& rig : rigs)  PrintRepakEntries(rig);
	verbose("[+] Succeeded!\n");

#ifdef _DEBUG
	print("Animation Data Compressed Types:\n");
	for (int i = 0; i < 8; i++) {
		print("\t%d: %d\n", i, comptypes[i]);
	}
#endif // _DEBUG

	if (!g_NoPause) system("pause");
	return 0;
}

static int RunMdlMode(const std::string& input_mdl, const std::string& override_rrig_path, const std::string& override_rseq_path) {
	auto writer = Writers.find(g_out_season);
	if (writer == Writers.end()) {
		printf("[!] Error: Unsupported assets version.\n");
		return 1;
	}

	std::ifstream mdl_stream(input_mdl, std::ios::binary);
	std::filesystem::path file_path = std::filesystem::absolute(input_mdl);
	std::string output_dir = g_Outpath.empty() ? file_path.parent_path().string() : g_Outpath;
	verbose("Reading: %s...\n", input_mdl.c_str());

	if (!std::filesystem::exists(input_mdl)) {
		printf("[!] Error: Input file does not exist.\n");
		return 1;
	}

	int magic = 0;
	mdl_stream.read(reinterpret_cast<char*>(&magic), sizeof(int));
	if (magic != 'TSDI') {
		printf("[!] Error: Input file is not a MDL file.\n");
		return 1;
	}

	int mdl_version = 0;
	mdl_stream.read(reinterpret_cast<char*>(&mdl_version), sizeof(int));

	uint32_t mdlFileSize = (uint32_t)std::filesystem::file_size(input_mdl);
	std::vector<char> buffer(mdlFileSize, 0);
	mdl_stream.seekg(0, std::ios::beg);
	mdl_stream.read(buffer.data(), mdlFileSize);
	mdl_stream.close();

	/* PARSE MDL */
	print("Parsing %s\n", file_path.filename().string().c_str());
	temp::rig_t rig;
	switch (mdl_version) {
	case 49:
		ParseMDL_v49(buffer.data(), rig, output_dir, override_rrig_path, override_rseq_path);
		break;
	case 53:
		ParseMDL_v53(buffer.data(), rig, output_dir, override_rrig_path, override_rseq_path);
		break;
	default:
		printf("Failed: This MDL v%d does not support yet, Only v49 and v53 are supported.\n", mdl_version);
		return 1;
	}

	/* WRITE RRIG/RSEQ */
	print("\n\nWriting %s\n", rig.name.c_str());
	writer->second.rrig(output_dir, rig);
	print("Writing sequences\n");
	writer->second.rseq(rig);

	/* PRINT REPAK ENTRIES */
	if (!g_NoEntries) print("\n\nRePak Entries:\n");
	PrintRepakEntries(rig);
	verbose("[+] Succeeded!\n");
	if (!g_NoPause) system("pause");
	return 0;
}

int main(int argc, char* argv[]) {
	std::string input_mdl;
	std::string override_rseq_path;
	std::string override_rrig_path;

	std::string usage = "Usage: \n" \
		"  Mdl  mode : R5-AnimConv.exe <model.mdl> [-o <out season>] [-outpath <path>] [-rp <override_rrig_path>] [-sp <override_rseq_path>] [-verbose <level>] [-ne] [-comperr <acceptable error>]\n" \
		"  Rseq mode : R5-AnimConv.exe <parent directory> [-i <in season>] [-o <out season>] [-outpath <path>] [-verbose <level>] [-ne] [-comperr <acceptable error>]\n";

	//**Options:**
	//	- `-i <season>` - Input assets season(RSEQ mode only, range: 7-28 and 30, default: 28)
	//  - `-o <season>` - Output assets season (range: 3 and 21, default: 3)
	//  - `-outpath <path>` - Output directory (default: .\conv\)
	//	- `-verbose <level>` - Verbose output (0: No verbose, 1: Minimal, 2: Full verbose, default: 1)
	//	- `-ne` - Suppress RePak entries output
	//	- `-skipevents` - Skip events that may cause crashes
	//	- `-nopause` - No pause at execution end
	//	- `-comperr <float>` - Compression error threshold(0.5-2.0 recommended, 0.0 lossless, default: 1.0)
	//	- `-rp <path>` - Override internal rrig path(MDL mode only)
	//	- `-sp <path>` - Override internal rseq path(MDL mode only)

	if (argc < 2) {
		printf("%s", usage.c_str());
		system("pause");
		return 1;
	}

	input_mdl = argv[1];
	for (int i = 2; i < argc; ++i) {
		std::string arg = argv[i];
		ARG_VAL("-i", g_in_season, "[!] Error: -i requires input assets season.\n");
		ARG_VAL("-o", g_out_season, "[!] Error: -o requires output assets season.\n");
		ARG_VAL("-outpath", g_Outpath, "[!] Error: -outpath requires a path.\n");
		ARG_INT("-verbose", g_VerboseLevel, "[!] Error: -verbose requires a number.\n");
		ARG_BOOL("-ne", g_NoEntries);
		ARG_BOOL("-skipevents", g_SkipEvents);
		ARG_BOOL("-nopause", g_NoPause);
		ARG_FLT("-comperr", g_AnimCompressError, "[!] Error: -comperr requires a number.\n");
		ARG_VAL("-rp", override_rrig_path, "[!] Error: -rp requires a path.\n");
		ARG_VAL("-sp", override_rseq_path, "[!] Error: -sp requires a path.\n");
		ARG_VAL("-dumptracks", g_DumpTracks, "[!] Error: -dumptracks requires a path.\n");
		ARG_VAL("-targetrigs", g_TargetRigDir, "[!] Error: -targetrigs requires a path.\n");
		ARG_VAL("-dumprig", g_DumpRig, "[!] Error: -dumprig requires a path.\n");
		ARG_BOOL("-snapconstpos", g_SnapConstPos);
		if (arg == "-collapsepose") {
			if (++i >= argc || !ParseCollapsePose(argv[i])) {
				printf("[!] Error: -collapsepose requires <param>=<value>[@<sequence substring>].\n");
				return 1;
			}
			continue;
		}

		printf("Unknown option: %s \n%s", arg.c_str(), usage.c_str());
		return 1;
	}

	if (!std::filesystem::exists(input_mdl)) {
		printf("[!] Error: Input path does not exist.\n");
		return 1;
	}

	// MDL mode: input is a .mdl file
	if (std::filesystem::is_regular_file(input_mdl) && (std::filesystem::path(input_mdl).extension() == ".mdl")) {
		return RunMdlMode(input_mdl, override_rrig_path, override_rseq_path);
	}

	// RSEQ mode: input is a directory that contains animrig/ and animseq/
	return RunRseqMode(input_mdl);
}