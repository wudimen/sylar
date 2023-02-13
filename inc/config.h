/*
	知识点：
		1> 静态变量有先后关系时如何保证先的变量一定先定义：
			将先的变量定义为一个静态方法 static ConfigVarMap& getData() ———— L197
		2> YAML::Node的遍历：
			Map(Scalar):
				node.Scalar()
			Map(Sequence)：
				for(size_t i = 0; i < node.size(); ++i) {
					node[i].Scalar()
			Map(Map) :
				for(auto it = node.begin(); it != node.end(); ++it) {
					it->first.Scalar() / it->second.Scalar()
		3> YAML::Node的初始化与插入：
			由已有的string初始化（可分辨Map与Sequence）
				YAML::Node node = YAML::Load(str);
			Sequence:
				YAML::Node node(YAML::NodeType::Sequence);
				node.push_back(tmp_node);
			Map:
				YAML::Node node(YAML::NodeType::Map);
				node[prefix] = value;
		4> YAML::Node的打印：
			stringstream ss;
			ss << node;
			cout << ss.str();

	运行原理（过程）：
		yaml文件 ---->  配置文件  ---->  打印
			Config::lookup():	创建不同类型的配置项
			Config::loadFromYaml(): --> Config::listAllNode()		将yaml文件转换成std::string类型
			如果有回调事件，则调用回调函数完成对于事件（log.cpp中初始化了一个“配置文件中若有log项则更新log文件”的回调事件：直接在log.cpp中初始化一个log配置项，且添加一个回调事件
			ConfigVarBase -> FromString() 		将std::string转换成各种类对象
			ConfigVarBase -> ToString()			将类对象转换成std::string类型，用于打印
		## 对于未知类型，需偏特化LexicalCast类（适应FromString与ToString方法）
*/
#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <iostream>
#include <string>
#include <boost/lexical_cast.hpp>
#include <yaml-cpp/yaml.h>
#include <set>
#include <map>
#include <list>
#include <functional>

#include "log.h"
#include "mutex.h"

namespace sylar {
class LoggerDefine;
class LogAppenderDefine;

template<typename F, typename T>
class LexicalCast {
public:
	T operator()(const F& f) {
		return boost::lexical_cast<T>(f);
	}
};

template<typename T>
class LexicalCast<std::vector<T>, std::string> {
public:
	std::string operator() (const std::vector<T>& f) {
		YAML::Node node(YAML::NodeType::Sequence);
		std::stringstream ss;
		for(auto& i : f) {
			node.push_back(YAML::Load(LexicalCast<T, std::string>() (i)));
		}
		ss << node;
		return ss.str();
	}
};

template<typename T>
class LexicalCast<std::string, std::vector<T> > {
public:
	std::vector<T> operator() (const std::string& f) {
		YAML::Node node = YAML::Load(f);
		std::vector<T> vec;
		std::stringstream ss;
		for(size_t i = 0; i < node.size(); ++i) {
			ss.str("");
			ss << node[i];
			vec.push_back(LexicalCast<std::string, T>() (ss.str()));
		}
		return vec;
	}
};

template<typename T>
class LexicalCast<std::list<T>, std::string> {
public:
	std::string operator() (const std::list<T>& f) {
		YAML::Node node(YAML::NodeType::Sequence);
		std::stringstream ss;
		for(auto& i : f) {
			node.push_back(YAML::Load(LexicalCast<T, std::string>() (i)));
		}
		ss << node;
		return ss.str();
	}
};

template<typename T>
class LexicalCast<std::string, std::list<T> > {
public:
	std::list<T> operator() (const std::string& f) {
		YAML::Node node = YAML::Load(f);
		std::list<T> m_list;
		std::stringstream ss;
		for(size_t i = 0; i < node.size(); ++i) {
			ss.str("");
			ss << node[i];
			m_list.push_back(LexicalCast<std::string, T>() (ss.str()));
		}
		return m_list;
	}
};

template<typename T>
class LexicalCast<std::set<T>, std::string> {
public:
	std::string operator() (const std::set<T>& f) {
		YAML::Node node(YAML::NodeType::Sequence);
		std::stringstream ss;
		for(auto& i : f) {
			node.push_back(YAML::Load(LexicalCast<T, std::string>() (i)));
		}
		ss << node;
		return ss.str();
	}
};

template<typename T>
class LexicalCast<std::string, std::set<T> > {
public:
	std::set<T> operator() (const std::string& f) {
		YAML::Node node = YAML::Load(f);
		std::set<T> m_set;
		std::stringstream ss;
		for(size_t i = 0; i < node.size(); ++i) {
			ss.str("");
			ss << node[i];
// std::cout << "+++test in L140+++" << ss.str() <<  std::endl;
			m_set.insert(LexicalCast<std::string, T>() (ss.str()));
			//m_set.insert(std::make_pair(node[i].first.Scalar(), LexicalCast<std::string, T>() (node[i].Scalar())));
// std::cout << "_set: " << node[i].Scalar() << std::endl;
		}
		return m_set;
	}
};


template<typename T>
class LexicalCast<std::map<std::string, T>, std::string> {
public:
	std::string operator() (const std::map<std::string, T>& f) {
		YAML::Node node(YAML::NodeType::Map);
		std::stringstream ss;
		for(auto& it : f) {
			node[it.first] = YAML::Load(LexicalCast<T, std::string>() (it.second));
		}
		ss << node;
		return ss.str();
	}
};

template<typename T>
class LexicalCast<std::string, std::map<std::string, T> > {
public:
	std::map<std::string, T> operator() (const std::string& f) {
		YAML::Node node = YAML::Load(f);
		std::map<std::string, T> m_map;
		std::stringstream ss;
		for(auto it = node.begin(); it != node.end(); ++it) {
			ss.str();
			ss << it->second;
			m_map[it->first.Scalar()] = LexicalCast<std::string, T>() (ss.str());
// std::cout << "_map: " << it->first.Scalar() << "_______" << m_map[it->first.Scalar()] << std::endl;
			// vec.push_back(LexicalCast<std::string, T>() (node[i].Scalar()));
		}
		return m_map;
	}
};



class ConfigVarBase{
public:
	typedef std::shared_ptr<ConfigVarBase> ptr;

	ConfigVarBase(const std::string name, const std::string description = "") : m_name(name), m_description(description) {};

	virtual ~ConfigVarBase() {};

	// 将值转换成字符串
	// 用于打印配置项内容（在打印时使用）
	virtual std::string ToString() = 0;

	// 从字符串初始化值
	// 用于设置配置项内容的值（在Config中的lookup中进行初始化，以及在loadFromYaml中进行部分更新）
	virtual void FromString(const std::string& str) = 0;

	std::string& getName() {
		return m_name;
	}

	std::string& getDescription() {
		return m_description;
	}
private:
	std::string m_name;
	std::string m_description;
};

template<typename T, typename FromStr = LexicalCast<std::string, T>, typename ToStr = LexicalCast<T, std::string> >
class ConfigVar : public ConfigVarBase {
public:
	typedef std::shared_ptr<ConfigVar> ptr;
	typedef SpinLock MutexType;
	typedef RWMutex RWMutexType;
	typedef std::function<void (const T& old_val, const T& new_val)> m_cb_func;

	ConfigVar(const std::string name, const T default_value, const std::string description) : 
		ConfigVarBase(name, description), 
		m_value(default_value) {
	}

	std::string ToString() override {
		try{
			RWMutexType::RLock lock(m_rwmutex);
			return ToStr()(m_value);
		} catch(std::exception& e) {
			std::cout << "ToString exception From" << getName() << "to string" << std::endl;
		}
		return nullptr;
	}

	void FromString(const std::string& str) override {
		try{
			T t = FromStr()(str);
			setValue(t);
		} catch(std::exception& e) {
			std::cout << "FromString exception From string:" << str << "to T" << std::endl;
		}
	}

	void setValue(T& val) {
		{
			MutexType::Lock lock(m_mutex);
			if(val == m_value) {
				return;
			}
		}
// std::cout << "+++nvar name+++" << getName() << std::endl;
/*
	std::set<LoggerDefine> val_1();
	std::set<LoggerDefine> val_2();
	LoggerDefine l_1("nme_1", "--m--1--m--", "debug");
	LoggerDefine l_2("nme_2", "--m--2--m--", "debug");
	LoggerDefine l_3("nme_3", "--m--3--m--", "debug");
	val_1.insert(l_1);
	val_2.insert(l_1);
	val_2.insert(l_2);
	val_2.insert(l_3);
*/		
		{
			RWMutexType::RLock lock(m_rwmutex);
			for(auto& i : m_cbs) {
//				i.second(val_1, val_2);
				 i.second(m_value, val);
			}
		}
		RWMutexType::WLock lock(m_rwmutex);
		m_value = val;
	}

	T getValue() {
		RWMutexType::RLock lock(m_rwmutex);
		return m_value;
	}

	void addCB(m_cb_func cb) {
		MutexType::Lock lock(m_mutex);
		static uint64_t id = 0;
		++id;
		m_cbs[id] = cb;
	}

	void delCB(uint64_t id) {
		MutexType::Lock lock(m_mutex);
		m_cbs.erase(id);
	}

	void clearCB() {
		MutexType::Lock lock(m_mutex);
		m_cbs.clear();
	}
private:
	T m_value;
	std::map<uint64_t, m_cb_func> m_cbs;
	MutexType m_mutex;
	RWMutexType m_rwmutex;
};

class Config {
public:
	typedef std::map<std::string, ConfigVarBase::ptr> ConfigVarMap;
	typedef RWMutex RWMutexType;

	template<typename T>
	static typename ConfigVar<T>::ptr lookup(const std::string name, const T& default_value, const std::string& description = "") {
		RWMutexType::WLock lock(getMutex());
		auto it = getData().find(name);
		if(it != getData().end()) {
			auto tmp = std::dynamic_pointer_cast<ConfigVar<T> >(it->second);
			return tmp;
		}
		if(std::string::npos != name.find_first_not_of("abcdefghijklmnopqrstuvwxyz_-0123456789i.")) {
			std::cout << "fatal name: " << name << std::endl;
			return nullptr;
		}

		typename ConfigVar<T>::ptr conf(new ConfigVar<T>(name, default_value, description));
		getData()[name] = conf;
		return conf;
	}

	static void listAllNode(std::string prefix, YAML::Node& root, std::list<std::pair<std::string, YAML::Node> >& nodes) {
		if(std::string::npos != prefix.find_first_not_of("abcdefghijklmnopqrstuvwxyz_-0123456789i.")) {
			std::cout << "invaid name: " << prefix << std::endl;
			return;
		}

//std::cout << "__LIST__name: " << prefix << "\tvalue: " << root.Scalar() << std::endl;
/*
		if(root.IsSequence()) {
			YAML::Node node(YAML::NodeType::Sequence);
			for(size_t i = 0; i < root.size(); ++i) {
				node.push_back(root[i]);
				nodes.push_back(std::make_pair(prefix, node));
			}
std::cout << "__Sequence__name: " << prefix << "\tvalue: " << node.Scalar() << "root.size: " << root.size() << std::endl;
		} else {
*/			nodes.push_back(std::make_pair(prefix, root));
//		}

		if(root.IsMap()) {
			for(auto it = root.begin(); it != root.end(); ++it) {
				listAllNode(!prefix.empty() ? prefix + '.' + it->first.Scalar() : it->first.Scalar(), it->second, nodes);
			}
		}
	}

	static void loadFromYaml(YAML::Node node) {
		std::list<std::pair<std::string, YAML::Node> > nodes;
		listAllNode("", node, nodes);
		for(auto& it : nodes) {
			RWMutexType::RLock lock(getMutex());
			auto old_it = getData().find(it.first);
// std::cout << "name: " << it.first << "   value: " << it.second.Scalar() << std::endl;
			if(old_it != getData().end()) {
				if(it.second.IsScalar()) {
					old_it->second->FromString(it.second.Scalar());
				} else {
					std::stringstream ss;
					ss << it.second;
					old_it->second->FromString(ss.str());
				}
			}
		}
	}
private:
	// 为什么要有这个静态函数： 因为这个静态变量可能在其他静态变量要使用时却未被定义，且一定要在其他静态量之前定义；
	// 为什么能在其他静态变量之前定义： 其他变量要使用的话就要调用这个函数，因此一定会提前定义该变量与函数；
	static ConfigVarMap& getData() {
		static ConfigVarMap s_datas; 
		return s_datas;
	}

	static RWMutexType& getMutex() {
		static RWMutexType m_rwmutex;
		return m_rwmutex;
	}
};

}

#endif
