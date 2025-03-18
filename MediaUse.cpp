#include "MediaUse.h"

using namespace MediaUse;

AVDataInfo::AVDataInfo() :data(nullptr), pts(0), size(0) {

}

AVDataInfo::AVDataInfo(unsigned char* data, int64_t pts, size_t size) :data(data), pts(pts), size(size) {
	
}

AVDataInfo::~AVDataInfo() {

}

void AVDataInfo::clear() {
	if (data) {
		delete[] data;
		data = nullptr;
	}
	pts = 0;
	size = 0;
}


