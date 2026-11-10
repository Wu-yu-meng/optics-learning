#include "communication/messages/SingleStepMessage.hpp"
#include "construction/OpticalCavity.hpp"

void SingleStepMessage::apply(OpticalCavity &cavity) const
{
    cavity.set_running(false);
    cavity.run_once();
}
