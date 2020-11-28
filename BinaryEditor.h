#pragma once
#include <libelf.h>
#include <gelf.h>
#include <map>
#include <list>
#include <vector>

#define DEFAULT_SECTION_SIZE 4096

enum ELF_CLASS
{
	ELF_CLASSNONE = 0,
	ELF_CLASS32 = 1,
	ELF_CLASS64 = 2
};

class BinaryEditor
{
private:
	BinaryEditor():_binary(nullptr), _fd(-1), _elf_class(ELF_CLASSNONE), _shstrndx(0), _extra_shname(0), _extra_section_name(".gnu.text")
	{}
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

	bool init(const char * elfname);

	void writeFile();

	ELF_CLASS getPlatform()
	{
		return _elf_class;
	}

	void patch_address(uint64_t address, const std::vector<uint8_t> & code);

	std::vector<uint8_t> get_content(uint64_t address, uint64_t size);

private:
	int segment_from_virtual_address(uint64_t address, GElf_Phdr & phdr);
	int section_from_virtual_address(uint64_t address, GElf_Shdr & shdr);
	const char * get_section_name(const GElf_Shdr & shdr)const;
	int set_shstr_ndx(size_t ndx);
	void set_new_shstr_section(const std::string & new_section_name);
	void addSegment();

	void loadCodeDefaultCaves();
private:
	Elf * _binary;
	int _fd;
	size_t _shstrndx;
	/* 新添加节的名字字符表中的偏移 */
	size_t _extra_shname;
	ELF_CLASS _elf_class;
	std::map<int, GElf_Shdr> _sections;
	std::map<int, GElf_Phdr> _segments;
	/* extra section name*/
	std::string _extra_section_name;
};

