#pragma once
#include <pch.h>
#include <core/parsers.h>

void GatherRigPaths(std::string in_dir, std::filesystem::directory_entry dir, std::vector<temp::rig_t>& rrig);
void PrintRepakEntries(temp::rig_t& rig);
