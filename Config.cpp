#include "Config.h"
#include "CJsonObject.hpp"
#include <iostream>
#include "BinaryEditor.h"

using namespace neb;

class JsonWrapper
{
public:
	JsonWrapper(const std::string & str):_obj(str)
	{
	}

	~JsonWrapper() 
	{
	}

	CJsonObject & get()
	{
		return _obj;
	}

private:
	CJsonObject _obj;
};

Config::Config():_json(0)
{

}

Config::~Config()
{
	delete _json;
	_json = nullptr;
}

void Config::loadPatchs(std::list<ConfigUnit> & patches)const
{
	CJsonObject & array = _json->get()["patches"];
	int size = array.GetArraySize();
	for (int i = 0; i < size; ++i)
	{
		CJsonObject obj;
		if (array.Get(i, obj))
		{
			std::string asmtext, addr;
			obj.Get("asmtext", asmtext);
			obj.Get("address", addr);
			if (addr.empty() || asmtext.empty())
			{
				continue;
			}
			ConfigUnit u;
			u.addr = std::stoul(addr, nullptr, 16);
			u.asmtext = asmtext;
			obj.Get("replace", u.is_replace);
			patches.push_back(u);
		}
	}
}

Config * Config::_instance = nullptr;

Config * Config::instance()
{
	if (_instance == nullptr)
	{
		_instance = new Config;
	}
	return _instance;
}

void Config::destroy()
{
	delete _instance;
	_instance = nullptr;
}

void Config::init(const std::string & filename)
{
	FILE * fp = fopen(filename.c_str(), "r");
	if (fp)
	{
		fseek(fp, 0, SEEK_END);
		long length = ftell(fp);
		char * buff = new char[length];
		fseek(fp, 0, SEEK_SET);
		fread(buff, length, 1, fp);
		fclose(fp);

		_json = new JsonWrapper(buff);
	}
	else
	{
		std::cerr << "config.json not found" << std::endl;
	}
}
