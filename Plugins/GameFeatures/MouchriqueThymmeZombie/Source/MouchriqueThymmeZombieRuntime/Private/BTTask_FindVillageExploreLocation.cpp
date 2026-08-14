#include "BTTask_FindVillageExploreLocation.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"
#include "DrawDebugHelpers.h"

#include "StudentPerceptorThymmeMouchrique.h"
#include "SurvivorAIShared.h"

UBTTask_FindVillageExploreLocation::UBTTask_FindVillageExploreLocation()
{
	NodeName = TEXT("Find Village Explore Location");
}

EBTNodeResult::Type UBTTask_FindVillageExploreLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
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

	if (!StudentPerceptor || StudentPerceptor->IsVillageDepleted())
	{
		return EBTNodeResult::Failed;
	}

	FVector VillageExploreLocation;

	if (!StudentPerceptor->GetVillageCircleExploreLocation(VillageExploreLocation))
	{
		return EBTNodeResult::Failed;
	}

	Blackboard->SetValueAsVector(TEXT("MoveLocation"), VillageExploreLocation);
	Blackboard->SetValueAsVector(TEXT("LastExploreLocation"), VillageExploreLocation);
	Blackboard->SetValueAsBool(SurvivorBB::IsTravellingBetweenVillages, false);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[Explore] Circle exploring around last known village.")
	);

	DrawDebugSphere(
		Pawn->GetWorld(),
		VillageExploreLocation,
		80.f,
		12,
		FColor::Green,
		false,
		1.f
	);

	DrawDebugLine(
		Pawn->GetWorld(),
		Pawn->GetActorLocation(),
		VillageExploreLocation,
		FColor::Green,
		false,
		1.f,
		0,
		3.f
	);

	return EBTNodeResult::Succeeded;
}
