#include "InstrumentManager.h"
#include "CSEngine.h"
#include "KSEngine.h"
#include "BinaryEditor.h"
#include <iostream>

InstrumentManager * InstrumentManager::_instance = nullptr;

CodeCave InstrumentManager::getCodeCave(unsigned int size)
{
	CodeCave cave;
	for (std::list<CodeCave>::iterator ite = m_caves.begin(); ite != m_caves.end(); ++ite)
	{
		if (ite->size == size)
		{
			cave = *ite;
			m_caves.erase(ite);
			break;
		}
		else if (ite->size > size)
		{
			cave.virtual_addr = ite->virtual_addr;
			cave.size = size;
			ite->size -= size;
			ite->virtual_addr += size;
			break;
		}
	}

	//重新排序，按照地址从小到大排序
	m_caves.sort();

	return cave;
}
//addressOffset，当PIE开启时，需要先添加段，添加段后地址就变了，而dyninst对此不感知
void InstrumentManager::generateJmpCode(const cs_insn * insns, size_t count, const char * assembly, CodeCave * cave, std::vector<PatchUnit> & patchUnits)
{
	//printf("Cave: addr = 0x%x, size = %lu\n", cave->virtual_addr, cave->size);
	std::vector<uint8_t> jmpToUpCode;
	std::vector<uint8_t> dstCode;
	bool isX32 = BinaryEditor::instance()->getPlatform() == ELF_CLASS::ELF_CLASS32;

	std::string jmpTo = "jmp " + std::to_string(cave->virtual_addr);
	const uint64_t jmpToUpCodeAddr = insns[0].address;
	KSEngine::instance()->assemble(jmpTo.c_str(), jmpToUpCodeAddr, jmpToUpCode);
	if (jmpToUpCode.empty())
	{
		throw 1;
	}

	size_t newInsnBytes = 0;
	//跳过去之后，先执行补丁代码再复制原被JMP占用的指令代码
	std::vector<uint8_t> patchCode;
	KSEngine::instance()->assemble(assembly, cave->virtual_addr, patchCode);
	if (patchCode.empty())
	{
		throw 1;
	}
	newInsnBytes += patchCode.size();
	//该跳转指令需要占用原来几条指令的空间？
	std::string insnsToMove;
	uint64_t moveInsnBytes = 0;
	size_t insnIndex = 0;
	//std::cout << "getJmpCodeBegin Old insns:" << std::endl;
	for (; insnIndex < count; ++insnIndex)
	{
		const cs_insn & insn = insns[insnIndex];
		//如果操作数中有RIP，这个指令就迁移不了
		if (CSEngine::instance()->isInsnOphasRIP(insn))
		{
			printf("0x%" PRIx64 ":\t%s\t%s\n", insn.address, insn.mnemonic, insn.op_str);
			std::cerr << "OP has EIP/RIP, break." << std::endl;
			throw 1;
		}
		moveInsnBytes += insn.size;
		insnsToMove += insn.mnemonic;
		insnsToMove += " ";
		insnsToMove += insn.op_str;
		insnsToMove += std::string(";");
		//printf("0x%" PRIx64 ":\t%s\t%s\n", insn.address, insn.mnemonic, insn.op_str);
		if (moveInsnBytes >= jmpToUpCode.size())
		{
			break;
		}
	}

	//在新位置将原来的指令翻译一遍
	std::vector<uint8_t> insnsToMoveCodeNew;
	KSEngine::instance()->assemble(insnsToMove.c_str(), cave->virtual_addr + newInsnBytes, insnsToMoveCodeNew);
	if (insnsToMoveCodeNew.empty())
	{
		throw 1;
	}
	newInsnBytes += insnsToMoveCodeNew.size();

	std::string jmpBack = "jmp " + std::to_string(insns[insnIndex + 1].address);
	std::vector<uint8_t> jmpBackCode;
	KSEngine::instance()->assemble(jmpBack.c_str(), cave->virtual_addr + newInsnBytes, jmpBackCode);
	if (jmpBackCode.empty())
	{
		throw 1;
	}
	newInsnBytes += jmpBackCode.size();

	dstCode.insert(dstCode.end(), patchCode.begin(), patchCode.end());
	dstCode.insert(dstCode.end(), insnsToMoveCodeNew.begin(), insnsToMoveCodeNew.end());
	dstCode.insert(dstCode.end(), jmpBackCode.begin(), jmpBackCode.end());

	if (dstCode.size() > cave->size)
	{
		//空间不足
		return;
	}

	uint64_t patch_addr = cave->virtual_addr;
	cave->size -= dstCode.size();
	cave->virtual_addr += dstCode.size();

	//std::cout << "Patch code:" << std::endl;
	//CSEngine::instance()->disasmShow(jmpToCode, insns[0].address);
	//CSEngine::instance()->disasmShow(dstCode, patch_addr);

	patchUnits.push_back(PatchUnit(patch_addr, dstCode));
	patchUnits.push_back(PatchUnit(jmpToUpCodeAddr, jmpToUpCode));
}

void InstrumentManager::generateJmpCode(const cs_insn * insns, size_t count, const char * assembly, std::vector<PatchUnit> & patchUnits)
{
	for (std::list<CodeCave>::iterator ite = m_caves.begin(); ite != m_caves.end(); ++ite)
	{
		generateJmpCode(insns, count, assembly, &*ite, patchUnits);
		if (!patchUnits.empty())
		{
			return;
		}
	}
	//std::cout << "getJmpCodeCave not enough cave." << std::endl;
	//现有cave不能满足要求，添加新段
	CodeCave * cave = BinaryEditor::instance()->addSection();
	generateJmpCode(insns, count, assembly, cave, patchUnits);
	InstrumentManager::instance()->addCodeCave(*cave);
}

CodeCave * InstrumentManager::addCodeCave(const CodeCave & cave)
{
	m_caves.push_back(cave);
	return &*m_caves.rbegin();
}


