#include <iostream>
#include "BinaryEditor.h"
#include "CSEngine.h"
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include "InstrumentManager.h"

BinaryEditor * BinaryEditor::_instance = nullptr;

bool BinaryEditor::init(const char * elfname)
{
	if (elf_version(EV_CURRENT) == EV_NONE)
		return false;

	_fd = open(elfname, O_RDWR);
	if (_fd < 0)
	{
		return false;
	}

	_binary = elf_begin(_fd, ELF_C_RDWR, (Elf *)0);
	if (_binary == nullptr)
	{
		return false;
	}

	if (elf_kind(_binary) != ELF_K_ELF)
	{
		std::cerr << "Not a ELF file." << std::endl;
		return false;
	}

	int clazz = gelf_getclass(_binary);
	if (clazz == ELFCLASS32)
	{
		_elf_class = ELF_CLASS32;
	}
	else if (clazz == ELFCLASS64)
	{
		_elf_class = ELF_CLASS64;
	}
	else
	{
		_elf_class = ELF_CLASSNONE;
	}

	if (elf_getshstrndx(_binary, &_shstrndx) < 0)
	{
		std::cerr << "cannot get section header string table index." << std::endl;
		return false;
	}

	size_t segment_total_num = 0;
	if (elf_getphdrnum(_binary, &segment_total_num) < 0)
	{
		std::cerr << "cannot determine number of program header." << std::endl;
		return false;
	}

	size_t section_total_num = 0;
	if (elf_getshdrnum(_binary, &section_total_num) < 0)
	{
		std::cerr << "cannot get section total number." << std::endl;
		return false;
	}

	/* load all segments */
	for (size_t i = 0; i < segment_total_num; ++i)
	{
		GElf_Phdr phdr;
		if(gelf_getphdr(_binary, i, &phdr) == nullptr)
			continue;

		_segments[i] = phdr;
	}

	/* load all sections */
	for (size_t i  = 1; i < section_total_num; ++i)
	{
		Elf_Scn * scn = elf_getscn(_binary, i);
		GElf_Shdr shdr;
		if (gelf_getshdr(scn, &shdr) == nullptr)
		{
			std::cerr << elf_errmsg(elf_errno()) << std::endl;
			continue;
		}

		//const char * name = elf_strptr(_binary, _shstrndx, shdr.sh_name);
		//std::cout << name << " virtual_address = " << std::hex << shdr.sh_addr << " size = " << shdr.sh_size << std::endl;

		_sections[i] = shdr;
	}

	loadCodeDefaultCaves();
	return true;
}

void BinaryEditor::writeFile()
{
	elf_flagelf(_binary, ELF_C_SET, ELF_F_LAYOUT);
	elf_update(_binary, ELF_C_WRITE);
	elf_end(_binary);
	close(_fd);
}

void BinaryEditor::patch_address(uint64_t address, const std::vector<uint8_t> & code)
{
	GElf_Shdr shdr;
	int id = section_from_virtual_address(address, shdr);
	if (id >= 0)
	{
		//std::cout << "SectionName=" << get_section_name(shdr) << " virtualAddr = " << std::hex << shdr.sh_addr << " size = " << shdr.sh_size << std::endl;
		uint64_t offset = address - shdr.sh_addr;
		if (code.size() > shdr.sh_size - offset)
		{
			std::cerr << "write data out of section bound." << std::endl;
			return;
		}

		Elf_Scn * scn = elf_getscn(_binary, id);
		Elf_Data * data = elf_getdata(scn, nullptr);
		memcpy(data->d_buf + offset, code.data(), code.size());
		elf_flagscn(scn, ELF_C_SET, ELF_F_DIRTY);
	}
}

void BinaryEditor::addSegment()
{

}

/*一个ELF文件加载到进程中的只看Segment，section是链接使用的。
因此寻找code cave可以使用加载进内存中但又没什么用的Segement。
比如PT_NOTE、PT_GNU_EH_FRAME，并修改标志位使该段可执行。
函数间的空隙太小，多为10字节以下，暂不考虑使用。
*/
void BinaryEditor::loadCodeDefaultCaves()
{
	for (auto & section : _sections)
	{
		const std::string & name = elf_strptr(_binary, _shstrndx, section.second.sh_name);
		if (name == ".eh_frame" || name == ".eh_frame_hdr")
		{
			CodeCave cave;
			cave.virtual_addr = section.second.sh_addr;
			cave.size = section.second.sh_size;
			InstrumentManager::instance()->addCodeCave(cave);
			std::cout << "LOAD cave " << name << " size: " << cave.size << std::endl;

			GElf_Phdr phdr;
			int id = segment_from_virtual_address(cave.virtual_addr, phdr);
			if (id >= 0 && !(phdr.p_flags & PF_X))
			{
				phdr.p_flags |= PF_X;
				gelf_update_phdr(_binary, id, &phdr);
				std::cout << "Segment " << std::hex << phdr.p_vaddr << " add X flag. " << std::endl;
			}
		}
	}
}

int BinaryEditor::set_shstr_ndx(size_t ndx) 
{
	GElf_Ehdr ehdr_mem;
	GElf_Ehdr * ehdr = gelf_getehdr(_binary, &ehdr_mem);
	if (ehdr == NULL)
		return -1;

	if (ndx < SHN_LORESERVE)
		ehdr->e_shstrndx = ndx;
	else 
	{
		ehdr->e_shstrndx = SHN_XINDEX;
		Elf_Scn * zscn = elf_getscn(_binary, 0);
		GElf_Shdr zshdr_mem;
		GElf_Shdr * zshdr = gelf_getshdr(zscn, &zshdr_mem);
		if (zshdr == NULL)
			return -1;
		zshdr->sh_link = ndx;
		if (gelf_update_shdr(zscn, zshdr) == 0)
			return -1;
	}

	if (gelf_update_ehdr(_binary, ehdr) == 0)
		return -1;

	return 0;
}

std::vector<uint8_t> BinaryEditor::get_content(uint64_t address, uint64_t size)
{
	GElf_Shdr shdr;
	std::vector<uint8_t> content;
	int id = section_from_virtual_address(address, shdr);
	if (id >= 0)
	{
		uint64_t offset = address - shdr.sh_addr;
		Elf_Scn * scn = elf_getscn(_binary, id);
		Elf_Data * data = elf_getdata(scn, nullptr);
		content.resize(size);
		memcpy(content.data(), data->d_buf + offset, size);
	}
	return content;
}

int BinaryEditor::segment_from_virtual_address(uint64_t address, GElf_Phdr & phdr)
{
	for (auto & segment : _segments)
	{
		int id = segment.first;
		uint64_t start = segment.second.p_vaddr;
		uint64_t end = start + segment.second.p_memsz;
		if (address >= start && address < end)
		{
			phdr = segment.second;
			return id;
		}
	}

	return -1;
}

int BinaryEditor::section_from_virtual_address(uint64_t address, GElf_Shdr & shdr)
{
	for (auto & section : _sections)
	{
		uint64_t start = section.second.sh_addr;
		uint64_t end = start + section.second.sh_size;
		if (address >= start && address < end)
		{
			shdr = section.second;
			return section.first;
		}
	}

	return -1;
}

const char * BinaryEditor::get_section_name(const GElf_Shdr & shdr) const
{
	return elf_strptr(_binary, _shstrndx, shdr.sh_name);
}

