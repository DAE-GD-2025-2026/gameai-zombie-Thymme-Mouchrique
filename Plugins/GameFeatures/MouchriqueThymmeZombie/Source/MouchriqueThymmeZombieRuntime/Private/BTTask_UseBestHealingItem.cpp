#include "BTTask_UseBestHealingItem.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"

#include "Common/HealthComponent.h"
#include "Common/StaminaComponent.h"
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
	UHealthComponent* Health = Pawn->FindComponentByClass<UHealthComponent>();
	UStaminaComponent* Stamina = Pawn->FindComponentByClass<UStaminaComponent>();

	if (!Inventory)
	{
		return EBTNodeResult::Failed;
	}

	//TODO: treshold values should be tested & tweaked (although I think they're fine for now)
	const bool bNeedsHealth = Health && Health->GetHealth() <= 5;
	const bool bNeedsEnergy = Stamina && Stamina->GetCurrentStamina() <= 3.f;

	Blackboard->SetValueAsBool(TEXT("IsLowHealth"), bNeedsHealth);
	Blackboard->SetValueAsBool(TEXT("IsLowEnergy"), bNeedsEnergy);

	if (!bNeedsHealth && !bNeedsEnergy)
	{
		return EBTNodeResult::Failed;
	}

	const TArray<ABaseItem*>& Items = Inventory->GetInventory();

	// Prioritize health over stamina
	if (bNeedsHealth)
	{
		for (int SlotIdx = 0; SlotIdx < Items.Num(); ++SlotIdx)
		{
			ABaseItem* Item = Items[SlotIdx];

			if (!Item || !Item->GetName().Contains(TEXT("Medkit")))
			{
				continue;
			}

			if (Inventory->UseItem(SlotIdx))
			{
				UE_LOG(LogTemp, Warning, TEXT("Used medkit: %s"), *Item->GetName());

				if (Item->GetValue() == 0)
				{
					Inventory->RemoveItem(SlotIdx);
				}

				Blackboard->SetValueAsBool(TEXT("IsLowHealth"), false);
				return EBTNodeResult::Succeeded;
			}
		}
	}

	if (bNeedsEnergy)
	{
		for (int SlotIdx = 0; SlotIdx < Items.Num(); ++SlotIdx)
		{
			ABaseItem* Item = Items[SlotIdx];

			if (!Item || !Item->GetName().Contains(TEXT("Food")))
			{
				continue;
			}

			if (Inventory->UseItem(SlotIdx))
			{
				UE_LOG(LogTemp, Warning, TEXT("Used food: %s"), *Item->GetName());

				if (Item->GetValue() == 0)
				{
					Inventory->RemoveItem(SlotIdx);
				}

				Blackboard->SetValueAsBool(TEXT("IsLowEnergy"), false);
				return EBTNodeResult::Succeeded;
			}
		}
	}

	return EBTNodeResult::Failed;
}