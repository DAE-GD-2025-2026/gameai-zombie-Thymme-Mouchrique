#include "BTTask_FindFleeLocation.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"
#include "GameFramework/Pawn.h"
#include "DrawDebugHelpers.h"

UBTTask_FindFleeLocation::UBTTask_FindFleeLocation()
{
	NodeName = TEXT("Find Flee Location");
}

EBTNodeResult::Type UBTTask_FindFleeLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AAIController* Controller = OwnerComp.GetAIOwner();

	if (!Blackboard || !Controller)
	{
		return EBTNodeResult::Failed;
	}

	APawn* Pawn = Controller->GetPawn();
	AActor* Enemy = Cast<AActor>(Blackboard->GetValueAsObject(TEXT("TargetEnemy")));

	if (!Pawn || !Enemy)
	{
		return EBTNodeResult::Failed;
	}

	const FVector PawnLocation = Pawn->GetActorLocation();
	const FVector EnemyLocation = Enemy->GetActorLocation();

	FVector DirectionAway = PawnLocation - EnemyLocation;
	DirectionAway.Z = 0.f;

	if (DirectionAway.IsNearlyZero())
	{
		return EBTNodeResult::Failed;
	}

	DirectionAway.Normalize();

	constexpr float FleeDistance = 700.f;
	constexpr float SearchRadius = 300.f;

	const FVector DesiredLocation = PawnLocation + DirectionAway * FleeDistance;

	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(Pawn->GetWorld());

	if (!NavSystem)
	{
		return EBTNodeResult::Failed;
	}

	FNavLocation NavLocation;

	const bool bFoundLocation = NavSystem->GetRandomPointInNavigableRadius(
		DesiredLocation,
		SearchRadius,
		NavLocation
	);

	if (!bFoundLocation)
	{
		return EBTNodeResult::Failed;
	}

	Blackboard->SetValueAsVector(TEXT("MoveLocation"), NavLocation.Location);

	DrawDebugLine(Pawn->GetWorld(), PawnLocation, NavLocation.Location, FColor::Green, false, 2.f, 0, 4.f);
	DrawDebugSphere(Pawn->GetWorld(), NavLocation.Location, 80.f, 12, FColor::Green, false, 2.f);

	return EBTNodeResult::Succeeded;
}