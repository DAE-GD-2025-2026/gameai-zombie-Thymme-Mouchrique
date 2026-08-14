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
	FVector ExploreDirection = Blackboard->GetValueAsVector(TEXT("ExploreDirection"));

	// if there is no exploration direction yet, use where player is facing
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

	constexpr float ExploreRadius = 1400.f;
	constexpr float MinimumDistanceFromLastTarget = 700.f;

	FNavLocation BestLocation;
	float BestScore = TNumericLimits<float>::Lowest();
	bool bFoundLocation = false;

	// try multiple points instead of just taking first random one
	for (int Attempt = 0; Attempt < 12; ++Attempt)
	{
		FNavLocation CandidateLocation;

		if (!NavSystem->GetRandomReachablePointInRadius(PawnLocation, ExploreRadius, CandidateLocation))
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

		if (DirectionToCandidate.IsNearlyZero())
		{
			continue;
		}

		DirectionToCandidate.Normalize();

		// do not keep picking places behind us
		const float ForwardAlignment = FVector::DotProduct(ExploreDirection, DirectionToCandidate);

		if (ForwardAlignment < 0.15f)
		{
			continue;
		}

		if (!LastExploreLocation.IsNearlyZero())
		{
			const float DistanceFromLastTarget = FVector::Dist2D(CandidateLocation.Location, LastExploreLocation);

			if (DistanceFromLastTarget < MinimumDistanceFromLastTarget)
			{
				continue;
			}
		}

		// prefer moving forward and actually making some distance
		const float Score = ForwardAlignment * 1000.f + DistanceToCandidate * 0.25f;

		if (Score > BestScore)
		{
			BestScore = Score;
			BestLocation = CandidateLocation;
			bFoundLocation = true;
		}
	}

	if (!bFoundLocation)
	{
		ExploreDirection = ExploreDirection.RotateAngleAxis(60.f, FVector::UpVector);
		Blackboard->SetValueAsVector(TEXT("ExploreDirection"), ExploreDirection);
		return EBTNodeResult::Failed;
	}

	Blackboard->SetValueAsVector(TEXT("MoveLocation"), BestLocation.Location);
	Blackboard->SetValueAsVector(TEXT("LastExploreLocation"), BestLocation.Location);
	Blackboard->SetValueAsBool(SurvivorBB::IsTravellingBetweenVillages, true);

	return EBTNodeResult::Succeeded;
}