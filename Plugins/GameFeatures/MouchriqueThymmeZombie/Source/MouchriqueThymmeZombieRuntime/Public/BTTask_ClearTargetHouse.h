#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_ClearTargetHouse.generated.h"

UCLASS()
class MOUCHRIQUETHYMMEZOMBIERUNTIME_API UBTTask_ClearTargetHouse : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_ClearTargetHouse();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};