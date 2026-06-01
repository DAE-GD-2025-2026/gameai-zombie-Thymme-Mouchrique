#include "BTTask_FindFleeLocation.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"
#include "GameFramework/Pawn.h"

UBTTask_FindFleeLocation::UBTTask_FindFleeLocation()
{
	NodeName = TEXT("Find Flee Location");
}

EBTNodeResult::Type UBTTask_FindFleeLocation::ExecuteTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory
)
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AAIController* Controller = OwnerComp.GetAIOwner();

	if (!Blackboard || !Controller)
	{
		return EBTNodeResult::Failed;
	}

	APawn* Pawn = Controller->GetPawn();

	AActor* Enemy = Cast<AActor>(
		Blackboard->GetValueAsObject(TEXT("TargetEnemy"))
	);

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

	const FVector DesiredLocation =
		PawnLocation + DirectionAway * 1500.f;

	UNavigationSystemV1* NavSystem =
		UNavigationSystemV1::GetCurrent(GetWorld());

	if (!NavSystem)
	{
		return EBTNodeResult::Failed;
	}

	FNavLocation NavLocation;

	const bool bFound =
		NavSystem->GetRandomPointInNavigableRadius(
			DesiredLocation,
			400.f,
			NavLocation
		);

	if (!bFound)
	{
		return EBTNodeResult::Failed;
	}

	Blackboard->SetValueAsVector(
		TEXT("MoveLocation"),
		NavLocation.Location
	);

	return EBTNodeResult::Succeeded;
}