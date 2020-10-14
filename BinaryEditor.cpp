#include "BinaryEditor.h"
#include "CSEngine.h"

BinaryEditor * BinaryEditor::_instance = nullptr;

bool BinaryEditor::init(const std::string & elfname)
{
	try {
		_binary = Parser::parse(elfname);
	}
	catch (const LIEF::exception& e) {
		std::cerr << e.what() << std::endl;
		return false;
	}
	loadCodeDefaultCaves();
	return true;
}

void BinaryEditor::writeFile(const std::string & elfname)
{
	_binary->write(elfname);
}

void BinaryEditor::patch_address(uint64_t address, const std::vector<uint8_t> & code)
{
	_binary->patch_address(address, code);
}

/*一个ELF文件加载到进程中的只看Segment，section是链接使用的。
因此寻找code cave可以使用加载进内存中但又没什么用的Segement。
比如PT_NOTE、PT_GNU_EH_FRAME，并修改标志位使该段可执行。
函数间的空隙太小，多为10字节以下，暂不考虑使用。
*/
void BinaryEditor::loadCodeDefaultCaves()
{
	for (Segment & segment : _binary->segments())
	{
		SEGMENT_TYPES type = segment.type();
		if (type == SEGMENT_TYPES::PT_GNU_EH_FRAME)
		{
			CodeCave cave;
			cave.virtual_addr = segment.virtual_address();
			cave.size = segment.virtual_size();
			std::cout << "load cave " << std::hex << cave.virtual_addr << ", size " << std::hex << cave.size << std::endl;
			segment.add(ELF_SEGMENT_FLAGS::PF_X);
			InstrumentManager::instance()->addCodeCave(cave);
		}
	}
}

CodeCave * BinaryEditor::addSection(size_t size)
{
	std::cout<< "\033[31m" << "Add section size " << size << "\033[0m" <<std::endl;
	//没有足够大小的cave了，只能添加段
	Section new_section{ ".gnu.text" };
	new_section.add(ELF_SECTION_FLAGS::SHF_EXECINSTR);
	new_section.add(ELF_SECTION_FLAGS::SHF_ALLOC);
	std::vector<uint8_t> data(size, 0x90);
	new_section.content(data);
	new_section = _binary->add(new_section);
	CodeCave cave;
	cave.virtual_addr = new_section.virtual_address();
	cave.size = new_section.size();
	return InstrumentManager::instance()->addCodeCave(cave);
}

