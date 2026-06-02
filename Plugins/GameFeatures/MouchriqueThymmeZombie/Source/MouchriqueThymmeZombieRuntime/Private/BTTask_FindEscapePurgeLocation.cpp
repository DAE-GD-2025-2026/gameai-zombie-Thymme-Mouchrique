#include "BTTask_FindEscapePurgeLocation.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"

UBTTask_FindEscapePurgeLocation::UBTTask_FindEscapePurgeLocation()
{
	NodeName = TEXT("Find Escape Purge Location");
}

EBTNodeResult::Type UBTTask_FindEscapePurgeLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AAIController* Controller = OwnerComp.GetAIOwner();

	if (!Blackboard || !Controller)
	{
		return EBTNodeResult::Failed;
	}

	APawn* Pawn = Controller->GetPawn();
	AActor* PurgeZone = Cast<AActor>(Blackboard->GetValueAsObject(TEXT("TargetPurgeZone")));

	if (!Pawn || !PurgeZone)
	{
		return EBTNodeResult::Failed;
	}

	const FVector PawnLocation = Pawn->GetActorLocation();
	const FVector PurgeLocation = PurgeZone->GetActorLocation();

	FVector DirectionAway = PawnLocation - PurgeLocation;
	DirectionAway.Z = 0.f;

	if (DirectionAway.IsNearlyZero())
	{
		DirectionAway = Pawn->GetActorForwardVector();
		DirectionAway.Z = 0.f;
	}

	if (DirectionAway.IsNearlyZero())
	{
		return EBTNodeResult::Failed;
	}

	DirectionAway.Normalize();

	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(Pawn->GetWorld());

	if (!NavSystem)
	{
		return EBTNodeResult::Failed;
	}

	constexpr float EscapeDistance = 1800.f;
	constexpr float SearchRadius = 600.f;

	const FVector DesiredLocation = PawnLocation + DirectionAway * EscapeDistance;

	FNavLocation EscapeLocation;

	const bool bFoundLocation = NavSystem->GetRandomReachablePointInRadius(
		DesiredLocation,
		SearchRadius,
		EscapeLocation
	);

	if (!bFoundLocation)
	{
		return EBTNodeResult::Failed;
	}

	Blackboard->SetValueAsVector(TEXT("MoveLocation"), EscapeLocation.Location);

	DrawDebugLine(Pawn->GetWorld(), PawnLocation, EscapeLocation.Location, FColor::Red, false, 2.f, 0, 5.f);
	DrawDebugSphere(Pawn->GetWorld(), EscapeLocation.Location, 120.f, 12, FColor::Red, false, 2.f);

	return EBTNodeResult::Succeeded;
}