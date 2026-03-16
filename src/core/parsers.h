#pragma once
#include <string>
#include <unordered_map>

#include <rrig/rrig.h>
#include <rseq/rseq.h>

using ParseRRIGFn = void(*)(char*, temp::rig_t&);
using ParseRSEQFn = void(*)(std::string, temp::rig_t&);

struct ParserSet {
	ParseRRIGFn rrig;
	ParseRSEQFn rseq;
};

static const std::unordered_map<std::string, ParserSet> Parsers = {
	{"9",  {ParseRRIG_v121, ParseRSEQ_v10}},
	{"10", {ParseRRIG_v121, ParseRSEQ_v10}},
	{"11", {ParseRRIG_v121, ParseRSEQ_v10}},
	{"12", {ParseRRIG_v121, ParseRSEQ_v10}},
	{"13", {ParseRRIG_v13,  ParseRSEQ_v10}},
	{"14", {ParseRRIG_v14,  ParseRSEQ_v10}},
	{"15", {ParseRRIG_v16,  ParseRSEQ_v11}},
	{"16", {ParseRRIG_v16,  ParseRSEQ_v11}},
	{"17", {ParseRRIG_v16,  ParseRSEQ_v11}},
	{"18", {ParseRRIG_v16,  ParseRSEQ_v11}},
	{"19", {ParseRRIG_v16,  ParseRSEQ_v11}},
	{"20", {ParseRRIG_v17,  ParseRSEQ_v11}},
	{"21", {ParseRRIG_v17,  ParseRSEQ_v11}},
	{"22", {ParseRRIG_v17,  ParseRSEQ_v11}},
	{"23", {ParseRRIG_v17,  ParseRSEQ_v11}},
	{"24", {ParseRRIG_v17,  ParseRSEQ_v12}},
	{"25", {ParseRRIG_v17,  ParseRSEQ_v12}},
	{"26", {ParseRRIG_v19,  ParseRSEQ_v12}},
	{"27", {ParseRRIG_v19,  ParseRSEQ_v121}},
	{"28", {ParseRRIG_v19,  ParseRSEQ_v121}},
};
