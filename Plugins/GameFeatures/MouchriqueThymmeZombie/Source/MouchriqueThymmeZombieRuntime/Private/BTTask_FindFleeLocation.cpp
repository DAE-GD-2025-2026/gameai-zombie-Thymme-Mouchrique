#include "BTTask_FindFleeLocation.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"

#include "Survivor/SurvivorPawn.h"
#include "StudentPerceptorThymmeMouchrique.h"
#include "SurvivorAIShared.h"

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
		UE_LOG(LogTemp, Warning, TEXT("[FleeTask] failed: no controller or blackboard"));
		return EBTNodeResult::Failed;
	}

	ASurvivorPawn* Pawn = Cast<ASurvivorPawn>(Controller->GetPawn());

	if (!Pawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FleeTask] failed: no pawn"));
		return EBTNodeResult::Failed;
	}

	// only flee if combat decision actually says to flee
	if (!Blackboard->GetValueAsBool(SurvivorBB::ShouldFleeEnemy))
	{
		UE_LOG(LogTemp, Warning, TEXT("[FleeTask] failed: ShouldFleeEnemy is false"));
		return EBTNodeResult::Failed;
	}

	Pawn->StartRunning();
	MoveRefreshTimer = 0.f;

	if (!UpdateFleeMove(OwnerComp))
	{
		// no point yet, keep the flee task alive and try again next update
		UE_LOG(LogTemp, Warning, TEXT("[FleeTask] no first point yet, retrying"));
	}

	return EBTNodeResult::InProgress;
}

void UBTTask_FindFleeLocation::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AAIController* Controller = OwnerComp.GetAIOwner();

	if (!Blackboard || !Controller)
	{
		StopRunning(OwnerComp);
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	if (!Blackboard->GetValueAsBool(SurvivorBB::ShouldFleeEnemy) ||
		!Blackboard->GetValueAsObject(SurvivorBB::TargetEnemy))
	{
		StopRunning(OwnerComp);
		UE_LOG(LogTemp, Warning, TEXT("[FleeTask] finished"));
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	MoveRefreshTimer += DeltaSeconds;

	// update the flee point while the zombie moves instead of committing to one old point
	if (MoveRefreshTimer < 0.45f)
	{
		return;
	}

	MoveRefreshTimer = 0.f;

	if (!UpdateFleeMove(OwnerComp))
	{
		// keep the current move instead of freezing because one refresh failed
		UE_LOG(LogTemp, Warning, TEXT("[FleeTask] could not refresh flee point, keeping current move"));
	}
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

	ASurvivorPawn* Pawn = Cast<ASurvivorPawn>(Controller->GetPawn());

	if (!Pawn)
	{
		return false;
	}

	AActor* Enemy = Cast<AActor>(Blackboard->GetValueAsObject(SurvivorBB::TargetEnemy));
	UStudentPerceptor* Perceptor = Pawn->FindComponentByClass<UStudentPerceptor>();

	if (!Enemy || !Perceptor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FleeTask] failed: no enemy or perceptor"));
		return false;
	}

	FVector EnemyLocation;

	// use last place enemy was actually seen instead of tracking it through walls
	if (!Perceptor->GetLastKnownEnemyLocation(Enemy, EnemyLocation))
	{
		UE_LOG(LogTemp, Warning, TEXT("[FleeTask] failed: no enemy location"));
		return false;
	}

	const FVector PawnLocation = Pawn->GetActorLocation();

	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(Pawn->GetWorld());

	if (!NavSystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FleeTask] failed: no navigation system"));
		return false;
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

	FVector FleeDirection = DirectionAway;

	if (!SeparationForce.IsNearlyZero())
	{
		SeparationForce.Normalize();
		FleeDirection = (DirectionAway * 0.8f + SeparationForce * 0.6f).GetSafeNormal();
	}

	if (FleeDirection.IsNearlyZero())
	{
		FleeDirection = DirectionAway;
	}

	// curve a little so we do not just run backwards into the same zombie
	const FVector SideDirection(-FleeDirection.Y, FleeDirection.X, 0.f);
	FleeDirection = (FleeDirection + SideDirection * 0.25f).GetSafeNormal();

	constexpr float FleeDistance = 800.f;
	constexpr float SearchRadius = 400.f;
	constexpr float MinimumMoveDistance = 250.f;

	const FVector DesiredLocation = PawnLocation + FleeDirection * FleeDistance;

	FNavLocation BestFleeLocation;
	float BestScore = TNumericLimits<float>::Lowest();
	bool bFoundFleeLocation = false;

	const float CurrentDistanceFromEnemy = FVector::Dist2D(PawnLocation, EnemyLocation);

	// try a few reachable spots and choose one that actually gets further from enemy
	for (int Attempt = 0; Attempt < 10; ++Attempt)
	{
		FNavLocation CandidateLocation;

		if (!NavSystem->GetRandomReachablePointInRadius(DesiredLocation, SearchRadius, CandidateLocation))
		{
			continue;
		}

		const float DistanceFromEnemy = FVector::Dist2D(CandidateLocation.Location, EnemyLocation);
		const float TravelDistance = FVector::Dist2D(PawnLocation, CandidateLocation.Location);

		if (TravelDistance < MinimumMoveDistance || DistanceFromEnemy <= CurrentDistanceFromEnemy)
		{
			continue;
		}

		float ClosestThreatDistance = DistanceFromEnemy;

		for (const FVector& ThreatLocation : VisibleThreatLocations)
		{
			ClosestThreatDistance = FMath::Min(
				ClosestThreatDistance,
				FVector::Dist2D(CandidateLocation.Location, ThreatLocation)
			);
		}

		const float Score = ClosestThreatDistance - TravelDistance * 0.15f;

		if (Score > BestScore)
		{
			BestScore = Score;
			BestFleeLocation = CandidateLocation;
			bFoundFleeLocation = true;
		}
	}

	if (!bFoundFleeLocation)
	{
		// fallback around the player in case the point far away is outside the navmesh
		for (int Attempt = 0; Attempt < 10; ++Attempt)
		{
			FNavLocation CandidateLocation;

			if (!NavSystem->GetRandomReachablePointInRadius(PawnLocation, 650.f, CandidateLocation))
			{
				continue;
			}

			const float DistanceFromEnemy = FVector::Dist2D(CandidateLocation.Location, EnemyLocation);
			const float TravelDistance = FVector::Dist2D(PawnLocation, CandidateLocation.Location);

			if (TravelDistance < MinimumMoveDistance || DistanceFromEnemy <= CurrentDistanceFromEnemy)
			{
				continue;
			}

			float ClosestThreatDistance = DistanceFromEnemy;

			for (const FVector& ThreatLocation : VisibleThreatLocations)
			{
				ClosestThreatDistance = FMath::Min(
					ClosestThreatDistance,
					FVector::Dist2D(CandidateLocation.Location, ThreatLocation)
				);
			}

			const float Score = ClosestThreatDistance - TravelDistance * 0.15f;

			if (Score > BestScore)
			{
				BestScore = Score;
				BestFleeLocation = CandidateLocation;
				bFoundFleeLocation = true;
			}
		}
	}

	if (!bFoundFleeLocation)
	{
		// last try in the general escape direction, moving is better than standing still here
		const FVector FallbackTarget = PawnLocation + FleeDirection * 450.f;
		FNavLocation ProjectedFallback;

		if (NavSystem->ProjectPointToNavigation(
			FallbackTarget,
			ProjectedFallback,
			FVector(350.f, 350.f, 250.f)))
		{
			const float FallbackDistance =
				FVector::Dist2D(PawnLocation, ProjectedFallback.Location);

			if (FallbackDistance > 100.f)
			{
				Blackboard->SetValueAsVector(
					SurvivorBB::MoveLocation,
					ProjectedFallback.Location);

				Controller->MoveToLocation(ProjectedFallback.Location, 100.f);

				UE_LOG(LogTemp, Warning, TEXT("[FleeTask] using simple fallback point"));
				return true;
			}
		}

		UE_LOG(LogTemp, Warning, TEXT("[FleeTask] could not find a new flee point"));
		return false;
	}

	Blackboard->SetValueAsVector(SurvivorBB::MoveLocation, BestFleeLocation.Location);
	Controller->MoveToLocation(BestFleeLocation.Location, 100.f);

	return true;
}

void UBTTask_FindFleeLocation::StopRunning(UBehaviorTreeComponent& OwnerComp) const
{
	AAIController* Controller = OwnerComp.GetAIOwner();

	if (!Controller)
	{
		return;
	}

	Controller->StopMovement();

	if (ASurvivorPawn* Pawn = Cast<ASurvivorPawn>(Controller->GetPawn()))
	{
		Pawn->StopRunning();
	}
}
