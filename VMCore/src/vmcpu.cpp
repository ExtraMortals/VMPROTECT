#include "../include/vmcpu.hpp"

#ifdef _WIN32_DEV_ENVIRONMENT
    #include <string>
#endif

// #include <bitset>

VMCPU::VMCPU()
{
    AS = (PADDRESS_SPACE) new ADDRESS_SPACE;
    REGS = (PREGISTERSS) new REGISTERSS;

    memset(AS->codeData, 0, sizeof(AS->codeData));

    REGS->PC = 0;
    REGS->SP = sizeof(AS->stack) / sizeof(VDWORD);

    areFramesNeeded = false;
    currentFrameNumber = 0;
    isError = false;

    #ifdef _WIN32_DEV_ENVIRONMENT
        sysBus = new WIN32();
    #else //_LINUX_DEV_ENVIRONMENT
        sysBus = new UNIX();
    #endif
}

VMCPU::~VMCPU()
{
    if(areFramesNeeded)
    {
        std::string fileNameToRemove;
        for (const auto& [key, value] : frameMap) 
        {
            fileNameToRemove = "./.cached." + std::to_string(key) + ".frame";
            int retVal = sysBus->deleteFile(fileNameToRemove);
        }
    }
    delete AS;
    delete REGS;
    delete sysBus;
}

void VMCPU::getDataFromCodeData(std::string &arg1, int startFrom)
{
    int counter = startFrom;
    int frameNumberToRestore = currentFrameNumber;
    std::stringstream ss;
    VBYTE b;
    while(true)
    {
        if(areFramesNeeded && (counter >= CODE_DATA_SIZE)) counter = loadFrame(counter);
        b = AS->codeData[counter++];
        if((b == 0x3) && (AS->codeData[counter] == 0xD)) break;
        ss << std::hex << b;
    }
    arg1 = ss.str();
    currentFrameNumber = frameNumberToRestore;
    restoreFrame();
    return;
} 

bool VMCPU::loadCode(VBYTE *mcode, int mcsize)
{
    memset(AS->codeData, 0, CODE_DATA_SIZE*sizeof(*(AS->codeData)));
    memset(AS->stack, 0, STACK_SIZE*sizeof(*(AS->stack)));
    memset(AS->dataBuffer, 0, INPUT_BUFFER_SIZE*sizeof(*(AS->dataBuffer)));
    if((unsigned) (mcsize) > (sizeof(AS->codeData) / sizeof(AS->codeData[0]))) 
    {
        std::cout << "[ERROR 101001] TOO BIG A CODE TO EXECUTE!\n";
        return false;
    }
    memcpy(AS->codeData, mcode, mcsize);
    for(int i = 0; i < 8; i++)
    {
        REGS->R[i] = (VDWORD) 0;
    }
    REGS->CF = 0;
    REGS->ZF = 0;
    return true;
}

void VMCPU::vmPrint(VBYTE s)
{
    std::cout << s;
}

void VMCPU::vmPrintHX(VDWORD hx)
{
    std::cout << std::hex << hx;
}

void VMCPU::vmPrintN(VBYTE s)
{
    std::cout << s << std::endl;
}

void VMCPU::vmPrintHXN(VDWORD hx)
{
    std::cout << std::hex << hx << std::endl;
}

void VMCPU::run()
{
    bool exit = false;
    VBYTE opcode;

    while(!exit)
    {
        if(areFramesNeeded && (REGS->PC >= CODE_DATA_SIZE)) REGS->PC = loadFrame(REGS->PC);
        if(isError) return;
        opcode = AS->codeData[REGS->PC++];
        exit = executer(opcode);
    }
    return;
}

VBYTE VMCPU::getByteFromFrame(int bytePosition)
{
    #ifdef V_DEBUG
        std::cout << "[DEBUG] Get a byte from frame" << std::endl;
    #endif
    char byteToReturn;
    auto sum = 0;
    auto frameNumber = -1;
    for (const auto& [key, value] : frameMap) {
        sum += value;
        if(sum >= (bytePosition + 1))
        {
            frameNumber = key;
            break;
        }
    }
    if(frameNumber == -1) goto error_getByteFromFrame;
    else
    {
        std::string frameToLoadName = ".cached." + std::to_string(frameNumber) + ".frame";
        std::ifstream fileBinToRead;
        fileBinToRead.open(frameToLoadName, std::ios::binary);
        auto positionToGet = frameMap[frameNumber] - (sum - bytePosition);
        if(fileBinToRead.is_open())
        {
            auto counter = 0;
            while(fileBinToRead.get(byteToReturn))
            {
                if(counter == positionToGet) break;
                ++counter;
            }
            fileBinToRead.close();
        }
        else goto error_getByteFromFrame;
    }
    goto ok_getByteFromFrame;

error_getByteFromFrame:
        std::cout << "[ERROR] Failed get a byte from frame" << std::endl;
        isError = true;
        return 0x0;
ok_getByteFromFrame:
    isError = false;
    return byteToReturn;
}

int VMCPU::loadFrame(int pc)
{
    #ifdef V_DEBUG
        std::cout << "[DEBUG] Load a frame" << std::endl;
    #endif
    auto sum = 0;
    auto frameNumber = -1;
    for (const auto& [key, value] : frameMap) {
        sum += value;
        if(sum >= (pc+1))
        {
            frameNumber = key;
            break;
        }
    }
    #ifdef V_DEBUG
        std::cout << "[DEBUG] Frame number: " << frameNumber << std::endl;
    #endif
    if(frameNumber == -1) goto error_loadFrame;
    else
    {
        std::string frameToLoadName = ".cached." + std::to_string(frameNumber) + ".frame";
        std::ifstream fileBinToRead;
        fileBinToRead.open(frameToLoadName, std::ios::binary);
        if(fileBinToRead.is_open())
        {
            char vb;
            auto counter = 0;
            memset(AS->codeData, 0, CODE_DATA_SIZE*sizeof(*(AS->codeData)));
            while(fileBinToRead.get(vb)) AS->codeData[counter++] = vb;
            fileBinToRead.close();
        }
        else goto error_loadFrame;
    }
    goto ok_loadFrame;

error_loadFrame:
        std::cout << "[ERROR] Failed load a frame" << std::endl;
        isError = true;
        return 0;

ok_loadFrame:
    currentFrameNumber = frameNumber;
    isError = false;
    return (frameMap[frameNumber] - (sum - pc));
}

void VMCPU::restoreFrame()
{
    #ifdef V_DEBUG
        std::cout << "[DEBUG] Restore a frame" << std::endl;
    #endif
    std::string frameToLoadName = ".cached." + std::to_string(currentFrameNumber) + ".frame";
    std::ifstream fileBinToRead;
    fileBinToRead.open(frameToLoadName, std::ios::binary);
    if(fileBinToRead.is_open())
    {
        char vb;
        auto counter = 0;
        memset(AS->codeData, 0, CODE_DATA_SIZE*sizeof(*(AS->codeData)));
        while(fileBinToRead.get(vb)) AS->codeData[counter++] = vb;
        fileBinToRead.close();
    }
    else goto error_restoreFrame;
    goto ok_restoreFrame;

error_restoreFrame:
    std::cout << "[ERROR] Failed restore a frame" << std::endl;
    isError = true;
    return;
ok_restoreFrame:
    isError = false;
    return;
}