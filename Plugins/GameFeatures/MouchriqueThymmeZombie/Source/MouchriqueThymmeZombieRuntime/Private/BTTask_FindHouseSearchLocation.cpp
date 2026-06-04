#include "BTTask_FindHouseSearchLocation.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"

UBTTask_FindHouseSearchLocation::UBTTask_FindHouseSearchLocation()
{
	NodeName = TEXT("Find House Search Location");
}

EBTNodeResult::Type UBTTask_FindHouseSearchLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AAIController* Controller = OwnerComp.GetAIOwner();

	if (!Blackboard || !Controller)
	{
		return EBTNodeResult::Failed;
	}

	APawn* Pawn = Controller->GetPawn();
	AActor* House = Cast<AActor>(Blackboard->GetValueAsObject(TEXT("TargetHouse")));

	if (!Pawn || !House)
	{
		return EBTNodeResult::Failed;
	}

	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(Pawn->GetWorld());

	if (!NavSystem)
	{
		return EBTNodeResult::Failed;
	}

	FNavLocation SearchLocation;

	// find a random location around the house to search for the player
	const bool bFoundLocation = NavSystem->GetRandomReachablePointInRadius(
		House->GetActorLocation(),
		300.f,
		SearchLocation
	);

	if (!bFoundLocation)
	{
		return EBTNodeResult::Failed;
	}

	Blackboard->SetValueAsVector(TEXT("MoveLocation"), SearchLocation.Location);

	// rotate towards house
	FVector DirectionToHouse = House->GetActorLocation() - Pawn->GetActorLocation();
	DirectionToHouse.Z = 0.f;

	if (!DirectionToHouse.IsNearlyZero())
	{
		Pawn->SetActorRotation(DirectionToHouse.Rotation());
	}

	DrawDebugLine(Pawn->GetWorld(), Pawn->GetActorLocation(), SearchLocation.Location, FColor::Yellow, false, 2.f, 0, 4.f);
	DrawDebugSphere(Pawn->GetWorld(), SearchLocation.Location, 100.f, 12, FColor::Yellow, false, 2.f);

	return EBTNodeResult::Succeeded;
}