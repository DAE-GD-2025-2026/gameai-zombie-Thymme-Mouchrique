#include "BTTask_FindRandomLocation.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"
#include "GameFramework/Pawn.h"
#include "DrawDebugHelpers.h"

UBTTask_FindRandomLocation::UBTTask_FindRandomLocation()
{
	NodeName = TEXT("Find Random Location");
}

EBTNodeResult::Type UBTTask_FindRandomLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AAIController* Controller = OwnerComp.GetAIOwner();

	if (!Blackboard || !Controller)
	{
		return EBTNodeResult::Failed;
	}

	APawn* Pawn = Controller->GetPawn();

	if (!Pawn)
	{
		return EBTNodeResult::Failed;
	}

	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(Pawn->GetWorld());

	if (!NavSystem)
	{
		return EBTNodeResult::Failed;
	}

	FNavLocation NavLocation;

	const bool bFound = NavSystem->GetRandomReachablePointInRadius(
		Pawn->GetActorLocation(),
		2500.f,
		NavLocation
	);

	if (!bFound)
	{
		return EBTNodeResult::Failed;
	}

	Blackboard->SetValueAsVector(TEXT("MoveLocation"), NavLocation.Location);

	DrawDebugSphere(Pawn->GetWorld(), NavLocation.Location, 80.f, 12, FColor::Blue, false, 2.f);

	return EBTNodeResult::Succeeded;
}