// [INFERRED] path and name: PE 101989B0 is a free helper shared by BBFire and the information
// environment's enemy predicate; header-only here.
#ifndef AI_AIDLL_AI_AIKNOWLEDGE_OBJECTOCCUPANCY_H
#define AI_AIDLL_AI_AIKNOWLEDGE_OBJECTOCCUPANCY_H

#include "Information.h"
#include "InformationManager.h"
#include "IPIUnit.h"

namespace dice { namespace hfe { namespace ai {

// Whether the object, or the primary object it belongs to, or any of that primary's secondary
// objects, is occupied. A secondary object whose primary is unknown is dereferenced as shipped.
// PE 101989B0.
inline bool isOccupiedObject(unsigned int handle, int side)
{
    Information* information = informationManager->getInformation(handle);
    if (!information)
        return false;
    if (!information->isPrimary())
        information = informationManager->getInformation(information->getPrimaryObject(side));
    if (!information->getUnit())
        return false;
    if (information->getUnit()->isOccupied())
        return true;
    int iterator;
    for (Information* secondary =
             informationManager->getInformation(information->getFirstSecondaryObject(iterator, side));
         secondary;
         secondary = informationManager->getInformation(information->getSecondaryObject(iterator, side)))
    {
        if (secondary->getUnit() && secondary->getUnit()->isOccupied())
            return true;
    }
    return false;
}

} } }
#endif
