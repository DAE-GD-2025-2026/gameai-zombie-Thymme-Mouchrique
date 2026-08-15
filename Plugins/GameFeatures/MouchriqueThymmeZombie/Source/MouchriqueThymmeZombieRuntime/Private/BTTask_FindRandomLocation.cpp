#include "BTTask_FindRandomLocation.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"

#include "StudentPerceptorThymmeMouchrique.h"
#include "SurvivorAIShared.h"

UBTTask_FindRandomLocation::UBTTask_FindRandomLocation()
{
	NodeName = TEXT("Find Random Location");
	bCreateNodeInstance = true;
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

	UStudentPerceptor* StudentPerceptor = Pawn->FindComponentByClass<UStudentPerceptor>();

	if (!StudentPerceptor)
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

	FVector LastVillageLocation;
	const bool bHasVillageLocation = StudentPerceptor->GetLastKnownVillageLocation(LastVillageLocation);

	// remember where the first village was
	// later villages are generated roughly around this area in a ring
	if (!bHasOriginVillageLocation && bHasVillageLocation)
	{
		OriginVillageLocation = LastVillageLocation;
		OriginVillageLocation.Z = 0.f;
		bHasOriginVillageLocation = true;
	}

	FVector ExploreDirection = Blackboard->GetValueAsVector(TEXT("ExploreDirection"));

	if (ExploreDirection.IsNearlyZero())
	{
		ExploreDirection = Pawn->GetActorForwardVector();
		ExploreDirection.Z = 0.f;

		if (ExploreDirection.IsNearlyZero())
		{
			ExploreDirection = FVector::ForwardVector;
		}

		ExploreDirection.Normalize();
		Blackboard->SetValueAsVector(TEXT("ExploreDirection"), ExploreDirection);
	}

	FVector SearchDirection = ExploreDirection;

	if (bHasOriginVillageLocation && bHasVillageLocation)
	{
		FVector RadialDirection = LastVillageLocation - OriginVillageLocation;
		RadialDirection.Z = 0.f;

		// once we're far enough from the original village we're probably on the outer village ring
		if (RadialDirection.Size2D() > 1000.f)
		{
			RadialDirection.Normalize();

			// tangent makes us travel around the ring instead of randomly cutting across the map
			FVector TangentDirection(
				-RadialDirection.Y * RingDirection,
				RadialDirection.X * RingDirection,
				0.f
			);

			TangentDirection.Normalize();

			// keep some outward pressure so imperfect village-center estimates don't pull us inward
			SearchDirection = (TangentDirection * 0.85f + RadialDirection * 0.20f).GetSafeNormal();

			UE_LOG(LogTemp, Warning, TEXT("[Explore] Searching around inferred village ring."));
		}
		else
		{
			// starting village is near our inferred center, so first we need to get out toward the ring
			SearchDirection = ExploreDirection;

			UE_LOG(LogTemp, Warning, TEXT("[Explore] Leaving starting village to find outer villages."));
		}
	}

	constexpr float SearchDistance = 1800.f;
	constexpr float MinimumMoveDistance = 700.f;

	constexpr float MinimumDistanceFromLastTarget = 650.f;

	FNavLocation BestLocation;
	float BestScore = TNumericLimits<float>::Lowest();
	bool bFoundLocation = false;

	for (int Attempt = 0; Attempt < 16; ++Attempt)
	{
		FNavLocation CandidateLocation;

		if (!NavSystem->GetRandomReachablePointInRadius(PawnLocation, SearchDistance, CandidateLocation))
		{
			continue;
		}

		if (StudentPerceptor->IsInRecentDangerZone(CandidateLocation.Location))
		{
			continue;
		}

		FVector DirectionToCandidate = CandidateLocation.Location - PawnLocation;
		DirectionToCandidate.Z = 0.f;

		const float DistanceToCandidate = DirectionToCandidate.Size2D();

		if (DistanceToCandidate < MinimumMoveDistance)
		{
			continue;
		}

		DirectionToCandidate.Normalize();

		const float SearchAlignment = FVector::DotProduct(SearchDirection, DirectionToCandidate);

		if (SearchAlignment < 0.25f)
		{
			continue;
		}

		if (!LastExploreLocation.IsNearlyZero())
		{
			const float DistanceFromLastTarget = FVector::Dist2D(
				CandidateLocation.Location,
				LastExploreLocation
			);

			if (DistanceFromLastTarget < MinimumDistanceFromLastTarget)
			{
				continue;
			}
		}

		// mostly reward going in our ring-search direction
		const float Score = SearchAlignment * 1200.f + DistanceToCandidate * 0.20f;

		if (Score > BestScore)
		{
			BestScore = Score;
			BestLocation = CandidateLocation;
			bFoundLocation = true;
		}
	}

	if (!bFoundLocation)
	{
		// direction might be blocked, turn a bit but don't completely lose our search pattern
		ExploreDirection = ExploreDirection.RotateAngleAxis(45.f * RingDirection, FVector::UpVector);
		ExploreDirection.Normalize();

		Blackboard->SetValueAsVector(TEXT("ExploreDirection"), ExploreDirection);

		// if one side keeps failing this eventually lets us try the other way
		RingDirection *= -1.f;

		UE_LOG(LogTemp, Warning, TEXT("[Explore] Ring path blocked, changing search direction."));

		return EBTNodeResult::Failed;
	}

	Blackboard->SetValueAsVector(SurvivorBB::MoveLocation, BestLocation.Location);
	Blackboard->SetValueAsVector(TEXT("LastExploreLocation"), BestLocation.Location);
	Blackboard->SetValueAsBool(SurvivorBB::IsTravellingBetweenVillages, true);

	return EBTNodeResult::Succeeded;
}