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

/*һ��ELF�ļ����ص������е�ֻ��Segment��section������ʹ�õġ�
���Ѱ��code cave����ʹ�ü��ؽ��ڴ��е���ûʲô�õ�Segement��
����PT_NOTE��PT_GNU_EH_FRAME�����޸ı�־λʹ�öο�ִ�С�
������Ŀ�϶̫С����Ϊ10�ֽ����£��ݲ�����ʹ�á�
*/
void BinaryEditor::loadCodeDefaultCaves()
{
	for (const Section & section : _binary->sections())
	{
		const std::string & name = section.name();
		if (name == ".eh_frame" || name == ".eh_frame_hdr")
		{
			CodeCave cave;
			cave.virtual_addr = section.virtual_address();
			cave.size = section.size();
			InstrumentManager::instance()->addCodeCave(cave);
			std::cout << "LOAD cave " << name << " size: " << cave.size << std::endl;

			Segment & segment = _binary->segment_from_virtual_address(cave.virtual_addr);
			if (!segment.has(ELF_SEGMENT_FLAGS::PF_X))
			{
				segment.add(ELF_SEGMENT_FLAGS::PF_X);
				std::cout << "Segment " << std::hex << segment.virtual_address() << " add X flag. " << std::endl;
			}
		}
	}
}

CodeCave * BinaryEditor::addSection(size_t size)
{
	std::cout<< "\033[31m" << "Add section size " << size << "\033[0m" <<std::endl;
	//û���㹻��С��cave�ˣ�ֻ����Ӷ�
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

