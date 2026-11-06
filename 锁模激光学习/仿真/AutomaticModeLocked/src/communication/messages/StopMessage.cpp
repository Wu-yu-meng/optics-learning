#include "communication/messages/StopMessage.hpp"
#include "construction/OpticalCavity.hpp"

void StopMessage::apply(OpticalCavity &cavity) const
{
    cavity.stop();
}
