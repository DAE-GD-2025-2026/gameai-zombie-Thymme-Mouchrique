#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_AttackTargetEnemy.generated.h"

UCLASS()
class MOUCHRIQUETHYMMEZOMBIERUNTIME_API UBTTask_AttackTargetEnemy : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_AttackTargetEnemy();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
	// remember last shot so behavior tree does not waste ammo by firing too fast
	float LastAttackTime = -1000.f;
};