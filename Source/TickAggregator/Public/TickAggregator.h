// Copyright (c) 2023 Sebastian Ploch

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"


class FTickAggregatorModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
