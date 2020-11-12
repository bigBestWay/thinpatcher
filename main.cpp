#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include "BinaryEditor.h"
#include "KSEngine.h"
#include "CSEngine.h"
#include "Config.h"

void usage(const char * programe)
{
	std::cerr << "Usage: " << programe << " <binary> <config.json>" << std::endl;
}

void insert_code(const char * assembly, uint64_t address)
{
    cs_insn * insns = nullptr;
    size_t count = 0;
    try
    {
        //第1步，获取给定地址的代码并反编译，写死0x1000字节应该足够了
        const std::vector<uint8_t> & code = BinaryEditor::instance()->get_content(address, 0x1000);
        count = CSEngine::instance()->disasm(code, address, &insns);
        if (count <= 1)
        {
            std::cerr << "disass fail. "<< std::endl;
            return;
        }

        std::vector<PatchUnit> patchUnits;
        InstrumentManager::instance()->generateJmpCode(insns, count, assembly, patchUnits);
        for (const PatchUnit & unit : patchUnits)
        {
        #if 1
            std::cout << "Patch code:" << std::endl;
            CSEngine::instance()->disasmShow(unit.code, unit.address);
        #endif
            BinaryEditor::instance()->patch_address(unit.address, unit.code);
        }

    }
    catch(int& e)
    {
        std::cerr << "generateJmpCode excepiton. " << std::endl;
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
    
    cs_free(insns, count);
}

void replace_code(const char * assembly, uint64_t address)
{
    std::vector<uint8_t> patchcode;
    KSEngine::instance()->assemble(assembly, address, patchcode);
    cs_insn * insns = nullptr;
    size_t count = 0;
    try
    {
        const std::vector<uint8_t> & code = BinaryEditor::instance()->get_content(address, 0x1000);
        count = CSEngine::instance()->disasm(code, address, &insns);
        if (count <= 1)
        {
            std::cerr << "disass fail. "<< std::endl;
            return;
        }
        
        size_t size = 0;
        for (size_t i = 0; i < count; i++)
        {
            const cs_insn & insn = insns[i];
            size += insn.size;
            if (size >= patchcode.size())
            {
                //多余的指令码使用NOP补齐
                size_t rest = size - patchcode.size();
                patchcode.insert(patchcode.end(), rest, '\x90');
                break;
            }
        }
                
        BinaryEditor::instance()->patch_address(address, patchcode);
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
    cs_free(insns, count);
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        usage(argv[0]);
        return 1;
    }
    
    std::string elfIn = argv[1];
	if (!BinaryEditor::instance()->init(elfIn))
	{
		std::cerr << "BinaryEditor parse failed" << std::endl;
		return 1;
	}

    Config::instance()->init(argv[2]);
    std::list<ConfigUnit> configs;
    Config::instance()->loadPatchs(configs);

    int index = 0;
    for (const ConfigUnit & config : configs)
    {
        std::cout << "\e[1m""\e[0;34m""----------- " << ++index << " -----------" "\e[0m"<< std::endl;
        uint64_t address = config.addr;
        if (address == 0)
        {
            std::cerr << "'address' param must be a hex number." << std::endl;
            continue;
        }
                
        bool isInsert = !config.is_replace;
        if (isInsert)
        {
            std::cout << "**INSERT MODE**" << std::endl;
            insert_code(config.asmtext.c_str(), address);
        }
        else
        {
            std::cout << "**REPLACE MODE**" << std::endl;
            replace_code(config.asmtext.c_str(), address);
        }
        std::cout << "ASM text:" << std::endl << config.asmtext << std::endl;
    }
    
    std::string elfout = elfIn + "_patched";
	unlink(elfout.c_str());
	BinaryEditor::instance()->writeFile(elfout);
	chmod(elfout.c_str(), S_IRWXU|S_IXGRP|S_IRGRP|S_IXOTH|S_IROTH);
	std::cout << "\033[1m\033[31m" << elfout << " generated." << "\033[0m" << std::endl;
    
    return 0;
}