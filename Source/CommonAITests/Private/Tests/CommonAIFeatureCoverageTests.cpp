#if WITH_DEV_AUTOMATION_TESTS

#include "Fixtures/CommonAIVendorProbe.h"
#include "AIController.h"
#include "Engine/CurveTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Components/SceneComponent.h"
#include "Contracts/CommonAIWorkProviderComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Misc/AutomationTest.h"
#include "CommonAITags.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"
#include <limits>

// Registered runtime tags are neutral fixture keys; DeveloperTool modules cannot declare native tags.
namespace CommonAIVendorProbe
{
    const FGameplayTag Task_A = CommonAITags::Activity_Work;
    const FGameplayTag Task_B = CommonAITags::Activity_Wait;
    const FGameplayTag Task_Fallback = CommonAITags::Capability_Work;
    const FGameplayTag Input_X = CommonAITags::Consideration_WorkAvailable;
    const FGameplayTag Input_Y = CommonAITags::State_Busy_Working;
    const FGameplayTag Input_New = CommonAITags::Reason_TargetLost;

    constexpr float FrameSeconds = 1.0f / 60.0f;
    constexpr float ShortIntervalSeconds = 0.05f;
    constexpr float IntervalObservationSeconds = 0.2f;

    struct FWorld
    {
        UWorld* World = nullptr;
        AAIController* Controller = nullptr;
        UCommonAIVendorProbeComponent* Component = nullptr;
        UAthenaAIAgentPreset* Preset = nullptr;

        explicit FWorld(bool bWithAISystem = false)
        {
            const UWorld::InitializationValues Settings = UWorld::InitializationValues()
                .AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false)
                .CreateAISystem(bWithAISystem).ShouldSimulatePhysics(false).SetTransactional(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
            GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            World->InitializeActorsForPlay(FURL());
            World->GetWorldSettings()->NotifyBeginPlay();
            Controller = World->SpawnActor<AAIController>();
            Controller->Possess(World->SpawnActor<APawn>());
            Component = NewObject<UCommonAIVendorProbeComponent>(Controller);
            Preset = NewObject<UAthenaAIAgentPreset>(Component);
            Preset->TaskNameShortener = Preset->ConsiderationNameShortener = Preset->ActionNameShortener = 0;
            Preset->ConsdrCurveTable = NewObject<UCurveTable>(Preset);
            UCommonAIVendorProbeFallback* Fallback = NewObject<UCommonAIVendorProbeFallback>(Preset);
            Fallback->Configure(Task_Fallback, {});
            Preset->FallbackTask = Fallback;
        }
        ~FWorld()
        {
            World->EndPlay(EEndPlayReason::Quit);
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
        }
        UCommonAIVendorProbeTask* AddTask(FGameplayTag Id, const FGameplayTagContainer& Inputs, int8 Priority = 1)
        {
            UCommonAIVendorProbeTask* Task = NewObject<UCommonAIVendorProbeTask>(Preset);
            Task->Configure(Id, Inputs, Priority);
            Preset->Tasks.Add(Task);
            for (const FGameplayTag Input : Inputs)
            {
                if (!Preset->GetConsiderations().HasTagExact(Input))
                {
                    FAAIPresetConsiderationEntry Entry;
                    Entry.ConsiderationTag = Input;
                    Preset->Considerations.Add(Entry);
                }
                FRichCurve& Curve = Preset->ConsdrCurveTable->AddRichCurve(Preset->GetTableRowNameFast(Id, Input));
                Curve.AddKey(0.0f, 0.0f);
                Curve.AddKey(1.0f, 1.0f);
            }
            return Task;
        }
        void Start()
        {
            Component->Prepare(Preset);
            Component->RegisterComponent();
            check(Component->EnableAthenaAI(false));
            Component->GetDataManager()->ResumeScoring();
        }
        UCommonAIVendorProbeTask* Task(FGameplayTag Id) const
        {
            return CastChecked<UCommonAIVendorProbeTask>(Component->GetTaskByTag(Id));
        }
        UAthenaAIDataManager* Data() const { return Component->GetDataManager(); }
        void Tick(float Seconds)
        {
            for (int32 Frame = 0; Frame < FMath::CeilToInt(Seconds / FrameSeconds); ++Frame)
            {
                ++GFrameCounter;
                World->Tick(LEVELTICK_All, FrameSeconds);
            }
        }
    };
}

using namespace CommonAIVendorProbe;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonAIVendorScoringTest, "CommonAI.Vendor.Scoring",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommonAIVendorScoringTest::RunTest(const FString& Parameters)
{
    FWorld Fixture;
    FGameplayTagContainer TwoInputs(Input_X);
    TwoInputs.AddTag(Input_Y);
    Fixture.AddTask(Task_A, TwoInputs);
    Fixture.AddTask(Task_B, FGameplayTagContainer(Input_X));
    Fixture.Start();
    Fixture.Data()->SetConsiderationDefaults(Input_X, 1.0f, 0.0f, 0.5f);
    Fixture.Data()->SetConsiderationDefaults(Input_Y, 1.0f, 0.0f, 0.5f);
    Fixture.Component->ForceTaskScoring();
    constexpr float ObservedTwoInputScore = 0.5625f;
    constexpr float FloatingDivisionReferenceScore = 0.390625f;
    TestEqual(TEXT("Single linear consideration"), Fixture.Task(Task_B)->GetScore(), 0.5f);
    TestEqual(TEXT("Installed integer-factor two-input score"), Fixture.Task(Task_A)->GetScore(), ObservedTwoInputScore);
    TestTrue(TEXT("Integer factor differs materially from floating factor"),
        !FMath::IsNearlyEqual(Fixture.Task(Task_A)->GetScore(), FloatingDivisionReferenceScore));
    TestTrue(TEXT("Actual scorer chooses the two-input task"), Fixture.Component->GetCurrentTask() == Fixture.Task(Task_A));
    AddInfo(FString::Printf(TEXT("Two linear inputs at 0.5: installed=%g, floating-factor reference=%g; selected=A"),
        Fixture.Task(Task_A)->GetScore(), FloatingDivisionReferenceScore));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonAIVendorNormalizationTest, "CommonAI.Vendor.Normalization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommonAIVendorNormalizationTest::RunTest(const FString& Parameters)
{
    FWorld Fixture;
    Fixture.AddTask(Task_A, FGameplayTagContainer(Input_X));
    Fixture.Start();
    Fixture.Data()->SetConsiderationDefaults(Input_X, 100.0f, 10.0f, 40.0f);
    FAAIConsideration Existing;
    TestTrue(TEXT("Initialized input is queryable"), Fixture.Data()->GetConsiderationBP(Input_X, Existing));
    TestTrue(TEXT("Correct explicit initialization normalizes"), FMath::IsNearlyEqual(Existing.Value, 1.0f / 3.0f));
    Fixture.Data()->SetConsiderationValueNorm(Input_New, 40.0f, 100.0f, 10.0f, false, false);
    FAAIConsideration Created;
    TestTrue(TEXT("Normalized setter creates the missing input"), Fixture.Data()->GetConsiderationBP(Input_New, Created));
    TestEqual(TEXT("Installed implicit creation stores raw value as minimum"), Created.MinValue, 40.0f);
    TestTrue(TEXT("First result still uses explicit range"), FMath::IsNearlyEqual(Created.Value, 1.0f / 3.0f));
    Fixture.Data()->SetConsiderationValue(Input_New, 40.0f, false);
    Fixture.Data()->GetConsiderationBP(Input_New, Created);
    TestEqual(TEXT("Subsequent ordinary update uses the altered minimum"), Created.Value, 0.0f);
    AddInfo(TEXT("Explicit defaults preserve min=10; implicit SetValueNorm creation stores min=40 and a subsequent value=40 becomes normalized=0."));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonAIVendorPoliciesTest, "CommonAI.Vendor.TaskPolicies",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommonAIVendorPoliciesTest::RunTest(const FString& Parameters)
{
    FWorld Fixture;
    Fixture.AddTask(Task_A, FGameplayTagContainer(Input_X), 1)->ConfigureMomentum(2, 10, 0.0f);
    Fixture.AddTask(Task_B, FGameplayTagContainer(Input_Y), 2);
    Fixture.Start();
    Fixture.Data()->SetConsiderationValue(Input_X, 0.5f, false);
    Fixture.Data()->SetConsiderationValue(Input_Y, 0.5f, false);
    Fixture.Component->ForceTaskScoring();
    TestTrue(TEXT("Priority resolves equal scores"), Fixture.Component->GetCurrentTask() == Fixture.Task(Task_B));
    Fixture.Task(Task_B)->SetReplaceResistance(0.2f);
    Fixture.Data()->SetConsiderationValue(Input_X, 0.6f, true);
    TestTrue(TEXT("Resistance prevents insufficient replacement"), Fixture.Component->GetCurrentTask() == Fixture.Task(Task_B));
    Fixture.Task(Task_B)->SetIsInterruptible(false);
    Fixture.Data()->SetConsiderationValue(Input_X, 0.9f, true);
    TestTrue(TEXT("Non-interruptible task retains execution"), Fixture.Component->GetCurrentTask() == Fixture.Task(Task_B));
    Fixture.Task(Task_B)->SetIsInterruptible(true);
    Fixture.Data()->SetConsiderationValue(Input_Y, 0.0f, true);
    UCommonAIVendorProbeTask* A = Fixture.Task(Task_A);
    TestTrue(TEXT("High score starts A"), Fixture.Component->GetCurrentTask() == A);
    A->CompleteNaturally();
    Fixture.Component->ForceTaskScoring();
    TestEqual(TEXT("Natural completion increments momentum"), A->GetCurrentMomentum(), uint8(1));
    TestEqual(TEXT("Ten percent momentum reduction"), A->GetScore(), 0.81f);
    A->CompleteNaturally();
    Fixture.Component->ForceTaskScoring();
    TestEqual(TEXT("Maximum momentum makes score zero"), A->GetScore(), 0.0f);
    TestTrue(TEXT("No eligible task chooses fallback"), Fixture.Component->GetCurrentTask() == Fixture.Component->GetFallbackTask());
    A->SetMaxMomentum(0);
    A->ConfigureUnlock(ShortIntervalSeconds);
    Fixture.Component->ForceTaskScoring();
    A->FailAction();
    TestTrue(TEXT("Aborted action locks task"), A->IsLocked());
    TestEqual(TEXT("Aborted action resets momentum"), A->GetCurrentMomentum(), uint8(0));
    Fixture.Tick(IntervalObservationSeconds);
    TestFalse(TEXT("Abort unlock timer restores eligibility"), A->IsLocked());
    Fixture.Component->ForceTaskScoring();
    A->Stop(Fixture.Data(), Fixture.Controller, EAAITaskResult::Failed);
    Fixture.Tick(IntervalObservationSeconds);
    TestTrue(TEXT("Failed setup has no abort-unlock timer"), A->IsLocked());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonAIVendorServicesTest, "CommonAI.Vendor.Services",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommonAIVendorServicesTest::RunTest(const FString& Parameters)
{
    FWorld Fixture;
    FGameplayTagContainer Inputs(Input_X);
    Inputs.AddTag(Input_Y);
    Fixture.AddTask(Task_A, Inputs);
    UCommonAIVendorProbeService* Conditional = NewObject<UCommonAIVendorProbeService>(Fixture.Preset);
    Conditional->Configure(Input_X, EAServiceConditionType::GreaterEqual, 0.5f, ShortIntervalSeconds);
    Fixture.Preset->Services.Add(Conditional);
    Fixture.Preset->Services.Add(NewObject<UCommonAIVendorProbeService>(Fixture.Preset));
    UCommonAIVendorProbeService* Both = NewObject<UCommonAIVendorProbeService>(Fixture.Preset);
    Both->Configure(Input_X, EAServiceConditionType::GreaterEqual, 0.5f, ShortIntervalSeconds);
    Both->Configure(Input_Y, EAServiceConditionType::GreaterEqual, 0.5f, ShortIntervalSeconds);
    Fixture.Preset->Services.Add(Both);
    UCommonAIVendorProbeService* NoTick = NewObject<UCommonAIVendorProbeService>(Fixture.Preset);
    NoTick->Configure(Input_X, EAServiceConditionType::GreaterEqual, 0.5f, 0.0f);
    Fixture.Preset->Services.Add(NoTick);
    Fixture.Start();
    auto* Service = CastChecked<UCommonAIVendorProbeService>(Fixture.Component->GetServices()[0]);
    auto* EmptyService = CastChecked<UCommonAIVendorProbeService>(Fixture.Component->GetServices()[1]);
    auto* BothService = CastChecked<UCommonAIVendorProbeService>(Fixture.Component->GetServices()[2]);
    auto* NoTickService = CastChecked<UCommonAIVendorProbeService>(Fixture.Component->GetServices()[3]);
    Fixture.Data()->SetConsiderationDefaults(Input_X, 1.0f, 0.0f, 0.2f);
    Fixture.Data()->SetConsiderationDefaults(Input_Y, 1.0f, 0.0f, 0.8f);
    Fixture.Data()->UpdateAllServices();
    Fixture.Data()->SetConsiderationValue(Input_X, 0.8f, false);
    Fixture.Data()->SetConsiderationValue(Input_Y, 0.2f, false);
    TestEqual(TEXT("A non-scoring batch can transiently start a multi-condition service"), BothService->Starts, 1);
    TestEqual(TEXT("The final input in that batch stops it again"), BothService->Stops, 1);
    TestTrue(TEXT("Condition starts service even without scoring"), Service->IsRunning());
    TestEqual(TEXT("Service start observes the previous stored value"), Service->ValueObservedOnStart, 0.2f);
    Fixture.Tick(IntervalObservationSeconds);
    TestTrue(TEXT("Service timer executes"), Service->Ticks > 0);
    TestEqual(TEXT("Zero interval runs start without a tick timer"), NoTickService->Starts, 1);
    TestEqual(TEXT("Zero interval does not tick"), NoTickService->Ticks, 0);
    Fixture.Data()->PauseScoring();
    Fixture.Data()->SetConsiderationValue(Input_X, 0.1f, false);
    TestTrue(TEXT("Default pause defers condition reevaluation"), Service->IsRunning());
    const int32 BeforePausedTick = Service->Ticks;
    Fixture.Tick(IntervalObservationSeconds);
    TestTrue(TEXT("Pausing scoring does not stop an already running timer"), Service->Ticks > BeforePausedTick);
    Fixture.Data()->ResumeScoring();
    Fixture.Data()->UpdateAllServices();
    TestFalse(TEXT("Reconciliation stops service after condition loss"), Service->IsRunning());
    const int32 BeforeStoppedTick = Service->Ticks;
    Fixture.Tick(IntervalObservationSeconds);
    TestEqual(TEXT("Stopped timer stays stopped"), Service->Ticks, BeforeStoppedTick);
    TestFalse(TEXT("Empty conditions do not auto-start in vendor"), EmptyService->IsRunning());
    AddInfo(TEXT("Service conditions are evaluated before storing the updated consideration; pause suppresses condition updates, not active timer execution."));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonAIVendorTransitionsTest, "CommonAI.Vendor.Transitions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommonAIVendorTransitionsTest::RunTest(const FString& Parameters)
{
    FWorld Fixture;
    Fixture.AddTask(Task_A, FGameplayTagContainer(Input_X), 2);
    Fixture.AddTask(Task_B, FGameplayTagContainer(Input_X), 1);
    Fixture.Start();
    Fixture.Data()->SetConsiderationDefaults(Input_X, 100.0f, 10.0f, 40.0f);
    Fixture.Component->ForceTaskScoring();
    auto* A = Fixture.Task(Task_A);
    auto* B = Fixture.Task(Task_B);
    const int32 BeforeA = A->StartCount;
    const int32 BeforeB = B->StartCount;
    A->SetIsInterruptible(false);
    Fixture.Component->ForceTaskStart(UCommonAIVendorProbeTask::StaticClass(), true);
    TestEqual(TEXT("Class force-start restarts matching A"), A->StartCount, BeforeA + 1);
    TestEqual(TEXT("Class force-start also starts matching B"), B->StartCount, BeforeB + 1);
    TestTrue(TEXT("Last same-class instance is left current"), Fixture.Component->GetCurrentTask() == B);
    TestTrue(TEXT("Force command can hold scoring"), Fixture.Data()->IsScoringPaused());
    Fixture.Component->ConfigureAutoResume(true, true);
    TStrongObjectPtr<UCommonAIVendorEventRecorder> Recorder(NewObject<UCommonAIVendorEventRecorder>());
    Fixture.Component->OnTaskEnded.AddDynamic(Recorder.Get(), &UCommonAIVendorEventRecorder::OnEnded);
    B->CompleteNaturally();
    TestEqual(TEXT("Auto-rescore path broadcasts terminal delegate twice"), Recorder->EndCount, 2);
    Fixture.Component->ConfigureAutoResume(false, false);
    Fixture.Component->ConfigureAutoPossession(true);
    Fixture.Controller->UnPossess();
    Fixture.Controller->Possess(Fixture.World->SpawnActor<APawn>());
    FAAIConsideration AfterPossession;
    Fixture.Data()->GetConsiderationBP(Input_X, AfterPossession);
    TestEqual(TEXT("Installed possession path retains old raw input"), AfterPossession.RawValue, 40.0f);
    TestEqual(TEXT("Installed possession path retains old range"), AfterPossession.MinValue, 10.0f);
    AddInfo(TEXT("ForceTaskStart is class-wide and bypasses interruptibility; auto-rescore can duplicate terminal events; installed possession retains old raw values/ranges."));
    return true;
}

#include "CommonAIProfile.h"
#include "CommonAIAgentComponent.h"
#include "Execution/CommonAIExecution.h"
#include "Evaluation/CommonAIEvaluator.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonAIGeneralDefinitionsTest, "CommonAI.General.Definitions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommonAIGeneralDefinitionsTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UCommonAIProfile> Profile(NewObject<UCommonAIProfile>());
    TestFalse(TEXT("An empty general profile is invalid"), Profile->ValidateProfile().IsEmpty());
    FCommonAIConsiderationDefinition Input;
    Input.Id = CommonAITags::Consideration_WorkAvailable;
    Input.SourceId = CommonAITags::Source_External;
    Input.bHasInitialValue = true;
    Profile->Considerations.Add(Input);
    UCurveTable* Curves = NewObject<UCurveTable>(Profile.Get());
    FRichCurve& Curve = Curves->AddRichCurve(TEXT("Availability"));
    Curve.AddKey(0.0f, 0.0f);
    Curve.AddKey(1.0f, 1.0f);
    FCommonAITaskDefinition Task;
    Task.Activity = CommonAITags::Activity_Work;
    FCommonAICurveBinding Binding;
    Binding.Consideration = Input.Id;
    Binding.Curve.CurveTable = Curves;
    Binding.Curve.RowName = TEXT("Availability");
    Task.Considerations.Add(Binding);
    FCommonAIActionDefinition First;
    First.Id = CommonAITags::Action_Wait;
    First.Kind = ECommonAIActionKind::Wait;
    First.DurationSeconds = 0.1f;
    FCommonAIActionDefinition Second = First;
    Second.EntryId = FGuid::NewGuid();
    Task.Actions = { First, Second };
    Profile->Tasks.Add(Task);
    TestTrue(TEXT("Repeated action type with distinct occurrences is valid"), Profile->ValidateProfile().IsEmpty());
    Profile->Tasks[0].Actions[1].EntryId = First.EntryId;
    TestFalse(TEXT("Duplicate occurrence identity is rejected"), Profile->ValidateProfile().IsEmpty());
    Profile->Tasks[0].Actions[1].EntryId = Second.EntryId;
    Profile->Considerations[0].Maximum = Profile->Considerations[0].Minimum;
    TestFalse(TEXT("Zero-width normalization range is rejected"), Profile->ValidateProfile().IsEmpty());
    Profile->Considerations[0].Maximum = 1.0f;
    Profile->Considerations[0].Route = ECommonAIInputRoute::Evaluator;
    Profile->Considerations[0].Evaluator = NewObject<UCommonAINumericEvaluator>(Profile.Get());
    FCommonAIServiceDefinition Service;
    Service.Id = CommonAITags::Action_Execute;
    Service.EvaluatedInputs.AddTag(Input.Id);
    FCommonAIServiceCondition Lower;
    Lower.Consideration = Input.Id;
    Lower.NormalizedThreshold = 0.2f;
    FCommonAIServiceCondition Upper = Lower;
    Upper.Comparison = ECommonAIComparison::Less;
    Upper.NormalizedThreshold = 0.8f;
    Service.Conditions = { Lower, Upper };
    Profile->Services.Add(Service);
    TestTrue(TEXT("Duplicate condition inputs cannot silently lose a constraint"), Profile->ValidateProfile().ContainsByPredicate(
        [](const FString& Error) { return Error.Contains(TEXT("only one comparison per input")); }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonAIGeneralExecutionTest, "CommonAI.General.ExecutionContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommonAIGeneralExecutionTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UCommonAIAgentComponent> Agent(NewObject<UCommonAIAgentComponent>());
    TStrongObjectPtr<UCommonAIWaitExecution> Execution(NewObject<UCommonAIWaitExecution>());
    FCommonAIExecutionContext Context;
    Context.Agent = Agent.Get();
    Context.Request.Execution = FGuid::NewGuid();
    Context.Invocation = FGuid::NewGuid();
    Execution->DurationSeconds = 1.0f;
    TestTrue(TEXT("A definition instance starts"), Execution->BeginExecution(Context));
    Execution->TickExecution(0.5f);
    TestEqual(TEXT("Finite wait remains running before its duration"), Execution->GetResult().Status, ECommonAIInteractionStatus::Running);
    Execution->CancelExecution(CommonAITags::Reason_Disabled);
    TestEqual(TEXT("Wait acknowledges cancellation"), Execution->GetResult().Status, ECommonAIInteractionStatus::Cancelled);
    const FGuid OldInvocation = Context.Invocation;
    Context.Invocation = FGuid::NewGuid();
    TestTrue(TEXT("Same instance can execute again"), Execution->BeginExecution(Context));
    Agent->AddTagContribution(Execution.Get(), FGameplayTagContainer(CommonAITags::State_Busy_Working));
    const FGuid OtherOwner = Agent->AddTagContribution(Agent.Get(), FGameplayTagContainer(CommonAITags::State_Busy_Working));
    TestFalse(TEXT("Old callback cannot complete new execution"), Execution->CompleteExecution(OldInvocation, ECommonAIInteractionStatus::Completed, {}));
    Execution->TickExecution(1.0f);
    TestEqual(TEXT("New execution completes normally"), Execution->GetResult().Status, ECommonAIInteractionStatus::Completed);
    TestFalse(TEXT("Completion is terminal and unique"), Execution->CompleteExecution(Context.Invocation, ECommonAIInteractionStatus::Failed, {}));
    Execution->ReleaseContext();
    TestTrue(TEXT("Execution cleanup preserves another source's tag"), Agent->GetEffectiveTags().HasTag(CommonAITags::State_Busy));
    Agent->RemoveTagContribution(Agent.Get(), OtherOwner);
    TestFalse(TEXT("Reused execution cannot leak its tag contribution"), Agent->GetEffectiveTags().HasTag(CommonAITags::State_Busy));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonAIGeneralEvaluationTest, "CommonAI.General.Evaluation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommonAIGeneralEvaluationTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UCommonAINumericEvaluator> Numeric(NewObject<UCommonAINumericEvaluator>());
    TStrongObjectPtr<UCommonAIDistanceEvaluator> Distance(NewObject<UCommonAIDistanceEvaluator>());
    FCommonAINumericContext Number;
    Number.Value = 0.25f;
    const FInstancedStruct NumberContext = FInstancedStruct::Make(Number);
    TestEqual(TEXT("Numeric context produces raw value"), Numeric->Evaluate(NumberContext).RawValue, 0.25f);
    TestEqual(TEXT("Wrong context type remains unavailable"), Distance->Evaluate(NumberContext).Status, ECommonAIValueStatus::Unavailable);
    FCommonAILocationContext Locations;
    Locations.Target = FVector(3.0f, 4.0f, 0.0f);
    TestEqual(TEXT("Distance evaluator uses the typed locations"), Distance->Evaluate(FInstancedStruct::Make(Locations)).RawValue, 5.0f);
    Number.Value = std::numeric_limits<float>::infinity();
    TestEqual(TEXT("Nonfinite input cannot become a valid score"), Numeric->Evaluate(FInstancedStruct::Make(Number)).Status, ECommonAIValueStatus::Unavailable);
    return true;
}
#include "Fixtures/CommonAIProfileFixture.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonAIGeneralSelectionTest, "CommonAI.General.SelectionAndOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommonAIGeneralSelectionTest::RunTest(const FString& Parameters)
{
    FWorld Fixture;
    UCommonAIAgentComponent* Agent = NewObject<UCommonAIAgentComponent>(Fixture.Controller);
    Agent->bStartAutomatically = false;
    Agent->Profile = CommonAITestProfiles::MakeSelection(Agent);
    Agent->RegisterComponent();
    Agent->RegisterInputSource(CommonAITags::Source_External, Agent);
    if (!TestTrue(TEXT("General profile starts"), Agent->StartAgent())) return false;
    Fixture.Tick(0.5f);
    const FCommonAIAgentSnapshot Initial = Agent->GetSnapshot();
    const FCommonAITaskSnapshot* Farming = Initial.Tasks.FindByPredicate([](const auto& Task) { return Task.Activity == CommonAITags::Activity_Work_Farming; });
    if (!TestNotNull(TEXT("All configured tasks are visible"), Farming)) return false;
    TestEqual(TEXT("CommonAI corrects the count factor without replacing the scorer"), Farming->Score, 0.390625f);
    TestEqual(TEXT("Normal selection uses the corrected score"), Initial.Activity, CommonAITags::Activity_Work_Crafting.GetTag());
    FCommonAIInputUpdate Update;
    Update.Id = CommonAITags::Consideration_DistanceTarget;
    Update.RawValue = 1.0f;
    const FGuid Generation = Agent->GetInputGeneration();
    TestFalse(TEXT("An unbound source cannot overwrite a numeric input"), Agent->ApplyInputUpdates(Fixture.Controller, Generation, { Update }));
    TestTrue(TEXT("The bound source can submit an update"), Agent->ApplyInputUpdates(Agent, Generation, { Update }));
    TestFalse(TEXT("Stale input generation is rejected"), Agent->ApplyInputUpdates(Agent, FGuid::NewGuid(), { Update }));
    Fixture.Tick(0.75f);
    TestEqual(TEXT("Updated input changes normal selection"), Agent->GetSnapshot().Activity, CommonAITags::Activity_Work_Farming.GetTag());
    Update.bOverrideRange = true;
    Update.Minimum = 2.0f;
    Update.Maximum = 2.0f;
    TestFalse(TEXT("Invalid batch publishes nothing"), Agent->ApplyInputUpdates(Agent, Generation, { Update }));
    FCommonAIConsiderationSnapshot Value;
    Agent->GetConsideration(Update.Id, Value);
    TestEqual(TEXT("Rejected range leaves authoritative value intact"), Value.RawValue, 1.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonAIGeneralCommandTest, "CommonAI.General.CommandPauseOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommonAIGeneralCommandTest::RunTest(const FString& Parameters)
{
    FWorld Fixture;
    UCommonAIAgentComponent* Agent = NewObject<UCommonAIAgentComponent>(Fixture.Controller);
    Agent->bStartAutomatically = false;
    Agent->Profile = CommonAITestProfiles::MakeSelection(Agent);
    Agent->Profile->Tasks[0].WaitDurationSeconds = 0.5f;
    Agent->RegisterComponent();
    if (!Agent->StartAgent()) return false;
    Fixture.Tick(0.5f);
    const FGuid ManualPause = Agent->PauseScoring(Fixture.Controller, CommonAITags::Reason_Disabled);
    const FGuid Command = Agent->RequestActivity(CommonAITags::Activity_Work_Farming);
    TestTrue(TEXT("Command has stable identity"), Command.IsValid());
    Fixture.Tick(0.25f);
    TestEqual(TEXT("Exact activity is selected among same native adapter class"), Agent->GetSnapshot().Activity, CommonAITags::Activity_Work_Farming.GetTag());
    Fixture.Tick(0.75f);
    TestEqual(TEXT("Command completes normally"), Agent->GetSnapshot().Command.Status, ECommonAICommandStatus::Completed);
    TestTrue(TEXT("Command completion preserves another source's pause"), Agent->GetSnapshot().bScoringPaused);
    TestFalse(TEXT("Wrong source cannot remove pause"), Agent->ResumeScoring(Agent, ManualPause));
    TestTrue(TEXT("Pause owner can release it"), Agent->ResumeScoring(Fixture.Controller, ManualPause));
    Fixture.Tick(0.5f);
    TestFalse(TEXT("Scoring resumes after the last hold"), Agent->GetSnapshot().bScoringPaused);
    return true;
}
#include "Fixtures/CommonAIGeneralFixtures.h"
#include "Integration/CommonAIBehaviorTreeExecution.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonAIGeneralServicesTest, "CommonAI.General.ServicesAndEvaluation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommonAIGeneralServicesTest::RunTest(const FString& Parameters)
{
    FWorld Fixture;
    UCommonAIAgentComponent* Agent = NewObject<UCommonAIAgentComponent>(Fixture.Controller);
    Agent->bStartAutomatically = false;
    Agent->Profile = CommonAITestProfiles::MakeSelection(Agent);
    UCommonAIProfile* Profile = Agent->Profile;
    Profile->ScoringMode = ECommonAIScoringMode::OnInputChange;
    Profile->ObservationIntervalSeconds = 0.0f;
    TStrongObjectPtr<UCommonAIProbeObservations> Provider(NewObject<UCommonAIProbeObservations>());
    FCommonAINumericContext Number;
    Number.Value = 0.8f;
    Provider->Values.Add(CommonAITags::Consideration_DistanceTarget, FInstancedStruct::Make(Number));
    Profile->Considerations[1].Route = ECommonAIInputRoute::Evaluator;
    Profile->Considerations[1].bHasInitialValue = false;
    Profile->Considerations[1].Evaluator = NewObject<UCommonAINumericEvaluator>(Profile);
    FCommonAIServiceDefinition Service;
    Service.Id = CommonAITags::Action_Execute;
    Service.IntervalSeconds = ShortIntervalSeconds;
    Service.EvaluatedInputs.AddTag(CommonAITags::Consideration_DistanceTarget);
    Profile->Services.Add(Service);
    Agent->RegisterComponent();
    Agent->RegisterInputSource(CommonAITags::Source_External, Provider.Get());
    if (!TestTrue(TEXT("Evaluator/service profile starts"), Agent->StartAgent())) return false;
    Fixture.Tick(0.5f);
    FCommonAIConsiderationSnapshot Value;
    Agent->GetConsideration(CommonAITags::Consideration_DistanceTarget, Value);
    TestEqual(TEXT("Typed source produces a valid evaluator value"), Value.Status, ECommonAIValueStatus::Value);
    TestEqual(TEXT("Evaluator result reaches native data manager"), Value.RawValue, 0.8f);
    TestTrue(TEXT("Unconditional service is explicitly started"), Agent->GetSnapshot().Services[0].bRunning);
    Provider->Values.Reset();
    Fixture.Tick(0.5f);
    Agent->GetConsideration(CommonAITags::Consideration_DistanceTarget, Value);
    TestEqual(TEXT("Unavailable source is fail-closed"), Value.Status, ECommonAIValueStatus::Unavailable);
    Agent->StopAgent();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonAIGeneralAtomicServicesTest, "CommonAI.General.AtomicServiceInputs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommonAIGeneralAtomicServicesTest::RunTest(const FString& Parameters)
{
    FWorld Fixture;
    UCommonAIAgentComponent* Agent = NewObject<UCommonAIAgentComponent>(Fixture.Controller);
    Agent->bStartAutomatically = false;
    Agent->Profile = CommonAITestProfiles::MakeSelection(Agent);
    Agent->Profile->bPauseAffectsServices = false;
    Agent->Profile->Considerations[0].InitialValue = 0.2f;
    Agent->Profile->Considerations[1].InitialValue = 0.8f;
    TStrongObjectPtr<UCommonAIProbeServiceRecorder> Recorder(NewObject<UCommonAIProbeServiceRecorder>());
    UCommonAIProbeServiceBehavior* Behavior = NewObject<UCommonAIProbeServiceBehavior>(Agent->Profile);
    Behavior->Recorder = Recorder.Get();
    Behavior->WatchedInput = CommonAITags::Consideration_WorkAvailable;
    FCommonAIServiceDefinition Service;
    Service.Id = CommonAITags::Action_Execute;
    Service.IntervalSeconds = ShortIntervalSeconds;
    Service.Behavior = Behavior;
    for (const FCommonAIConsiderationDefinition& Input : Agent->Profile->Considerations)
    {
        FCommonAIServiceCondition Condition;
        Condition.Consideration = Input.Id;
        Condition.NormalizedThreshold = 0.5f;
        Service.Conditions.Add(Condition);
    }
    Agent->Profile->Services.Add(Service);
    Agent->RegisterComponent();
    Agent->RegisterInputSource(CommonAITags::Source_External, Agent);
    if (!Agent->StartAgent()) return false;
    FCommonAIInputUpdate X;
    X.Id = CommonAITags::Consideration_WorkAvailable;
    X.RawValue = 0.8f;
    FCommonAIInputUpdate Y;
    Y.Id = CommonAITags::Consideration_DistanceTarget;
    Y.RawValue = 0.2f;
    TestTrue(TEXT("Coherent batch accepted"), Agent->ApplyInputUpdates(Agent, Agent->GetInputGeneration(), { X, Y }));
    Fixture.Tick(0.2f);
    TestEqual(TEXT("Mixed intermediate values cannot start the service"), Recorder->Starts, 0);
    Y.RawValue = 0.8f;
    Agent->ApplyInputUpdates(Agent, Agent->GetInputGeneration(), { Y });
    Fixture.Tick(0.2f);
    TestEqual(TEXT("Final valid conditions start once"), Recorder->Starts, 1);
    TestEqual(TEXT("Start callback observes published values"), Recorder->ValueOnStart, 0.8f);
    TestTrue(TEXT("Service tick runs through the vendor timer"), Recorder->Ticks > 0);
    Agent->StopAgent();
    Fixture.Tick(0.5f);
    TestEqual(TEXT("Service stops during shutdown"), Recorder->Stops, 1);
    X.RawValue = 0.2f;
    Agent->ApplyInputUpdates(Agent, Agent->GetInputGeneration(), { X }, false);
    X.RawValue = 0.8f;
    Agent->ApplyInputUpdates(Agent, Agent->GetInputGeneration(), { X }, false);
    const int32 StoppedTicks = Recorder->Ticks;
    Fixture.Tick(0.2f);
    TestEqual(TEXT("Disabled input updates cannot restart services"), Recorder->Starts, 1);
    TestEqual(TEXT("Disabled services cannot resume ticking"), Recorder->Ticks, StoppedTicks);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonAIBehaviorTreeTest, "CommonAI.General.BehaviorTreeSafeStop",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommonAIBehaviorTreeTest::RunTest(const FString& Parameters)
{
    FWorld Fixture(true);
    UCommonAIAgentComponent* Agent = NewObject<UCommonAIAgentComponent>(Fixture.Controller);
    Agent->bStartAutomatically = false;
    Agent->Profile = CommonAITestProfiles::MakeSelection(Agent);
    Agent->Profile->Tasks.SetNum(1);
    FCommonAITaskDefinition& Task = Agent->Profile->Tasks[0];
    Task.Kind = ECommonAITaskKind::BehaviorTree;
    Task.TickIntervalSeconds = ShortIntervalSeconds;
    UBehaviorTree* Tree = NewObject<UBehaviorTree>(Agent->Profile);
    UBTComposite_Sequence* Root = NewObject<UBTComposite_Sequence>(Tree);
    UBTTask_CommonAIProbeLatentAbort* Wait = NewObject<UBTTask_CommonAIProbeLatentAbort>(Root);
    FBTCompositeChild Child;
    Child.ChildTask = Wait;
    Root->Children.Add(Child);
    Tree->RootNode = Root;
    Task.BehaviorTree = Tree;
    Agent->RegisterComponent();
    if (!TestTrue(TEXT("BT profile starts"), Agent->StartAgent())) return false;
    Fixture.Tick(0.5f);
    UBehaviorTreeComponent* Brain = Cast<UBehaviorTreeComponent>(Fixture.Controller->GetBrainComponent());
    if (!TestNotNull(TEXT("Controller owns the explicitly selected engine BT"), Brain)) return false;
    TestTrue(TEXT("BT is executing"), Brain->TreeHasBeenStarted());
    Agent->StopAgent();
    Fixture.Tick(0.1f);
    TestTrue(TEXT("Safe stop waits for latent abort"), Brain->IsAbortPending());
    TestTrue(TEXT("Agent stays enabled while draining"), Agent->GetSnapshot().bEnabled);
    Fixture.Tick(0.5f);
    TestFalse(TEXT("Tree is stopped after acknowledgement"), Brain->TreeHasBeenStarted());
    TestFalse(TEXT("Agent disables after the real safe stop"), Agent->GetSnapshot().bEnabled);
    TestEqual(TEXT("Abort completes as domain cancellation"), Agent->GetSnapshot().LastResult.Status, ECommonAIInteractionStatus::Cancelled);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonAICurveAuthoringTest, "CommonAI.General.CurveAuthoring",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommonAICurveAuthoringTest::RunTest(const FString& Parameters)
{
#if WITH_EDITOR
    const FString PackageName = TEXT("/CommonAI/Profiles/TransientCurveProbe_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UPackage* Package = CreatePackage(*PackageName);
    TStrongObjectPtr<UCommonAIProfile> Profile(CommonAITestProfiles::MakeSelection(Package));
    UCurveTable* Table = NewObject<UCurveTable>(Package);
    FRichCurve& Existing = Table->AddRichCurve(TEXT("Authored"));
    Existing.AddKey(0.0f, 0.3f);
    Existing.AddKey(1.0f, 0.7f);
    const FRichCurve Before = Existing;
    Profile->AuthoringCurves = Table;
    Profile->Tasks[0].Considerations[0].Curve.CurveTable = Table;
    Profile->Tasks[0].Considerations[0].Curve.RowName = TEXT("Authored");
    Profile->Tasks[0].Considerations[1].Curve = {};
    Profile->GenerateMissingCurves();
    TestTrue(TEXT("Authored curve is preserved"), *Table->FindRichCurve(TEXT("Authored"), TEXT("Test")) == Before);
    const FCurveTableRowHandle Generated = Profile->Tasks[0].Considerations[1].Curve;
    TestTrue(TEXT("Missing pair obtains a generated row"), Generated.CurveTable == Table && Generated.IsValid(TEXT("Test")));
    const int32 Rows = Table->GetRowMap().Num();
    Profile->GenerateMissingCurves();
    TestEqual(TEXT("Generation is idempotent"), Table->GetRowMap().Num(), Rows);
    TStrongObjectPtr<UCommonAIProfile> Example(NewObject<UCommonAIProfile>(Package));
    Example->AuthoringCurves = NewObject<UCurveTable>(Package);
    Example->CreateGeneralExample();
    TestTrue(TEXT("Starter authoring creates a valid general profile"), Example->ValidateProfile().IsEmpty());
    TestEqual(TEXT("Starter contains competing tasks"), Example->Tasks.Num(), 2);
    TestEqual(TEXT("Starter includes an evaluator service"), Example->Services.Num(), 1);
    Example->CreateGeneralExample();
    TestEqual(TEXT("Starter cannot overwrite existing definitions"), Example->Tasks.Num(), 2);
    Package->SetDirtyFlag(false);
#endif
    return true;
}
#include "Contracts/CommonAIWorkSite.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonAIBindingPayloadTest, "CommonAI.General.BindingPayload",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommonAIBindingPayloadTest::RunTest(const FString& Parameters)
{
    FWorld Fixture;
    UCommonAIAgentComponent* Agent = NewObject<UCommonAIAgentComponent>(Fixture.Controller);
    Agent->bStartAutomatically = false;
    Agent->Profile = CommonAITestProfiles::MakeSelection(Agent);
    FCommonAITaskDefinition& Task = Agent->Profile->Tasks[0];
    Task.Kind = ECommonAITaskKind::Sequence;
    Task.bCommandOnly = true;
    Task.Considerations.Reset();
    auto Add = [&Task](ECommonAIActionKind Kind, FGameplayTag Id)
    {
        FCommonAIActionDefinition& Step = Task.Actions.AddDefaulted_GetRef();
        Step.Kind = Kind;
        Step.Id = Id;
        Step.BindingId = CommonAITags::Capability_Work;
    };
    Add(ECommonAIActionKind::ResolveBinding, CommonAITags::Action_ResolveBinding);
    Add(ECommonAIActionKind::Reserve, CommonAITags::Action_Reserve);
    Add(ECommonAIActionKind::Interact, CommonAITags::Action_Interact);
    Add(ECommonAIActionKind::Release, CommonAITags::Action_Release);
    AActor* Target = Fixture.World->SpawnActor<AActor>();
    UCommonAISignaledWorkSite* Site = NewObject<UCommonAISignaledWorkSite>(Target);
    Site->Activities.AddTag(CommonAITags::Activity_Work_Farming);
    Site->RegisterComponent();
    TStrongObjectPtr<UCommonAIProbeInteractionRecorder> Recorder(NewObject<UCommonAIProbeInteractionRecorder>());
    Site->OnInteractionStarted.AddDynamic(Recorder.Get(), &UCommonAIProbeInteractionRecorder::OnStarted);
    Agent->OnActivityProgress.AddDynamic(Recorder.Get(), &UCommonAIProbeInteractionRecorder::OnProgress);
    Agent->RegisterComponent();
    Agent->RegisterExecutionBinding(CommonAITags::Capability_Work, Site);
    if (!Agent->StartAgent()) return false;
    Fixture.Tick(0.5f);
    FCommonAINumericContext Number;
    Number.Value = 17.0f;
    FInstancedStruct Payload = FInstancedStruct::Make(Number);
    const FGuid Command = Agent->RequestActivityWithContext(CommonAITags::Activity_Work_Farming, Payload,
        ECommonAITransitionMode::AfterNaturalCompletion);
    Payload.GetMutable<FCommonAINumericContext>().Value = 99.0f;
    Fixture.Tick(2.0f);
    TestTrue(TEXT("Typed command is tracked"), Command.IsValid());
    TestEqual(TEXT("Binding executes once after the previous activity ends"), Recorder->Starts, 1);
    TestTrue(TEXT("Activity action/state changes are observable"), Recorder->ProgressCount > 0);
    const FCommonAINumericContext* Captured = Recorder->Request.Payload.GetPtr<FCommonAINumericContext>();
    if (!TestNotNull(TEXT("Target receives typed command context"), Captured)) return false;
    TestEqual(TEXT("Queued command owns a snapshot of its payload"), Captured->Value, 17.0f);
    TestTrue(TEXT("Interaction has its own operation identity"), Recorder->Request.Operation.IsValid());
    Site->SignalCompletion(Recorder->Request, true);
    Fixture.Tick(0.5f);
    TestEqual(TEXT("External completion finishes the command"), Agent->GetSnapshot().Command.Status, ECommonAICommandStatus::Completed);
    TestTrue(TEXT("Binding replacement checks expected identity"),
        Agent->UpdateExecutionBinding(CommonAITags::Capability_Work, Site, nullptr));
    return true;
}
#include "Diagnostics/CommonAIInspectorWidget.h"
#include "Components/Button.h"
#include "Widgets/SWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonAIInspectorControlsTest, "CommonAI.General.InspectorControls",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommonAIInspectorControlsTest::RunTest(const FString& Parameters)
{
    FWorld Fixture;
    UCommonAIAgentComponent* Agent = NewObject<UCommonAIAgentComponent>(Fixture.Controller);
    Agent->bStartAutomatically = false;
    Agent->Profile = CommonAITestProfiles::MakeSelection(Agent);
    Agent->RegisterComponent();
    if (!Agent->StartAgent()) return false;
    Fixture.Tick(0.5f);
    TStrongObjectPtr<UCommonAIInspectorWidget> Widget(CreateWidget<UCommonAIInspectorWidget>(Fixture.World, UCommonAIInspectorWidget::StaticClass()));
    if (!TestNotNull(TEXT("Native inspector is available without a vendor widget"), Widget.Get())) return false;
    Widget->InspectActor(Fixture.Controller);
    TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();
    UButton* Pause = Cast<UButton>(Widget->GetWidgetFromName(TEXT("PauseButton")));
    if (!TestNotNull(TEXT("Inspector exposes pause control"), Pause)) return false;
    TestTrue(TEXT("Inspector reports actual task identities"), Widget->GetInspectionText().Contains(CommonAITags::Activity_Work_Farming.GetTag().ToString()));
    TestTrue(TEXT("Inspector reports the corrected native score"), Widget->GetInspectionText().Contains(TEXT("0.391")));
    const FGuid ExternalHold = Agent->PauseScoring(Fixture.Controller, CommonAITags::Reason_Disabled);
    Pause->OnClicked.Broadcast();
    Pause->OnClicked.Broadcast();
    TestTrue(TEXT("Debug control does not release an external pause"), Agent->GetSnapshot().bScoringPaused);
    Agent->ResumeScoring(Fixture.Controller, ExternalHold);
    TestFalse(TEXT("Scoring resumes after all owners release"), Agent->GetSnapshot().bScoringPaused);
    Widget->ReleasePause();
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonAIStaleEvaluatorTest, "CommonAI.General.StaleEvaluator",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommonAIStaleEvaluatorTest::RunTest(const FString& Parameters)
{
    FWorld Fixture;
    UCommonAIAgentComponent* Agent = NewObject<UCommonAIAgentComponent>(Fixture.Controller);
    Agent->bStartAutomatically = false;
    Agent->Profile = CommonAITestProfiles::MakeSelection(Agent);
    auto& Input = Agent->Profile->Considerations[1];
    Input.Route = ECommonAIInputRoute::Evaluator;
    Input.bHasInitialValue = false;
    Input.Evaluator = NewObject<UCommonAINumericEvaluator>(Agent->Profile);
    FCommonAIServiceDefinition Service;
    Service.Id = CommonAITags::Action_Execute;
    Service.IntervalSeconds = 0.0f;
    Service.EvaluatedInputs.AddTag(Input.Id);
    Agent->Profile->Services.Add(Service);
    TStrongObjectPtr<UCommonAIProbeObservations> Provider(NewObject<UCommonAIProbeObservations>());
    FCommonAINumericContext Number;
    Number.Value = 0.8f;
    Provider->Values.Add(Input.Id, FInstancedStruct::Make(Number));
    Provider->bUnregisterDuringRead = true;
    Agent->RegisterComponent();
    Agent->RegisterInputSource(CommonAITags::Source_External, Provider.Get());
    if (!Agent->StartAgent()) return false;
    Fixture.Tick(0.5f);
    FCommonAIConsiderationSnapshot Value;
    Agent->GetConsideration(Input.Id, Value);
    TestEqual(TEXT("Queued result cannot revive an unregistered producer"), Value.Status, ECommonAIValueStatus::Unavailable);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonAIReentrantReservationTest, "CommonAI.General.ReentrantReservation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommonAIReentrantReservationTest::RunTest(const FString& Parameters)
{
    FWorld Fixture;
    UCommonAIAgentComponent* Agent = NewObject<UCommonAIAgentComponent>(Fixture.Controller);
    Agent->bStartAutomatically = false;
    Agent->Profile = CommonAITestProfiles::MakeWork(Agent);
    Agent->RegisterComponent();
    AActor* Target = Fixture.World->SpawnActor<AActor>();
    UCommonAIProbeReentrantSite* Site = NewObject<UCommonAIProbeReentrantSite>(Target);
    Site->Agent = Agent;
    Site->RegisterComponent();
    TStrongObjectPtr<UCommonAIProbeInteractionRecorder> Recorder(NewObject<UCommonAIProbeInteractionRecorder>());
    Site->OnInteractionStarted.AddDynamic(Recorder.Get(), &UCommonAIProbeInteractionRecorder::OnStarted);
    Agent->AddInteractionTarget(Site);
    if (!Agent->StartAgent()) return false;
    Fixture.Tick(1.0f);
    TestFalse(TEXT("Stop during acquisition reaches disabled state"), Agent->GetSnapshot().bEnabled);
    TestEqual(TEXT("Cancelled acquisition does not start the effect"), Recorder->Starts, 0);
    FCommonAIInteractionRequest Query;
    Query.Agent = Fixture.Controller;
    Query.Activity = CommonAITags::Activity_Work;
    TestTrue(TEXT("Reentrant cancellation releases the newly acquired lease"), ICommonAIInteraction::Execute_QueryInteraction(Site, Query).bAvailable);

    FWorld ContentionFixture;
    ACharacter* Character = ContentionFixture.World->SpawnActor<ACharacter>();
    ContentionFixture.Controller->Possess(Character);
    Character->GetCharacterMovement()->DisableMovement();
    UCommonAIAgentComponent* RetryingAgent = NewObject<UCommonAIAgentComponent>(ContentionFixture.Controller);
    RetryingAgent->bStartAutomatically = false;
    RetryingAgent->Profile = CommonAITestProfiles::MakeWork(RetryingAgent);
    RetryingAgent->Profile->ScoringMode = ECommonAIScoringMode::Manual;
    RetryingAgent->Profile->bRescoreAfterCompletion = false;
    // A normal authored wait establishes a deterministic race window between the production Find and Reserve steps.
    constexpr float ContentionWindowSeconds = 0.2f;
    FCommonAIActionDefinition ContentionWindow;
    ContentionWindow.Kind = ECommonAIActionKind::Wait;
    ContentionWindow.Id = CommonAITags::Action_Wait;
    ContentionWindow.DurationSeconds = ContentionWindowSeconds;
    ContentionWindow.TickIntervalSeconds = FrameSeconds;
    RetryingAgent->Profile->Tasks[0].Actions.Insert(ContentionWindow, 1);
    RetryingAgent->RegisterComponent();
    auto MakeSite = [&ContentionFixture]()
    {
        AActor* Owner = ContentionFixture.World->SpawnActor<AActor>();
        USceneComponent* Root = NewObject<USceneComponent>(Owner);
        Owner->SetRootComponent(Root);
        Root->RegisterComponent();
        UCommonAISignaledWorkSite* NewSite = NewObject<UCommonAISignaledWorkSite>(Owner);
        NewSite->bWaitForSafeCancellation = false;
        NewSite->RegisterComponent();
        return NewSite;
    };
    UCommonAISignaledWorkSite* FirstSite = MakeSite();
    UCommonAISignaledWorkSite* SecondSite = MakeSite();
    RetryingAgent->AddInteractionTarget(FirstSite);
    RetryingAgent->AddInteractionTarget(SecondSite);
    RetryingAgent->GetProvider()->SetActivityTargets(CommonAITags::Activity_Work, { FirstSite, SecondSite });
    TStrongObjectPtr<UCommonAIProbeInteractionRecorder> RetryRecorder(NewObject<UCommonAIProbeInteractionRecorder>());
    SecondSite->OnInteractionStarted.AddDynamic(RetryRecorder.Get(), &UCommonAIProbeInteractionRecorder::OnStarted);
    if (!TestTrue(TEXT("The contended production sequence starts"), RetryingAgent->StartAgent())) return false;
    if (!TestTrue(TEXT("Ordinary utility scoring selects the contended task"), RetryingAgent->Rescore())) return false;
    constexpr float MaximumPipelineSeconds = 2.0f;
    const int32 MaximumPipelineFrames = FMath::CeilToInt(MaximumPipelineSeconds / FrameSeconds);
    FCommonAIAgentSnapshot Selected;
    for (int32 Frame = 0; Frame < MaximumPipelineFrames; ++Frame)
    {
        Selected = RetryingAgent->GetSnapshot();
        if (Selected.Target.Get() == FirstSite && Selected.CurrentAction == CommonAITags::Action_Wait) break;
        ContentionFixture.Tick(FrameSeconds);
    }
    if (!TestTrue(TEXT("Find selects the first ordered point before reservation"), Selected.Target.Get() == FirstSite
        && Selected.CurrentAction == CommonAITags::Action_Wait && !Selected.Interaction.Operation.IsValid())) return false;
    FCommonAIInteractionRequest CompetingLease;
    CompetingLease.Agent = ContentionFixture.World->SpawnActor<AAIController>();
    CompetingLease.Execution = FGuid::NewGuid();
    CompetingLease.Operation = FGuid::NewGuid();
    CompetingLease.Generation = FGuid::NewGuid();
    CompetingLease.Activity = CommonAITags::Activity_Work;
    if (!TestTrue(TEXT("A competing execution reserves the selected point before Reserve runs"),
        ICommonAIInteraction::Execute_ReserveInteraction(FirstSite, CompetingLease))) return false;
    auto DescribeSnapshot = [](const FCommonAIAgentSnapshot& Snapshot)
    {
        return FString::Printf(TEXT("phase=%s action=%s target=%s result=%s reason=%s execution=%s operation=%s"),
            *UEnum::GetValueAsString(Snapshot.Phase), *Snapshot.CurrentAction.ToString(), *GetNameSafe(Snapshot.Target.Get()),
            *UEnum::GetValueAsString(Snapshot.LastResult.Status), *Snapshot.LastResult.Reason.ToString(),
            *Snapshot.Execution.ToString(EGuidFormats::Short), *Snapshot.Interaction.Operation.ToString(EGuidFormats::Short));
    };
    constexpr int32 MaximumPipelineTraceEntries = 12;
    TArray<FString> PipelineTrace;
    FCommonAIInteractionRequest LastReservationRequest;
    auto RecordPipelineState = [&]()
    {
        const FCommonAIAgentSnapshot Snapshot = RetryingAgent->GetSnapshot();
        if (Snapshot.Interaction.Operation.IsValid()) LastReservationRequest = Snapshot.Interaction;
        const FString Description = DescribeSnapshot(Snapshot);
        if (PipelineTrace.Num() < MaximumPipelineTraceEntries && (PipelineTrace.IsEmpty() || PipelineTrace.Last() != Description))
            PipelineTrace.Add(Description);
    };
    RecordPipelineState();
    for (int32 Frame = 0; Frame < MaximumPipelineFrames && RetryRecorder->Starts == 0; ++Frame)
    {
        ContentionFixture.Tick(FrameSeconds);
        RecordPipelineState();
    }
    const FCommonAIAgentSnapshot Retried = RetryingAgent->GetSnapshot();
    const bool bRetryReachedInteraction = RetryRecorder->Starts == 1 && Retried.Execution == Selected.Execution
        && Retried.Target.Get() == SecondSite && Retried.Phase == ECommonAIPhase::Interacting;
    TestEqual(TEXT("The second point begins exactly once"), RetryRecorder->Starts, 1);
    TestTrue(TEXT("The same production execution acquires the second ordered point"), Retried.Execution == Selected.Execution
        && Retried.Target.Get() == SecondSite && Retried.Phase == ECommonAIPhase::Interacting);
    TestEqual(TEXT("Retry preserves the first point's competing lease"),
        ICommonAIInteraction::Execute_GetInteractionResult(FirstSite, CompetingLease).Status, ECommonAIInteractionStatus::Reserved);
    SecondSite->SignalCompletion(RetryRecorder->Request, true);
    for (int32 Frame = 0; Frame < MaximumPipelineFrames
        && RetryingAgent->GetSnapshot().LastResult.Status != ECommonAIInteractionStatus::Completed; ++Frame)
    {
        ContentionFixture.Tick(FrameSeconds);
        RecordPipelineState();
    }
    TestEqual(TEXT("The retry finishes through the production Release step"),
        RetryingAgent->GetSnapshot().LastResult.Status, ECommonAIInteractionStatus::Completed);
    FCommonAIInteractionRequest RetryQuery;
    RetryQuery.Agent = ContentionFixture.Controller;
    RetryQuery.Activity = CommonAITags::Activity_Work;
    if (!bRetryReachedInteraction || RetryingAgent->GetSnapshot().LastResult.Status != ECommonAIInteractionStatus::Completed)
    {
        AddInfo(FString::Printf(TEXT("Selected: %s\nRetried: %s\nFinal: %s\nPipeline transitions:\n%s"),
            *DescribeSnapshot(Selected), *DescribeSnapshot(Retried), *DescribeSnapshot(RetryingAgent->GetSnapshot()),
            *FString::Join(PipelineTrace, TEXT("\n"))));
        const FCommonAIInteractionResult FirstResult = ICommonAIInteraction::Execute_GetInteractionResult(FirstSite, CompetingLease);
        const FCommonAIInteractionResult SecondResult = ICommonAIInteraction::Execute_GetInteractionResult(SecondSite, LastReservationRequest);
        const FCommonAIActionDefinition* MoveDefinition = RetryingAgent->Profile->Tasks[0].Actions.FindByPredicate(
            [](const FCommonAIActionDefinition& Action) { return Action.Kind == ECommonAIActionKind::Move; });
        FAIMoveRequest MoveQuery(SecondSite->GetOwner());
        if (MoveDefinition) MoveQuery.SetAcceptanceRadius(MoveDefinition->AcceptanceRadiusCentimeters);
        const UPathFollowingComponent* Path = ContentionFixture.Controller->GetPathFollowingComponent();
        AddInfo(FString::Printf(TEXT("Lease diagnostics: firstAvailable=%s firstStatus=%s firstReason=%s secondAvailable=%s secondStatus=%s secondReason=%s secondStarts=%d\nMovement diagnostics: pawn=%s navAgent=%s first=%s second=%s pathStatus=%d reachesSecond=%s"),
            ICommonAIInteraction::Execute_QueryInteraction(FirstSite, RetryQuery).bAvailable ? TEXT("true") : TEXT("false"),
            *UEnum::GetValueAsString(FirstResult.Status), *FirstResult.Reason.ToString(),
            ICommonAIInteraction::Execute_QueryInteraction(SecondSite, RetryQuery).bAvailable ? TEXT("true") : TEXT("false"),
            *UEnum::GetValueAsString(SecondResult.Status), *SecondResult.Reason.ToString(), RetryRecorder->Starts,
            *Character->GetActorLocation().ToString(), *Character->GetNavAgentLocation().ToString(),
            *FirstSite->GetOwner()->GetActorLocation().ToString(), *SecondSite->GetOwner()->GetActorLocation().ToString(),
            Path ? static_cast<int32>(Path->GetStatus()) : INDEX_NONE, Path && Path->HasReached(MoveQuery) ? TEXT("true") : TEXT("false")));
    }
    TestTrue(TEXT("Production completion frees only the second point"),
        ICommonAIInteraction::Execute_QueryInteraction(SecondSite, RetryQuery).bAvailable);
    TestEqual(TEXT("Completion cleanup still preserves the competing first lease"),
        ICommonAIInteraction::Execute_GetInteractionResult(FirstSite, CompetingLease).Status, ECommonAIInteractionStatus::Reserved);
    TestTrue(TEXT("The competing owner can release its own lease"),
        ICommonAIInteraction::Execute_ReleaseInteraction(FirstSite, CompetingLease, false));
    TestTrue(TEXT("The first point is free after its owner's release"),
        ICommonAIInteraction::Execute_QueryInteraction(FirstSite, RetryQuery).bAvailable);
    RetryingAgent->StopAgent();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommonAIManualScoringTest, "CommonAI.General.ManualEvaluation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommonAIManualScoringTest::RunTest(const FString& Parameters)
{
    FWorld Fixture;
    UCommonAIAgentComponent* Agent = NewObject<UCommonAIAgentComponent>(Fixture.Controller);
    Agent->bStartAutomatically = false;
    Agent->Profile = CommonAITestProfiles::MakeSelection(Agent);
    Agent->Profile->ScoringMode = ECommonAIScoringMode::Manual;
    Agent->Profile->ObservationIntervalSeconds = 0.0f;
    Agent->Profile->Considerations[0].Route = ECommonAIInputRoute::Evaluator;
    Agent->Profile->Considerations[0].Evaluator = NewObject<UCommonAINumericEvaluator>(Agent->Profile);
    Agent->RegisterComponent();
    Agent->RegisterInputSource(CommonAITags::Source_External, Agent);
    if (!TestTrue(TEXT("Manual profile starts without polling"), Agent->StartAgent())) return false;
    Fixture.Tick(0.5f);
    TestEqual(TEXT("Manual start leaves fallback until explicit scoring"), Agent->GetSnapshot().Activity, CommonAITags::Activity_Wait.GetTag());
    FCommonAINumericContext Number;
    Number.Value = 0.9f;
    TestTrue(TEXT("Evaluator is callable without a service"),
        Agent->EvaluateInput(Agent, Agent->GetInputGeneration(), CommonAITags::Consideration_WorkAvailable, FInstancedStruct::Make(Number), false));
    Fixture.Tick(0.25f);
    TestEqual(TEXT("Evaluation without rescore does not choose a task"), Agent->GetSnapshot().Activity, CommonAITags::Activity_Wait.GetTag());
    TestTrue(TEXT("Explicit rescore is accepted"), Agent->Rescore());
    Fixture.Tick(0.25f);
    TestEqual(TEXT("Requested pass completes its safe transition"), Agent->GetSnapshot().Activity, CommonAITags::Activity_Work_Crafting.GetTag());
    FCommonAIInputUpdate Update;
    Update.Id = CommonAITags::Consideration_DistanceTarget;
    Update.RawValue = 1.0f;
    Agent->ApplyInputUpdates(Agent, Agent->GetInputGeneration(), { Update }, false);
    Fixture.Tick(0.5f);
    TestEqual(TEXT("No hidden periodic pass in manual mode"), Agent->GetSnapshot().Activity, CommonAITags::Activity_Work_Crafting.GetTag());
    Agent->Rescore();
    Fixture.Tick(0.25f);
    TestEqual(TEXT("Next explicit pass selects the new winner"), Agent->GetSnapshot().Activity, CommonAITags::Activity_Work_Farming.GetTag());
    return true;
}
#endif
