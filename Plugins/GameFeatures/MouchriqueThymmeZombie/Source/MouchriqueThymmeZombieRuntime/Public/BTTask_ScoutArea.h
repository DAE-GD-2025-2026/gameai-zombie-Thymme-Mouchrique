#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_ScoutArea.generated.h"

USTRUCT()
struct FScoutAreaMemory
{
	GENERATED_BODY()

	float ElapsedTime = 0.f;
};

UCLASS()
class MOUCHRIQUETHYMMEZOMBIERUNTIME_API UBTTask_ScoutArea : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_ScoutArea();

	virtual uint16 GetInstanceMemorySize() const override;
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};