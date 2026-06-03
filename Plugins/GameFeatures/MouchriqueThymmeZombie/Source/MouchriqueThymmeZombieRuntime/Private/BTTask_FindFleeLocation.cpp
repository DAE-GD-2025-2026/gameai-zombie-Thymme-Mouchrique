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

	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(Pawn->GetWorld());

	if (!NavSystem)
	{
		return EBTNodeResult::Failed;
	}

	// if we see a house, flee there first.
	AActor* House = Cast<AActor>(Blackboard->GetValueAsObject(TEXT("TargetHouse")));

	if (House)
	{
		const float DistanceToHouse = FVector::Dist2D(PawnLocation, House->GetActorLocation());

		if (DistanceToHouse > 350.f)
		{
			FNavLocation HouseNavLocation;

			const bool bFoundHouseLocation = NavSystem->ProjectPointToNavigation(House->GetActorLocation(), HouseNavLocation);

			if (bFoundHouseLocation)
			{
				Blackboard->SetValueAsVector(TEXT("MoveLocation"), HouseNavLocation.Location);

				DrawDebugLine(Pawn->GetWorld(), PawnLocation, HouseNavLocation.Location, FColor::Cyan, false, 2.f, 0, 4.f);
				DrawDebugSphere(Pawn->GetWorld(), HouseNavLocation.Location, 100.f, 12, FColor::Cyan, false, 2.f);

				return EBTNodeResult::Succeeded;
			}
		}
	}

	// just flee lol
	// p sure this causes the player and enemy to endlessly run in the same direction (thus blocking each other)
	FVector DirectionAway = PawnLocation - EnemyLocation; // this is a hack for enemy blocking player
	DirectionAway.Z = 0.f;

	if (DirectionAway.IsNearlyZero())
	{
		return EBTNodeResult::Failed;
	}

	DirectionAway.Normalize();

	const float DistanceToEnemy = FVector::Dist2D(PawnLocation, EnemyLocation);

	FVector SideDirection = FVector::CrossProduct(DirectionAway, FVector::UpVector);
	SideDirection.Z = 0.f;
	SideDirection.Normalize();

	// randomly choose left or right so we dont always dodge into the same wall
	if (FMath::RandBool())
	{
		SideDirection *= -1.f;
	}

	FVector FleeDirection = DirectionAway;

	if (DistanceToEnemy < 500.f)
	{
		FleeDirection = (DirectionAway * 0.6f + SideDirection * 0.8f).GetSafeNormal();
	}

	constexpr float FleeDistance = 800.f;
	constexpr float SearchRadius = 350.f;

	const FVector DesiredLocation = PawnLocation + FleeDirection * FleeDistance;

	FNavLocation FleeNavLocation;

	const bool bFoundFleeLocation = NavSystem->GetRandomReachablePointInRadius(
		DesiredLocation,
		SearchRadius,
		FleeNavLocation
	);

	if (!bFoundFleeLocation)
	{
		return EBTNodeResult::Failed;
	}

	Blackboard->SetValueAsVector(TEXT("MoveLocation"), FleeNavLocation.Location);

	DrawDebugLine(Pawn->GetWorld(), PawnLocation, FleeNavLocation.Location, FColor::Green, false, 2.f, 0, 4.f);
	DrawDebugSphere(Pawn->GetWorld(), FleeNavLocation.Location, 80.f, 12, FColor::Green, false, 2.f);

	return EBTNodeResult::Succeeded;
}