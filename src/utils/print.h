#pragma once

extern bool g_EnableVerbose;
extern bool g_NoEntries;
void verbose(const char* format, ...);

class ProgressBar {
public:
	int barWidth = 50;
	size_t current = 0;
	size_t total = 0;

	ProgressBar(size_t total);
	void Add(size_t val = 1);
	void Print();
	void AddAndPrint(size_t val = 1);
private:
	float progress = 0.0f;
	void Update();
};