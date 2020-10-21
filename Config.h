#pragma once
#include <string>
#include <cstdint>
#include <list>

struct ConfigUnit
{
	uint64_t addr;
	std::string asmtext;
	bool is_replace;
};

class JsonWrapper;
class Config
{
public:
	static Config * instance();
	static void destroy();
	void init(const std::string & filename);
	void loadPatchs(std::list<ConfigUnit> & patches)const;
private:
	Config();
	~Config();
private:
	static Config * _instance;
	JsonWrapper * _json;
};

