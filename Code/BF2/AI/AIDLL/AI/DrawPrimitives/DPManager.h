// Owns AI debug primitives.
// Preserve virtual declaration order. [INFERRED] Return types require validation where implementations remain incomplete.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_DRAWPRIMITIVES_DPMANAGER_H
#define AI_AIDLL_AI_DRAWPRIMITIVES_DPMANAGER_H

#include <string>
#include <vector>
#include <list>
#include <set>
#include <map>

#include "../Main/AIDllTypes.h"
#include "DrawPrimitive.h"

namespace dice {
namespace hfe {
namespace ai {

// Draw-manager interface. Registration guards the roster static, which may remain
// stale until updateStaticMethods refreshes it.
class IDPManager
{
public:
    IDPManager();
    virtual ~IDPManager();
    virtual bool deleteCircle(DrawPrimitive *&) = 0;
    virtual int getCircleAttributes(DrawPrimitive *, float &, unsigned int &, Vec3 &, float &, Vec3 &, bool &, int &) = 0;
    virtual int setCircleAttributes(DrawPrimitive *, float, unsigned int, const Vec3&, float, const Vec3&, bool, int) = 0;
    virtual bool deleteLine(DrawPrimitive *&) = 0;
    virtual int getLineAttributes(DrawPrimitive *, float &, unsigned int &, std::vector<Vec3> &, float &) = 0;
    virtual int setLineAttributes(DrawPrimitive *, float, unsigned int, const std::vector<Vec3>&, float) = 0;
    virtual bool delete2dText(DrawPrimitive *&) = 0;
    virtual int get2dTextAttributes(DrawPrimitive *, float &, unsigned int &, Vec2 &, std::vector<std::string> &, int &, AITextAlignmentX &, AITextAlignmentY &) = 0;
    virtual int set2dTextAttributes(DrawPrimitive *, float, unsigned int, const Vec2&, const std::vector<std::string>&, int, AITextAlignmentX, AITextAlignmentY) = 0;
    virtual bool delete3dText(DrawPrimitive *&) = 0;
    virtual int get3dTextAttributes(DrawPrimitive *, float &, unsigned int &, Vec3 &, std::vector<std::string> &, int &, AITextAlignmentX &, AITextAlignmentY &) = 0;
    virtual int set3dTextAttributes(DrawPrimitive *, float, unsigned int, const Vec3&, const std::vector<std::string>&, int, AITextAlignmentX, AITextAlignmentY) = 0;
    virtual bool deleteTriangle(DrawPrimitive *&) = 0;
    virtual int getTriangleAttributes(DrawPrimitive *, float &, std::vector<Vec3> &, std::vector<unsigned int>&, bool &) = 0;
    virtual int setTriangleAttributes(DrawPrimitive *, float &, const std::vector<Vec3>&, const std::vector<unsigned int>&, bool &) = 0;
    virtual bool deletePolygon(DrawPrimitive *&) = 0;
    virtual int getPolygonAttributes(DrawPrimitive *, float &, bool &) = 0;
    virtual int setPolygonAttributes(DrawPrimitive *, float &, bool &) = 0;
    // bool
    virtual bool deleteMesh(DrawPrimitive *&) = 0;
    virtual void setTextGamma(float) = 0;
    virtual float getTextGamma() = 0;
    virtual void drawAll() = 0;
    virtual void nodrawUpdate() = 0;
    virtual void deleteAll() = 0;
};

// IDPStaticCreator - 14 vtable slots.
// The first-wins guard reads AISingletonServer::m_iDPDynamicCreator - the DLL-side half of the roster.
class IDPStaticCreator
{
public:
    IDPStaticCreator();
    virtual ~IDPStaticCreator();
    virtual DPStaticCircle * createStaticCircle(float, unsigned int, const Vec3&, float, const Vec3&, bool, int) = 0;
    virtual DPStaticLine * createStaticLine(float, unsigned int, const Vec3&, const Vec3&, float) = 0;
    virtual int createStaticLine(float, unsigned int, const std::vector<Vec3>&, float) = 0;
    virtual DrawPrimitive * createStatic2dText(float, unsigned int, const Vec2&, const std::string&, int, AITextAlignmentX, AITextAlignmentY) = 0;
    virtual DrawPrimitive * createStatic2dText(float, unsigned int, const Vec2&, const std::vector<std::string>&, int, AITextAlignmentX, AITextAlignmentY) = 0;
    virtual int createStatic3dText(float, unsigned int, const Vec3&, const std::string&, int, AITextAlignmentX, AITextAlignmentY) = 0;
    virtual int createStatic3dText(float, unsigned int, const Vec3&, const std::vector<std::string>&, int, AITextAlignmentX, AITextAlignmentY) = 0;
    virtual int createStaticTriangle(float, const std::vector<Vec3>&, const std::vector<unsigned int>&, bool) = 0;
    virtual int createStaticPolygon(float, const std::vector<Vec3>&, const std::vector<unsigned int>&, bool) = 0;
    virtual DPStaticMesh * createStaticMesh(float, unsigned int *, Vec3 *, unsigned int *, int, bool) = 0;
    virtual int createStaticMesh(float, unsigned int *, Polygon *, Vec3 *, unsigned int *, int, bool) = 0;
    virtual int testStatics(Mat4 &) = 0;
};

// IDPDynamicCreator - 12 vtable slots.
// The first-wins guard reads AISingletonServer::m_iDPDynamicCreator - the DLL-side half of the roster.
class IDPDynamicCreator
{
public:
    IDPDynamicCreator();
    virtual ~IDPDynamicCreator();
    virtual DPStaticCircle * createDPDynamicCircle(float, unsigned int, const Vec3&, float, Bot *, DPDynamicTypes, const Vec3&, bool, int) = 0;
    virtual DPStaticCircle * createDPDynamicCircle(float, unsigned int, const Vec3&, float, unsigned int, DPDynamicTypes, const Vec3&, bool, int) = 0;
    virtual DPStaticLine * createDynamicLine(float, unsigned int, const Vec3&, const Vec3&, float, Bot *, DPDynamicTypes) = 0;
    virtual DPStaticLine * createDynamicLine(float, unsigned int, const Vec3&, const Vec3&, float, unsigned int, DPDynamicTypes) = 0;
    virtual int createDynamicLine(float, unsigned int, const std::vector<Vec3>&, float, Bot *, DPDynamicTypes) = 0;
    virtual int createDynamicLine(float, unsigned int, const std::vector<Vec3>&, float, unsigned int, DPDynamicTypes) = 0;
    virtual int createDynamic3dText(float, unsigned int, const Vec3&, const std::string&, Bot *, DPDynamicTypes, int, AITextAlignmentX, AITextAlignmentY) = 0;
    virtual int createDynamic3dText(float, unsigned int, const Vec3&, const std::vector<std::string>&, Bot *, DPDynamicTypes, int, AITextAlignmentX, AITextAlignmentY) = 0;
    virtual int createDynamic3dText(float, unsigned int, const Vec3&, const std::string&, unsigned int, DPDynamicTypes, int, AITextAlignmentX, AITextAlignmentY) = 0;
    virtual int createDynamic3dText(float, unsigned int, const Vec3&, const std::vector<std::string>&, unsigned int, DPDynamicTypes, int, AITextAlignmentX, AITextAlignmentY) = 0;
};

// IDPAIObjectCreator - 7 vtable slots.
// The first-wins guard reads AISingletonServer::m_iDPDynamicCreator - the DLL-side half of the roster.
class IDPAIObjectCreator
{
public:
    IDPAIObjectCreator();
    virtual ~IDPAIObjectCreator();
    virtual DPStaticCircle * createDPAIObjectCircle(float, unsigned int, const Vec3&, float, unsigned int, DPDynamicTypes, const Vec3&, bool, int) = 0;
    virtual DPStaticLine * createAIObjectLine(float, unsigned int, const Vec3&, const Vec3&, float, unsigned int, DPDynamicTypes) = 0;
    virtual int createAIObjectLine(float, unsigned int, const std::vector<Vec3>&, float, unsigned int, DPDynamicTypes) = 0;
    virtual int createAIObject3dText(float, unsigned int, const Vec3&, const std::string&, unsigned int, DPDynamicTypes, int, AITextAlignmentX, AITextAlignmentY) = 0;
    virtual int createAIObject3dText(float, unsigned int, const Vec3&, const std::vector<std::string>&, unsigned int, DPDynamicTypes, int, AITextAlignmentX, AITextAlignmentY) = 0;
};

// DPManager - sizeof == 104, from BF2AI::init step 7.
class DPManager : public IDPManager, public IDPStaticCreator, public IDPDynamicCreator, public IDPAIObjectCreator
{
public:
    DPManager();
    virtual ~DPManager();

    // IDPManager
    virtual bool deleteCircle(DrawPrimitive *&);
    virtual int getCircleAttributes(DrawPrimitive *, float &, unsigned int &, Vec3 &, float &, Vec3 &, bool &, int &);
    virtual int setCircleAttributes(DrawPrimitive *, float, unsigned int, const Vec3&, float, const Vec3&, bool, int);
    virtual bool deleteLine(DrawPrimitive *&);
    virtual int getLineAttributes(DrawPrimitive *, float &, unsigned int &, std::vector<Vec3> &, float &);
    virtual int setLineAttributes(DrawPrimitive *, float, unsigned int, const std::vector<Vec3>&, float);
    virtual bool delete2dText(DrawPrimitive *&);
    virtual int get2dTextAttributes(DrawPrimitive *, float &, unsigned int &, Vec2 &, std::vector<std::string> &, int &, AITextAlignmentX &, AITextAlignmentY &);
    virtual int set2dTextAttributes(DrawPrimitive *, float, unsigned int, const Vec2&, const std::vector<std::string>&, int, AITextAlignmentX, AITextAlignmentY);
    virtual bool delete3dText(DrawPrimitive *&);
    virtual int get3dTextAttributes(DrawPrimitive *, float &, unsigned int &, Vec3 &, std::vector<std::string> &, int &, AITextAlignmentX &, AITextAlignmentY &);
    virtual int set3dTextAttributes(DrawPrimitive *, float, unsigned int, const Vec3&, const std::vector<std::string>&, int, AITextAlignmentX, AITextAlignmentY);
    virtual bool deleteTriangle(DrawPrimitive *&);
    virtual int getTriangleAttributes(DrawPrimitive *, float &, std::vector<Vec3> &, std::vector<unsigned int>&, bool &);
    virtual int setTriangleAttributes(DrawPrimitive *, float &, const std::vector<Vec3>&, const std::vector<unsigned int>&, bool &);
    virtual bool deletePolygon(DrawPrimitive *&);
    virtual int getPolygonAttributes(DrawPrimitive *, float &, bool &);
    virtual int setPolygonAttributes(DrawPrimitive *, float &, bool &);
    virtual bool deleteMesh(DrawPrimitive *&);
    virtual void setTextGamma(float);
    virtual float getTextGamma();
    virtual void drawAll();
    virtual void nodrawUpdate();
    virtual void deleteAll();

    // IDPStaticCreator
    virtual DPStaticCircle * createStaticCircle(float, unsigned int, const Vec3&, float, const Vec3&, bool, int);
    virtual DPStaticLine * createStaticLine(float, unsigned int, const Vec3&, const Vec3&, float);
    virtual int createStaticLine(float, unsigned int, const std::vector<Vec3>&, float);
    virtual DrawPrimitive * createStatic2dText(float, unsigned int, const Vec2&, const std::string&, int, AITextAlignmentX, AITextAlignmentY);
    virtual DrawPrimitive * createStatic2dText(float, unsigned int, const Vec2&, const std::vector<std::string>&, int, AITextAlignmentX, AITextAlignmentY);
    virtual int createStatic3dText(float, unsigned int, const Vec3&, const std::string&, int, AITextAlignmentX, AITextAlignmentY);
    virtual int createStatic3dText(float, unsigned int, const Vec3&, const std::vector<std::string>&, int, AITextAlignmentX, AITextAlignmentY);
    virtual int createStaticTriangle(float, const std::vector<Vec3>&, const std::vector<unsigned int>&, bool);
    virtual int createStaticPolygon(float, const std::vector<Vec3>&, const std::vector<unsigned int>&, bool);
    virtual DPStaticMesh * createStaticMesh(float, unsigned int *, Vec3 *, unsigned int *, int, bool);
    virtual int createStaticMesh(float, unsigned int *, Polygon *, Vec3 *, unsigned int *, int, bool);
    virtual int testStatics(Mat4 &);

    // IDPDynamicCreator
    virtual DPStaticCircle * createDPDynamicCircle(float, unsigned int, const Vec3&, float, Bot *, DPDynamicTypes, const Vec3&, bool, int);
    virtual DPStaticCircle * createDPDynamicCircle(float, unsigned int, const Vec3&, float, unsigned int, DPDynamicTypes, const Vec3&, bool, int);
    virtual DPStaticLine * createDynamicLine(float, unsigned int, const Vec3&, const Vec3&, float, Bot *, DPDynamicTypes);
    virtual DPStaticLine * createDynamicLine(float, unsigned int, const Vec3&, const Vec3&, float, unsigned int, DPDynamicTypes);
    virtual int createDynamicLine(float, unsigned int, const std::vector<Vec3>&, float, Bot *, DPDynamicTypes);
    virtual int createDynamicLine(float, unsigned int, const std::vector<Vec3>&, float, unsigned int, DPDynamicTypes);
    virtual int createDynamic3dText(float, unsigned int, const Vec3&, const std::string&, Bot *, DPDynamicTypes, int, AITextAlignmentX, AITextAlignmentY);
    virtual int createDynamic3dText(float, unsigned int, const Vec3&, const std::vector<std::string>&, Bot *, DPDynamicTypes, int, AITextAlignmentX, AITextAlignmentY);
    virtual int createDynamic3dText(float, unsigned int, const Vec3&, const std::string&, unsigned int, DPDynamicTypes, int, AITextAlignmentX, AITextAlignmentY);
    virtual int createDynamic3dText(float, unsigned int, const Vec3&, const std::vector<std::string>&, unsigned int, DPDynamicTypes, int, AITextAlignmentX, AITextAlignmentY);

    // IDPAIObjectCreator
    virtual DPStaticCircle * createDPAIObjectCircle(float, unsigned int, const Vec3&, float, unsigned int, DPDynamicTypes, const Vec3&, bool, int);
    virtual DPStaticLine * createAIObjectLine(float, unsigned int, const Vec3&, const Vec3&, float, unsigned int, DPDynamicTypes);
    virtual int createAIObjectLine(float, unsigned int, const std::vector<Vec3>&, float, unsigned int, DPDynamicTypes);
    virtual int createAIObject3dText(float, unsigned int, const Vec3&, const std::string&, unsigned int, DPDynamicTypes, int, AITextAlignmentX, AITextAlignmentY);
    virtual int createAIObject3dText(float, unsigned int, const Vec3&, const std::vector<std::string>&, unsigned int, DPDynamicTypes, int, AITextAlignmentX, AITextAlignmentY);

private:
    // Seven maps from the handle given to callers to the owned primitive, deleted through its
    // virtual destructor, in the constructor's order (PE offsets; the deletes prove which kind
    // each holds). The mapped objects' own classes stay [FRONTIER]: nothing creates one yet.
    /* +0x10 */ std::map<DrawPrimitive*, DrawPrimitive*> m_lines;
    /* +0x1C */ std::map<DrawPrimitive*, DrawPrimitive*> m_circles;
    /* +0x28 */ std::map<DrawPrimitive*, DrawPrimitive*> m_text2d;
    /* +0x34 */ std::map<DrawPrimitive*, DrawPrimitive*> m_text3d;
    /* +0x40 */ std::map<DrawPrimitive*, DrawPrimitive*> m_triangles;
    /* +0x4C */ std::map<DrawPrimitive*, DrawPrimitive*> m_polygons;
    /* +0x58 */ std::map<DrawPrimitive*, DrawPrimitive*> m_meshes;
    /* +0x64 */ float m_textGamma;             // = 1.0f
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_DRAWPRIMITIVES_DPMANAGER_H
