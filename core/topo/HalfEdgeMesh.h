#pragma once
#include "../mem/Allocators.h"
#include "../Common.h"
#include "../LinAlg.h"

namespace HalfEdge
{
  struct HalfEdge;
  struct Face;

  struct Vertex {
    HalfEdge* leadingEdge = nullptr;
  };


  struct HalfEdge {
    Vertex* srcVtx = nullptr;
    HalfEdge* nextInFace = nullptr;
    HalfEdge* nextInEdge = nullptr;
    Face* face = nullptr;
  };

  struct Face {
    HalfEdge* leadingEdge = nullptr;
  };


  class IStore
  {
  public:
    virtual Vertex* allocVertex() = 0;
    virtual HalfEdge* allocHalfEdge() = 0;
    virtual Face* allocFace() = 0;

    virtual void freeVertex(Vertex*) = 0;
    virtual void freeHalfEdge(HalfEdge*) = 0;
    virtual void freeFace(Face*) = 0;
  };



  class Mesh : NonCopyable
  {
  public:
    uint32_t getBoundaryEdgeCount() const { return boundaryEdges; }
    uint32_t getManifoldEdgeCount() const { return manifoldEdges; }
    uint32_t getNonManifoldEdgeCount() const { return nonManifoldEdges; }

    Vector<Vertex*> vertices;
    Vector<HalfEdge*> halfEdges;
    Vector<Face*> faces;

  protected:
    Logger logger = nullptr;

    Mesh(IStore* store, Logger logger) : store(store), logger(logger) {}

    Vertex* createVertex(HalfEdge* leading = nullptr);
    HalfEdge* createHalfEdge(Vertex* srcVtx=nullptr, HalfEdge* nextInFace=nullptr, HalfEdge* nextInEdge=nullptr, Face* face = nullptr);
    Face* createFace(HalfEdge* leading = nullptr);

    void insert(uint32_t vtxCount, uint32_t* indices, uint32_t* offsets, uint32_t faceCount);

  private:
    IStore* store = nullptr;
    uint32_t boundaryEdges = 0;
    uint32_t manifoldEdges = 0;
    uint32_t nonManifoldEdges = 0;

    void stitchRange(uint32_t heA, uint32_t heB);
  };


  


  struct R3Vertex : Vertex {
    Vec3f pos;
  };

  class R3Mesh : public Mesh, IStore
  {
  public:

    R3Mesh(Logger logger) : Mesh(this, logger) {}

    void insert(const Vec3f* vtx, uint32_t vtxCount, uint32_t* indices, uint32_t* offsets, uint32_t faceCount);


  private:
    Pool<R3Vertex> vertexStore;
    Pool<HalfEdge> halfEdgeStore;
    Pool<Face> faceStore;

    Vertex* allocVertex() override { return vertexStore.alloc(); }
    HalfEdge* allocHalfEdge() override { return halfEdgeStore.alloc(); }
    Face* allocFace() override { return faceStore.alloc(); }

    void freeVertex(Vertex* v) override { vertexStore.release(v); }
    void freeHalfEdge(HalfEdge* he)  override { halfEdgeStore.release(he); }
    void freeFace(Face* f) override { faceStore.release(f); }
  };

}
