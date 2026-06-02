#include "BTTask_GrabTargetItem.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"

#include "Common/InventoryComponent.h"
#include "Items/BaseItem.h"

UBTTask_GrabTargetItem::UBTTask_GrabTargetItem()
{
	NodeName = TEXT("Grab Target Item");
}

EBTNodeResult::Type UBTTask_GrabTargetItem::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
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

	ABaseItem* Item = Cast<ABaseItem>(Blackboard->GetValueAsObject(TEXT("TargetItem")));
	if (!Item)
	{
		return EBTNodeResult::Failed;
	}

	UInventoryComponent* Inventory = Pawn->FindComponentByClass<UInventoryComponent>();
	if (!Inventory)
	{
		return EBTNodeResult::Failed;
	}

	for (int SlotIdx = 0; SlotIdx < Inventory->GetInventoryCapacity(); ++SlotIdx)
	{
		if (Inventory->GrabItem(SlotIdx, Item))
		{
			const FString ItemName = Item->GetName();

			UE_LOG(LogTemp, Warning, TEXT("Grabbed item: %s"), *ItemName);

			if (ItemName.Contains(TEXT("Pistol")) || ItemName.Contains(TEXT("Shotgun")))
			{
				Blackboard->SetValueAsBool(TEXT("HasWeapon"), true);
			}

			Blackboard->ClearValue(TEXT("TargetItem"));

			return EBTNodeResult::Succeeded;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("Failed to grab item: %s, clearing TargetItem"), *Item->GetName());
	Blackboard->ClearValue(TEXT("TargetItem")); 
	
	return EBTNodeResult::Failed;

}