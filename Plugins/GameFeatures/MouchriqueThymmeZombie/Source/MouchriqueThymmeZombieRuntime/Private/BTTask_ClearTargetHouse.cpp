#include "BTTask_ClearTargetHouse.h"

#include "BehaviorTree/BlackboardComponent.h"

UBTTask_ClearTargetHouse::UBTTask_ClearTargetHouse()
{
	NodeName = TEXT("Clear Target House");
}

EBTNodeResult::Type UBTTask_ClearTargetHouse::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();

	if (!Blackboard)
	{
		return EBTNodeResult::Failed;
	}

	Blackboard->ClearValue(TEXT("TargetHouse"));

	return EBTNodeResult::Succeeded;
}