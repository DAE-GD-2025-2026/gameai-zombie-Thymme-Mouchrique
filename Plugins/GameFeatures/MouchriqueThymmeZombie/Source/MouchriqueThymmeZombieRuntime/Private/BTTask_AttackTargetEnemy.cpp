#include "BTTask_AttackTargetEnemy.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"

#include "Common/InventoryComponent.h"
#include "Items/BaseItem.h"

UBTTask_AttackTargetEnemy::UBTTask_AttackTargetEnemy()
{
	NodeName = TEXT("Attack Target Enemy");
}

EBTNodeResult::Type UBTTask_AttackTargetEnemy::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AAIController* Controller = OwnerComp.GetAIOwner();

	if (!Blackboard || !Controller)
	{
		return EBTNodeResult::Failed;
	}

	APawn* Pawn = Controller->GetPawn();
	AActor* Enemy = Cast<AActor>(Blackboard->GetValueAsObject(TEXT("TargetEnemy")));

	if (!Pawn || !Enemy)
	{
		return EBTNodeResult::Failed;
	}

	UInventoryComponent* Inventory = Pawn->FindComponentByClass<UInventoryComponent>();

	if (!Inventory)
	{
		return EBTNodeResult::Failed;
	}

	FVector DirectionToEnemy = Enemy->GetActorLocation() - Pawn->GetActorLocation();
	DirectionToEnemy.Z = 0.f;

	if (DirectionToEnemy.IsNearlyZero())
	{
		return EBTNodeResult::Failed;
	}

	DirectionToEnemy.Normalize();

	Pawn->SetActorRotation(DirectionToEnemy.Rotation());

	const TArray<ABaseItem*>& Items = Inventory->GetInventory();

	for (int SlotIdx = 0; SlotIdx < Items.Num(); ++SlotIdx)
	{
		ABaseItem* Item = Items[SlotIdx];

		if (!Item)
		{
			continue;
		}

		const FString ItemName = Item->GetName();

		const bool bIsWeapon =
			ItemName.Contains(TEXT("Pistol")) ||
			ItemName.Contains(TEXT("Shotgun"));

		if (!bIsWeapon)
		{
			continue;
		}

		if (Item->GetValue() <= 0)
		{
			Inventory->RemoveItem(SlotIdx);
			continue;
		}

		// use item = shoot in this case
		if (Inventory->UseItem(SlotIdx))
		{
			UE_LOG(LogTemp, Warning, TEXT("Attacked enemy using: %s"), *ItemName);

			if (Item->GetValue() <= 0)
			{
				Inventory->RemoveItem(SlotIdx);
			}

			return EBTNodeResult::Succeeded;
		}
	}

	Blackboard->SetValueAsBool(TEXT("HasWeapon"), false);
	return EBTNodeResult::Failed;
}