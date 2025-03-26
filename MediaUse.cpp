#include "MediaUse.h"

/**
* @Author:       Li
* @Version:      1.0
* @Date:         2025-03-26
* @Description:  MediaUses.h的实现
**/

using namespace MediaUse;


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        默认构造函数，数据地址赋null，pts=0，size=0
* @Param:        void
* @Return:       void
**/
AVDataInfo::AVDataInfo() :data(nullptr), pts(0), size(0) {

}

/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        带参数构造函数，给数据赋值
* @Param:        @data (unsigned char *) 指定数据地址
*                @pts  int64_t           指定数据pts
*                @size size_t            指定数据大小（单位自定义）
* @Return:       void
**/
AVDataInfo::AVDataInfo(unsigned char* data, int64_t pts, size_t size) :data(data), pts(pts), size(size) {
	
}

/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        稀构函数
* @Param:        void
* @Return:       void
**/
AVDataInfo::~AVDataInfo() {

}

/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        释放数据，会对data执行delete，pts=0，size=0
* @Param:        void
* @Return:       void
**/
void AVDataInfo::clear() {
	if (data) {
		delete[] data;
		data = nullptr;
	}
	pts = 0;
	size = 0;
}


