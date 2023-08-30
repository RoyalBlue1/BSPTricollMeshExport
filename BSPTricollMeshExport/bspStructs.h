#pragma once


struct dcollbrush_t;


struct Vector3 {
    float x;
    float y;
    float z;
};



struct Tricoll_Header
{
    uint16_t flags;
    uint16_t texInfoFlags;
    uint16_t texData;
    uint16_t vertCount;
    uint16_t trisCount;//also count of belvelStart
    uint16_t bevelIndicesCount;
    int32_t vertStart;
    uint32_t trisStart;//also start of belvelStart
    uint32_t nodeStart;
    uint32_t bevelIndicesStart;
    float origin[3];
    float scale;

};



struct Tricoll_Node{
    short vals[8];//just a guess because 16bit intrinics are used on this at engine.dll + 0x1D1B10
};

struct Tricoll_Tri{
    uint32_t data;//bitpacked
};

struct Tricoll_Bevel_Start{
    uint16_t val;
};

struct Tricoll_Bevel_Index{
    uint8_t gap_0[4];
};

struct CMGrid{
    float cellSize;
    int cellOrg[2];
    int cellCount[2];
    int straddleGroupCount;
    int basePlaneOffset;
};

struct colBrush{
    uint8_t gap_0[32];
};

struct GridCell
{
    short geoSetStart;
    short geoSetCount;

};

struct GeoSet{
    short straddleGroup;
    short primCount;
    uint32_t primStart;
};

struct CollPrimitive{
    unsigned int val; 

};

struct dtexdata_t
{
    Vector3 reflectivity;
    int nameStringTableID;
    int width;
    int height;
    int view_width;
    int view_height;
    uint32_t flags;
};


struct bspCollision {
	GeoSet* geoSets;
    uint32_t geoSetCount;
    uint32_t subModelCount;
    CollPrimitive* primitives;
    int* uniqueContents;
    Vector3* verts;
    uint32_t vertCount;
    Tricoll_Header* tricollHeaders;
    Tricoll_Tri* tricollTris;
    uint32_t primitiveCount;
    dcollbrush_t* brushes;
	
};

struct bspLump_t
{
    int32_t offset;
    int32_t length;
    int32_t version;
    int32_t padding;
};

struct bspHeader_t
{
    int32_t ident;       // "rBSP", 1347633778
    int32_t version;     // -
    int32_t mapRevision; // 30
    int32_t lastLump;    // 127

    bspLump_t lumps[128];
};

struct dcollbrush_t {
    Vector3 origin; // size: 12
    char nonAxialCount[2]; // size: 2
    short priorBrushCount; // size: 2
    Vector3  extent; // size: 12
    int priorNonAxialCount; // size: 4
};