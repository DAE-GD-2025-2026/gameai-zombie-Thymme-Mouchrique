#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_UseBestHealingItem.generated.h"

UCLASS()
class MOUCHRIQUETHYMMEZOMBIERUNTIME_API UBTTask_UseBestHealingItem : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_UseBestHealingItem();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};