#ifndef _ENDIAN_H_
#define _ENDIAN_H_

#define SYLAR_LITTLE_ENDIAN 1
#define SYLAR_BIG_ENDIAN 2

#include <byteswap.h>
#include <stdint.h>
#include <iostream>

namespace sylar {


// 8字节类型的字节序转换
template<class T>
typename std::enable_if<sizeof(T) == sizeof(uint64_t), T>::type byteswap(T value) {
	return (T)bswap_64((uint64_t)value);
}

// 4字节类型的字节序转换
template<class T>
typename std::enable_if<sizeof(T) == sizeof(uint32_t), T>::type byteswap(T value) {
	return (T)bswap_32((uint32_t)value);
}

// 2字节类型的字节序转换
template<class T>
typename std::enable_if<sizeof(T) == sizeof(uint16_t), T>::type byteswap(T value) {
	return (T)bswap_16((uint16_t)value);
}

#if BYTE_ORDER == BIG_ENDIAN
#define SYLAR_BYTE_ORDER SYLAR_BIG_ENDIAN
#else
#define SYLAR_BYTE_ORDER SYLAR_LITTLE_ENDIAN
#endif

#if SYLAR_BYTE_ORDER == SYLAR_BIG_ENDIAN

template<class T>
T byteswapOnLittleEndian(T t) {
	return t;
}

template<class T>
T byteswapOnBigEndian(T t) {
	return byteswap(t);
}

#else

template<class T>
T byteswapOnBigEndian(T t) {
	return t;
}

template<class T>
T byteswapOnLittleEndian(T t) {
	return byteswap(t);
}

#endif

}


#endif
