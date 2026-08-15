#include "BTTask_FindHouseSearchLocation.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"

#include "SurvivorAIShared.h"

UBTTask_FindHouseSearchLocation::UBTTask_FindHouseSearchLocation()
{
	NodeName = TEXT("Find House Search Location");
}

EBTNodeResult::Type UBTTask_FindHouseSearchLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AAIController* Controller = OwnerComp.GetAIOwner();

	if (!Blackboard || !Controller)
	{
		return EBTNodeResult::Failed;
	}

	APawn* Pawn = Controller->GetPawn();
	AActor* House = Cast<AActor>(Blackboard->GetValueAsObject(SurvivorBB::TargetHouse));

	if (!Pawn || !House)
	{
		return EBTNodeResult::Failed;
	}

	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(Pawn->GetWorld());

	if (!NavSystem)
	{
		return EBTNodeResult::Failed;
	}

	static const FVector2D SearchOffsets[] =
	{
		FVector2D(0.f, 0.f),
		FVector2D(200.f, 0.f),
		FVector2D(0.f, 200.f),
		FVector2D(-200.f, 0.f),
		FVector2D(0.f, -200.f)
	};

	int SearchIndex = Blackboard->GetValueAsInt(TEXT("HouseSearchCount"));

	if (SearchIndex >= UE_ARRAY_COUNT(SearchOffsets))
	{
		Blackboard->ClearValue(SurvivorBB::TargetHouse);
		Blackboard->SetValueAsInt(TEXT("HouseSearchCount"), 0);
		return EBTNodeResult::Failed;
	}

	// find a random location around the house to search for the player
	// use fixed spots now so we actually search different parts around the house
	for (int Attempt = 0; Attempt < UE_ARRAY_COUNT(SearchOffsets); ++Attempt)
	{
		const int Index = (SearchIndex + Attempt) % UE_ARRAY_COUNT(SearchOffsets);
		const FVector2D Offset = SearchOffsets[Index];

		const FVector DesiredLocation = House->GetActorLocation() + FVector(Offset.X, Offset.Y, 0.f);

		FNavLocation SearchLocation;

		if (!NavSystem->ProjectPointToNavigation(DesiredLocation, SearchLocation))
		{
			continue;
		}

		Blackboard->SetValueAsVector(SurvivorBB::MoveLocation, SearchLocation.Location);
		Blackboard->SetValueAsInt(TEXT("HouseSearchCount"), Index + 1);

		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}