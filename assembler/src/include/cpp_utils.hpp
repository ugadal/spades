/*
 * cpp_utils.hpp
 *
 *  Created on: Nov 14, 2011
 *      Author: valery
 */

#pragma once

#include <boost/static_assert.hpp>

namespace utils
{

// arrays

template <class T, size_t N>
size_t array_size(T (&arr)[N])
{
    return N;
}

template <class T, size_t N>
T* array_end(T (&arr)[N])
{
    return &arr[N];
}

template <size_t EXPECTED_SIZE, class T, size_t N>
void check_array_size(T (&arr)[N])
{
    BOOST_STATIC_ASSERT(EXPECTED_SIZE == N);
}

template <class T>
T identity_function(const T& t) {
	return t;
}

} // namespace utils
