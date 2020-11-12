#SilverThinPatcher  
一款CTF AWD二进制防御补丁工具  
##编译安装  
Ubuntu 16.04 x64，使用cmake编译：
```
cmake .
make
```
##依赖组件  
###CMake  
```
apt install cmake
```
###LIEF 0.10.1  
https://github.com/lief-project/LIEF  
直接下载已编译好的SDK包(x86_64)  
```
wget https://github.com/lief-project/LIEF/releases/download/0.10.1/LIEF-0.10.1-Linux.tar.gz
tar xvf LIEF-0.10.1-Linux.tar.gz
cd LIEF-0.10.1-Linux
cp -r include/ /usr/local/
cp -r lib/ /usr/local/
```
或者下载源码，编译安装（推荐）  
```
wget https://github.com/lief-project/LIEF/archive/0.10.1.tar.gz
tar xvf 0.10.1.tar.gz
cd LIEF-0.10.1
```
LIEF在x32 NO-PIE的情况下，添加segment会导致BUG：
```
Inconsistency detected by ld.so: rtld.c: 1191: dl_main: Assertion `GL(dl_rtld_map).l_libname' failed!
```
解决方法：
在LIEF源码中添加section会添加一个Segment，添加完Segment后调用replace方法将此新段与PT_NOTE互换，即临时解决了此问题。
```
src/ELF/Binary.tcc 631行:
-Segment& segment_added = this->add(new_segment);
+Segment * p_segment_added = nullptr;
+if(this->type() == ELF_CLASS::ELFCLASS32 && this->header().file_type() != E_TYPE::ET_DYN)
+{
+  Segment & note = this->get(SEGMENT_TYPES::PT_NOTE);
+  p_segment_added = &this->replace(new_segment,note);
+}
+else
+{
+  p_segment_added = &this->add(new_segment);
+}
+Segment& segment_added = *p_segment_added;
```
打完补丁后
```
cmake .
make -j4
make install
```
###keystone  
https://github.com/keystone-engine/keystone  
下载源码，编译安装
```
wget https://github.com/keystone-engine/keystone/archive/0.9.2.tar.gz
tar xvf 0.9.2.tar.gz
cd keystone-0.9.2
cmake .
make -j4
make install
```
###capstone  
https://github.com/aquynh/capstone  
```
apt install libcapstone3 libcapstone-dev
```
或者下载源码，编译安装
```
wget https://github.com/aquynh/capstone/archive/4.0.2.tar.gz
tar xvf 4.0.2.tar.gz
cd capstone-4.0.2
./make.sh
```
###CJsonObject  
https://github.com/Bwar/CJsonObject  
这个工程稍微比较麻烦，因为开发者只提供了代码没有想发布链接库的意思，需要我们手工编译生成。
```
git clone https://github.com/Bwar/CJsonObject.git
cd demo
make
cd ..
ar crv libCJsonObject.a cJSON.o CJsonObject.o
cp CJsonObject.hpp /usr/local/include
cp cJSON.h /usr/local/include
cp libCJsonObject.a /usr/local/lib
```
##使用
```
SilverThinPatcher <binary> <config>
binary: elf文件路径  
config: json配置文件路径
{
    "patches":[
        {
            "address": "0x400814",
            "asmtext": "NOP;NOP;xor rax,rax;",
            "replace" : 0 #插入模式，1为覆写模式
        },
        {
            "address": "0x40081E",
            "asmtext": "NOP;NOP;xor rbx,rbx;",
            "replace" : 0
        }
    ]
}
```
##Contact
nu00string@gmail.com
