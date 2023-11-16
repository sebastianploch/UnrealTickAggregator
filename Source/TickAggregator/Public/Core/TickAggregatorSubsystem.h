// Copyright (c) 2023 Sebastian Ploch

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TickAggregatorSubsystem.generated.h"


struct FTickAggregatorTickFunction final : public FTickFunction
{
	class UTickAggregatorSubsystem* Owner {nullptr};
	TUniqueFunction<void(float)> TickFunction;

public:
	FTickAggregatorTickFunction();

	bool Init(ETickingGroup InTickGroup, class UTickAggregatorSubsystem* InOwner, TUniqueFunction<void(float)>&& InTickFunction);
	void Reset();
	
	//~ FTickFunction Interface
	virtual void ExecuteTick(float  InDeltaTime, ELevelTick InTickType, ENamedThreads::Type InCurrentThread, const FGraphEventRef& InMyCompletionGraphEvent) override;
	virtual FString DiagnosticMessage() override;
	virtual FName DiagnosticContext(bool bInDetailed)  override;
	//~ FTickFunction Interface
};

template<>
struct TStructOpsTypeTraits<FTickAggregatorTickFunction> : public TStructOpsTypeTraitsBase2<FTickAggregatorTickFunction>
{
	enum
	{
		WithCopy = false
	};
};

struct FTickAggregateContainer
{
	FName ActorClass {NAME_None};
	ETickingGroup TickGroup {TG_PrePhysics};
	TArray<TObjectPtr<AActor>> Actors {};

public:
	bool operator==(const FTickAggregateContainer& InRHS) const
	{
		return ActorClass == InRHS.ActorClass && TickGroup == InRHS.TickGroup;
	}
	
	friend inline uint32 GetTypeHash(const FTickAggregateContainer& InKey)
	{
		return GetTypeHash(InKey.ActorClass);
	}
};

/**
 * 
 */
UCLASS()
class TICKAGGREGATOR_API UTickAggregatorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	UTickAggregatorSubsystem();
	
	virtual void Initialize(FSubsystemCollectionBase& InOutCollection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type InWorldType) const override;

	UFUNCTION(BlueprintCallable, Category="TickAggregator")
	bool IsActorRegistered(AActor* InActor) const;

	void Tick_PrePhysics(float InDeltaTime);
	void Tick_DuringPhysics(float InDeltaTime);
	void Tick_PostPhysics(float InDeltaTime);
	void Tick_PostUpdateWork(float InDeltaTime);

private:
	void RegisterActor(AActor* InActor);
	void UnregisterActor(AActor* InActor);

	void AddToContainer(AActor* InActor);
	void RemoveFromContainer(AActor* InActor);
	
	FTickAggregateContainer* FindContainer(AActor* InActor);
	FTickAggregateContainer* FindContainer(FName InActorClass, ETickingGroup InTickGroup);
	
	void OnActorsFinishedInitialise(const UWorld::FActorsInitializedParams& InParams);
	
	UFUNCTION()
	void OnActorEndPlay(AActor* InActor , EEndPlayReason::Type InEndPlayReason);

	void RegisterTickFunctions();
	void UnregisterTickFunctions();

	static TArray<TObjectPtr<AActor>> GetCompactLinearArrayFromContainers(const TSet<FTickAggregateContainer>& InContainers);
	
private:
	FTickAggregatorTickFunction PrePhysicsTickFunction;
	FTickAggregatorTickFunction DuringPhysicsTickFunction;
	FTickAggregatorTickFunction PostPhysicsTickFunction;
	FTickAggregatorTickFunction PostUpdateWorkTickFunction;
	
	TSet<FTickAggregateContainer> PrePhysicsContainers;
	TSet<FTickAggregateContainer> DuringPhysicsContainers;
	TSet<FTickAggregateContainer> PostPhysicsContainers;
	TSet<FTickAggregateContainer> PostUpdateWorkContainers;
};
