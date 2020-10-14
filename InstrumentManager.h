#pragma once
#include <list>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <string>

class cs_insn;
struct CodeCave
{
	unsigned long virtual_addr;
	unsigned long size;
	CodeCave():virtual_addr(0), size(0)
	{}
	bool operator<(const CodeCave & o)const
	{
		return this->virtual_addr < o.virtual_addr;
	}
};

struct PatchUnit 
{
	uint64_t address;
	std::vector<uint8_t> code;
	PatchUnit(uint64_t addr, std::vector<uint8_t> & c)
	{
		address = addr;
		code.swap(c);
	}
	PatchUnit() :address(0)
	{

	}
};

class InstrumentManager
{
private:
	InstrumentManager() {}
	~InstrumentManager() {}
public:
	static InstrumentManager * instance()
	{
		if (_instance == nullptr)
		{
			_instance = new InstrumentManager();
		}
		return _instance;
	}

	static void destory()
	{
		delete _instance;
		_instance = nullptr;
	}

	CodeCave getCodeCave(unsigned int size);

	void generateJmpCode(const cs_insn * insns, size_t count, const char * assembly, std::vector<PatchUnit> & patchUnits);

	CodeCave * addCodeCave(const CodeCave & cave);
private:
	static void generateJmpCode(const cs_insn * insns, size_t count, const char * assembly, CodeCave * cave, std::vector<PatchUnit> & patchUnits);
private:
	std::list<CodeCave> m_caves;
private:
	static InstrumentManager * _instance;
};

