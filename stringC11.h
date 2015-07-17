#include <limits>

namespace std {
	using ::snprintf;
	using ::vsnprintf;

	inline string to_string(int __val) {
		char buf[4 * sizeof(int)];
		const int len = snprintf(buf, 4 * sizeof(int), "%d", __val);
		return string(buf, buf + len);
	}

	inline string to_string(long long __val) {
		char buf[4 * sizeof(long long)];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
		const int len = snprintf(buf, 4 * sizeof(long long), "%I64d", __val);
#pragma GCC diagnostic pop
		return string(buf, buf + len);
	}

	inline string to_string(double __val) {
	    const int __n = std::numeric_limits<double>::max_exponent10 + 20;
		char buf[__n];
		const int len = snprintf(buf, __n, "%f", __val);
		return string(buf, buf + len);
	}

}