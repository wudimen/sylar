#include "log.h"
#include "config.h"

void test_1() {
#define DF_XX(type, name, path,  description) \
	sylar::ConfigVar<type >::ptr name = sylar::Config::lookup(#path,  std::list<int>{1,2,3}, #description);

	sylar::ConfigVar<int>::ptr conf_int_var = sylar::Config::lookup("system.int", (const int)8080, "system port");
	sylar::ConfigVar<float>::ptr conf_float_var = sylar::Config::lookup("system.float", (float)1.2f, "system version");
	sylar::ConfigVar<std::vector<int> >::ptr conf_int_vec_var = sylar::Config::lookup("system.int_vec", std::vector<int>{1,2,3}, "system vec");
	DF_XX(std::list<int>, conf_list_int_var, system.list_int, system_list);
	sylar::ConfigVar<std::set<int> >::ptr conf_set_int_var = sylar::Config::lookup("system.set_int", std::set<int>{1,2,3}, "system set");
	conf_set_int_var->addCB([](const std::set<int>& old_val, const std::set<int>& new_val) {
		std::cout << "===old_val+++" << std::endl;
		for(auto i : old_val) {
			std::cout << i << " ";
		}
		std::cout << std::endl;
		std::cout << "===new_val+++" << std::endl;
		for(auto i : new_val) {
			std::cout << i << " ";
		}
		std::cout << std::endl;
	});

	sylar::ConfigVar<std::map<std::string, int> >::ptr conf_map_int_var = sylar::Config::lookup("system.map_int", std::map<std::string, int>{{"n_1", 1}, {"n_2", 2}, {"n_3", 3}}, "system map");

#undef DF_XX
	
#define SH_XX(name, type, prefix) \
	std::cout << #prefix "  " #type "  " << name->ToString() << std::endl;

	std::cout << conf_int_var->ToString() << std::endl;
	std::cout << conf_float_var->ToString() << std::endl;
	std::cout << conf_int_vec_var->ToString() << std::endl;
	SH_XX(conf_list_int_var, list_int, before);
	SH_XX(conf_set_int_var, set_int, before);
	SH_XX(conf_map_int_var, map_int, before);

	YAML::Node node = YAML::LoadFile("../conf/config_1.yaml");
	sylar::Config::loadFromYaml(node);

	std::cout << conf_int_var->ToString() << std::endl;
	std:: cout << conf_float_var->ToString() << std::endl;
	std::cout << conf_int_vec_var->ToString() << std::endl;
	SH_XX(conf_list_int_var, list_int, after);
	SH_XX(conf_set_int_var, set_int, after);
	SH_XX(conf_map_int_var, map_int, after);
#undef SH_XX
}

class Person {
public:
	Person(const std::string& name, int age, bool sex) : 
		m_name(name), m_age(age), m_sex(sex) {
	}

	Person() {}

	std::string toString() const {
		std::stringstream ss;
		ss << "name: ";
		ss << m_name << "\tage: ";
		ss << m_age << "\tsex: ";
		ss << (m_sex ? "man" : "woman");
		return ss.str();
	}

	bool operator== (const Person& oth) {
		return oth.m_name == m_name && oth.m_age == m_age && oth.m_sex == m_sex;
	}

public:
	std::string m_name;
	int m_age;
	bool m_sex;
};

namespace sylar{
template<>
class LexicalCast<Person, std::string> {
public:
	std::string operator() (const Person& p) {
		std::stringstream ss;
		YAML::Node node(YAML::NodeType::Map);

		node["name"] = p.m_name;
		node["age"] = p.m_age;
		node["sex"] = p.m_sex;

		ss << node;
		return ss.str();
	}
};

template<>
class LexicalCast<std::string, Person> {
public:
	Person operator() (const std::string& str) {
		Person p;
		YAML::Node node = YAML::Load(str);
		
		p.m_name = node["name"].as<std::string>();
		p.m_age = node["age"].as<int>();
		p.m_sex = node["sex"].as<bool>();

		return p;
	}
};
}

void test_Person() {
	sylar::ConfigVar<Person>::ptr conf_pers_var = sylar::Config::lookup("system.person", Person("linjie", 22, true), "system_members");
	std::cout << conf_pers_var->ToString() << std::endl;
	conf_pers_var->addCB([](const Person& t1, const Person& t2) {
std::cout << "----PRINT LAMBDA: " << std::endl;
			std::cout << t1.toString() << std::endl;
			std::cout << t2.toString() << std::endl;
		});

	YAML::Node node = YAML::LoadFile("../conf/config_1.yaml");
	sylar::Config::loadFromYaml(node);

	std::cout << conf_pers_var->ToString() << std::endl;
}

void test_log() {
	sylar::ConfigVar<std::set<sylar::LoggerDefine> >::ptr conf_log_var = sylar::Config::lookup("log_system", std::set<sylar::LoggerDefine>(), "static logs");
	// sylar::ConfigVar<sylar::LoggerDefine>::ptr conf_log_var = sylar::Config::lookup("log_system", sylar::LoggerDefine(), "system.logs");
	sylar::Logger::ptr logger = sylar::LoggerManager::getInstance()->getLogger("system");
	logger->addAppender(sylar::LogAppender::ptr(new sylar::StdoutLogAppender()));

	// std::cout << conf_log_var->ToString() << std::endl;
	std::cout << "-------test before-------" << std::endl;
	SYLAR_LOG_INFO(logger) << "test";

	YAML::Node node = YAML::LoadFile("../conf/config_1.yaml");
	sylar::Config::loadFromYaml(node);

	// std::cout << conf_log_var->ToString() << std::endl;
	std::cout << "-------test after--------" << std::endl;
	SYLAR_LOG_INFO(logger) << "test";

	sylar::Logger::ptr logger_2 = sylar::LoggerManager::getInstance()->getLogger("system_1");
	SYLAR_LOG_INFO(logger_2) << "test_system_1";
}

int main(void) {
	test_log();
	std::cout << "proc end" << std::endl;

	return 0;
}
