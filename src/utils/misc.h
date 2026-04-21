#pragma once
#include <pch.h>
#include <core/parsers.h>

extern bool g_EnableVerbose;
extern bool g_NoEntries;
extern bool g_SkipEvents;
extern bool g_NoPause;

void GatherRigPaths(std::string in_dir, std::filesystem::directory_entry dir, std::vector<temp::rig_t>& rrig);
void PrintRepakEntries(temp::rig_t& rig);
