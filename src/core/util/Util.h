#pragma once

#include "../core.h"

namespace Util {
	template<class FwdIt1, class FwdIt2>
	bool moveIter(FwdIt1 src, FwdIt2 dst) {
		if (src < dst) {
			while (src != dst) {
				std::iter_swap(src, src + 1);
				++src;
			}
			return true;
		} else if (src > dst) {
			while (src != dst) {
				std::iter_swap(src, src - 1);
				--src;
			}
			return true;
		}
		return false;
	}

	template<class Vec>
	Vec::iterator iter(const Vec& vec, size_t idx) {
		return vec.begin() + idx;
	}

	uint64_t nextPowerOf2(uint64_t v);

	template<size_t strLen>
	double getMemorySizeMagnitude(uint64_t bytes, char(&sizeLabel)[strLen]) {
		strcpy_s(sizeLabel, strLen >= 5 ? "bytes" : "B");
		double size = bytes;
		if (size >= 1024) {
			size /= 1024;
			strcpy_s(sizeLabel, strLen >= 3 ? "KiB" : strLen >= 2 ? "KB" : "K");
			if (size >= 1024) {
				size /= 1024;
				strcpy_s(sizeLabel, strLen >= 3 ? "MiB" : strLen >= 2 ? "MB" : "M");
				if (size >= 1024) {
					size /= 1024;
					strcpy_s(sizeLabel, strLen >= 3 ? "GiB" : strLen >= 2 ? "GB" : "G");
				}
			}
		}

		return size;
	}
}
