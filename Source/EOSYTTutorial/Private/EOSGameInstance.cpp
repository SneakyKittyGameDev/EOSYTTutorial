// Fill out your copyright notice in the Description page of Project Settings.


#include "EOSGameInstance.h"

#include "Interfaces/OnlineIdentityInterface.h"
#include "OnlineSubsystem.h"
#include "Kismet/GameplayStatics.h"

UEOSGameInstance::UEOSGameInstance()
{
	bIsLoggedIn = false;
}

void UEOSGameInstance::Init()
{
	Super::Init();

	OnlineSubsystem = IOnlineSubsystem::Get();
	Login();
}

void UEOSGameInstance::Login()
{
	if (OnlineSubsystem)
	{
		if (IOnlineIdentityPtr Identity = OnlineSubsystem->GetIdentityInterface())
		{
			FOnlineAccountCredentials Credentials;
			Credentials.Id = FString();
			Credentials.Token = FString();
			Credentials.Type = FString("accountportal");

			Identity->OnLoginCompleteDelegates->AddUObject(this, &UEOSGameInstance::OnLoginComplete);
			Identity->Login(0, Credentials);
		}
	}
}

void UEOSGameInstance::OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
	UE_LOG(LogTemp, Warning, TEXT("OnLoginComplete Success: %d"), bWasSuccessful);
	
	if (OnlineSubsystem)
	{
		if (IOnlineIdentityPtr Identity = OnlineSubsystem->GetIdentityInterface())
		{
			switch (Identity->GetLoginStatus(0))
			{
			case ELoginStatus::LoggedIn : bIsLoggedIn = true; break;
			case ELoginStatus::NotLoggedIn : bIsLoggedIn = false; break;
			default : bIsLoggedIn = false;
			}
			
			Identity->ClearOnLoginCompleteDelegates(0, this);
		}
	}
}

void UEOSGameInstance::CreateSession()
{
	if (bIsLoggedIn)
	{
		if (OnlineSubsystem)
		{
			if (IOnlineSessionPtr SessionPtr = OnlineSubsystem->GetSessionInterface())
			{
				FOnlineSessionSettings SessionSettings;
				SessionSettings.bIsDedicated = false;
				SessionSettings.bShouldAdvertise = true;
				SessionSettings.bIsLANMatch = true;
				SessionSettings.NumPublicConnections = 5;
				SessionSettings.bAllowJoinInProgress = true;
				SessionSettings.bAllowJoinViaPresence = true;
				SessionSettings.bUsesPresence = true;

				SessionPtr->OnCreateSessionCompleteDelegates.AddUObject(this, &UEOSGameInstance::OnCreateSessionComplete);
				SessionPtr->CreateSession(0, FName("Test Session"), SessionSettings);
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Cannot Create Session: Not Logged In"));
	}
}

void UEOSGameInstance::DestroySession()
{
	if (OnlineSubsystem)
	{
		if (IOnlineSessionPtr SessionPtr = OnlineSubsystem->GetSessionInterface())
		{
			UE_LOG(LogTemp, Warning, TEXT("Destroying Session: Test Session"));
			SessionPtr->DestroySession(FName("Test Session"));
			
		}
	}
}

void UEOSGameInstance::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogTemp, Warning, TEXT("OnCreateSessionComplete: %d"), bWasSuccessful);
	
	if (OnlineSubsystem)
	{
		if (IOnlineSessionPtr SessionPtr = OnlineSubsystem->GetSessionInterface())
		{
			SessionPtr->ClearOnCreateSessionCompleteDelegates(this);
			GetWorld()->ServerTravel(FString("ThirdPersonExampleMap?listen"), false);
		}
	}
}

void UEOSGameInstance::OnFindSessionsComplete(bool bWasSuccessful)
{
	UE_LOG(LogTemp, Warning, TEXT("OnFindSessionsComplete: %d"), bWasSuccessful);
	if (bWasSuccessful)
	{
		UE_LOG(LogTemp, Warning, TEXT("Found Server Count: %d"), SearchSettings->SearchResults.Num());
		if (SearchSettings->SearchResults.Num())
		{
			FOnlineSessionSearchResult SearchResult = SearchSettings->SearchResults[0];
			UE_LOG(LogTemp, Warning, TEXT("Joining Server with Host: %s"), *SearchResult.Session.OwningUserName);
			if (OnlineSubsystem)
			{
				if (IOnlineSessionPtr SessionPtr = OnlineSubsystem->GetSessionInterface())
				{
					SessionPtr->OnJoinSessionCompleteDelegates.AddUObject(this, &UEOSGameInstance::OnJoinSessionComplete);
					SessionPtr->JoinSession(0, FName("Test Session"), SearchResult);
				}
			}
		}
	}
}

void UEOSGameInstance::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type JoinResult)
{
	UE_LOG(LogTemp, Warning, TEXT("OnJoinSessionComplete"));

	if (OnlineSubsystem)
	{
		if (IOnlineSessionPtr SessionPtr = OnlineSubsystem->GetSessionInterface())
		{
			FString ConnectionInfo = FString();
			SessionPtr->GetResolvedConnectString(SessionName, ConnectionInfo);
			UE_LOG(LogTemp, Warning, TEXT("Got Session Pointer"));
			if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
			{
				UE_LOG(LogTemp, Warning, TEXT("Attempting Client Travel"));
				PC->ClientTravel(ConnectionInfo, ETravelType::TRAVEL_Relative, true);
			}
		}
	}
}

void UEOSGameInstance::FindSessions()
{
	if (OnlineSubsystem)
	{
		if (IOnlineSessionPtr SessionPtr = OnlineSubsystem->GetSessionInterface())
		{
			UE_LOG(LogTemp, Warning, TEXT("Finding Sessions"));
			
			SearchSettings = MakeShareable(new FOnlineSessionSearch());
			SearchSettings->bIsLanQuery = true;
			SearchSettings->MaxSearchResults = 10000;
			SearchSettings->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);
			
			SessionPtr->OnFindSessionsCompleteDelegates.AddUObject(this, &UEOSGameInstance::OnFindSessionsComplete);
			SessionPtr->FindSessions(0, SearchSettings.ToSharedRef());
		}
	}
}
