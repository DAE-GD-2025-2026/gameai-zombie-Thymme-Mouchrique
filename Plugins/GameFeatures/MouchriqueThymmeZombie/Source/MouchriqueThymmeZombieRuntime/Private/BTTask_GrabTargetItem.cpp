#include "BTTask_GrabTargetItem.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"

#include "Common/InventoryComponent.h"
#include "Items/BaseItem.h"

#include "SurvivorAIShared.h"

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
	AActor* TargetItemActor = Cast<AActor>(Blackboard->GetValueAsObject(SurvivorBB::TargetItem));

	if (!Pawn || !TargetItemActor)
	{
		return EBTNodeResult::Failed;
	}

	UInventoryComponent* Inventory = Pawn->FindComponentByClass<UInventoryComponent>();
	ABaseItem* Item = Cast<ABaseItem>(TargetItemActor);

	if (!Inventory || !Item)
	{
		Blackboard->ClearValue(SurvivorBB::TargetItem);
		return EBTNodeResult::Failed;
	}

	const TArray<ABaseItem*>& Items = Inventory->GetInventory();

	int SlotIdx = INDEX_NONE;

	// find first empty inventory slot
	for (int Index = 0; Index < Items.Num(); ++Index)
	{
		if (!Items[Index])
		{
			SlotIdx = Index;
			break;
		}
	}

	if (SlotIdx == INDEX_NONE)
	{
		Blackboard->ClearValue(SurvivorBB::TargetItem);
		return EBTNodeResult::Failed;
	}

	if (Inventory->GrabItem(SlotIdx, Item))
	{
		UE_LOG(LogTemp, Warning, TEXT("Grabbed item: %s"), *Item->GetName());

		// update weapon state from actual inventory instead of item name
		Blackboard->SetValueAsBool(SurvivorBB::HasWeapon, InventoryHasUsableWeapon(Inventory));

		Blackboard->ClearValue(SurvivorBB::TargetItem);

		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}