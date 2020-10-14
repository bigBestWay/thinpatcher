#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include "BinaryEditor.h"
#include "KSEngine.h"
#include "CSEngine.h"

void usage(const char * programe)
{
	std::cerr << "Usage: " << programe << " <binary> <address> <asm file> [--replace]" << std::endl;
	std::cerr << "       [--replace] insert code at 'address' by default."<< std::endl;
}

char * readFile(int fd)
{
    off_t count = lseek(fd, 0, SEEK_END);
    char * content = new char[count];
    lseek(fd, 0, SEEK_SET);
    read(fd, content, count);
    return content;
}

void insert_code(const char * assembly, uint64_t address)
{
    //第1步，获取给定地址的代码并反编译，写死0x1000字节应该足够了
    const std::vector<uint8_t> & code = BinaryEditor::instance()->get_content(address, 0x1000);
    cs_insn * insns = nullptr;
    size_t count = CSEngine::instance()->disasm(code, address, &insns);
    if (count <= 1)
    {
        std::cerr << "disass fail. "<< std::endl;
        return;
    }

    try
    {
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
        cs_free(insns, count);
        return;
    }
    
    cs_free(insns, count);
}

void replace_code(const char * assembly, uint64_t address)
{
    std::vector<uint8_t> patchcode;
    KSEngine::instance()->assemble(assembly, address, patchcode);
    BinaryEditor::instance()->patch_address(address, patchcode);
}

int main(int argc, char *argv[])
{
    if (argc < 4)
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

    uint64_t address = std::stoull(argv[2], nullptr, 16);
    if (address == 0)
    {
        std::cerr << "'address' param must be a hex number." << std::endl;
        return 1;
    }
    

    std::string asmfile = argv[3];
    int fd = open(asmfile.c_str(), 0);
    if (fd < 0)
    {
        std::cerr << "cannot access file " << asmfile << std::endl;
        return 1;
    }
    
    bool isInsert = true;
    if (argc == 5)
    {
        std::string arg4 = argv[4];
        if (arg4 != "--replace")
        {
            std::cerr << "unkown param '" << arg4 << "'" << std::endl;
            return 1;
        }
        isInsert = false;
    }

    char * content = readFile(fd);
    std::cout << "ASM text:" << std::endl << std::string(content) << std::endl;
    if (isInsert)
    {
        insert_code(content, address);
    }
    else
    {
        replace_code(content, address);
    }

    std::string elfout = elfIn + "_patched";
	unlink(elfout.c_str());
	BinaryEditor::instance()->writeFile(elfout);
	chmod(elfout.c_str(), S_IRWXU|S_IXGRP|S_IRGRP|S_IXOTH|S_IROTH);
	std::cout << "\033[1m\033[31m" << elfout << " generated." << "\033[0m" << std::endl;
    
    return 0;
}