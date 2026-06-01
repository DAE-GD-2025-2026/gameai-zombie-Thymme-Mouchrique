#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_FindFleeLocation.generated.h"

UCLASS()
class MOUCHRIQUETHYMMEZOMBIERUNTIME_API UBTTask_FindFleeLocation : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_FindFleeLocation();

	virtual EBTNodeResult::Type ExecuteTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory
	) override;
};