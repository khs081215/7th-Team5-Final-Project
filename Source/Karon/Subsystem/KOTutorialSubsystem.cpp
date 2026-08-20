// Fill out your copyright notice in the Description page of Project Settings.


#include "KOTutorialSubsystem.h"

#include "GMRouterSubsystem.h"
#include "Data/KODataTableTypes.h"
#include "Data/Character/Enemy/KOEnemyDeveloperSettings.h"
#include "Engine/AssetManager.h"
#include "Tests/AutomationCommon.h"
#include "UI/KOUISubsystem.h"

UKOTutorialSubsystem* UKOTutorialSubsystem::Get(UObject* WorldContext)
{
	if (!WorldContext||!WorldContext->GetWorld())
	{
		return nullptr;
	}
	UGameInstance* GI = WorldContext->GetWorld()->GetGameInstance();
	if (!GI)
	{
		return nullptr;
	}
    
	return GI->GetSubsystem<UKOTutorialSubsystem>();
}

void UKOTutorialSubsystem::Preload(UObject* WorldContext, FName TutorialName)
{
	if (VideoMap.Contains(TutorialName))
	{
		if (VideoMap[TutorialName].VideoSource.IsValid())
		{
			SetTutorial(WorldContext,TutorialName);
			return;	
		}
		if (!VideoMap[TutorialName].VideoSource.IsNull())
		{
			FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
			TSharedPtr<FStreamableHandle> StreamingHandle = Streamable.RequestAsyncLoad(
			VideoMap[TutorialName].VideoSource.ToSoftObjectPath(),
			FStreamableDelegate::CreateUObject(this, &ThisClass::SetTutorial,WorldContext, TutorialName)
			);
			StreamableHandles.Add(TutorialName,StreamingHandle);
		}
	}
}

void UKOTutorialSubsystem::PreloadAll(UObject* WorldContext)
{
	for (auto& VideoData : VideoMap)
	{
		if (!StreamableHandles.Contains(VideoData.Key))
		{
			FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
			TSharedPtr<FStreamableHandle> StreamingHandle = Streamable.RequestAsyncLoad(
			VideoData.Value.VideoSource.ToSoftObjectPath()
			);
			StreamableHandles.Add(VideoData.Key,StreamingHandle);
		}
	}
}

void UKOTutorialSubsystem::UnloadAll(UObject* WorldContext)
{
	for (auto& StreamData:StreamableHandles)
	{
		StreamData.Value->ReleaseHandle();
		StreamData.Value.Reset();
	}
	StreamableHandles.Reset();
}

void UKOTutorialSubsystem::SetTutorial(UObject* WorldContext, FName TutorialName)
{
	UKOUISubsystem* UISubsystem=UKOUISubsystem::Get(WorldContext);
	if (!UISubsystem||!VideoMap.Contains(TutorialName))
	{
		return;
	}
	
	UKOUISubsystem::OpenWidget(this, KOGameplayTags::UI_Widget_Guide);
	UGMRouterSubsystem::BroadcastMessage(GetWorld(),
				KOGameplayTags::Event_TutorialVideo,
				FInstancedStruct::Make(VideoMap[TutorialName]));
}

void UKOTutorialSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const UKOEnemyDeveloperSettings* DeveloperSettings = GetDefault<UKOEnemyDeveloperSettings>();
	if(!IsValid(DeveloperSettings))
	{
		return;
	}
	
	UDataTable* DT = DeveloperSettings->TutorialDataTable.LoadSynchronous();
	
	if (IsValid(DT))
	{
		DT->ForeachRow<FKOTutorialRow>(TEXT("KOTutorial Init"), 
			[this](const FName& Key, const FKOTutorialRow& Value)
			{
				VideoMap.Add(Key,Value.VideoData);
			});
	}
}
