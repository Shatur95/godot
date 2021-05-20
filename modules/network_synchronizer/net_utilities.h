/*************************************************************************/
/*  net_utilities.h                                                      */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

/**
	@author AndreaCatania
*/

#ifndef NET_UTILITIES_H
#define NET_UTILITIES_H

#include "core/local_vector.h"
#include "core/math/math_defs.h"
#include "core/typedefs.h"

#ifdef DEBUG_ENABLED
#define NET_DEBUG_PRINT(msg) \
	print_line(String("[Net] ") + msg)
#define NET_DEBUG_WARN(msg) \
	WARN_PRINT(String("[Net] ") + msg)
#define NET_DEBUG_ERR(msg) \
	ERR_PRINT(String("[Net] ") + msg)
#else
#define NET_DEBUG_PRINT(msg)
#define NET_DEBUG_WARN(msg)
#define NET_DEBUG_ERR(msg)
#endif

template <class T>
class RingAverager {
	LocalVector<T> data;
	uint32_t index = 0;

	T avg_sum = 0;

public:
	RingAverager(uint32_t p_size, T p_default);
	void resize(uint32_t p_size, T p_default);
	void reset(T p_default);

	void push(T p_value);

	/// Maximum value.
	T max() const;

	/// Minumum value.
	T min(uint32_t p_consider_last) const;

	/// Median value.
	T average() const;

private:
	// Used to avoid accumulate precision loss.
	void force_recompute_avg_sum();
};

template <class T>
RingAverager<T>::RingAverager(uint32_t p_size, T p_default) {
	resize(p_size, p_default);
}

template <class T>
void RingAverager<T>::resize(uint32_t p_size, T p_default) {
	data.resize(p_size);

	reset(p_default);
}

template <class T>
void RingAverager<T>::reset(T p_default) {
	for (uint32_t i = 0; i < data.size(); i += 1) {
		data[i] = p_default;
	}

	index = 0;
	force_recompute_avg_sum();
}

template <class T>
void RingAverager<T>::push(T p_value) {
	avg_sum -= data[index];
	avg_sum += p_value;
	data[index] = p_value;

	index = (index + 1) % data.size();
	if (index == 0) {
		// Each cycle recompute the sum.
		force_recompute_avg_sum();
	}
}

template <class T>
T RingAverager<T>::max() const {
	CRASH_COND(data.size() == 0);

	T a = data[0];
	for (uint32_t i = 1; i < data.size(); i += 1) {
		a = MAX(a, data[i]);
	}
	return a;
}

template <class T>
T RingAverager<T>::min(uint32_t p_consider_last) const {
	CRASH_COND(data.size() == 0);
	p_consider_last = MIN(p_consider_last, data.size());

	const uint32_t youngest = (index == 0 ? data.size() : index) - 1;
	const uint32_t oldest = (index + (data.size() - p_consider_last)) % data.size();

	T a = data[oldest];

	uint32_t i = oldest;
	do {
		i = (i + 1) % data.size();
		a = MIN(a, data[i]);
	} while (i != youngest);

	return a;
}

template <class T>
T RingAverager<T>::average() const {
	CRASH_COND(data.size() == 0);

#ifdef DEBUG_ENABLED
	T a = data[0];
	for (uint32_t i = 1; i < data.size(); i += 1) {
		a += data[i];
	}
	a = a / T(data.size());
	T b = avg_sum / T(data.size());
	ERR_FAIL_COND_V_MSG(ABS(a - b) > (CMP_EPSILON * 4.0), b, "The `Averager` accumulated a lot of precision loss: " + rtos(ABS(a - b)));
	return b;
#else
	// Divide it by the buffer size is wrong when the buffer is not yet fully
	// initialized. However, this is wrong just for the first run.
	// I'm leaving it as is because solve it mean do more operations. All this
	// just to get the right value for the first few frames.
	return avg_sum / T(data.size());
#endif
}

template <class T>
void RingAverager<T>::force_recompute_avg_sum() {
#ifdef DEBUG_ENABLED
	// This class is not supposed to be used with 0 size.
	CRASH_COND(data.size() <= 0);
#endif
	avg_sum = data[0];
	for (uint32_t i = 1; i < data.size(); i += 1) {
		avg_sum += data[i];
	}
}

#endif
