#include "../src/Bot/EquipmentDurabilityProbe.h"
#include <iostream>
#include <string>
#include <cassert>
int main(int argc,char** argv)
{
    const std::string script=Bot::EquipmentDurabilityProbe::Lua();
    if(argc==2 && std::string(argv[1])=="--lua")
    { std::cout<<script<<"\nDURABILITY_RESULT={mind,dn,durabilityKnown};"; return 0; }
    assert(script.find("dt:SetInventoryItem('player',slot)")!=std::string::npos);
    assert(script.find("durabilityKnown=0")!=std::string::npos);
}
