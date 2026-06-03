#include "BTTask_ClearTargetHouse.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "StudentPerceptorThymmeMouchrique.h"

#include "AIController.h"
#include "GameFramework/Pawn.h"

UBTTask_ClearTargetHouse::UBTTask_ClearTargetHouse()
{
	NodeName = TEXT("Update House Search");
}

EBTNodeResult::Type UBTTask_ClearTargetHouse::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();

	if (!Blackboard)
	{
		return EBTNodeResult::Failed;
	}

	if (Blackboard->GetValueAsObject(TEXT("TargetItem")))
	{
		return EBTNodeResult::Succeeded;
	}

	const int CurrentCount = Blackboard->GetValueAsInt(TEXT("HouseSearchCount"));
	const int NewCount = CurrentCount + 1;

	Blackboard->SetValueAsInt(TEXT("HouseSearchCount"), NewCount);

	// prevent spamming the same house and set the house as already searched
	if (NewCount >= 4)
	{
		AActor* TargetHouse = Cast<AActor>(Blackboard->GetValueAsObject(TEXT("TargetHouse")));

		if (APawn* Pawn = Cast<APawn>(OwnerComp.GetAIOwner()->GetPawn()))
		{
			if (UStudentPerceptor* Perceptor = Pawn->FindComponentByClass<UStudentPerceptor>())
			{
				Perceptor->MarkHouseSearched(TargetHouse);
			}
		}

		Blackboard->ClearValue(TEXT("TargetHouse"));
		Blackboard->SetValueAsInt(TEXT("HouseSearchCount"), 0);
	}

	return EBTNodeResult::Succeeded;
}