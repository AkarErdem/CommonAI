#include "CommonAITags.h"

namespace CommonAITags
{
    UE_DEFINE_GAMEPLAY_TAG(Activity_Work, "CommonAI.Activity.Work");
    UE_DEFINE_GAMEPLAY_TAG(Activity_Wait, "CommonAI.Activity.Wait");
    UE_DEFINE_GAMEPLAY_TAG(Capability_Work, "CommonAI.Capability.Work");
    UE_DEFINE_GAMEPLAY_TAG(State_Busy, "CommonAI.State.Busy");
    UE_DEFINE_GAMEPLAY_TAG(State_Busy_Working, "CommonAI.State.Busy.Working");
    UE_DEFINE_GAMEPLAY_TAG(State_Busy_Cancelling, "CommonAI.State.Busy.Cancelling");
    UE_DEFINE_GAMEPLAY_TAG(Reason_Replaced, "CommonAI.Reason.Cancel.Replaced");
    UE_DEFINE_GAMEPLAY_TAG(Reason_Disabled, "CommonAI.Reason.Cancel.Disabled");
    UE_DEFINE_GAMEPLAY_TAG(Reason_ProfileChanged, "CommonAI.Reason.Cancel.ProfileChanged");
    UE_DEFINE_GAMEPLAY_TAG(Reason_TargetLost, "CommonAI.Reason.Failure.TargetLost");
    UE_DEFINE_GAMEPLAY_TAG(Reason_InteractionFailed, "CommonAI.Reason.Failure.Interaction");
    UE_DEFINE_GAMEPLAY_TAG(Reason_MovementFailed, "CommonAI.Reason.Failure.Movement");
    UE_DEFINE_GAMEPLAY_TAG(Reason_NoOpportunity, "CommonAI.Reason.Failure.NoOpportunity");
    UE_DEFINE_GAMEPLAY_TAG(Reason_ReservationFailed, "CommonAI.Reason.Failure.Reservation");
    UE_DEFINE_GAMEPLAY_TAG(Reason_CancelPending, "CommonAI.Reason.Wait.SafePoint");
    UE_DEFINE_GAMEPLAY_TAG(Consideration_WorkAvailable, "CommonAI.Consideration.WorkAvailable");
}
