#pragma once
#include <vector>

struct AudioConfigurationEncoding {
	std::vector<uint8_t> data;
};

struct FrameConfigurationEncoding {
	int width;
	int height;
	std::vector<uint8_t> data;
};
//SimpleEncoder
class Encoder {
public:
	bool InitializeEncoder();
	std::vector<uint8_t> Encode(const AudioConfigurationEncoding& configs);
	std::vector<uint8_t> Encode(const FrameConfigurationEncoding& configs);
};