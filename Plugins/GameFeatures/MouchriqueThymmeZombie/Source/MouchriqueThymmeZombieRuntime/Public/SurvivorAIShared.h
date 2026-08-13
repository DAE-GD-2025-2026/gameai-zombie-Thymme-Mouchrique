#pragma once

#include "CoreMinimal.h"
#include "Items/BaseItem.h"
#include "Items/ItemType.h"
#include "Common/InventoryComponent.h"

// helper file to centralize duplicated names (such as blackboard values)

namespace SurvivorBB
{
	static const FName TargetEnemy(TEXT("TargetEnemy"));
	static const FName TargetHouse(TEXT("TargetHouse"));
	static const FName TargetItem(TEXT("TargetItem"));
	static const FName TargetPurgeZone(TEXT("TargetPurgeZone"));
	static const FName MoveLocation(TEXT("MoveLocation"));
	static const FName ShouldAttackEnemy(TEXT("ShouldAttackEnemy"));
	static const FName ShouldFleeEnemy(TEXT("ShouldFleeEnemy"));
	static const FName ShouldFleeToHouse(TEXT("ShouldFleeToHouse"));
	static const FName HasWeapon(TEXT("HasWeapon"));
	static const FName IsLowHealth(TEXT("IsLowHealth"));
	static const FName IsLowEnergy(TEXT("IsLowEnergy"));
	static const FName IsRunnerEnemy(TEXT("IsRunnerEnemy"));
}

enum class EKnownZombieType : uint8
{
	Normal,
	Runner,
	Heavy,
	Unknown
};

inline bool IsWeaponItemType(const EItemType ItemType)
{
	return ItemType == EItemType::Pistol || ItemType == EItemType::Shotgun;
}

inline bool IsUsableWeaponItem(const ABaseItem* Item)
{
	return Item &&
		IsWeaponItemType(Item->GetItemType()) &&
		Item->GetValue() > 0;
}

inline bool InventoryHasUsableWeapon(const UInventoryComponent* Inventory)
{
	if (!Inventory)
	{
		return false;
	}

	for (const ABaseItem* Item : Inventory->GetInventory())
	{
		if (IsUsableWeaponItem(Item))
		{
			return true;
		}
	}

	return false;
}

inline int GetTotalUsableAmmo(const UInventoryComponent* Inventory)
{
	if (!Inventory)
	{
		return 0;
	}

	int TotalAmmo = 0;

	for (const ABaseItem* Item : Inventory->GetInventory())
	{
		if (IsUsableWeaponItem(Item))
		{
			TotalAmmo += FMath::Max(0, Item->GetValue());
		}
	}

	return TotalAmmo;
}

inline EKnownZombieType ClassifyZombie(const AActor* Zombie)
{
	if (!Zombie)
	{
		return EKnownZombieType::Unknown;
	}

	const FString Name = Zombie->GetName();

	if (Name.Contains(TEXT("Runner")))
	{
		return EKnownZombieType::Runner;
	}

	if (Name.Contains(TEXT("Heavy")))
	{
		return EKnownZombieType::Heavy;
	}

	return EKnownZombieType::Normal;
}