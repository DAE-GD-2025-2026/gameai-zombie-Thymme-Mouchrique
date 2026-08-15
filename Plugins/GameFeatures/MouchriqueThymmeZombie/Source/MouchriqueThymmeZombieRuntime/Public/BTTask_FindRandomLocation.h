#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_FindRandomLocation.generated.h"

UCLASS()
class MOUCHRIQUETHYMMEZOMBIERUNTIME_API UBTTask_FindRandomLocation : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_FindRandomLocation();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
	// first village gives us a rough center to search around later
	FVector OriginVillageLocation = FVector::ZeroVector;
	bool bHasOriginVillageLocation = false;

	// keep going around the same way instead of bouncing back and forth
	float RingDirection = 1.f;
};