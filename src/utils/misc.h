#pragma once
#include <pch.h>
#include <core/parsers.h>

extern std::string g_in_season;
extern std::string g_out_season;
extern uint8_t g_VerboseLevel;
extern bool g_NoEntries;
extern bool g_SkipEvents;
extern bool g_NoPause;

void GatherRigPaths(std::string in_dir, std::filesystem::directory_entry dir, std::vector<temp::rig_t>& rrig);
void PrintRepakEntries(temp::rig_t& rig);
