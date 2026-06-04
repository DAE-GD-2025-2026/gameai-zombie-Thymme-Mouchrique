#include "BTTask_FindRandomLocation.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"
#include "GameFramework/Pawn.h"
#include "DrawDebugHelpers.h"
#include "StudentPerceptorThymmeMouchrique.h"

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

	// FIRST: if we know any unsearched house, don't explore randomly
	// Force the BT to use the Search House branch instead
	if (UStudentPerceptor* StudentPerceptor = Pawn->FindComponentByClass<UStudentPerceptor>())
	{
		if (StudentPerceptor->TryUpdateTargetHouse())
		{
			UE_LOG(LogTemp, Warning, TEXT("[Explore] Known unsearched house found. Cancelling random explore."));
			return EBTNodeResult::Failed;
		}

		FVector VillageExploreLocation;
		if (StudentPerceptor->GetVillageExploreLocation(VillageExploreLocation))
		{
			Blackboard->SetValueAsVector(TEXT("MoveLocation"), VillageExploreLocation);
			Blackboard->SetValueAsVector(TEXT("LastExploreLocation"), VillageExploreLocation);

			UE_LOG(LogTemp, Warning, TEXT("[Explore] Exploring around last known village."));

			DrawDebugSphere(Pawn->GetWorld(), VillageExploreLocation, 80.f, 12, FColor::Green, false, 1.f);
			DrawDebugLine(Pawn->GetWorld(), Pawn->GetActorLocation(), VillageExploreLocation, FColor::Green, false, 1.f, 0, 3.f);

			return EBTNodeResult::Succeeded;
		}
	}

	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(Pawn->GetWorld());

	if (!NavSystem)
	{
		return EBTNodeResult::Failed;
	}

	const FVector PawnLocation = Pawn->GetActorLocation();
	const FVector LastExploreLocation = Blackboard->GetValueAsVector(TEXT("LastExploreLocation"));

	FVector ExploreDirection = Blackboard->GetValueAsVector(TEXT("ExploreDirection"));
	ExploreDirection.Z = 0.f;

	// If we do not have a direction yet, pick one.
	if (ExploreDirection.IsNearlyZero())
	{
		ExploreDirection = FMath::VRand();
		ExploreDirection.Z = 0.f;
		ExploreDirection.Normalize();

		Blackboard->SetValueAsVector(TEXT("ExploreDirection"), ExploreDirection);
	}

	constexpr float ExploreRadius = 1400.f;
	constexpr float MinDistanceFromPawn = 700.f;
	constexpr float MinDistanceFromLastExplore = 900.f;
	constexpr int MaxAttempts = 20;

	FNavLocation BestLocation;
	float BestScore = TNumericLimits<float>::Lowest(); // min float value coz starts at worst
	bool bFoundGoodLocation = false;

	for (int Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		FNavLocation CandidateLocation;

		const bool bFound = NavSystem->GetRandomReachablePointInRadius(
			PawnLocation,
			ExploreRadius,
			CandidateLocation
		);

		if (!bFound)
		{
			continue;
		}

		FVector DirectionToCandidate = CandidateLocation.Location - PawnLocation;
		DirectionToCandidate.Z = 0.f;

		if (DirectionToCandidate.IsNearlyZero())
		{
			continue;
		}

		const float DistanceFromPawn = DirectionToCandidate.Size();
		DirectionToCandidate.Normalize();

		const float DirectionAlignment = FVector::DotProduct(ExploreDirection, DirectionToCandidate);

		// reject points clearly behind the current explore direction
		if (DirectionAlignment < 0.15f)
		{
			continue;
		}

		const float DistanceFromLastExplore = FVector::Dist2D(LastExploreLocation, CandidateLocation.Location);

		if (DistanceFromPawn < MinDistanceFromPawn)
		{
			continue;
		}

		if (!LastExploreLocation.IsNearlyZero() && DistanceFromLastExplore < MinDistanceFromLastExplore)
		{
			continue;
		}

		// Score prefers:
		// - far enough movement
		// - same general direction
		// - not close to previous explore point
		float CandidateScore = DistanceFromPawn;
		CandidateScore += DirectionAlignment * 800.f;

		if (!LastExploreLocation.IsNearlyZero())
		{
			CandidateScore += DistanceFromLastExplore * 0.25f;
		}

		if (CandidateScore > BestScore)
		{
			BestScore = CandidateScore;
			BestLocation = CandidateLocation;
			bFoundGoodLocation = true;
		}
	}

	// if no point was found in that direction, pick a new explore direction next time
	if (!bFoundGoodLocation)
	{
		Blackboard->ClearValue(TEXT("ExploreDirection"));
		return EBTNodeResult::Failed;
	}

	// slowly update direction to match the successful movement
	FVector NewDirection = BestLocation.Location - PawnLocation;
	NewDirection.Z = 0.f;

	if (!NewDirection.IsNearlyZero())
	{
		NewDirection.Normalize();
		Blackboard->SetValueAsVector(TEXT("ExploreDirection"), NewDirection);
	}

	Blackboard->SetValueAsVector(TEXT("MoveLocation"), BestLocation.Location);
	Blackboard->SetValueAsVector(TEXT("LastExploreLocation"), BestLocation.Location);

	DrawDebugSphere(Pawn->GetWorld(), BestLocation.Location, 80.f, 12, FColor::Blue, false, 1.f);
	DrawDebugLine(Pawn->GetWorld(), PawnLocation, BestLocation.Location, FColor::Blue, false, 1.f, 0, 3.f);

	return EBTNodeResult::Succeeded;
}