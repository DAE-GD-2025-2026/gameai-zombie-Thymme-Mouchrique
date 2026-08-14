#include "BTTask_UseBestHealingItem.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"

#include "Common/HealthComponent.h"
#include "Common/StaminaComponent.h"
#include "Common/InventoryComponent.h"

#include "Items/BaseItem.h"
#include "Items/ItemType.h"

namespace
{
	// find the consumable that fills the missing amount with the least waste
	int FindBestConsumableSlot(const TArray<ABaseItem*>& Items, EItemType ItemType, int MissingAmount)
	{
		int BestSlot = INDEX_NONE;
		int BestWaste = TNumericLimits<int>::Max();
		int BestValue = -1;

		for (int SlotIdx = 0; SlotIdx < Items.Num(); ++SlotIdx)
		{
			const ABaseItem* Item = Items[SlotIdx];

			// skip empty slots, wrong item types or empty consumables
			if (!Item || Item->GetItemType() != ItemType || Item->GetValue() <= 0)
			{
				continue;
			}

			const int ItemValue = Item->GetValue();

			// anything above what we are missing gets wasted
			const int Waste = FMath::Max(0, ItemValue - MissingAmount);

			// prefer less waste, if waste is the same take the stronger item
			if (Waste < BestWaste || (Waste == BestWaste && ItemValue > BestValue))
			{
				BestWaste = Waste;
				BestValue = ItemValue;
				BestSlot = SlotIdx;
			}
		}

		return BestSlot;
	}
}

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

	UHealthComponent* Health = Pawn->FindComponentByClass<UHealthComponent>();
	UStaminaComponent* Stamina = Pawn->FindComponentByClass<UStaminaComponent>();
	UInventoryComponent* Inventory = Pawn->FindComponentByClass<UInventoryComponent>();

	if (!Inventory)
	{
		return EBTNodeResult::Failed;
	}

	const TArray<ABaseItem*>& Items = Inventory->GetInventory();

	const bool bNeedsHealth =
		Health &&
		Health->GetMaxHealth() > 0 &&
		static_cast<float>(Health->GetHealth()) / static_cast<float>(Health->GetMaxHealth()) <= 0.35f;

	const bool bNeedsEnergy =
		Stamina &&
		Stamina->GetMaxStamina() > 0.f &&
		Stamina->GetCurrentStamina() / Stamina->GetMaxStamina() <= 0.30f;

	if (bNeedsHealth)
	{
		const int MissingHealth = Health->GetMaxHealth() - Health->GetHealth();
		const int BestSlot = FindBestConsumableSlot(Items, EItemType::Medkit, MissingHealth);

		if (BestSlot != INDEX_NONE && Inventory->UseItem(BestSlot))
		{
			if (Items.IsValidIndex(BestSlot) && Items[BestSlot] && Items[BestSlot]->GetValue() <= 0)
			{
				Inventory->RemoveItem(BestSlot);
			}

			return EBTNodeResult::Succeeded;
		}
	}

	if (bNeedsEnergy)
	{
		const int MissingEnergy = FMath::CeilToInt(Stamina->GetMaxStamina() - Stamina->GetCurrentStamina());
		const int BestSlot = FindBestConsumableSlot(Items, EItemType::Food, MissingEnergy);

		if (BestSlot != INDEX_NONE && Inventory->UseItem(BestSlot))
		{
			if (Items.IsValidIndex(BestSlot) && Items[BestSlot] && Items[BestSlot]->GetValue() <= 0)
			{
				Inventory->RemoveItem(BestSlot);
			}

			return EBTNodeResult::Succeeded;
		}
	}

	return EBTNodeResult::Failed;
}