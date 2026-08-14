#include "BTTask_AttackTargetEnemy.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"

#include "Common/InventoryComponent.h"
#include "Items/BaseItem.h"
#include "Items/ItemType.h"

#include "StudentPerceptorThymmeMouchrique.h"
#include "SurvivorAIShared.h"

UBTTask_AttackTargetEnemy::UBTTask_AttackTargetEnemy()
{
	NodeName = TEXT("Attack Target Enemy");

	// task stores last attack time so each AI needs its own copy
	bCreateNodeInstance = true;
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

	if (!Pawn)
	{
		return EBTNodeResult::Failed;
	}

	// only attack if combat decision actually says to attack
	if (!Blackboard->GetValueAsBool(SurvivorBB::ShouldAttackEnemy))
	{
		return EBTNodeResult::Failed;
	}

	AActor* Enemy = Cast<AActor>(
		Blackboard->GetValueAsObject(
			SurvivorBB::TargetEnemy
		)
	);

	if (!Enemy)
	{
		return EBTNodeResult::Failed;
	}

	UInventoryComponent* Inventory = Pawn->FindComponentByClass<UInventoryComponent>();
	UStudentPerceptor* Perceptor = Pawn->FindComponentByClass<UStudentPerceptor>();

	if (!Inventory || !Perceptor)
	{
		return EBTNodeResult::Failed;
	}

	// do not attack enemy if it is only remembered and not actually visible
	if (!Perceptor->IsEnemyCurrentlyVisible(Enemy))
	{
		return EBTNodeResult::Failed;
	}

	FVector EnemyLocation;

	// use position stored by perception instead of reading enemy actor position directly
	if (!Perceptor->GetLastKnownEnemyLocation(Enemy, EnemyLocation))
	{
		return EBTNodeResult::Failed;
	}

	FVector DirectionToEnemy = EnemyLocation - Pawn->GetActorLocation();
	DirectionToEnemy.Z = 0.f;

	const float DistanceToEnemy = DirectionToEnemy.Size();

	if (DirectionToEnemy.IsNearlyZero())
	{
		return EBTNodeResult::Failed;
	}

	DirectionToEnemy.Normalize();

	// face enemy
	Pawn->SetActorRotation(DirectionToEnemy.Rotation());

	const TArray<ABaseItem*>& Items = Inventory->GetInventory();

	// INDEX_NONE is a unreal defined constant for -1, used to indicate no valid index (I find this pretty cool)
	int BestWeaponSlot = INDEX_NONE;
	float BestWeaponScore = TNumericLimits<float>::Lowest();
	float BestWeaponCooldown = 0.35f;

	const EKnownZombieType EnemyType = ClassifyZombie(Enemy);

	// score all usable weapons instead of only checking distance
	for (int SlotIdx = 0; SlotIdx < Items.Num(); ++SlotIdx)
	{
		ABaseItem* Item = Items[SlotIdx];

		if (!IsUsableWeaponItem(Item))
		{
			continue;
		}

		float Score = static_cast<float>(Item->GetValue());
		float Cooldown = 0.35f;

		if (Item->GetItemType() == EItemType::Shotgun)
		{
			// shotgun is better close up
			if (DistanceToEnemy < 450.f)
			{
				Score += 40.f;
			}
			else
			{
				Score -= 20.f;
			}

			// runner getting close is dangerous so shotgun gets bonus
			if (EnemyType == EKnownZombieType::Runner)
			{
				Score += 15.f;
			}

			// heavy takes a lot of ammo so shotgun is less attractive
			if (EnemyType == EKnownZombieType::Heavy)
			{
				Score -= 10.f;
			}

			Cooldown = 0.60f;
		}
		else if (Item->GetItemType() == EItemType::Pistol)
		{
			// pistol is better when enemy is not right on top of player
			if (DistanceToEnemy >= 350.f)
			{
				Score += 25.f;
			}
			else
			{
				Score += 5.f;
			}

			// pistol saves shotgun ammo when fighting heavy
			if (EnemyType == EKnownZombieType::Heavy)
			{
				Score += 5.f;
			}
		}

		// avoid wasting the last shot if another weapon is available
		if (Item->GetValue() <= 1)
		{
			Score -= 15.f;
		}

		if (Score > BestWeaponScore)
		{
			BestWeaponScore = Score;
			BestWeaponSlot = SlotIdx;
			BestWeaponCooldown = Cooldown;
		}
	}

	// no weapon with ammo found
	if (BestWeaponSlot == INDEX_NONE)
	{
		return EBTNodeResult::Failed;
	}

	const float CurrentTime = Pawn->GetWorld()->GetTimeSeconds();

	// stop behavior tree from firing weapon every time task gets executed
	if (CurrentTime - LastAttackTime < BestWeaponCooldown)
	{
		return EBTNodeResult::Failed;
	}

	ABaseItem* Weapon = Items[BestWeaponSlot];

	if (!Weapon)
	{
		return EBTNodeResult::Failed;
	}

	if (!Inventory->UseItem(BestWeaponSlot))
	{
		return EBTNodeResult::Failed;
	}

	LastAttackTime = CurrentTime;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Attacked enemy using: %s"),
		*Weapon->GetName()
	);

	// remove empty weapon after last shot
	if (Weapon->GetValue() <= 0)
	{
		Inventory->RemoveItem(BestWeaponSlot);
	}

	return EBTNodeResult::Succeeded;
}