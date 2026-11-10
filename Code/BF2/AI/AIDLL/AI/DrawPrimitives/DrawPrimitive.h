// Base of every AI debug draw primitive.
// [INFERRED] Only the virtual destructor is attested: DPManager deletes each owned primitive
// through slot 0 of its table with the deleting flag set.

#ifndef AI_AIDLL_AI_DRAWPRIMITIVES_DRAWPRIMITIVE_H
#define AI_AIDLL_AI_DRAWPRIMITIVES_DRAWPRIMITIVE_H

namespace dice {
namespace hfe {
namespace ai {

class DrawPrimitive
{
public:
    virtual ~DrawPrimitive() {}
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_DRAWPRIMITIVES_DRAWPRIMITIVE_H
