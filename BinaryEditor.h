#pragma once
#include <LIEF/ELF.hpp>
#include "InstrumentManager.h"
using namespace LIEF::ELF;

#define DEFAULT_SECTION_SIZE 4096

class BinaryEditor
{
private:
	BinaryEditor(){}
	~BinaryEditor() {}
	static BinaryEditor * _instance;
public:
	static BinaryEditor * instance()
	{
		if (_instance == nullptr)
		{
			_instance = new BinaryEditor();
		}
		return _instance;
	}

	static void destroy()
	{
		delete _instance;
		_instance = nullptr;
	}

	bool init(const std::string & elfname);

	void writeFile(const std::string & elfname);

	ELF_CLASS getPlatform()
	{
		return _binary->type();
	}

	void patch_address(uint64_t address, const std::vector<uint8_t> & code);

	CodeCave * addSection(size_t size = 4096);

	//GOT在ld阶段立即绑定，而不是lazy模式
	bool isBindNow()const;

	std::vector<uint8_t> get_content(uint64_t address, uint64_t size)
	{
		return _binary->get_content_from_virtual_address(address, size);
	}
private:
	void loadCodeDefaultCaves();
private:
	std::unique_ptr<Binary> _binary;
};

