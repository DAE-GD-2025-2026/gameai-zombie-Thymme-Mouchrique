#include "StudentPerceptorThymmeMouchrique.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Perception/AIPerceptionComponent.h"

UStudentPerceptor::UStudentPerceptor()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UStudentPerceptor::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT("StudentPerceptor BeginPlay"));

	UAIPerceptionComponent* PerceptionComp = GetOwner()->FindComponentByClass<UAIPerceptionComponent>();

	if (!PerceptionComp)
	{
		UE_LOG(LogTemp, Error, TEXT("No PerceptionComponent found"));
		return;
	}

	PerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &UStudentPerceptor::OnPerceptionUpdated);

	UE_LOG(LogTemp, Warning, TEXT("Bound perception callback"));
}

void UStudentPerceptor::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor) return;

	UE_LOG(LogTemp, Warning, TEXT("Perceived Actor: %s"), *Actor->GetName());

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		UE_LOG(LogTemp, Error, TEXT("OwnerPawn invalid"));
		return;
	}

	AAIController* AIController = Cast<AAIController>(OwnerPawn->GetController());
	if (!AIController)
	{
		UE_LOG(LogTemp, Error, TEXT("AIController invalid"));
		return;
	}

	UBlackboardComponent* Blackboard = AIController->GetBlackboardComponent();
	if (!Blackboard)
	{
		UE_LOG(LogTemp, Error, TEXT("Blackboard invalid"));
		return;
	}

	const bool bIsZombie =
		Actor->GetName().Contains(TEXT("Zombie")) ||
		Actor->GetName().Contains(TEXT("Runner")) ||
		Actor->GetName().Contains(TEXT("Heavy"));

	UE_LOG(LogTemp, Warning, TEXT("IsZombie: %s"), bIsZombie ? TEXT("TRUE") : TEXT("FALSE"));

	if (!bIsZombie) return;

	if (Stimulus.WasSuccessfullySensed())
	{
		UE_LOG(LogTemp, Warning, TEXT("SETTING TargetEnemy"));

		Blackboard->SetValueAsObject(TEXT("TargetEnemy"), Actor);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("CLEARING TargetEnemy"));

		if (Blackboard->GetValueAsObject(TEXT("TargetEnemy")) == Actor)
		{
			Blackboard->ClearValue(TEXT("TargetEnemy"));
		}
	}
}