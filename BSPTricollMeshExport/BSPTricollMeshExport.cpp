// BSPTricollMeshExport.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <filesystem>
#include <fstream>
#include <set>
#include <map>
#include "spdlog/spdlog.h"
#include "bspStructs.h"

namespace  fs = std::filesystem;

template <typename T> T* getBspLump(std::ifstream& file, bspLump_t& lump,int lumpId = -1) {
    if (lump.length % sizeof(T)) {
        spdlog::error("Lump 0x{:X} has a funny size",lumpId);
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

    bsp->geoSets = getBspLump<GeoSet>(file,header.lumps[0x57],0x57);
    bsp->geoSetCount = header.lumps[0x57].length / sizeof(GeoSet);
    bsp->subModelCount = header.lumps[0xE].length / 32;
    bsp->primitives = getBspLump<CollPrimitive>(file,header.lumps[0x59],0x59);
    bsp->primitiveCount = header.lumps[0x59].length / sizeof(CollPrimitive);
    bsp->uniqueContents = getBspLump<int>(file,header.lumps[0x5B]);
    bsp->verts = getBspLump<Vector3>(file,header.lumps[0x3]);
    bsp->vertCount = header.lumps[0x3].length / sizeof(Vector3);
    bsp->tricollHeaders = getBspLump<Tricoll_Header>(file,header.lumps[0x45],0x45);
    bsp->tricollTris = getBspLump<Tricoll_Tri>(file,header.lumps[0x42],0x42);
    bsp->texData = getBspLump<dtexdata_t>(file,header.lumps[0x2,0x2]);
    bsp->texDataStringData = getBspLump<const char>(file,header.lumps[0x2B],0x2B);
    bsp->texDataStringTable = getBspLump<uint32_t>(file,header.lumps[0x2C],0x2C);
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
std::map<std::string,std::set<Face>> faces;


void addFace(std::string& mat,uint32_t a, uint32_t b, uint32_t c) {
    Face face(a,b,c);
    if (faces.find(mat) != faces.end()) {
        faces[mat].insert(face);
    }
    else {
        std::set<Face> newSet;
        newSet.insert(face);
        faces.emplace(mat,newSet);
    }
    
}


void addTricoll(bspCollision *bsp,uint32_t tricollIndex) {
    static std::set<uint32_t> usedTricolls;
    if(usedTricolls.contains(tricollIndex))return;
    Tricoll_Header* tricollHeader = &bsp->tricollHeaders[tricollIndex];
    Vector3* vertBase = &bsp->verts[tricollHeader->vertStart];
    Tricoll_Tri* triBase = &bsp->tricollTris[tricollHeader->trisStart];
    dtexdata_t* texData = &bsp->texData[tricollHeader->texData];
    std::string texName = std::string(&bsp->texDataStringData[bsp->texDataStringTable[texData->nameStringTableID]]);
    for (int j = 0; j < tricollHeader->trisCount; j++) {
        uint32_t tri = triBase[j].data;
        uint32_t vert0 = tri & 0x3FF;
        uint32_t vert1 = vert0 + ((tri>>10) & 0x7F);
        uint32_t vert2 = vert0 + ((tri>>17) & 0x7F);

        addFace(texName, tricollHeader->vertStart + vert0, tricollHeader->vertStart + vert1, tricollHeader->vertStart + vert2);
    }
}

void addPrimitive(bspCollision* bsp, CollPrimitive prim,uint32_t flag) {
    if(!(bsp->uniqueContents[prim.val&0xFF]&flag))return;
    switch ((prim.val>>29)&0x7) {
    case 2:
    {
        uint16_t index = (prim.val >> 8) & 0x1FFFFF;
        addTricoll(bsp,index);
        break;
    }
    }
}

void writeString(std::ofstream& file, std::string& out) {
    try {
        file.write(out.c_str(), out.size());
    }
    catch (std::ios_base::failure& e) {
        spdlog::error(e.what());
        exit(1);
    }
}
    
int main(int argc,const char** argv)
{
    if (argc < 4) {
        spdlog::error("Not enough params");
        return 1;
    }
    fs::path bspFilePath = fs::path(argv[1]);
    uint32_t flag = std::stoul(argv[2],0,16); 
    bspCollision bsp;
    loadBspFromFile(&bsp,bspFilePath);
    std::set<uint32_t> usedTricolls;
    for (uint32_t i = 0; i < bsp.geoSetCount; i++) {
        
        GeoSet* geoSet = &bsp.geoSets[i];
        
        spdlog::info("{}       {}",i,geoSet->primCount);
        if (geoSet->primCount == 1) {
            addPrimitive(&bsp,(CollPrimitive)geoSet->primStart,flag);
        }
        else {
            int start = (geoSet->primStart>>8) & 0x1FFFFF;
            for (int j = 0; j < geoSet->primCount; j++) {
                addPrimitive(&bsp,bsp.primitives[start+j],flag);
            }
        }
        
    }
    
    fs::path exportFilePath = fs::path(argv[3]);
    std::ofstream exportFile{exportFilePath};
    exportFile.exceptions(std::ofstream::badbit | std::ofstream::failbit);

    for (uint32_t i = 0; i < bsp.vertCount; i++) {
        Vector3& vert = bsp.verts[i];
        std::string out = std::format("v {} {} {}\n",vert.x,vert.y,vert.z);
        writeString(exportFile,out);
    }
    int i = 0;
    for (const std::pair<std::string,std::set<Face>>& group: faces) {
        std::string out = std::format("g group{}\n",i++);
        out += std::format("usemtl {}\n",group.first);
        writeString(exportFile,out);
        const std::set<Face>& fac = group.second;
        for (const Face& face : fac) {
            std::string out = std::format("f {} {} {}\n",face.a+1,face.b+1,face.c+1);
            writeString(exportFile,out);
        }
    }

    
    exportFile.close();
    
    return 0;
}
