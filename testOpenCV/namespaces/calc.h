#pragma once
#include <opencv2/core/core.hpp>
#include "Util/Vec.h"

/**
 * \brief Calculation utility functionality
 */
namespace calc {

	enum class SlobeDirection {
		VERTICAL, HORIZONTAL, DESCENDING, ASCENDING
	};

	/**
	* Round to the nearest integer (stolen from opencv)
	* @param value The value to round
	* @return Nearest integer as double
	*/
	__forceinline
	int round(double value) {
#if ((defined _MSC_VER && defined _M_X64) || (defined __GNUC__ && defined __x86_64__ \
    && defined __SSE2__ && !defined __APPLE__) || CV_SSE2) && !defined(__CUDACC__)
		__m128d t = _mm_set_sd(value);
		return _mm_cvtsd_si32(t);
#elif defined _MSC_VER && defined _M_IX86
		int t;
		__asm
		{
			fld value;
			fistp t;
		}
		return t;
#elif ((defined _MSC_VER && defined _M_ARM) || defined CV_ICC || \
        defined __GNUC__) && defined HAVE_TEGRA_OPTIMIZATION
		TEGRA_ROUND_DBL(value);
#elif defined CV_ICC || defined __GNUC__
# if defined ARM_ROUND_DBL
		ARM_ROUND_DBL(value);
# else
		return static_cast<int>(lrint(value));
# endif
#else
		/* it's ok if round does not comply with IEEE754 standard;
		the tests should allow +/-1 difference when the tested functions use round */
		return static_cast<int>(floor(d + 0.5));
#endif
	}

	/** Brief Calculates the manhattan distance
	* Manhattan distance between two points
	* @param x1 Point #1 x
	* @param x2 Point #2 x
	* @param y1 Point #1 y
	* @param y2 Point #2 y
	* @return The manhattan distance between the two points
	*/
	template <typename T>
	__forceinline
	T dist_manhattan(const T x1, const T x2, const T y1, const T y2) {
		static_assert(std::is_fundamental<T>::value, "type is only possible for fundamental types.");
		return abs(x2 - x1 + y2 - y1);
	}

	/**
	 * \brief Computes the distance (pyta)
	 * \tparam T The type indicator
	 * \param x1 X of first point
	 * \param x2 X of second point
	 * \param y1 Y of first point
	 * \param y2 Y of second point
	 * \return The distance (rooted)
	 */
	template <typename T>
	__forceinline
	double dist_real(const T x1, const T x2, const T y1, const T y2) {
		static_assert(std::is_fundamental<T>::value, "type is only possible for fundamental types.");
		T x = pow(x2 - x1, 2);
		T y = pow(y2 - y1, 2);
		return sqrt(x + y);
	}

	template <typename T>
	__forceinline
	double angle_points(const T x1, const T x2, const T y1, const T y2) {
		static_assert(std::is_fundamental<T>::value, "type is only possible for fundamental types.");
		T x = std::pow(x2 - x1, 2);
		T y = std::pow(y2 - y1, 2);
		return std::sqrt(x + y);
	}

	template <typename T>
	__forceinline
	double slope(const T x1, const T x2, const T y1, const T y2) {
		static_assert(std::is_fundamental<T>::value, "type is only possible for fundamental types.");
		T dx = x2 - x1;
		T dy = y2 - y1;
		return dy / dx;
	}

	__forceinline
	SlobeDirection slobe_direction(const double slope) {
		if (std::isinf(slope))
			return SlobeDirection::VERTICAL;
		if (slope < 0.0)
			return SlobeDirection::DESCENDING;
		if (slope > 0.0)
			return SlobeDirection::ASCENDING;
		return SlobeDirection::HORIZONTAL;
	}

	/**
	 * \brief Solves a quadratic equation
	 * \tparam T The base type
	 * \param a a operand
	 * \param b b operand
	 * \param c c operanc
	 * \return Tuple in format <bool, T, T>, where bool indicates if both roots are real regardless if they are the same or not.
	 */
	template <typename T>
	__forceinline
	std::tuple<bool, T, T> quadratic_equation(T a, T b, T c) {

		double a2 = a * a;
		double det = b * b - 2 * a2 * c;

		if (det > 0.0) {
			det = std::sqrt(det);
			return std::tuple<bool, T, T>(true, (-b + det) / a2, (-b - det) / a2);
		}
		
		if (det == 0.0) {
			det = std::sqrt(det);
			T res = (-b + det) / a2;
			return std::tuple<bool, T, T>(true, res, res);
		}
		
		return std::tuple<bool, T, T>(true, -b / a2, std::sqrt(-det) / a2);

	}

#ifdef CV_VERSION
	template <typename T>
	__forceinline
	double dist_manhattan(cv::Point_<T>& p1, cv::Point_<T>& p2) {
		static_assert(std::is_fundamental<T>::value, "type is only possible for fundamental types.");
		return dist_manhattan(p1.x, p2.x, p1.y, p1.y);
	}

	template <typename T>
	__forceinline
	double dist_real(cv::Point_<T>& p1, cv::Point_<T>& p2) {
		static_assert(std::is_fundamental<T>::value, "type is only possible for fundamental types.");
		return cv::norm(p2 - p1);
	}

	template <typename T>
	__forceinline
	double slope(cv::Point_<T>& p1, cv::Point_<T>& p2) {
		static_assert(std::is_fundamental<T>::value, "type is only possible for fundamental types.");
		return slope(p1.x, p2.x, p1.y, p2.y);
	}

#else

	template <typename T>
	__forceinline
	double dist_manhattan(v2<T>& p1, v2<T>& p2) {
		static_assert(std::is_fundamental<T>::value, "type is only possible for fundamental types.");
		return dist_manhattan(p1.x, p2.x, p1.y, p1.y);
	}

	template <typename T>
	__forceinline
	double dist_real(v2<T>& p1, v2<T>& p2) {
		static_assert(std::is_fundamental<T>::value, "type is only possible for fundamental types.");
		return dist_real(p1.x, p2.x, p1.y, p2.y);
	}

	template <typename T>
	__forceinline
	double slope(v2<T>& p1, v2<T>& p2) {
		static_assert(std::is_fundamental<T>::value, "type is only possible for fundamental types.");
		return slope(p1.x, p2.x, p1.y, p2.y);
	}



#endif

	/** Brief Determines the highest of two values
	* Max of two ints
	* @param a operand #1
	* @param b operand #2
	* @return the highest of the two operands, defaults to operand #1
	*/
	template <class T>
	__forceinline
	T maxval(T a, T b) {
		static_assert(std::is_fundamental<T>::value, "type is only possible for fundamental types.");
		return cv::max(a, b);
	}

	/** Brief Determines the highest of three values
	* Max of three fundamental values
	* @param a operand #1
	* @param b operand #2
	* @param c operand #3
	* @return the highest of the three operands
	*/
	template <class T>
	__forceinline
	T maxval(T a, T b, T c) {
		static_assert(std::is_fundamental<T>::value, "type is only possible for fundamental types.");
		return maxval(maxval(a, b), c);
	}

}