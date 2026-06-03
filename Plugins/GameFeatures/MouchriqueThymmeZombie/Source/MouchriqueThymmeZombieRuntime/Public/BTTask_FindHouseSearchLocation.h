#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_FindHouseSearchLocation.generated.h"

UCLASS()
class MOUCHRIQUETHYMMEZOMBIERUNTIME_API UBTTask_FindHouseSearchLocation : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_FindHouseSearchLocation();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};