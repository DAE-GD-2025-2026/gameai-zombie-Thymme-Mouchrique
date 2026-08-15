#include "BTTask_FindFleeLocation.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"
#include "GameFramework/Pawn.h"

#include "StudentPerceptorThymmeMouchrique.h"
#include "SurvivorAIShared.h"
#include "Survivor/SurvivorPawn.h"

UBTTask_FindFleeLocation::UBTTask_FindFleeLocation()
{
	NodeName = TEXT("Find Flee Location");
	bNotifyTick = true;
	bCreateNodeInstance = true;
}

EBTNodeResult::Type UBTTask_FindFleeLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UE_LOG(LogTemp, Warning, TEXT("[FleeTask] started"));

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AAIController* Controller = OwnerComp.GetAIOwner();

	if (!Blackboard || !Controller)
	{
		return EBTNodeResult::Failed;
	}

	ASurvivorPawn* Pawn = Cast<ASurvivorPawn>(Controller->GetPawn());

	if (!Pawn)
	{
		return EBTNodeResult::Failed;
	}

	if (!Blackboard->GetValueAsBool(SurvivorBB::ShouldFleeEnemy))
	{
		return EBTNodeResult::Failed;
	}

	Pawn->StartRunning();
	MoveRefreshTimer = 0.f;

	// first search can fail, task will just retry
	UpdateFleeMove(OwnerComp);

	return EBTNodeResult::InProgress;
}

void UBTTask_FindFleeLocation::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AAIController* Controller = OwnerComp.GetAIOwner();

	if (!Blackboard || !Controller || !Controller->GetPawn())
	{
		StopRunning(OwnerComp);
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	if (!Blackboard->GetValueAsBool(SurvivorBB::ShouldFleeEnemy) || !Blackboard->GetValueAsObject(SurvivorBB::TargetEnemy))
	{
		StopRunning(OwnerComp);
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	if (ASurvivorPawn* Pawn = Cast<ASurvivorPawn>(Controller->GetPawn()))
	{
		Pawn->StartRunning();
	}

	MoveRefreshTimer += DeltaSeconds;

	if (MoveRefreshTimer < 0.45f)
	{
		return;
	}

	MoveRefreshTimer = 0.f;

	UpdateFleeMove(OwnerComp);
}

EBTNodeResult::Type UBTTask_FindFleeLocation::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	StopRunning(OwnerComp);
	return EBTNodeResult::Aborted;
}

bool UBTTask_FindFleeLocation::UpdateFleeMove(UBehaviorTreeComponent& OwnerComp)
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AAIController* Controller = OwnerComp.GetAIOwner();

	if (!Blackboard || !Controller)
	{
		return false;
	}

	APawn* Pawn = Controller->GetPawn();

	if (!Pawn)
	{
		return false;
	}

	UStudentPerceptor* Perceptor = Pawn->FindComponentByClass<UStudentPerceptor>();
	AActor* Enemy = Cast<AActor>(Blackboard->GetValueAsObject(SurvivorBB::TargetEnemy));

	if (!Enemy || !Perceptor)
	{
		return false;
	}

	FVector EnemyLocation;

	// only use where perception last saw the enemy
	if (!Perceptor->GetLastKnownEnemyLocation(Enemy, EnemyLocation))
	{
		return false;
	}

	const FVector PawnLocation = Pawn->GetActorLocation();
	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(Pawn->GetWorld());

	if (!NavSystem)
	{
		return false;
	}

	FVector DirectionAway = PawnLocation - EnemyLocation;
	DirectionAway.Z = 0.f;

	if (DirectionAway.IsNearlyZero())
	{
		DirectionAway = -Pawn->GetActorForwardVector();
		DirectionAway.Z = 0.f;
	}

	if (DirectionAway.IsNearlyZero())
	{
		DirectionAway = FVector::ForwardVector;
	}

	DirectionAway.Normalize();

	// also avoid the other zombies we currently see
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
		DirectionAway = (DirectionAway * 0.8f + SeparationForce * 0.65f).GetSafeNormal();
	}

	// if we're out of guns, use a safe house as part of the escape route
	// hopefully we find another weapon inside while running through it
	if (!Blackboard->GetValueAsBool(SurvivorBB::HasWeapon))
	{
		AActor* House = Perceptor->GetClosestKnownUnsearchedHouse(PawnLocation);
		FVector HouseLocation;

		if (House && Perceptor->GetKnownHouseLocation(House, HouseLocation))
		{
			FVector ToHouse = HouseLocation - PawnLocation;
			ToHouse.Z = 0.f;

			const float DistanceToHouse = ToHouse.Size2D();

			if (DistanceToHouse > 150.f && !ToHouse.IsNearlyZero())
			{
				ToHouse.Normalize();

				const float SafetyAlignment = FVector::DotProduct(ToHouse, DirectionAway);
				const float CurrentEnemyDistance = FVector::Dist2D(PawnLocation, EnemyLocation);
				const float HouseEnemyDistance = FVector::Dist2D(HouseLocation, EnemyLocation);

				// sideways is fine, just don't run back through the zombie for it
				if (SafetyAlignment > -0.20f && HouseEnemyDistance > CurrentEnemyDistance + 50.f)
				{
					FNavLocation HouseNavLocation;

					if (NavSystem->ProjectPointToNavigation(HouseLocation, HouseNavLocation, FVector(450.f, 450.f, 250.f)))
					{
						Blackboard->SetValueAsVector(SurvivorBB::MoveLocation, HouseNavLocation.Location);
						Controller->MoveToLocation(HouseNavLocation.Location, 100.f);

						UE_LOG(LogTemp, Warning, TEXT("[FleeTask] unarmed, escaping through house: %s"), *House->GetName());
						return true;
					}
				}
			}
		}
	}

	// move away from the enemy and preferably outward from the village
	FVector LastVillageLocation;
	const bool bHasVillage = Perceptor->GetLastKnownVillageLocation(LastVillageLocation);

	FVector OutwardDirection = FVector::ZeroVector;

	if (bHasVillage)
	{
		OutwardDirection = PawnLocation - LastVillageLocation;
		OutwardDirection.Z = 0.f;

		if (!OutwardDirection.IsNearlyZero())
		{
			OutwardDirection.Normalize();
		}
	}

	FVector TravelDirection = DirectionAway;

	if (!OutwardDirection.IsNearlyZero())
	{
		// survival first, outward movement just helps us cover new ground
		TravelDirection = (DirectionAway * 0.85f + OutwardDirection * 0.30f).GetSafeNormal();
	}

	if (TravelDirection.IsNearlyZero())
	{
		TravelDirection = DirectionAway;
	}

	constexpr float FleeDistance = 1100.f;
	constexpr float SearchRadius = 600.f;
	constexpr float MinimumMoveDistance = 300.f;

	const FVector DesiredLocation = PawnLocation + TravelDirection * FleeDistance;

	FNavLocation BestLocation;
	float BestScore = TNumericLimits<float>::Lowest();
	bool bFoundLocation = false;

	const float CurrentEnemyDistance = FVector::Dist2D(PawnLocation, EnemyLocation);
	const float CurrentVillageDistance = bHasVillage ? FVector::Dist2D(PawnLocation, LastVillageLocation) : 0.f;

	for (int Attempt = 0; Attempt < 14; ++Attempt)
	{
		FNavLocation Candidate;

		if (!NavSystem->GetRandomReachablePointInRadius(DesiredLocation, SearchRadius, Candidate))
		{
			continue;
		}

		const float TravelDistance = FVector::Dist2D(PawnLocation, Candidate.Location);

		if (TravelDistance < MinimumMoveDistance)
		{
			continue;
		}

		const float EnemyDistance = FVector::Dist2D(Candidate.Location, EnemyLocation);

		// never deliberately choose somewhere closer to the main threat
		if (EnemyDistance <= CurrentEnemyDistance + 100.f)
		{
			continue;
		}

		if (Perceptor->IsInRecentDangerZone(Candidate.Location))
		{
			continue;
		}

		float ClosestThreatDistance = EnemyDistance;

		for (const FVector& ThreatLocation : VisibleThreatLocations)
		{
			ClosestThreatDistance = FMath::Min(
				ClosestThreatDistance,
				FVector::Dist2D(Candidate.Location, ThreatLocation));
		}

		float VillageProgress = 0.f;

		if (bHasVillage)
		{
			const float CandidateVillageDistance = FVector::Dist2D(Candidate.Location, LastVillageLocation);
			VillageProgress = CandidateVillageDistance - CurrentVillageDistance;
		}

		const FVector CandidateDirection = (Candidate.Location - PawnLocation).GetSafeNormal2D();
		const float DirectionProgress = FVector::DotProduct(CandidateDirection, TravelDirection) * 250.f;

		// threat distance matters most, then reward leaving the old village
		const float Score = ClosestThreatDistance + VillageProgress * 0.45f + DirectionProgress - TravelDistance * 0.08f;

		if (Score > BestScore)
		{
			BestScore = Score;
			BestLocation = Candidate;
			bFoundLocation = true;
		}
	}

	// local fallback if the ideal direction is blocked
	if (!bFoundLocation)
	{
		for (int Attempt = 0; Attempt < 10; ++Attempt)
		{
			FNavLocation Candidate;

			if (!NavSystem->GetRandomReachablePointInRadius(PawnLocation, 700.f, Candidate))
			{
				continue;
			}

			const float TravelDistance = FVector::Dist2D(PawnLocation, Candidate.Location);
			const float EnemyDistance = FVector::Dist2D(Candidate.Location, EnemyLocation);

			if (TravelDistance < 200.f || EnemyDistance <= CurrentEnemyDistance)
			{
				continue;
			}

			if (Perceptor->IsInRecentDangerZone(Candidate.Location))
			{
				continue;
			}

			BestLocation = Candidate;
			bFoundLocation = true;
			break;
		}
	}

	// last resort, just go straight away from the enemy
	if (!bFoundLocation)
	{
		FNavLocation ProjectedLocation;
		const FVector EmergencyLocation = PawnLocation + DirectionAway * 500.f;

		if (NavSystem->ProjectPointToNavigation(EmergencyLocation, ProjectedLocation, FVector(350.f, 350.f, 250.f)))
		{
			if (FVector::Dist2D(PawnLocation, ProjectedLocation.Location) > 100.f)
			{
				BestLocation = ProjectedLocation;
				bFoundLocation = true;
			}
		}
	}

	if (!bFoundLocation)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FleeTask] couldn't find new point, keeping current movement"));
		return false;
	}

	Blackboard->SetValueAsVector(SurvivorBB::MoveLocation, BestLocation.Location);
	Controller->MoveToLocation(BestLocation.Location, 100.f);

	return true;
}

void UBTTask_FindFleeLocation::StopRunning(UBehaviorTreeComponent& OwnerComp) const
{
	if (AAIController* Controller = OwnerComp.GetAIOwner())
	{
		Controller->StopMovement();

		if (ASurvivorPawn* Pawn = Cast<ASurvivorPawn>(Controller->GetPawn()))
		{
			Pawn->StopRunning();
		}
	}
}