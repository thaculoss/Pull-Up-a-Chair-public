#include "NifSupport.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <chrono>
int main(int argc,char** argv) {
    if(argc!=3)return 2;
    std::ifstream list(std::filesystem::u8path(argv[1]));std::ofstream out(std::filesystem::u8path(argv[2]));
    int count=0,accepted=0,refused=0;bool reportedRound=false;std::string line;
    out<<"model\tstatus\tmilliseconds\tleft\tnear\tright\tfar\ttop\n";
    while(std::getline(list,line)) {
        const auto separator=line.find('\t');if(separator==std::string::npos)continue;
        const auto path=line.substr(0,separator),model=line.substr(separator+1);++count;
        const auto start=std::chrono::steady_clock::now();
        try {
            std::ifstream file(std::filesystem::u8path(path),std::ios::binary);
            const auto mesh=inn::support::ReadMesh(file);const auto rectangle=inn::support::SafeRectangle(mesh);
            const auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
            out<<model<<'\t'<<(rectangle?"certified":"refused")<<'\t'<<ms;
            if(rectangle) {
                ++accepted;const auto& r=*rectangle;
                bool supported=false;
                for(const auto& plane:mesh.planes)if(std::abs(plane.z-r[4])<.001)
                    supported=inn::support::Covered(inn::support::Rectangle(r[0],r[1],r[2],r[3]),plane.triangles);
                if(!supported){std::cerr<<"UNSAFE rectangle: "<<model;return 1;}
                for(auto value:r)out<<'\t'<<value;
                if(model.find("nobletableraound01.nif")!=std::string::npos)reportedRound=true;
            } else ++refused;
            out<<'\n';
        } catch(const std::exception& e){++refused;out<<model<<"\trejected_mesh\t0\t"<<e.what()<<'\n';}
    }
    std::cout<<count<<" real NIF fixtures; "<<accepted<<" certified; "<<refused<<" conservatively refused.\n";
    if(!reportedRound||accepted<10){std::cerr<<"Insufficient real mesh coverage";return 1;}
    return 0;
}
