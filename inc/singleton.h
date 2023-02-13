#ifndef _SINGLETON_H_
#define _SINGLETON_H_

namespace sylar {

template<typename T>
class Singleton {
public:
	static T* GetInstance() {
		static T v;
		return &v;
	}
};

}

#endif
