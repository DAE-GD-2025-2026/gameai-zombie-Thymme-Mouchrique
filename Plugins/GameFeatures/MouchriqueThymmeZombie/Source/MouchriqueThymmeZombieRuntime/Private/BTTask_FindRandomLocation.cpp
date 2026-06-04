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

	const FVector PawnLocation = Pawn->GetActorLocation();
	const FVector LastExploreLocation = Blackboard->GetValueAsVector(TEXT("LastExploreLocation"));

	//TODO: tweak values
	constexpr float ExploreRadius = 1400.f;
	constexpr float MinDistanceFromPawn = 700.f;
	constexpr float MinDistanceFromLastExplore = 900.f;
	constexpr int MaxAttempts = 10;

	FNavLocation NavLocation;
	bool bFoundGoodLocation = false;

	for (int Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		const bool bFound = NavSystem->GetRandomReachablePointInRadius(
			PawnLocation,
			ExploreRadius,
			NavLocation
		);

		if (!bFound)
		{
			continue;
		}

		const float DistanceFromPawn = FVector::Dist2D(PawnLocation, NavLocation.Location);
		const float DistanceFromLastExplore = FVector::Dist2D(LastExploreLocation, NavLocation.Location);

		if (DistanceFromPawn < MinDistanceFromPawn)
		{
			continue;
		}

		if (!LastExploreLocation.IsNearlyZero() && DistanceFromLastExplore < MinDistanceFromLastExplore)
		{
			continue;
		}

		bFoundGoodLocation = true;
		break;
	}

	if (!bFoundGoodLocation)
	{
		return EBTNodeResult::Failed;
	}

	Blackboard->SetValueAsVector(TEXT("MoveLocation"), NavLocation.Location);
	Blackboard->SetValueAsVector(TEXT("LastExploreLocation"), NavLocation.Location);

	DrawDebugSphere(Pawn->GetWorld(), NavLocation.Location, 80.f, 12, FColor::Blue, false, 1.f);
	DrawDebugLine(Pawn->GetWorld(), PawnLocation, NavLocation.Location, FColor::Blue, false, 1.f, 0, 3.f);

	return EBTNodeResult::Succeeded;
}