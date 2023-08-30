// BSPTricollMeshExport.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <filesystem>
#include <fstream>
#include <set>
#include "spdlog/spdlog.h"
#include "bspStructs.h"

namespace  fs = std::filesystem;

template <typename T> T* getBspLump(std::ifstream& file, bspLump_t& lump) {
    if (lump.length % sizeof(T)) {
        spdlog::error("Lump 0x{:X} has a funny size");
        return 0;
    }
    void* buffer = malloc(lump.length);
    file.seekg(lump.offset);
    file.read((char*)buffer,lump.length);
    return (T*)buffer;
}



void loadBspFromFile(bspCollision* bsp,fs::path& filePath) {
    std::ifstream file{filePath,std::ifstream::binary};
    bspHeader_t header;
    file.read((char*)&header,sizeof(bspHeader_t));
    bsp->geoSets = getBspLump<GeoSet>(file,header.lumps[0x57]);
    bsp->geoSetCount = header.lumps[0x57].length / sizeof(GeoSet);
    bsp->subModelCount = header.lumps[0xE].length / 32;
    bsp->primitives = getBspLump<CollPrimitive>(file,header.lumps[0x59]);
    bsp->primitiveCount = header.lumps[0x59].length / sizeof(CollPrimitive);
    bsp->uniqueContents = getBspLump<int>(file,header.lumps[0x5B]);
    bsp->verts = getBspLump<Vector3>(file,header.lumps[0x3]);
    bsp->vertCount = header.lumps[0x3].length / sizeof(Vector3);
    bsp->tricollHeaders = getBspLump<Tricoll_Header>(file,header.lumps[0x45]);
    bsp->tricollTris = getBspLump<Tricoll_Tri>(file,header.lumps[0x42]);
}

struct Face {
    uint32_t a;
    uint32_t b;
    uint32_t c;

    bool operator==(const Face& other) {
        if((a == other.a)&&(b == other.b)&&(c == other.c))return true;
        
        return false;
    }
    Face(uint32_t x, uint32_t y, uint32_t z) {
        a = x;
        b = y;
        c = z;
        //bubblesort
        if (a < b) {
            uint32_t temp;
            temp = a;
            a = b;
            b = temp;
        }
        if (b < c) {
            uint32_t temp;
            temp = b;
            b = c;
            c = temp;
        }
        if (a < b) {
            uint32_t temp;
            temp = a;
            a = b;
            b = temp;
        }
    }

};
bool operator<(const Face& left,const Face& right) {
    if(left.a<right.a)return true;
    if(left.a>right.a)return false;
    if(left.b<right.b)return true;
    if(left.b>right.b)return false;
    if(left.c<right.c)return true;
    return false;
}


std::vector<Vector3> verts;
std::set<Face> faces;

/*
uint32_t addVert(Vector3& vert) {
    uint32_t index = 0;
    for (auto& v : verts) {
        if ((v.x == vert.x) && (v.y == vert.y) && (v.z == vert.z)) {
            //spdlog::info("found double vert");
            return index;
        }
        index++;
    }
    verts.push_back(vert);
    return index;
}

void addFace(Vector3& a, Vector3& b, Vector3& c) {
    Face face{ addVert(a), addVert(b), addVert(c) };
    faces.insert(face);
}
*/


void addFace(uint32_t a, uint32_t b, uint32_t c) {
    Face face(a,b,c);
    faces.insert(face);
}

int main(int argc,const char** argv)
{
    spdlog::info("{}",sizeof(CollPrimitive));
    if (argc < 4) {
        spdlog::error("Not enough params");
        return 1;
    }
    fs::path bspFilePath = fs::path(argv[1]);
    int flag = std::stoi(argv[2],0,16); 
    bspCollision bsp;
    loadBspFromFile(&bsp,bspFilePath);
    std::set<uint32_t> usedTricolls;
    for (uint32_t i = 0; i < bsp.geoSetCount; i++) {
        
        GeoSet* geoSet = &bsp.geoSets[i];
        int start = (geoSet->primStart>>8) & 0x1FFFFF;
        spdlog::info("{}       {}",i,geoSet->primCount);
        if (geoSet->primCount == 1) {
            CollPrimitive* prim = (CollPrimitive*) & geoSet->primStart;
            if(!(bsp.uniqueContents[prim->val&0xFF]&flag))continue;
            switch ((prim->val>>29)&0x7) {
            case 2:
                uint16_t index = (prim->val>>8)&0x1FFFFF;
                if(usedTricolls.contains(index))continue;
                usedTricolls.insert(index);
                Tricoll_Header* tricollHeader = &bsp.tricollHeaders[index];
                Vector3* vertBase = &bsp.verts[tricollHeader->vertStart];
                Tricoll_Tri* triBase = &bsp.tricollTris[tricollHeader->trisStart];
                for (int j = 0; j < tricollHeader->trisCount; j++) {
                    uint32_t tri = triBase[j].data;
                    uint32_t vert0 = tri & 0x3FF;
                    uint32_t vert1 = vert0 + ((tri>>10) & 0x7F);
                    uint32_t vert2 = vert0 + ((tri>>17) & 0x7F);
                    addFace(tricollHeader->vertStart+vert0,tricollHeader->vertStart+vert1,tricollHeader->vertStart+vert2);

                }
                break;
            }
        }
        else {
            for (int j = 0; j < geoSet->primCount; j++) {
                CollPrimitive* prim = &bsp.primitives[start+j];
                if(!(bsp.uniqueContents[prim->val&0xFF]&flag))continue;
                switch ((prim->val>>29)&0x7) {
                case 2:
                    uint16_t index = (prim->val>>8)&0x1FFFFF;
                    if(usedTricolls.contains(index))continue;
                    usedTricolls.insert(index);
                    Tricoll_Header* tricollHeader = &bsp.tricollHeaders[index];
                    Vector3* vertBase = &bsp.verts[tricollHeader->vertStart];
                    Tricoll_Tri* triBase = &bsp.tricollTris[tricollHeader->trisStart];
                    for (int j = 0; j < tricollHeader->trisCount; j++) {
                        uint32_t tri = triBase[j].data;
                        uint32_t vert0 = tri & 0x3FF;
                        uint32_t vert1 = vert0 + ((tri>>10) & 0x7F);
                        uint32_t vert2 = vert0 + ((tri>>17) & 0x7F);
                        addFace(tricollHeader->vertStart+vert0,tricollHeader->vertStart+vert1,tricollHeader->vertStart+vert2);


                    }
                    break;
                }
            }
        }
        
    }
    fs::path exportFilePath = fs::path(argv[3]);
    std::ofstream exportFile{exportFilePath};
    /*
    for (auto& vert : verts) {
        std::string out = std::format("v {} {} {}\n",vert.x,vert.y,vert.z);
        exportFile.write(out.c_str(),out.size());
    }*/
    for (int i = 0; i < bsp.vertCount; i++) {
        Vector3& vert = bsp.verts[i];
        std::string out = std::format("v {} {} {}\n",vert.x,vert.y,vert.z);
        exportFile.write(out.c_str(),out.size());
    }
    for (auto& face : faces) {
        std::string out = std::format("f {} {} {}\n",face.a+1,face.b+1,face.c+1);
        exportFile.write(out.c_str(),out.size());
    }
    return 0;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
