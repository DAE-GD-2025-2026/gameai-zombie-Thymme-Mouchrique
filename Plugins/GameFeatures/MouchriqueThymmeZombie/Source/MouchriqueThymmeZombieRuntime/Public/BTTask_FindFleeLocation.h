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

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
	bool UpdateFleeMove(UBehaviorTreeComponent& OwnerComp);
	void StopRunning(UBehaviorTreeComponent& OwnerComp) const;

	float MoveRefreshTimer = 0.f;
};
