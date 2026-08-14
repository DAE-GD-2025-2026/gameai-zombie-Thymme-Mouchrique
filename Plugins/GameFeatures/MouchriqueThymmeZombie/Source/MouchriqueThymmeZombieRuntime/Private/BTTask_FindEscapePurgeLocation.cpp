#include "BTTask_FindEscapePurgeLocation.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"

#include "StudentPerceptorThymmeMouchrique.h"
#include "SurvivorAIShared.h"

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

	if (!Pawn)
	{
		return EBTNodeResult::Failed;
	}

	UStudentPerceptor* Perceptor = Pawn->FindComponentByClass<UStudentPerceptor>();

	if (!Perceptor)
	{
		return EBTNodeResult::Failed;
	}

	FVector PurgeLocation;

	// use last purge location we actually perceived instead of tracking purge actor
	if (!Perceptor->GetLastKnownPurgeLocation(PurgeLocation))
	{
		return EBTNodeResult::Failed;
	}

	const FVector PawnLocation = Pawn->GetActorLocation();

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

	constexpr float EscapeDistance = 250.f;
	constexpr float SearchRadius = 450.f;

	const FVector DesiredLocation = PawnLocation + DirectionAway * EscapeDistance;

	FNavLocation BestLocation;
	float BestScore = TNumericLimits<float>::Lowest();
	bool bFoundLocation = false;

	// try a few spots and take the one that gets us further away
	for (int Attempt = 0; Attempt < 10; ++Attempt)
	{
		FNavLocation CandidateLocation;

		if (!NavSystem->GetRandomReachablePointInRadius(DesiredLocation, SearchRadius, CandidateLocation))
		{
			continue;
		}

		const float DistanceFromPurge = FVector::Dist2D(CandidateLocation.Location, PurgeLocation);
		const float TravelDistance = FVector::Dist2D(PawnLocation, CandidateLocation.Location);
		const float Score = DistanceFromPurge - TravelDistance * 0.25f;

		if (Score > BestScore)
		{
			BestScore = Score;
			BestLocation = CandidateLocation;
			bFoundLocation = true;
		}
	}

	if (!bFoundLocation)
	{
		return EBTNodeResult::Failed;
	}

	Blackboard->SetValueAsVector(SurvivorBB::MoveLocation, BestLocation.Location);

	return EBTNodeResult::Succeeded;
}