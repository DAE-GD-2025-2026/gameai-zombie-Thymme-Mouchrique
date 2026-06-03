#include "BTTask_ClearTargetHouse.h"

#include "BehaviorTree/BlackboardComponent.h"

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

	// prevent spamming the same house
	const int CurrentCount = Blackboard->GetValueAsInt(TEXT("HouseSearchCount"));
	const int NewCount = CurrentCount + 1;

	Blackboard->SetValueAsInt(TEXT("HouseSearchCount"), NewCount);

	if (NewCount >= 4)
	{
		Blackboard->ClearValue(TEXT("TargetHouse"));
		Blackboard->SetValueAsInt(TEXT("HouseSearchCount"), 0);
	}

	return EBTNodeResult::Succeeded;
}