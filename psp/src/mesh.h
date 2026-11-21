#ifndef KUBIK_MESH_H
#define KUBIK_MESH_H

/* Matches GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF */
typedef struct {
    float u, v;
    unsigned int color;
    float x, y, z;
} Vertex;

typedef struct {
    Vertex *v;
    int count;
    int cap;
} VertexBuf;

typedef struct {
    VertexBuf solid; /* opaque + alpha-tested blocks */
    VertexBuf water; /* translucent, drawn last */
    int dirty;
} ChunkMesh;

#define ATLAS_SIZE 64
#define ATLAS_TILES 4

void mesh_build(ChunkMesh *m, int cx, int cz);
void mesh_free(ChunkMesh *m);

#endif
