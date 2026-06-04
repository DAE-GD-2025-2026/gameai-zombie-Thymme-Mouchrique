#include "BTTask_ScoutArea.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"
#include "StudentPerceptorThymmeMouchrique.h"

UBTTask_ScoutArea::UBTTask_ScoutArea()
{
	NodeName = TEXT("Scout Area");
	bNotifyTick = true;
}

uint16 UBTTask_ScoutArea::GetInstanceMemorySize() const
{
	return sizeof(FScoutAreaMemory);
}

EBTNodeResult::Type UBTTask_ScoutArea::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FScoutAreaMemory* Memory = reinterpret_cast<FScoutAreaMemory*>(NodeMemory);

	if (!Memory)
	{
		return EBTNodeResult::Failed;
	}

	Memory->ElapsedTime = 0.f;

	return EBTNodeResult::InProgress;
}

void UBTTask_ScoutArea::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FScoutAreaMemory* Memory = reinterpret_cast<FScoutAreaMemory*>(NodeMemory);

	if (!Memory)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	AAIController* Controller = OwnerComp.GetAIOwner();

	if (!Controller)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	APawn* Pawn = Controller->GetPawn();

	if (!Pawn)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	constexpr float ScoutDuration = 1.5f;
	constexpr float ScoutTurnSpeed = 240.f;

	Pawn->AddActorWorldRotation(FRotator(0.f, ScoutTurnSpeed * DeltaSeconds, 0.f));

	Memory->ElapsedTime += DeltaSeconds;

	if (UStudentPerceptor* StudentPerceptor = Pawn->FindComponentByClass<UStudentPerceptor>())
	{
		if (StudentPerceptor->TryUpdateTargetHouse())
		{
			UE_LOG(LogTemp, Warning, TEXT("[Scout] Found house. Stopping Explore."));

			// Fail on purpose so Explore stops and the selector can pick Search House.
			FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
			return;
		}
	}

	if (Memory->ElapsedTime >= ScoutDuration)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Scout] Finished scouting. No house found."));

		// Success means continue Explore -> Find Random Location -> Move To.
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}