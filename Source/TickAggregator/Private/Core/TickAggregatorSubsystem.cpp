// Copyright (c) 2023 Sebastian Ploch

#include "Core/TickAggregatorSubsystem.h"

#include "EngineUtils.h"
#include "Core/TickAggregatorRegisterInterface.h"


FTickAggregatorTickFunction::FTickAggregatorTickFunction()
{
	bCanEverTick = true;
	bStartWithTickEnabled = true;
}

bool FTickAggregatorTickFunction::Init(ETickingGroup InTickGroup, UTickAggregatorSubsystem* InOwner, TUniqueFunction<void(float)>&& InTickFunction)
{
	if (!InOwner)
	{
		// TODO add error logging
		return false;
	}
	
	UWorld* world = InOwner->GetWorld();
	if (!world)
	{
		// TODO add error logging
		return false;
	}
	
	TickGroup = InTickGroup;
	Owner = InOwner;
	TickFunction = MoveTemp(InTickFunction);
	checkf(TickFunction, TEXT("Failed to bind tick function"));

	ULevel* currentLevel = world->GetCurrentLevel();
	if (!currentLevel)
	{
		// TODO add error logging
		return false;
	}
	
	RegisterTickFunction(currentLevel);
	return true;
}

void FTickAggregatorTickFunction::Reset()
{
	UnRegisterTickFunction();
	TickFunction.Reset();
	Owner = nullptr;
}

void FTickAggregatorTickFunction::ExecuteTick(float InDeltaTime, ELevelTick InTickType, ENamedThreads::Type InCurrentThread, const FGraphEventRef& InMyCompletionGraphEvent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTickAggregatorTickFunction::ExecuteTick);
	TickFunction(InDeltaTime);
}

FString FTickAggregatorTickFunction::DiagnosticMessage()
{
	return Owner->GetFullName() + TEXT("[TickComponent]");
}

FName FTickAggregatorTickFunction::DiagnosticContext(bool bInDetailed)
{
	return Owner->GetClass()->GetFName();
}

UTickAggregatorSubsystem::UTickAggregatorSubsystem()
{
}

void UTickAggregatorSubsystem::Initialize(FSubsystemCollectionBase& InOutCollection)
{
	Super::Initialize(InOutCollection);

	if (IsTemplate())
	{
		return;
	}

	UWorld* world = GetWorld();
	if (!world)
	{
		return;
	}
	
	RegisterTickFunctions();
	world->OnActorsInitialized.AddUObject(this, &UTickAggregatorSubsystem::OnActorsFinishedInitialise);
}

void UTickAggregatorSubsystem::Deinitialize()
{
	UnregisterTickFunctions();
	Super::Deinitialize();
}

bool UTickAggregatorSubsystem::DoesSupportWorldType(const EWorldType::Type InWorldType) const
{
	return InWorldType == EWorldType::PIE || InWorldType == EWorldType::Game;
}

void UTickAggregatorSubsystem::Tick_PrePhysics(float InDeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UTickAggregatorSubsystem::Tick_PrePhysics)

	for (const TObjectPtr<AActor>& actor : GetCompactLinearArrayFromContainers(PrePhysicsContainers))
	{
		actor->Tick(InDeltaTime);
	}
}

void UTickAggregatorSubsystem::Tick_DuringPhysics(float InDeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UTickAggregatorSubsystem::DuringPhysics)

	for (const TObjectPtr<AActor>& actor : GetCompactLinearArrayFromContainers(DuringPhysicsContainers))
	{
		actor->Tick(InDeltaTime);
	}
}

void UTickAggregatorSubsystem::Tick_PostPhysics(float InDeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UTickAggregatorSubsystem::PostPhysics)
	
	for (const TObjectPtr<AActor>& actor : GetCompactLinearArrayFromContainers(PostPhysicsContainers))
	{
		actor->Tick(InDeltaTime);
	}
}

void UTickAggregatorSubsystem::Tick_PostUpdateWork(float InDeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UTickAggregatorSubsystem::PostUpdateWork)

	for (const TObjectPtr<AActor>& actor : GetCompactLinearArrayFromContainers(PostUpdateWorkContainers))
	{
		actor->Tick(InDeltaTime);
	}
}

void UTickAggregatorSubsystem::RegisterActor(AActor* InActor)
{
	if (!InActor->PrimaryActorTick.bCanEverTick || !InActor->PrimaryActorTick.bStartWithTickEnabled)
	{
		// TODO add info logging
		return;
	}
	
	InActor->SetActorTickEnabled(false);
	InActor->PrimaryActorTick.bCanEverTick = false;
	InActor->PrimaryActorTick.bStartWithTickEnabled = false;

	AddToContainer(InActor);

	InActor->OnEndPlay.AddDynamic(this, &UTickAggregatorSubsystem::OnActorEndPlay);
}

void UTickAggregatorSubsystem::UnregisterActor(AActor* InActor)
{
	RemoveFromContainer(InActor);
}

void UTickAggregatorSubsystem::AddToContainer(AActor* InActor)
{
	if (!InActor)
	{
		// TODO add error logging
		return;
	}

	const FName actorClass = InActor->GetClass()->GetFName();
	const ETickingGroup tickGroup = InActor->PrimaryActorTick.TickGroup;

	if (FTickAggregateContainer* container = FindContainer(actorClass, tickGroup))
	{
		container->Actors.Add(InActor);
	}
	// Container doesn't exist, create a new one
	else
	{
		FTickAggregateContainer newContainer;
		newContainer.ActorClass = actorClass;
		newContainer.TickGroup = tickGroup;
		newContainer.Actors.Add(InActor);
		
		switch (tickGroup)
		{
		case TG_PrePhysics:
			PrePhysicsContainers.Add(MoveTemp(newContainer));
			break;
		case TG_DuringPhysics:
			DuringPhysicsContainers.Add(MoveTemp(newContainer));
			break;
		case TG_PostPhysics:
			PostPhysicsContainers.Add(MoveTemp(newContainer));
			break;
		case TG_PostUpdateWork:
			PostUpdateWorkContainers.Add(MoveTemp(newContainer));
			break;
		default:
			checkNoEntry(); // unhandled case
			break;
		}
	}
}

void UTickAggregatorSubsystem::RemoveFromContainer(AActor* InActor)
{
	if (!InActor)
	{
		// TODO add error logging
		return;
	}

	const FName actorClass = InActor->GetClass()->GetFName();
	const ETickingGroup tickGroup = InActor->PrimaryActorTick.TickGroup;

	FTickAggregateContainer* container = FindContainer(actorClass, tickGroup);
	if (!container)
	{
		return;
	}
	
	container->Actors.Remove(InActor);
}

FTickAggregateContainer* UTickAggregatorSubsystem::FindContainer(AActor* InActor)
{
	if (!InActor)
	{
		// TODO add error logging
		return nullptr;
	}
	
	const FName actorClass = InActor->GetClass()->GetFName();
	const ETickingGroup tickGroup = InActor->PrimaryActorTick.TickGroup;

	return FindContainer(actorClass, tickGroup);
}

FTickAggregateContainer* UTickAggregatorSubsystem::FindContainer(FName InActorClass, ETickingGroup InTickGroup)
{
	switch (InTickGroup)
	{
	case TG_PrePhysics:
		return PrePhysicsContainers.Find({InActorClass, InTickGroup});
	case TG_DuringPhysics:
		return DuringPhysicsContainers.Find({InActorClass, InTickGroup});
	case TG_PostPhysics:
		return PostPhysicsContainers.Find({InActorClass, InTickGroup});
	case TG_PostUpdateWork:
		return PostUpdateWorkContainers.Find({InActorClass, InTickGroup});
	default:
		checkNoEntry(); // unhandled case
		return nullptr;
	}
}

bool UTickAggregatorSubsystem::IsActorRegistered(AActor* InActor) const
{
	// const FTickAggregateContainer* container = Containers.Find(InActor->StaticClass()->GetClassPathName());
	// if (!container)
	// {
	// 	return false;
	// }
	//
	// return container->Actors.Find(InActor) != INDEX_NONE;
	// TODO implement this
	return false; 
}

void UTickAggregatorSubsystem::OnActorsFinishedInitialise(const UWorld::FActorsInitializedParams& InParams)
{
	const UWorld* world = GetWorld();
	check(InParams.World == world);

	for (TActorIterator<AActor> It(world); It; ++It)
	{
		AActor* actor = *It;
		if (!actor)
		{
			continue;
		}
		
		if (actor->Implements<UTickAggregatorRegisterInterface>() || actor->FindComponentByInterface<UTickAggregatorRegisterInterface>())
		{
			RegisterActor(actor);
		}
	}
}

void UTickAggregatorSubsystem::OnActorEndPlay(AActor* InActor, EEndPlayReason::Type InEndPlayReason)
{
	if (!InActor)
	{
		return;
	}

	UnregisterActor(InActor);
}

void UTickAggregatorSubsystem::RegisterTickFunctions()
{
	PrePhysicsTickFunction.Init(TG_PrePhysics, this, [this](float InDeltaTime) -> void {UTickAggregatorSubsystem::Tick_PrePhysics(InDeltaTime);});
	DuringPhysicsTickFunction.Init(TG_DuringPhysics, this, [this](float InDeltaTime) -> void {UTickAggregatorSubsystem::Tick_DuringPhysics(InDeltaTime);});
	PostPhysicsTickFunction.Init(TG_PostPhysics, this, [this](float InDeltaTime) -> void {UTickAggregatorSubsystem::Tick_PostPhysics(InDeltaTime);});
	PostUpdateWorkTickFunction.Init(TG_PostUpdateWork, this, [this](float InDeltaTime) -> void {UTickAggregatorSubsystem::Tick_PostUpdateWork(InDeltaTime);});
}

void UTickAggregatorSubsystem::UnregisterTickFunctions()
{
	PrePhysicsTickFunction.Reset();
	DuringPhysicsTickFunction.Reset();
	PostPhysicsTickFunction.Reset();
	PostUpdateWorkTickFunction.Reset();
}

TArray<TObjectPtr<AActor>> UTickAggregatorSubsystem::GetCompactLinearArrayFromContainers(const TSet<FTickAggregateContainer>& InContainers)
{
	// TODO add some amount caching per type to reserve space beforehand?
	TArray<TObjectPtr<AActor>> output;
	for (const FTickAggregateContainer& container : InContainers)
	{
		output.Append(container.Actors);
	}

	return output;
}
