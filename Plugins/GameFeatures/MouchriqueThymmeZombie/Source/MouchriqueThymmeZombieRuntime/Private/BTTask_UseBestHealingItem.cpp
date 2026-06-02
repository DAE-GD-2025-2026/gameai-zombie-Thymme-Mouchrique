#include "BTTask_UseBestHealingItem.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"

#include "Common/InventoryComponent.h"
#include "Items/BaseItem.h"

UBTTask_UseBestHealingItem::UBTTask_UseBestHealingItem()
{
	NodeName = TEXT("Use Best Healing Item");
}

EBTNodeResult::Type UBTTask_UseBestHealingItem::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AAIController* Controller = OwnerComp.GetAIOwner();

	if (!Blackboard || !Controller)
	{
		return EBTNodeResult::Failed;
	}

	APawn* Pawn = Controller->GetPawn();

	if (!Pawn)
	{
		return EBTNodeResult::Failed;
	}

	UInventoryComponent* Inventory = Pawn->FindComponentByClass<UInventoryComponent>();

	if (!Inventory)
	{
		return EBTNodeResult::Failed;
	}

	const bool bNeedsHealth = Blackboard->GetValueAsBool(TEXT("IsLowHealth"));
	const bool bNeedsEnergy = Blackboard->GetValueAsBool(TEXT("IsLowEnergy"));

	const TArray<ABaseItem*>& Items = Inventory->GetInventory();

	for (int SlotIdx = 0; SlotIdx < Items.Num(); ++SlotIdx)
	{
		ABaseItem* Item = Items[SlotIdx];

		if (!Item)
		{
			continue;
		}

		const FString ItemName = Item->GetName();

		const bool bIsMedkit = ItemName.Contains(TEXT("Medkit"));
		const bool bIsFood = ItemName.Contains(TEXT("Food"));

		if ((bNeedsHealth && bIsMedkit) || (bNeedsEnergy && bIsFood))
		{
			if (Inventory->UseItem(SlotIdx))
			{
				UE_LOG(LogTemp, Warning, TEXT("Used item: %s"), *ItemName);

				if (Item->GetValue() == 0)
				{
					Inventory->RemoveItem(SlotIdx);
				}

				return EBTNodeResult::Succeeded;
			}
		}
	}

	return EBTNodeResult::Failed;
}