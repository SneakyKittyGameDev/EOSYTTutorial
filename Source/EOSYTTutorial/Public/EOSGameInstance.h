// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "EOSGameInstance.generated.h"

/**
 * 
 */
UCLASS()
class EOSYTTUTORIAL_API UEOSGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UEOSGameInstance();

	virtual void Init() override;

	void Login();
	void OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);

	UFUNCTION(BlueprintCallable)
	void CreateSession();
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	UFUNCTION(BlueprintCallable)
	void DestroySession();
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);

	UFUNCTION(BlueprintCallable)
	void GetAllFriends();
	void OnGetAllFriendsComplete(int32 LocalUserNum, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr);

	UFUNCTION(BlueprintCallable)
	void ShowInviteUI();
	UFUNCTION(BlueprintCallable)
	void ShowFriendsUI();

protected:
	class IOnlineSubsystem* OnlineSubsystem;

	bool bIsLoggedIn;
};
