#include <pch.h>

#include <rrig/rrig.h>
#include <rseq/rseq.h>
#include <mdl/mdl.h>
#include <utils/rson_parser.h>
#include <utils/misc.h>
#include <core/cli.h>

std::string g_in_season = "28";
std::string g_out_season = "3";

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
		/* PARSE */
		{
			if (rig.rsonpath.empty()) {
				printf("[!] Skipping: no .rson was founded for %s\n", rig.rrigpath.c_str());
				continue;
			}

			/* PARSE RRIG */ {
				uint32_t rigFileSize = (uint32_t)std::filesystem::file_size(rig.rrigpath);
				std::ifstream rrig_stream(rig.rrigpath, std::ios::binary);
				rrig_stream.seekg(0, std::ios::beg);
				std::vector<char> buffer(rigFileSize);
				rrig_stream.read(buffer.data(), rigFileSize);
				rrig_stream.close();

				parser->second.rrig(buffer.data(), rig);
				std::replace(rig.name.begin(), rig.name.end(), '\\', '/');

				if (rig.rseqpaths.empty()) continue;
				
				printf("\Converting %s...\n", rig.name.c_str());
			}
			rig.sequences.reserve(rig.rseqpaths.size());

			/* PARSE RSEQ */ {
				parser->second.rseq(in_dir, rig);
			}
		}

		/* WRITE */
		{
			/* WRITE RRIG */ {
				if (std::filesystem::path(rig.name).extension() != ".rmdl") {
					writer->second.rrig(in_dir + "/conv", rig);
				}

				if (rig.rseqpaths.empty()) {
					continue;
				}
			}

			/* WRITE RSEQ */ {
				writer->second.rseq(rig);
			}
		}
		rig.sequences.clear();
	}
	printf("\n");

	/* PRINT REPAK ENTRIES */
	if (!g_NoEntries) printf("\n\nRePak Entries:\n");
	for (auto& rig : rigs)  PrintRepakEntries(rig);
	verbose("[+] Succeeded!\n");

#ifdef _DEBUG
	printf("Animation Data Compressed Types:\n");
	for (int i = 0; i < 8; i++) {
		printf("\t%d: %d\n", i, comptypes[i]);
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
	std::string output_dir = file_path.parent_path().string();
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
	printf("Parsing %s\n", file_path.filename().string().c_str());
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
	printf("\n\nWriting %s\n", rig.name.c_str());
	writer->second.rrig(output_dir, rig);
	printf("Writing sequences\n");
	writer->second.rseq(rig);

	/* PRINT REPAK ENTRIES */
	if (!g_NoEntries) printf("\n\nRePak Entries:\n");
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
		"  Mdl  mode : R5-AnimConv.exe <model.mdl> [-rp <override_rrig_path>] [-sp <override_rseq_path>] [-verbose] [-ne] [-comperr <acceptable error>]\n" \
		"  Rseq mode : R5-AnimConv.exe <parent directory> [-i <in season>] [-verbose] [-ne] [-comperr <acceptable error>]\n";

	//**Options:**
	//	- `-i <season>` - Input assets season(RSEQ mode only, range: 7–28, default: 28)
	//  - `-o <season>` - Output assets season (range: 3 and 21, default: 3)
	//	- `-verbose` - Enable verbose output
	//	- `-ne` - Suppress RePak entries output
	//	- `-skipevents` - Skip events that may cause crashes
	//	- `-nopause` - No pause at execution end
	//	- `-comperr <float>` - Compression error threshold(0.5–2.0 recommended, 0.0 lossless, default: 1.0)
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
		ARG_BOOL("-verbose", g_EnableVerbose);
		ARG_BOOL("-ne", g_NoEntries);
		ARG_BOOL("-skipevents", g_SkipEvents);
		ARG_BOOL("-nopause", g_NoPause);
		ARG_FLT("-comperr", g_AnimCompressError, "[!] Error: -comperr requires a number.\n");
		ARG_VAL("-rp", override_rrig_path, "[!] Error: -rp requires a path.\n");
		ARG_VAL("-sp", override_rseq_path, "[!] Error: -sp requires a path.\n");

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