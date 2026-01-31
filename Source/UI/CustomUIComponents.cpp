#include "CustomUIComponents.h"
#include "../Main/NEURONiKProcessor.h"

namespace NEURONiK::UI {

void ModulatedSlider::setModulationTarget(::NEURONiK::ModulationTarget target, ::NEURONiKProcessor* proc) 
{ 
    modTarget = target; 
    processor = proc;
}

std::atomic<float>* ModulatedSlider::getModulationAtomic() const 
{ 
    if (processor != nullptr && modTarget != ::NEURONiK::ModulationTarget::Count)
        return &(processor->getModulationValue(modTarget));
    return nullptr; 
}

} // namespace NEURONiK::UI
