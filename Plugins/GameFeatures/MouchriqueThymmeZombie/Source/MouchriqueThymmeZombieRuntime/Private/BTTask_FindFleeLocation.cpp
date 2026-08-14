#include "BTTask_FindFleeLocation.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"
#include "GameFramework/Pawn.h"

#include "StudentPerceptorThymmeMouchrique.h"
#include "SurvivorAIShared.h"

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

	if (!Pawn)
	{
		return EBTNodeResult::Failed;
	}

	// only flee if combat decision actually says to flee
	if (!Blackboard->GetValueAsBool(SurvivorBB::ShouldFleeEnemy))
	{
		return EBTNodeResult::Failed;
	}

	AActor* Enemy = Cast<AActor>(Blackboard->GetValueAsObject(SurvivorBB::TargetEnemy));

	UStudentPerceptor* Perceptor = Pawn->FindComponentByClass<UStudentPerceptor>();

	if (!Enemy || !Perceptor)
	{
		return EBTNodeResult::Failed;
	}

	FVector EnemyLocation;

	// use last place enemy was actually seen instead of tracking it through walls
	if (!Perceptor->GetLastKnownEnemyLocation(Enemy, EnemyLocation))
	{
		return EBTNodeResult::Failed;
	}

	const FVector PawnLocation = Pawn->GetActorLocation();

	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(Pawn->GetWorld());

	if (!NavSystem)
	{
		return EBTNodeResult::Failed;
	}

	FVector DirectionAway = PawnLocation - EnemyLocation;
	DirectionAway.Z = 0.f;

	if (DirectionAway.IsNearlyZero())
	{
		DirectionAway = -Pawn->GetActorForwardVector();
		DirectionAway.Z = 0.f;

		if (DirectionAway.IsNearlyZero())
		{
			DirectionAway = FVector::ForwardVector;
		}
	}

	DirectionAway.Normalize();

	// if we see a house, flee there first.
	AActor* House = Cast<AActor>(Blackboard->GetValueAsObject(SurvivorBB::TargetHouse));

	if (House)
	{
		FVector ToHouse = House->GetActorLocation() - PawnLocation;
		ToHouse.Z = 0.f;

		if (!ToHouse.IsNearlyZero())
		{
			ToHouse.Normalize();

			// make sure house is not basically behind the enemy
			const float SafetyAlignment = FVector::DotProduct(ToHouse,DirectionAway);

			if (SafetyAlignment >= -0.2f)
			{
				const float DistanceToHouse = FVector::Dist2D(PawnLocation, House->GetActorLocation());

				if (DistanceToHouse > 350.f)
				{
					FNavLocation HouseNavLocation;

					const bool bFoundHouseLocation = NavSystem->ProjectPointToNavigation(House->GetActorLocation(),HouseNavLocation);

					if (bFoundHouseLocation)
					{
						Blackboard->SetValueAsVector(SurvivorBB::MoveLocation,HouseNavLocation.Location);

						return EBTNodeResult::Succeeded;
					}
				}
			}
		}
	}

	// just flee lol
	// p sure this causes the player and enemy to endlessly run in the same direction (thus blocking each other)
	FVector FleeDirection = DirectionAway; // this is a hack for enemy blocking player

	// use visible enemies to push flee direction away from groups too
	TArray<FVector> VisibleThreatLocations;
	Perceptor->GetVisibleEnemyLocations(VisibleThreatLocations);

	FVector SeparationForce = FVector::ZeroVector;

	for (const FVector& ThreatLocation : VisibleThreatLocations)
	{
		FVector Away = PawnLocation - ThreatLocation;
		Away.Z = 0.f;

		const float DistanceSquared = FMath::Max(Away.SizeSquared2D(), 1.f);

		SeparationForce += Away.GetSafeNormal() / DistanceSquared;
	}

	if (!SeparationForce.IsNearlyZero())
	{
		SeparationForce.Normalize();

		FleeDirection = (DirectionAway * 0.8f + SeparationForce * 0.6f).GetSafeNormal();
	}

	if (FleeDirection.IsNearlyZero())
	{
		FleeDirection = DirectionAway;
	}

	constexpr float FleeDistance = 900.f;
	constexpr float SearchRadius = 350.f;

	const FVector DesiredLocation = PawnLocation + FleeDirection * FleeDistance;

	FNavLocation BestFleeLocation;

	float BestScore =
		TNumericLimits<float>::Lowest();

	bool bFoundFleeLocation = false;

	// try a few reachable spots and choose one that actually gets further from enemy
	for (int Attempt = 0; Attempt < 8; ++Attempt)
	{
		FNavLocation CandidateLocation;

		const bool bFoundCandidate =
			NavSystem->GetRandomReachablePointInRadius(
				DesiredLocation,
				SearchRadius,
				CandidateLocation
			);

		if (!bFoundCandidate)
		{
			continue;
		}

		const float DistanceFromEnemy = FVector::Dist2D(CandidateLocation.Location, EnemyLocation);

		const float TravelDistance = FVector::Dist2D(PawnLocation,CandidateLocation.Location); 

		const float Score = DistanceFromEnemy - TravelDistance * 0.25f;

		if (Score > BestScore)
		{
			BestScore = Score;
			BestFleeLocation = CandidateLocation;
			bFoundFleeLocation = true;
		}
	}

	if (!bFoundFleeLocation)
	{
		return EBTNodeResult::Failed;
	}

	Blackboard->SetValueAsVector(SurvivorBB::MoveLocation, BestFleeLocation.Location);

	return EBTNodeResult::Succeeded;
}