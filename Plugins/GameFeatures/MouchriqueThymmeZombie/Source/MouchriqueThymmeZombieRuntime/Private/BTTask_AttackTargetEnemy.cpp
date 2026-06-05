#include "BTTask_AttackTargetEnemy.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"

#include "Common/HealthComponent.h"
#include "Common/InventoryComponent.h"
#include "Items/BaseItem.h"


// helper functions
namespace
{
	bool IsPistol(const FString& ItemName)
	{
		return ItemName.Contains(TEXT("Pistol"));
	}

	bool IsShotgun(const FString& ItemName)
	{
		return ItemName.Contains(TEXT("Shotgun"));
	}

	bool IsUsableWeapon(const ABaseItem* Item)
	{
		if (!Item)
		{
			return false;
		}

		const FString ItemName = Item->GetName();

		return (IsPistol(ItemName) || IsShotgun(ItemName)) &&
			Item->GetValue() > 0;
	}

	bool TryUseWeapon(UInventoryComponent* Inventory,const TArray<ABaseItem*>& Items,
		const FString& WantedWeaponName,UBlackboardComponent* Blackboard)
	{
		if (!Inventory)
		{
			return false;
		}

		for (int SlotIdx = 0; SlotIdx < Items.Num(); ++SlotIdx)
		{
			ABaseItem* Item = Items[SlotIdx];

			if (!IsUsableWeapon(Item))
			{
				continue;
			}

			const FString ItemName = Item->GetName();

			if (!ItemName.Contains(WantedWeaponName))
			{
				continue;
			}

			if (Inventory->UseItem(SlotIdx))
			{
				UE_LOG(LogTemp, Warning, TEXT("Attacked enemy using: %s"), *ItemName);

				if (Item->GetValue() <= 0)
				{
					Inventory->RemoveItem(SlotIdx);
				}

				return true;
			}
		}

		return false;
	}

	bool HasAnyUsableWeapon(const TArray<ABaseItem*>& Items)
	{
		for (const ABaseItem* Item : Items)
		{
			if (IsUsableWeapon(Item))
			{
				return true;
			}
		}

		return false;
	}
}

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

	// reset target if we lose sight or get too far from the enemy or if dead
	const float DistanceToEnemy = DirectionToEnemy.Size();
	constexpr float MaxEnemyMemoryDistance = 1800.f;

	UHealthComponent* EnemyHealth = Enemy->FindComponentByClass<UHealthComponent>();

	if (DirectionToEnemy.IsNearlyZero() ||
		DistanceToEnemy > MaxEnemyMemoryDistance ||
		(EnemyHealth && EnemyHealth->IsDead()))
	{
		Blackboard->ClearValue(TEXT("TargetEnemy"));
		return EBTNodeResult::Failed;
	}

	DirectionToEnemy.Normalize();

	// face enemy
	Pawn->SetActorRotation(DirectionToEnemy.Rotation());

	const TArray<ABaseItem*>& Items = Inventory->GetInventory();

	if (!HasAnyUsableWeapon(Items))
	{

		// fix for firing but not having ammo left to fire again (led to player being stuck)

		Blackboard->SetValueAsBool(TEXT("HasWeapon"), false);
		Blackboard->SetValueAsBool(TEXT("ShouldAttackEnemy"), false);
		Blackboard->SetValueAsBool(TEXT("ShouldFleeEnemy"), true);

		UE_LOG(LogTemp, Warning, TEXT("No usable weapon left. Switching to flee enemy."));

		return EBTNodeResult::Failed;
	}

	constexpr float ShotgunPreferredDistance = 450.f;

	if (DistanceToEnemy <= ShotgunPreferredDistance)
	{
		// if enemy is close use shotgun first because spread is useful up close.
		if (TryUseWeapon(Inventory, Items, TEXT("Shotgun"), Blackboard))
		{
			return EBTNodeResult::Succeeded;
		}

		if (TryUseWeapon(Inventory, Items, TEXT("Pistol"), Blackboard))
		{
			return EBTNodeResult::Succeeded;
		}
	}
	else
	{
		// if enemy is farther away use pistol first because it is more accurate.
		if (TryUseWeapon(Inventory, Items, TEXT("Pistol"), Blackboard))
		{
			return EBTNodeResult::Succeeded;
		}

		if (TryUseWeapon(Inventory, Items, TEXT("Shotgun"), Blackboard))
		{
			return EBTNodeResult::Succeeded;
		}
	}

	Blackboard->SetValueAsBool(TEXT("HasWeapon"), false);
	return EBTNodeResult::Failed;
}