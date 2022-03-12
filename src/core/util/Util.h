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
}
