// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineUserEOSPlus.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemEOSPlus.h"
#include "EOSSettings.h"

enum class EOSSValue : uint8
{
	Null,
	Steam,
	PS4,
	XboxLive,
	Switch,
	Apple
};

static inline EOSSValue ToEOSSValue(FName OSSName)
{
	if (OSSName == STEAM_SUBSYSTEM)
	{
		return EOSSValue::Steam;
	}
	else if (OSSName == PS4_SUBSYSTEM)
	{
		return EOSSValue::PS4;
	}
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	else if (OSSName == LIVE_SUBSYSTEM)
	{
		return EOSSValue::XboxLive;
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	else if (OSSName == SWITCH_SUBSYSTEM)
	{
		return EOSSValue::Switch;
	}
	else if (OSSName == APPLE_SUBSYSTEM)
	{
		return EOSSValue::Apple;
	}
	return EOSSValue::Null;
}

static inline FName ToOSSName(EOSSValue OSSValue)
{
	if (OSSValue == EOSSValue::Steam)
	{
		return STEAM_SUBSYSTEM;
	}
	else if (OSSValue == EOSSValue::PS4)
	{
		return PS4_SUBSYSTEM;
	}
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	else if (OSSValue == EOSSValue::XboxLive)
	{
		return LIVE_SUBSYSTEM;
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	else if (OSSValue == EOSSValue::Switch)
	{
		return SWITCH_SUBSYSTEM;
	}
	else if (OSSValue == EOSSValue::Apple)
	{
		return APPLE_SUBSYSTEM;
	}
	return NULL_SUBSYSTEM;
}

inline FString BuildEOSPlusStringId(FUniqueNetIdPtr InBaseUniqueNetId, FUniqueNetIdPtr InEOSUniqueNetId)
{
	FString StrId = InBaseUniqueNetId.IsValid() ? InBaseUniqueNetId->ToString() : TEXT("");
	StrId += TEXT("_+_");
	StrId += InEOSUniqueNetId.IsValid() ? InEOSUniqueNetId->ToString() : TEXT("");
	return StrId;
}

FUniqueNetIdEOSPlus::FUniqueNetIdEOSPlus(FUniqueNetIdPtr InBaseUniqueNetId, FUniqueNetIdPtr InEOSUniqueNetId)
	: FUniqueNetIdString(BuildEOSPlusStringId(InBaseUniqueNetId, InEOSUniqueNetId), FName("EOSPlus"))
	, BaseUniqueNetId(InBaseUniqueNetId)
	, EOSUniqueNetId(InEOSUniqueNetId)
{
	int32 TotalBytes = GetSize();
	RawBytes.Empty(TotalBytes);
	RawBytes.AddZeroed(TotalBytes);

	if (EOSUniqueNetId.IsValid())
	{
		int32 EOSSize = EOSUniqueNetId->GetSize();
		FMemory::Memcpy(RawBytes.GetData(), EOSUniqueNetId->GetBytes(), EOSSize);
	}

	int32 Offset = EOS_NETID_BYTE_SIZE;
	// Default to the NULL OSS
	*(RawBytes.GetData() + Offset) = (uint8)EOSSValue::Null;

	if (BaseUniqueNetId.IsValid())
	{
		// For crossplatform support, identify the source
		*(RawBytes.GetData() + EOS_NETID_BYTE_SIZE) = (uint8)ToEOSSValue(BaseUniqueNetId->GetType());
		Offset += BASE_NETID_TYPE_SIZE;

		int32 BaseSize = BaseUniqueNetId->GetSize();
		// Always copy above the EOS ID
		FMemory::Memcpy(RawBytes.GetData() + Offset, BaseUniqueNetId->GetBytes(), BaseSize);
	}
}

const uint8* FUniqueNetIdEOSPlus::GetBytes() const
{
	return RawBytes.GetData();
}

int32 FUniqueNetIdEOSPlus::GetSize() const
{
	// Always account for EOS ID
	int32 Size = EOS_NETID_BYTE_SIZE + BASE_NETID_TYPE_SIZE;
	if (BaseUniqueNetId.IsValid())
	{
		Size += BaseUniqueNetId->GetSize();
	}
	return Size;
}

bool FUniqueNetIdEOSPlus::IsValid() const
{
	return BaseUniqueNetId.IsValid() || EOSUniqueNetId.IsValid();
}

FOnlineUserEOSPlus::FOnlineUserEOSPlus(FOnlineSubsystemEOSPlus* InSubsystem)
	: EOSPlus(InSubsystem)
{
	BaseUserInterface = EOSPlus->BaseOSS->GetUserInterface(); //We don't check it here, since some platforms might not implement it

	BaseIdentityInterface = EOSPlus->BaseOSS->GetIdentityInterface();
	check(BaseIdentityInterface.IsValid());
	EOSIdentityInterface = EOSPlus->EosOSS->GetIdentityInterface();
	check(EOSIdentityInterface.IsValid());
	BaseFriendsInterface = EOSPlus->BaseOSS->GetFriendsInterface();
	check(BaseFriendsInterface.IsValid());
	EOSFriendsInterface = EOSPlus->EosOSS->GetFriendsInterface();
	check(EOSFriendsInterface.IsValid());
	BasePresenceInterface = EOSPlus->BaseOSS->GetPresenceInterface();
	check(BasePresenceInterface.IsValid());
	EOSPresenceInterface = EOSPlus->EosOSS->GetPresenceInterface();
	check(EOSPresenceInterface.IsValid());

	BaseFriendsInterface->AddOnInviteReceivedDelegate_Handle(FOnInviteReceivedDelegate::CreateRaw(this, &FOnlineUserEOSPlus::OnInviteReceived));
	BaseFriendsInterface->AddOnInviteAcceptedDelegate_Handle(FOnInviteAcceptedDelegate::CreateRaw(this, &FOnlineUserEOSPlus::OnInviteAccepted));
	BaseFriendsInterface->AddOnInviteRejectedDelegate_Handle(FOnInviteRejectedDelegate::CreateRaw(this, &FOnlineUserEOSPlus::OnInviteRejected));
	BaseFriendsInterface->AddOnInviteAbortedDelegate_Handle(FOnInviteAbortedDelegate::CreateRaw(this, &FOnlineUserEOSPlus::OnInviteAborted));
	BaseFriendsInterface->AddOnFriendRemovedDelegate_Handle(FOnFriendRemovedDelegate::CreateRaw(this, &FOnlineUserEOSPlus::OnFriendRemoved));
	EOSFriendsInterface->AddOnInviteReceivedDelegate_Handle(FOnInviteReceivedDelegate::CreateRaw(this, &FOnlineUserEOSPlus::OnInviteReceived));
	EOSFriendsInterface->AddOnInviteAcceptedDelegate_Handle(FOnInviteAcceptedDelegate::CreateRaw(this, &FOnlineUserEOSPlus::OnInviteAccepted));
	EOSFriendsInterface->AddOnInviteRejectedDelegate_Handle(FOnInviteRejectedDelegate::CreateRaw(this, &FOnlineUserEOSPlus::OnInviteRejected));
	EOSFriendsInterface->AddOnInviteAbortedDelegate_Handle(FOnInviteAbortedDelegate::CreateRaw(this, &FOnlineUserEOSPlus::OnInviteAborted));
	EOSFriendsInterface->AddOnFriendRemovedDelegate_Handle(FOnFriendRemovedDelegate::CreateRaw(this, &FOnlineUserEOSPlus::OnFriendRemoved));
	// Only rebroadcast the platform notifications
	BasePresenceInterface->AddOnPresenceReceivedDelegate_Handle(FOnPresenceReceivedDelegate::CreateRaw(this, &FOnlineUserEOSPlus::OnPresenceReceived));
	BasePresenceInterface->AddOnPresenceArrayUpdatedDelegate_Handle(FOnPresenceArrayUpdatedDelegate::CreateRaw(this, &FOnlineUserEOSPlus::OnPresenceArrayUpdated));

	BaseIdentityInterface->AddOnLoginChangedDelegate_Handle(FOnLoginChangedDelegate::CreateRaw(this, &FOnlineUserEOSPlus::OnLoginChanged));
	BaseIdentityInterface->AddOnControllerPairingChangedDelegate_Handle(FOnControllerPairingChangedDelegate::CreateRaw(this, &FOnlineUserEOSPlus::OnControllerPairingChanged));
	for (int32 LocalUserNum = 0; LocalUserNum < MAX_LOCAL_PLAYERS; LocalUserNum++)
	{
		BaseIdentityInterface->AddOnLoginStatusChangedDelegate_Handle(LocalUserNum, FOnLoginStatusChangedDelegate::CreateRaw(this, &FOnlineUserEOSPlus::OnLoginStatusChanged));
		EOSIdentityInterface->AddOnLoginCompleteDelegate_Handle(LocalUserNum, FOnLoginCompleteDelegate::CreateRaw(this, &FOnlineUserEOSPlus::OnLoginComplete));
		BaseIdentityInterface->AddOnLoginCompleteDelegate_Handle(LocalUserNum, FOnLoginCompleteDelegate::CreateRaw(this, &FOnlineUserEOSPlus::OnBaseLoginComplete));
		BaseIdentityInterface->AddOnLogoutCompleteDelegate_Handle(LocalUserNum, FOnLogoutCompleteDelegate::CreateRaw(this, &FOnlineUserEOSPlus::OnLogoutComplete));

		BaseFriendsInterface->AddOnFriendsChangeDelegate_Handle(LocalUserNum, FOnFriendsChangeDelegate::CreateRaw(this, &FOnlineUserEOSPlus::OnFriendsChanged));
		EOSFriendsInterface->AddOnFriendsChangeDelegate_Handle(LocalUserNum, FOnFriendsChangeDelegate::CreateRaw(this, &FOnlineUserEOSPlus::OnFriendsChanged));
		BaseFriendsInterface->AddOnOutgoingInviteSentDelegate_Handle(LocalUserNum, FOnOutgoingInviteSentDelegate::CreateRaw(this, &FOnlineUserEOSPlus::OnOutgoingInviteSent));
		EOSFriendsInterface->AddOnOutgoingInviteSentDelegate_Handle(LocalUserNum, FOnOutgoingInviteSentDelegate::CreateRaw(this, &FOnlineUserEOSPlus::OnOutgoingInviteSent));
	}
}

FOnlineUserEOSPlus::~FOnlineUserEOSPlus()
{
	BaseIdentityInterface->ClearOnLoginChangedDelegates(this);
	BaseIdentityInterface->ClearOnControllerPairingChangedDelegates(this);
	BaseFriendsInterface->ClearOnInviteReceivedDelegates(this);
	BaseFriendsInterface->ClearOnInviteAcceptedDelegates(this);
	BaseFriendsInterface->ClearOnInviteRejectedDelegates(this);
	BaseFriendsInterface->ClearOnInviteAbortedDelegates(this);
	BaseFriendsInterface->ClearOnFriendRemovedDelegates(this);
	EOSFriendsInterface->ClearOnInviteReceivedDelegates(this);
	EOSFriendsInterface->ClearOnInviteAcceptedDelegates(this);
	EOSFriendsInterface->ClearOnInviteRejectedDelegates(this);
	EOSFriendsInterface->ClearOnInviteAbortedDelegates(this);
	EOSFriendsInterface->ClearOnFriendRemovedDelegates(this);
	BasePresenceInterface->ClearOnPresenceReceivedDelegates(this);
	BasePresenceInterface->ClearOnPresenceArrayUpdatedDelegates(this);

	for (int32 LocalUserNum = 0; LocalUserNum < MAX_LOCAL_PLAYERS; LocalUserNum++)
	{
		if (BaseUserInterface.IsValid())
		{
			BaseUserInterface->ClearOnQueryUserInfoCompleteDelegates(LocalUserNum, this);
		}

		BaseIdentityInterface->ClearOnLoginStatusChangedDelegates(LocalUserNum, this);
		BaseIdentityInterface->ClearOnLoginCompleteDelegates(LocalUserNum, this);
		BaseIdentityInterface->ClearOnLogoutCompleteDelegates(LocalUserNum, this);

		BaseFriendsInterface->ClearOnFriendsChangeDelegates(LocalUserNum, this);
		EOSFriendsInterface->ClearOnFriendsChangeDelegates(LocalUserNum, this);
		BaseFriendsInterface->ClearOnOutgoingInviteSentDelegates(LocalUserNum, this);
		EOSFriendsInterface->ClearOnOutgoingInviteSentDelegates(LocalUserNum, this);
	}
}

FUniqueNetIdEOSPlusPtr FOnlineUserEOSPlus::GetNetIdPlus(const FString& SourceId) const
{
	if (NetIdPlusToNetIdPlus.Contains(SourceId))
	{
		return NetIdPlusToNetIdPlus[SourceId];
	}
	if (BaseNetIdToNetIdPlus.Contains(SourceId))
	{
		return BaseNetIdToNetIdPlus[SourceId];
	}
	if (EOSNetIdToNetIdPlus.Contains(SourceId))
	{
		return EOSNetIdToNetIdPlus[SourceId];
	}
	return nullptr;
}

FUniqueNetIdPtr FOnlineUserEOSPlus::GetBaseNetId(const FString& SourceId) const
{
	if (NetIdPlusToBaseNetId.Contains(SourceId))
	{
		return NetIdPlusToBaseNetId[SourceId];
	}
	return nullptr;
}

FUniqueNetIdPtr FOnlineUserEOSPlus::GetEOSNetId(const FString& SourceId) const
{
	if (NetIdPlusToEOSNetId.Contains(SourceId))
	{
		return NetIdPlusToEOSNetId[SourceId];
	}
	return nullptr;
}

void FOnlineUserEOSPlus::Initialize()
{
	if (BaseUserInterface.IsValid())
	{
		for (int32 LocalUserNum = 0; LocalUserNum < MAX_LOCAL_PLAYERS; LocalUserNum++)
		{
			BaseUserInterface->AddOnQueryUserInfoCompleteDelegate_Handle(LocalUserNum, FOnQueryUserInfoCompleteDelegate::CreateThreadSafeSP(this, &FOnlineUserEOSPlus::OnQueryUserInfoCompleteBase));
		}
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("[FOnlineUserEOSPlus::Initialize] BaseUserInterface delegates not bound. Base interface not valid"));
	}
}

// IOnlineUser Interface

bool FOnlineUserEOSPlus::QueryUserInfo(int32 LocalUserNum, const TArray<FUniqueNetIdRef>& UserIds)
{
	TArray< FUniqueNetIdRef > BaseUserIds;
	if (BaseUserInterface.IsValid())
	{
		bool bArePlayerIdsValid = true;
		for (const FUniqueNetIdRef& UserId : UserIds)
		{
			FUniqueNetIdEOSPlusPtr NetIdPlus = GetNetIdPlus(UserId->ToString());
			if (NetIdPlus.IsValid())
			{
				const bool bIsBaseNetIdValid = ensure(NetIdPlus->GetBaseNetId().IsValid());
				if (bIsBaseNetIdValid)
				{
					BaseUserIds.Add(NetIdPlus->GetBaseNetId().ToSharedRef());
				}
				else
				{
					UE_LOG_ONLINE(Warning, TEXT("[FOnlineUserEOSPlus::QueryUserInfo] Unable to call method in base interface. Base id not valid for user (%s)."), *UserId->ToDebugString());
					bArePlayerIdsValid = false;
					break;
				}
			}
			else
			{
				UE_LOG_ONLINE(Warning, TEXT("[FOnlineUserEOSPlus::QueryUserInfo] Unable to call method in base interface. User not found (%s)."), *UserId->ToDebugString());
				bArePlayerIdsValid = false;
				break;
			}
		}

		if (bArePlayerIdsValid)
		{
			return BaseUserInterface->QueryUserInfo(LocalUserNum, BaseUserIds);
		}
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("[FOnlineUserEOSPlus::QueryUserInfo] Unable to call method in base interface. Base interface not valid."));
	}

	EOSPlus->ExecuteNextTick([this, LocalUserNum, BaseUserIds]() {
		TriggerOnQueryUserInfoCompleteDelegates(LocalUserNum, false, BaseUserIds, TEXT("Unable to call method in base interface."));
		});

	return true;
}

bool FOnlineUserEOSPlus::GetAllUserInfo(int32 LocalUserNum, TArray<TSharedRef<FOnlineUser>>& OutUsers)
{
	bool bResult = false;

	if (BaseUserInterface.IsValid())
	{
		TArray<TSharedRef<FOnlineUser>> BaseUsers;
		bResult = BaseUserInterface->GetAllUserInfo(LocalUserNum, BaseUsers);

		// We construct a list of Plus types to return
		for (const TSharedRef<FOnlineUser>& BaseUser : BaseUsers)
		{
			OutUsers.Add(MakeShareable(new FOnlineUserPlus(BaseUser, nullptr)));
		}
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("[FOnlineUserEOSPlus::GetAllUserInfo] Unable to call method in base interface. Base interface not valid."));
	}

	return bResult;
}

TSharedPtr<FOnlineUser> FOnlineUserEOSPlus::GetUserInfo(int32 LocalUserNum, const FUniqueNetId& UserId)
{
	TSharedPtr<FOnlineUserPlus> Result = nullptr;

	FUniqueNetIdEOSPlusPtr NetIdPlus = GetNetIdPlus(UserId.ToString());
	if (NetIdPlus.IsValid())
	{
		const bool bIsBaseNetIdValid = ensure(NetIdPlus->GetBaseNetId().IsValid());
		const bool bIsBaseUserInterfaceValid = BaseUserInterface.IsValid();
		if (bIsBaseNetIdValid && bIsBaseUserInterfaceValid)
		{
			// We make sure to always return a Plus type
			TSharedPtr<FOnlineUser> BaseResult = BaseUserInterface->GetUserInfo(LocalUserNum, *NetIdPlus->GetBaseNetId());
			if (BaseResult.IsValid())
			{
				Result = MakeShareable(new FOnlineUserPlus(BaseResult, nullptr));
			}
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("[FOnlineUserEOSPlus::GetUserInfo] Unable to call method in base interface. IsBaseNetIdValid=%s IsBaseUserInterfaceValid=%s."), *LexToString(bIsBaseNetIdValid), *LexToString(bIsBaseUserInterfaceValid));
		}
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("[FOnlineUserEOSPlus::GetUserInfo] Unable to call method in base interface. Unknown user (%s)"), *UserId.ToString());
	}

	return Result;
}

bool FOnlineUserEOSPlus::QueryUserIdMapping(const FUniqueNetId& UserId, const FString& DisplayNameOrEmail, const FOnQueryUserMappingComplete& Delegate)
{
	FUniqueNetIdEOSPlusPtr NetIdPlus = GetNetIdPlus(UserId.ToString());
	if (NetIdPlus.IsValid())
	{
		const bool bIsBaseNetIdValid = ensure(NetIdPlus->GetBaseNetId().IsValid());
		const bool bIsBaseUserInterfaceValid = BaseUserInterface.IsValid();
		if (bIsBaseNetIdValid && bIsBaseUserInterfaceValid)
		{
			return BaseUserInterface->QueryUserIdMapping(*NetIdPlus->GetBaseNetId(), DisplayNameOrEmail, Delegate);
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("[FOnlineUserEOSPlus::QueryUserIdMapping] Unable to call method in base interface. IsBaseNetIdValid=%s IsBaseUserInterfaceValid=%s."), *LexToString(bIsBaseNetIdValid), *LexToString(bIsBaseUserInterfaceValid));
		}
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("[FOnlineUserEOSPlus::QueryUserIdMapping] Unable to call method in base interface. Unknown user (%s)"), *UserId.ToString());
	}

	EOSPlus->ExecuteNextTick([this, NetIdPlus, DisplayNameOrEmail, Delegate]() {
		Delegate.ExecuteIfBound(false, *NetIdPlus, DisplayNameOrEmail, *FUniqueNetIdEOSPlus::EmptyId(), TEXT("Unable to call method in base interface."));
		});

	return true;
}

bool FOnlineUserEOSPlus::QueryExternalIdMappings(const FUniqueNetId& UserId, const FExternalIdQueryOptions& QueryOptions, const TArray<FString>& ExternalIds, const FOnQueryExternalIdMappingsComplete& Delegate)
{
	FUniqueNetIdEOSPlusPtr NetIdPlus = GetNetIdPlus(UserId.ToString());
	if (NetIdPlus.IsValid())
	{
		const bool bIsBaseNetIdValid = ensure(NetIdPlus->GetBaseNetId().IsValid());
		const bool bIsBaseUserInterfaceValid = BaseUserInterface.IsValid();
		if (bIsBaseNetIdValid && bIsBaseUserInterfaceValid)
		{
			return BaseUserInterface->QueryExternalIdMappings(*NetIdPlus->GetBaseNetId(), QueryOptions, ExternalIds, Delegate);
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("[FOnlineUserEOSPlus::QueryExternalIdMappings] Unable to call method in base interface. IsBaseNetIdValid=%s IsBaseUserInterfaceValid=%s."), *LexToString(bIsBaseNetIdValid), *LexToString(bIsBaseUserInterfaceValid));
		}
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("[FOnlineUserEOSPlus::QueryExternalIdMappings] Unable to call method in base interface. Unknown user (%s)"), *UserId.ToString());
	}

	EOSPlus->ExecuteNextTick([this, NetIdPlus, QueryOptions, ExternalIds, Delegate]() {
		Delegate.ExecuteIfBound(false, *NetIdPlus, QueryOptions, ExternalIds, TEXT("Unable to call method in base interface."));
	});

	return true;
}

void FOnlineUserEOSPlus::GetExternalIdMappings(const FExternalIdQueryOptions& QueryOptions, const TArray<FString>& ExternalIds, TArray<FUniqueNetIdPtr>& OutIds)
{
	if (BaseUserInterface.IsValid())
	{
		// We don't return Plus ids here because we want external id types
		BaseUserInterface->GetExternalIdMappings(QueryOptions, ExternalIds, OutIds);
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("[FOnlineUserEOSPlus::GetExternalIdMappings] Unable to call method in base interface. Base interface not valid."));
	}
}

FUniqueNetIdPtr FOnlineUserEOSPlus::GetExternalIdMapping(const FExternalIdQueryOptions& QueryOptions, const FString& ExternalId)
{
	FUniqueNetIdPtr Result = nullptr;

	if (BaseUserInterface.IsValid())
	{
		// We don't return a Plus id here because we want external id types
		Result = BaseUserInterface->GetExternalIdMapping(QueryOptions, ExternalId);
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("[FOnlineUserEOSPlus::GetExternalIdMapping] Unable to call method in base interface. Base interface not valid."));
	}

	return Result;
}

void FOnlineUserEOSPlus::OnQueryUserInfoCompleteBase(int32 LocalUserNum, bool bWasSuccessful, const TArray< FUniqueNetIdRef >& UserIds, const FString& ErrorStr)
{
	TArray< FUniqueNetIdRef > PlusUserIds;

	if (bWasSuccessful)
	{
		// We'll build a list of PlusUserIds from the UserIds we can find
		for (const FUniqueNetIdRef& UserId : UserIds)
		{
			FUniqueNetIdEOSPlusPtr NetIdPlus = GetNetIdPlus(UserId->ToString());
			if (NetIdPlus.IsValid())
			{
				PlusUserIds.Add(NetIdPlus.ToSharedRef());
			}
			else
			{
				UE_LOG_ONLINE(Warning, TEXT("[FOnlineUserEOSPlus::OnQueryUserInfoCompleteBase] User not found (%s)."), *UserId->ToDebugString());
			}
		}
	}

	TriggerOnQueryUserInfoCompleteDelegates(LocalUserNum, bWasSuccessful, PlusUserIds, ErrorStr);
}

// ~IOnlineUser Interface

bool FOnlineUserEOSPlus::Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials)
{
	LocalUserNumToLastLoginCredentials.Emplace(LocalUserNum, MakeShared<FOnlineAccountCredentials>(AccountCredentials));

	return BaseIdentityInterface->Login(LocalUserNum, AccountCredentials);
}

void FOnlineUserEOSPlus::OnLoginChanged(int32 LocalUserNum)
{
	TriggerOnLoginChangedDelegates(LocalUserNum);
}

void FOnlineUserEOSPlus::OnEOSLoginChanged(int32 LocalUserNum)
{
	const FEOSSettings& EOSSettings = UEOSSettings::GetSettings();

	if (!EOSSettings.bUseEAS && !EOSSettings.bUseEOSConnect)
	{
		return;
	}

	ELoginStatus::Type LoginStatus = EOSIdentityInterface->GetLoginStatus(LocalUserNum);
	if (LoginStatus == ELoginStatus::LoggedIn)
	{
		TriggerOnLoginChangedDelegates(LocalUserNum);
	}
	else if (LoginStatus == ELoginStatus::NotLoggedIn)
	{
		// @todo joeg - should we force a logout of the platform? Things will be broken either way...
		//		Logout(LocalUserNum);
	}
}

void FOnlineUserEOSPlus::OnLoginStatusChanged(int32 LocalUserNum, ELoginStatus::Type OldStatus, ELoginStatus::Type NewStatus, const FUniqueNetId& NewId)
{
	if (NewStatus == ELoginStatus::UsingLocalProfile)
	{
		const FEOSSettings& EOSSettings = UEOSSettings::GetSettings();

		if (!EOSSettings.bUseEAS && !EOSSettings.bUseEOSConnect)
		{
			EOSIdentityInterface->Logout(LocalUserNum);
		}
		TriggerOnLoginStatusChangedDelegates(LocalUserNum, OldStatus, NewStatus, NewId);
	}
}

void FOnlineUserEOSPlus::OnControllerPairingChanged(int32 LocalUserNum, FControllerPairingChangedUserInfo PreviousUser, FControllerPairingChangedUserInfo NewUser)
{
	// @todo joeg - probably needs special handling here, though I think it should be covered by login change
	TriggerOnControllerPairingChangedDelegates(LocalUserNum, PreviousUser, NewUser);
}

void FOnlineUserEOSPlus::OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
	if (!bWasSuccessful)
	{
		TriggerOnLoginCompleteDelegates(LocalUserNum, bWasSuccessful, UserId, Error);
		return;
	}

	AddPlayer(LocalUserNum);
	FUniqueNetIdEOSPlusPtr NetIdPlus = LocalUserNumToNetIdPlus[LocalUserNum];
	check(NetIdPlus.IsValid());

	TriggerOnLoginCompleteDelegates(LocalUserNum, bWasSuccessful, *NetIdPlus, Error);
}

void FOnlineUserEOSPlus::OnBaseLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& ErrorStr)
{
	if (!bWasSuccessful)
	{
		TriggerOnLoginCompleteDelegates(LocalUserNum, bWasSuccessful, UserId, ErrorStr);
		return;
	}

	const FEOSSettings& EOSSettings = UEOSSettings::GetSettings();
	const bool bForward = EOSSettings.bUseEAS || EOSSettings.bUseEOSConnect;
	if (bForward)
	{
		check(LocalUserNumToLastLoginCredentials.Contains(LocalUserNum));
		EOSIdentityInterface->Login(LocalUserNum, *LocalUserNumToLastLoginCredentials[LocalUserNum]);
	}
	else
	{
		AddPlayer(LocalUserNum);
		TriggerOnLoginCompleteDelegates(LocalUserNum, bWasSuccessful, UserId, ErrorStr);
	}
}

void FOnlineUserEOSPlus::OnLogoutComplete(int32 LocalUserNum, bool bWasSuccessful)
{
	// TODO: Make logout work the same way login does, triggering EOS after completion of Base
	TriggerOnLogoutCompleteDelegates(LocalUserNum, bWasSuccessful);
}

void FOnlineUserEOSPlus::AddPlayer(int32 LocalUserNum)
{
	if (LocalUserNumToNetIdPlus.Contains(LocalUserNum))
	{
		RemovePlayer(LocalUserNum);
	}

	FUniqueNetIdPtr BaseNetId = BaseIdentityInterface->GetUniquePlayerId(LocalUserNum);
	FUniqueNetIdPtr EOSNetId = EOSIdentityInterface->GetUniquePlayerId(LocalUserNum);
	FUniqueNetIdEOSPlusPtr PlusNetId = FUniqueNetIdEOSPlus::Create(BaseNetId, EOSNetId);

	BaseNetIdToNetIdPlus.Add(BaseNetId->ToString(), PlusNetId);
	EOSNetIdToNetIdPlus.Add(EOSNetId->ToString(), PlusNetId);
	NetIdPlusToBaseNetId.Add(PlusNetId->ToString(), BaseNetId);
	NetIdPlusToEOSNetId.Add(PlusNetId->ToString(), EOSNetId);
	NetIdPlusToNetIdPlus.Add(PlusNetId->ToString(), PlusNetId);
	LocalUserNumToNetIdPlus.Add(LocalUserNum, PlusNetId);

	// Add the local account
	TSharedPtr<FUserOnlineAccount> BaseAccount = BaseIdentityInterface->GetUserAccount(*BaseNetId);
	TSharedPtr<FUserOnlineAccount> EOSAccount = EOSIdentityInterface->GetUserAccount(*EOSNetId);
	TSharedRef<FOnlineUserAccountPlus> PlusAccount = MakeShared<FOnlineUserAccountPlus>(BaseAccount, EOSAccount);
	NetIdPlusToUserAccountMap.Add(PlusNetId->ToString(), PlusAccount);
}

FUniqueNetIdEOSPlusPtr FOnlineUserEOSPlus::AddRemotePlayer(FUniqueNetIdPtr BaseNetId, FUniqueNetIdPtr EOSNetId)
{
	FUniqueNetIdEOSPlusPtr PlusNetId = FUniqueNetIdEOSPlus::Create(BaseNetId, EOSNetId);

	BaseNetIdToNetIdPlus.Add(BaseNetId->ToString(), PlusNetId);
	EOSNetIdToNetIdPlus.Add(EOSNetId->ToString(), PlusNetId);
	NetIdPlusToBaseNetId.Add(PlusNetId->ToString(), BaseNetId);
	NetIdPlusToEOSNetId.Add(PlusNetId->ToString(), EOSNetId);
	NetIdPlusToNetIdPlus.Add(PlusNetId->ToString(), PlusNetId);

	return PlusNetId;
}

void FOnlineUserEOSPlus::RemovePlayer(int32 LocalUserNum)
{
	if (!LocalUserNumToNetIdPlus.Contains(LocalUserNum))
	{
		// We don't know about this user
		return;
	}

	FUniqueNetIdPtr BaseNetId = BaseIdentityInterface->GetUniquePlayerId(LocalUserNum);
	FUniqueNetIdPtr EOSNetId = EOSIdentityInterface->GetUniquePlayerId(LocalUserNum);
	FUniqueNetIdEOSPlusPtr PlusNetId = LocalUserNumToNetIdPlus[LocalUserNum];

	// Remove the user account first
	TSharedPtr<FOnlineUserAccountPlus> PlusAccount = NetIdPlusToUserAccountMap[PlusNetId->ToString()];
	NetIdPlusToUserAccountMap.Remove(PlusNetId->ToString());

	// Clean up the net id caches
	BaseNetIdToNetIdPlus.Remove(BaseNetId->ToString());
	NetIdPlusToBaseNetId.Remove(PlusNetId->ToString());
	EOSNetIdToNetIdPlus.Remove(EOSNetId->ToString());
	NetIdPlusToEOSNetId.Remove(PlusNetId->ToString());
	LocalUserNumToNetIdPlus.Remove(LocalUserNum);
}

bool FOnlineUserEOSPlus::Logout(int32 LocalUserNum)
{
	// Clean up the cached data for this user
	RemovePlayer(LocalUserNum);

	LocalUserNumToLastLoginCredentials.Remove(LocalUserNum);

	EOSIdentityInterface->Logout(LocalUserNum);
	return BaseIdentityInterface->Logout(LocalUserNum);
}

bool FOnlineUserEOSPlus::AutoLogin(int32 LocalUserNum)
{
	LocalUserNumToLastLoginCredentials.Emplace(LocalUserNum, MakeShared<FOnlineAccountCredentials>(FOnlineAccountCredentials()));

	return BaseIdentityInterface->AutoLogin(LocalUserNum);
}

TSharedPtr<FUserOnlineAccount> FOnlineUserEOSPlus::GetUserAccount(const FUniqueNetId& UserId) const
{
	if (NetIdPlusToUserAccountMap.Contains(UserId.ToString()))
	{
		return NetIdPlusToUserAccountMap[UserId.ToString()];
	}
	return nullptr;
}

TArray<TSharedPtr<FUserOnlineAccount>> FOnlineUserEOSPlus::GetAllUserAccounts() const
{
	TArray<TSharedPtr<FUserOnlineAccount>> Result;

	for (TMap<FString, TSharedRef<FOnlineUserAccountPlus>>::TConstIterator It(NetIdPlusToUserAccountMap); It; ++It)
	{
		Result.Add(It.Value());
	}
	return Result;
}

FUniqueNetIdPtr FOnlineUserEOSPlus::GetUniquePlayerId(int32 LocalUserNum) const
{
	if (LocalUserNumToNetIdPlus.Contains(LocalUserNum))
	{
		return LocalUserNumToNetIdPlus[LocalUserNum];
	}
	return nullptr;
}

FUniqueNetIdPtr FOnlineUserEOSPlus::CreateUniquePlayerId(uint8* Bytes, int32 Size)
{
	if (Size < EOS_NETID_BYTE_SIZE)
	{
		UE_LOG_ONLINE(Error, TEXT("Invalid size (%d) passed to FOnlineUserEOSPlus::CreateUniquePlayerId()"), Size);
		return nullptr;
	}
	// We know that the first EOS_NETID_BYTE_SIZE bytes are the EOS ids, so the rest is the platform id
	FUniqueNetIdPtr EOSNetId = EOSIdentityInterface->CreateUniquePlayerId(Bytes, EOS_NETID_BYTE_SIZE);
	int32 BaseByteOffset = EOS_NETID_BYTE_SIZE;
	int32 PlatformIdSize = Size - BaseByteOffset - BASE_NETID_TYPE_SIZE;
	if (PlatformIdSize < 0)
	{
		UE_LOG_ONLINE(Error, TEXT("Invalid size (%d) passed to FOnlineUserEOSPlus::CreateUniquePlayerId()"), Size);
		return nullptr;
	}
	uint8 OSSType = *(Bytes + BaseByteOffset);
	BaseByteOffset += BASE_NETID_TYPE_SIZE;
	FName OSSName = ToOSSName((EOSSValue)OSSType);
	FName BaseOSSName = EOSPlus->BaseOSS->GetSubsystemName();
	FUniqueNetIdPtr BaseNetId = nullptr;
	if (BaseOSSName == OSSName)
	{
		BaseNetId = BaseIdentityInterface->CreateUniquePlayerId(Bytes + BaseByteOffset, PlatformIdSize);
	}
	else
	{
		// Just create the pass through version that holds the other platform data but doesn't interpret it
		BaseNetId = MakeShared<FUniqueNetIdBinary>(Bytes + BaseByteOffset, PlatformIdSize, OSSName);
	}
	
	return AddRemotePlayer(BaseNetId, EOSNetId);
}

FUniqueNetIdPtr FOnlineUserEOSPlus::CreateUniquePlayerId(const FString& Str)
{
	// Split <id>_+_<id2> into two strings
	int32 FoundAt = Str.Find(TEXT("_+_"));
	if (FoundAt == -1)
	{
		UE_LOG_ONLINE(Error, TEXT("Couldn't parse string (%s) passed to FOnlineUserEOSPlus::CreateUniquePlayerId()"), *Str);
		return nullptr;
	}

	FUniqueNetIdPtr BaseNetId = BaseIdentityInterface->CreateUniquePlayerId(Str.Left(FoundAt));
	FUniqueNetIdPtr EOSNetId = EOSIdentityInterface->CreateUniquePlayerId(Str.Right(Str.Len() - FoundAt - 3));

	return AddRemotePlayer(BaseNetId, EOSNetId);
}

ELoginStatus::Type FOnlineUserEOSPlus::GetLoginStatus(int32 LocalUserNum) const
{
	return EOSIdentityInterface->GetLoginStatus(LocalUserNum);
}

ELoginStatus::Type FOnlineUserEOSPlus::GetLoginStatus(const FUniqueNetId& UserId) const
{
	FUniqueNetIdEOSPlusPtr NetIdPlus = GetNetIdPlus(UserId.ToString());
	if (!NetIdPlus.IsValid())
	{
		UE_LOG_ONLINE(Error, TEXT("[FOnlineUserEOSPlus::GetLoginStatus] NetIdPlus not found for UserId %s"), *UserId.ToString());
		return ELoginStatus::NotLoggedIn;
	}

	return EOSIdentityInterface->GetLoginStatus(*NetIdPlus->GetEOSNetId());
}

FString FOnlineUserEOSPlus::GetPlayerNickname(int32 LocalUserNum) const
{
	return BaseIdentityInterface->GetPlayerNickname(LocalUserNum);
}

FString FOnlineUserEOSPlus::GetPlayerNickname(const FUniqueNetId& UserId) const
{
	FString Result;

	FUniqueNetIdEOSPlusPtr NetIdPlus = GetNetIdPlus(UserId.ToString());
	if (NetIdPlus.IsValid())
	{
		// Do we wrap this and map or pass through or aggregate and pass through?
		Result = BaseIdentityInterface->GetPlayerNickname(*NetIdPlus->GetBaseNetId());
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("[FOnlineUserEOSPlus::GetPlayerNickname] User not found (%s)"), *UserId.ToString());
	}

	return Result;
}

FString FOnlineUserEOSPlus::GetAuthToken(int32 LocalUserNum) const
{
	return BaseIdentityInterface->GetAuthToken(LocalUserNum);
}

void FOnlineUserEOSPlus::GetUserPrivilege(const FUniqueNetId& UserId, EUserPrivileges::Type Privilege, const FOnGetUserPrivilegeCompleteDelegate& Delegate)
{
	FUniqueNetIdEOSPlusPtr NetIdPlus = GetNetIdPlus(UserId.ToString());
	if (NetIdPlus.IsValid())
	{
		FOnGetUserPrivilegeCompleteDelegate IntermediateDelegate = FOnGetUserPrivilegeCompleteDelegate::CreateLambda([this, OriginalDelegate = FOnGetUserPrivilegeCompleteDelegate(Delegate)](const FUniqueNetId& UserId, EUserPrivileges::Type Privilege, uint32 PrivilegeResults)
		{
			FUniqueNetIdEOSPlusPtr NetIdPlus = GetNetIdPlus(UserId.ToString());
			if (!NetIdPlus.IsValid())
			{
				UE_LOG_ONLINE(Warning, TEXT("[FOnlineUserEOSPlus::GetUserPrivilege] User not found (%s)"), *UserId.ToString());
			}

			OriginalDelegate.ExecuteIfBound(*NetIdPlus, Privilege, PrivilegeResults);
		});

		BaseIdentityInterface->GetUserPrivilege(*NetIdPlus->GetBaseNetId(), Privilege, IntermediateDelegate);
	}
	else
	{
		UE_LOG_ONLINE(Error, TEXT("[FOnlineUserEOSPlus::GetUserPrivilege] NetIdPlus not found for UserId %s"), *UserId.ToString());
	}
}

FString FOnlineUserEOSPlus::GetAuthType() const
{
	return BaseIdentityInterface->GetAuthType();
}

void FOnlineUserEOSPlus::RevokeAuthToken(const FUniqueNetId& LocalUserId, const FOnRevokeAuthTokenCompleteDelegate& Delegate)
{
	BaseIdentityInterface->RevokeAuthToken(LocalUserId, Delegate);
}

FPlatformUserId FOnlineUserEOSPlus::GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& UniqueNetId) const
{
	if (BaseNetIdToNetIdPlus.Contains(UniqueNetId.ToString()))
	{
		return BaseIdentityInterface->GetPlatformUserIdFromUniqueNetId(UniqueNetId);
	}
	if (NetIdPlusToBaseNetId.Contains(UniqueNetId.ToString()))
	{
		return BaseIdentityInterface->GetPlatformUserIdFromUniqueNetId(*NetIdPlusToBaseNetId[UniqueNetId.ToString()]);
	}
	return FPlatformUserId();
}

void FOnlineUserEOSPlus::GetLinkedAccountAuthToken(int32 LocalUserNum, const FOnGetLinkedAccountAuthTokenCompleteDelegate& Delegate) const
{
	// Pass through to the platform layer
	BaseIdentityInterface->GetLinkedAccountAuthToken(LocalUserNum, Delegate);
}

void FOnlineUserEOSPlus::OnFriendsChanged()
{
	for (int32 LocalUserNum = 0; LocalUserNum < MAX_LOCAL_PLAYERS; LocalUserNum++)
	{
		TriggerOnFriendsChangeDelegates(LocalUserNum);
	}
}

void FOnlineUserEOSPlus::OnOutgoingInviteSent()
{
	for (int32 LocalUserNum = 0; LocalUserNum < MAX_LOCAL_PLAYERS; LocalUserNum++)
	{
		TriggerOnOutgoingInviteSentDelegates(LocalUserNum);
	}
}

void FOnlineUserEOSPlus::OnInviteReceived(const FUniqueNetId& UserId, const FUniqueNetId& FriendId)
{
	FUniqueNetIdEOSPlusPtr NetIdPlus = GetNetIdPlus(UserId.ToString());
	if (!NetIdPlus.IsValid())
	{
		return;
	}
	FUniqueNetIdEOSPlusPtr FriendNetIdPlus = GetNetIdPlus(FriendId.ToString());
	if (!FriendNetIdPlus.IsValid())
	{
		return;
	}
	TriggerOnInviteReceivedDelegates(*NetIdPlus, *FriendNetIdPlus);
}

void FOnlineUserEOSPlus::OnInviteAccepted(const FUniqueNetId& UserId, const FUniqueNetId& FriendId)
{
	FUniqueNetIdEOSPlusPtr NetIdPlus = GetNetIdPlus(UserId.ToString());
	if (!NetIdPlus.IsValid())
	{
		return;
	}
	FUniqueNetIdEOSPlusPtr FriendNetIdPlus = GetNetIdPlus(FriendId.ToString());
	if (!FriendNetIdPlus.IsValid())
	{
		return;
	}
	TriggerOnInviteAcceptedDelegates(UserId, FriendId);
}

void FOnlineUserEOSPlus::OnInviteRejected(const FUniqueNetId& UserId, const FUniqueNetId& FriendId)
{
	FUniqueNetIdEOSPlusPtr NetIdPlus = GetNetIdPlus(UserId.ToString());
	if (!NetIdPlus.IsValid())
	{
		return;
	}
	FUniqueNetIdEOSPlusPtr FriendNetIdPlus = GetNetIdPlus(FriendId.ToString());
	if (!FriendNetIdPlus.IsValid())
	{
		return;
	}
	TriggerOnInviteRejectedDelegates(*NetIdPlus, *FriendNetIdPlus);
}

void FOnlineUserEOSPlus::OnInviteAborted(const FUniqueNetId& UserId, const FUniqueNetId& FriendId)
{
	FUniqueNetIdEOSPlusPtr NetIdPlus = GetNetIdPlus(UserId.ToString());
	if (!NetIdPlus.IsValid())
	{
		return;
	}
	FUniqueNetIdEOSPlusPtr FriendNetIdPlus = GetNetIdPlus(FriendId.ToString());
	if (!FriendNetIdPlus.IsValid())
	{
		return;
	}
	TriggerOnInviteAbortedDelegates(*NetIdPlus, *FriendNetIdPlus);
}

void FOnlineUserEOSPlus::OnFriendRemoved(const FUniqueNetId& UserId, const FUniqueNetId& FriendId)
{
	FUniqueNetIdEOSPlusPtr NetIdPlus = GetNetIdPlus(UserId.ToString());
	if (!NetIdPlus.IsValid())
	{
		return;
	}
	FUniqueNetIdEOSPlusPtr FriendNetIdPlus = GetNetIdPlus(FriendId.ToString());
	if (!FriendNetIdPlus.IsValid())
	{
		return;
	}
	TriggerOnFriendRemovedDelegates(*NetIdPlus, *FriendNetIdPlus);
}

bool FOnlineUserEOSPlus::ReadFriendsList(int32 LocalUserNum, const FString& ListName, const FOnReadFriendsListComplete& Delegate)
{
	return BaseFriendsInterface->ReadFriendsList(LocalUserNum, ListName,
		FOnReadFriendsListComplete::CreateLambda([this, IntermediateComplete = FOnReadFriendsListComplete(Delegate)](int32 LocalUserNum, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr)
	{
		// Skip reading EAS if not in use and if we errored at the platform level
		if (!UEOSSettings::GetSettings().bUseEAS || !bWasSuccessful)
		{
			IntermediateComplete.ExecuteIfBound(LocalUserNum, bWasSuccessful, ListName, ErrorStr);
			return;
		}
		// Read the EAS version too
		EOSFriendsInterface->ReadFriendsList(LocalUserNum, ListName,
			FOnReadFriendsListComplete::CreateLambda([this, OnComplete = FOnReadFriendsListComplete(IntermediateComplete)](int32 LocalUserNum, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr)
		{
			OnComplete.ExecuteIfBound(LocalUserNum, bWasSuccessful, ListName, ErrorStr);
		}));
	}));
}

bool FOnlineUserEOSPlus::DeleteFriendsList(int32 LocalUserNum, const FString& ListName, const FOnDeleteFriendsListComplete& Delegate)
{
	return BaseFriendsInterface->DeleteFriendsList(LocalUserNum, ListName, Delegate);
}

bool FOnlineUserEOSPlus::SendInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnSendInviteComplete& Delegate)
{
	if (!NetIdPlusToBaseNetId.Contains(FriendId.ToString()))
	{
		return false;
	}
	return BaseFriendsInterface->SendInvite(LocalUserNum, *NetIdPlusToBaseNetId[FriendId.ToString()], ListName, Delegate);
}

bool FOnlineUserEOSPlus::AcceptInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnAcceptInviteComplete& Delegate)
{
	if (!NetIdPlusToBaseNetId.Contains(FriendId.ToString()))
	{
		return false;
	}
	return BaseFriendsInterface->AcceptInvite(LocalUserNum, *NetIdPlusToBaseNetId[FriendId.ToString()], ListName, Delegate);
}

bool FOnlineUserEOSPlus::RejectInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	if (!NetIdPlusToBaseNetId.Contains(FriendId.ToString()))
	{
		return false;
	}
	return BaseFriendsInterface->RejectInvite(LocalUserNum, *NetIdPlusToBaseNetId[FriendId.ToString()], ListName);
}

bool FOnlineUserEOSPlus::DeleteFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	if (!NetIdPlusToBaseNetId.Contains(FriendId.ToString()))
	{
		return false;
	}
	return BaseFriendsInterface->DeleteFriend(LocalUserNum, *NetIdPlusToBaseNetId[FriendId.ToString()], ListName);
}

TSharedRef<FOnlineFriendPlus> FOnlineUserEOSPlus::AddFriend(TSharedRef<FOnlineFriend> Friend)
{
	FUniqueNetIdEOSPlusPtr NetIdPlus = nullptr;
	FUniqueNetIdRef NetId = Friend->GetUserId();
	if (NetId->GetType() == TEXT("EOS"))
	{
		// Grab or make a NetIdPlus
		if (EOSNetIdToNetIdPlus.Contains(NetId->ToString()))
		{
			NetIdPlus = EOSNetIdToNetIdPlus[NetId->ToString()];
		}
		else
		{
			NetIdPlus = FUniqueNetIdEOSPlus::Create(nullptr, NetId);
			EOSNetIdToNetIdPlus.Add(NetId->ToString(), NetIdPlus);

			if (!NetIdPlusToNetIdPlus.Contains(NetIdPlus->ToString()))
			{
				NetIdPlusToNetIdPlus.Add(NetIdPlus->ToString(), NetIdPlus);
			}
		}
		// Build a new friend plus and map them in
		TSharedRef<FOnlineFriendPlus> FriendPlus = MakeShared<FOnlineFriendPlus>(nullptr, Friend);
		NetIdPlusToFriendMap.Add(NetIdPlus->ToString(), FriendPlus);
		return FriendPlus;
	}
	// Grab or make a NetIdPlus
	if (BaseNetIdToNetIdPlus.Contains(NetId->ToString()))
	{
		NetIdPlus = BaseNetIdToNetIdPlus[NetId->ToString()];
	}
	else
	{
		NetIdPlus = FUniqueNetIdEOSPlus::Create(NetId, nullptr);
		BaseNetIdToNetIdPlus.Add(NetId->ToString(), NetIdPlus);

		if (!NetIdPlusToNetIdPlus.Contains(NetIdPlus->ToString()))
		{
			NetIdPlusToNetIdPlus.Add(NetIdPlus->ToString(), NetIdPlus);
		}
	}
	// Build a new friend plus and map them in
	TSharedRef<FOnlineFriendPlus> FriendPlus = MakeShared<FOnlineFriendPlus>(Friend, nullptr);
	NetIdPlusToFriendMap.Add(NetIdPlus->ToString(), FriendPlus);
	return FriendPlus;
}

TSharedRef<FOnlineFriendPlus> FOnlineUserEOSPlus::GetFriend(TSharedRef<FOnlineFriend> Friend)
{
	FUniqueNetIdRef NetId = Friend->GetUserId();
	if (NetIdPlusToFriendMap.Contains(NetId->ToString()))
	{
		return NetIdPlusToFriendMap[NetId->ToString()];
	}
	return AddFriend(Friend);
}

bool FOnlineUserEOSPlus::GetFriendsList(int32 LocalUserNum, const FString& ListName, TArray<TSharedRef<FOnlineFriend>>& OutFriends)
{
	OutFriends.Reset();

	TArray<TSharedRef<FOnlineFriend>> Friends;
	bool bWasSuccessful = BaseFriendsInterface->GetFriendsList(LocalUserNum, ListName, Friends);
	// Build the list of base friends
	for (TSharedRef<FOnlineFriend> Friend : Friends)
	{
		OutFriends.Add(GetFriend(Friend));
	}

	if (UEOSSettings::GetSettings().bUseEAS)
	{
		Friends.Reset();
		bWasSuccessful |= EOSFriendsInterface->GetFriendsList(LocalUserNum, ListName, Friends);
		// Build the list of EOS friends
		for (TSharedRef<FOnlineFriend> Friend : Friends)
		{
			OutFriends.Add(GetFriend(Friend));
		}
	}
	return bWasSuccessful;
}

TSharedPtr<FOnlineFriend> FOnlineUserEOSPlus::GetFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	if (!NetIdPlusToFriendMap.Num())
	{
		TArray<TSharedRef<FOnlineFriend>> Friends;
		GetFriendsList(LocalUserNum, ListName, Friends);
	}

	if (FriendId.GetType() == TEXT("EOS"))
	{
		TSharedPtr<FOnlineFriend> Friend = EOSFriendsInterface->GetFriend(LocalUserNum, *NetIdPlusToBaseNetId[FriendId.ToString()], ListName);
		return Friend.IsValid() ? GetFriend(Friend.ToSharedRef()) : Friend;
	}

	TSharedPtr<FOnlineFriend> Friend = EOSFriendsInterface->GetFriend(LocalUserNum, *NetIdPlusToBaseNetId[FriendId.ToString()], ListName);
	if (Friend.IsValid())
	{
		return GetFriend(Friend.ToSharedRef());
	}
	return nullptr;
}

bool FOnlineUserEOSPlus::IsFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	bool bIsFriend = false;
	if (NetIdPlusToBaseNetId.Contains(FriendId.ToString()))
	{
		bIsFriend = BaseFriendsInterface->IsFriend(LocalUserNum, *NetIdPlusToBaseNetId[FriendId.ToString()], ListName);
	}
	if (!bIsFriend && UEOSSettings::GetSettings().bUseEAS && NetIdPlusToEOSNetId.Contains(FriendId.ToString()))
	{
		bIsFriend = EOSFriendsInterface->IsFriend(LocalUserNum, *NetIdPlusToEOSNetId[FriendId.ToString()], ListName);
	}
	return bIsFriend;
}

bool FOnlineUserEOSPlus::QueryRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace)
{
	if (!NetIdPlusToBaseNetId.Contains(UserId.ToString()))
	{
		return false;
	}
	return BaseFriendsInterface->QueryRecentPlayers(*NetIdPlusToBaseNetId[UserId.ToString()], Namespace);
}

TSharedRef<FOnlineRecentPlayer> FOnlineUserEOSPlus::AddRecentPlayer(TSharedRef<FOnlineRecentPlayer> Player)
{
	FUniqueNetIdEOSPlusPtr NetIdPlus = nullptr;
	FUniqueNetIdRef NetId = Player->GetUserId();
	if (NetId->GetType() == TEXT("EOS"))
	{
		// Grab or make a NetIdPlus
		if (EOSNetIdToNetIdPlus.Contains(NetId->ToString()))
		{
			NetIdPlus = EOSNetIdToNetIdPlus[NetId->ToString()];
		}
		else
		{
			NetIdPlus = FUniqueNetIdEOSPlus::Create(nullptr, NetId);
			EOSNetIdToNetIdPlus.Add(NetId->ToString(), NetIdPlus);
		}
		// Build a new recent player plus and map them in
		TSharedRef<FOnlineRecentPlayerPlus> PlayerPlus = MakeShared<FOnlineRecentPlayerPlus>(nullptr, Player);
		NetIdPlusToRecentPlayerMap.Add(NetIdPlus->ToString(), PlayerPlus);
		return PlayerPlus;
	}
	// Grab or make a NetIdPlus
	if (BaseNetIdToNetIdPlus.Contains(NetId->ToString()))
	{
		NetIdPlus = BaseNetIdToNetIdPlus[NetId->ToString()];
	}
	else
	{
		NetIdPlus = FUniqueNetIdEOSPlus::Create(NetId, nullptr);
		BaseNetIdToNetIdPlus.Add(NetId->ToString(), NetIdPlus);
	}
	// Build a new recent player plus and map them in
	TSharedRef<FOnlineRecentPlayerPlus> PlayerPlus = MakeShared<FOnlineRecentPlayerPlus>(Player, nullptr);
	NetIdPlusToRecentPlayerMap.Add(NetIdPlus->ToString(), PlayerPlus);
	return PlayerPlus;
}

TSharedRef<FOnlineRecentPlayer> FOnlineUserEOSPlus::GetRecentPlayer(TSharedRef<FOnlineRecentPlayer> Player)
{
	FUniqueNetIdRef NetId = Player->GetUserId();
	if (NetIdPlusToRecentPlayerMap.Contains(NetId->ToString()))
	{
		return NetIdPlusToRecentPlayerMap[NetId->ToString()];
	}
	return AddRecentPlayer(Player);
}

bool FOnlineUserEOSPlus::GetRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace, TArray<TSharedRef<FOnlineRecentPlayer>>& OutRecentPlayers)
{
	OutRecentPlayers.Reset();

	if (!NetIdPlusToBaseNetId.Contains(UserId.ToString()))
	{
		return false;
	}

	TArray<TSharedRef<FOnlineRecentPlayer>> Players;
	bool bWasSuccessful = BaseFriendsInterface->GetRecentPlayers(*NetIdPlusToBaseNetId[UserId.ToString()], Namespace, Players);
	for (TSharedRef<FOnlineRecentPlayer> Player : Players)
	{
		OutRecentPlayers.Add(GetRecentPlayer(Player));
	}

	return bWasSuccessful;
}

bool FOnlineUserEOSPlus::BlockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId)
{
	if (!NetIdPlusToBaseNetId.Contains(PlayerId.ToString()))
	{
		return false;
	}
	return BaseFriendsInterface->BlockPlayer(LocalUserNum, *NetIdPlusToBaseNetId[PlayerId.ToString()]);
}

bool FOnlineUserEOSPlus::UnblockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId)
{
	if (!NetIdPlusToBaseNetId.Contains(PlayerId.ToString()))
	{
		return false;
	}
	return BaseFriendsInterface->UnblockPlayer(LocalUserNum, *NetIdPlusToBaseNetId[PlayerId.ToString()]);
}

bool FOnlineUserEOSPlus::QueryBlockedPlayers(const FUniqueNetId& UserId)
{
	if (!NetIdPlusToBaseNetId.Contains(UserId.ToString()))
	{
		return false;
	}
	return BaseFriendsInterface->QueryBlockedPlayers(*NetIdPlusToBaseNetId[UserId.ToString()]);
}

TSharedRef<FOnlineBlockedPlayer> FOnlineUserEOSPlus::AddBlockedPlayer(TSharedRef<FOnlineBlockedPlayer> Player)
{
	FUniqueNetIdEOSPlusPtr NetIdPlus = nullptr;
	FUniqueNetIdRef NetId = Player->GetUserId();
	if (NetId->GetType() == TEXT("EOS"))
	{
		if (EOSNetIdToNetIdPlus.Contains(NetId->ToString()))
		{
			NetIdPlus = EOSNetIdToNetIdPlus[NetId->ToString()];
		}
		else
		{
			NetIdPlus = FUniqueNetIdEOSPlus::Create(nullptr, NetId);
			EOSNetIdToNetIdPlus.Add(NetId->ToString(), NetIdPlus);
		}
		TSharedRef<FOnlineBlockedPlayerPlus> PlayerPlus = MakeShared<FOnlineBlockedPlayerPlus>(nullptr, Player);
		NetIdPlusToBlockedPlayerMap.Add(NetIdPlus->ToString(), PlayerPlus);
		return PlayerPlus;
	}
	if (BaseNetIdToNetIdPlus.Contains(NetId->ToString()))
	{
		NetIdPlus = BaseNetIdToNetIdPlus[NetId->ToString()];
	}
	else
	{
		NetIdPlus = FUniqueNetIdEOSPlus::Create(NetId, nullptr);
		BaseNetIdToNetIdPlus.Add(NetId->ToString(), NetIdPlus);
	}
	TSharedRef<FOnlineBlockedPlayerPlus> PlayerPlus = MakeShared<FOnlineBlockedPlayerPlus>(Player, nullptr);
	NetIdPlusToBlockedPlayerMap.Add(NetIdPlus->ToString(), PlayerPlus);
	return PlayerPlus;
}

TSharedRef<FOnlineBlockedPlayer> FOnlineUserEOSPlus::GetBlockedPlayer(TSharedRef<FOnlineBlockedPlayer> Player)
{
	FUniqueNetIdRef NetId = Player->GetUserId();
	if (NetIdPlusToBlockedPlayerMap.Contains(NetId->ToString()))
	{
		return NetIdPlusToBlockedPlayerMap[NetId->ToString()];
	}
	return AddBlockedPlayer(Player);
}

bool FOnlineUserEOSPlus::GetBlockedPlayers(const FUniqueNetId& UserId, TArray< TSharedRef<FOnlineBlockedPlayer> >& OutBlockedPlayers)
{
	OutBlockedPlayers.Reset();

	if (!NetIdPlusToBaseNetId.Contains(UserId.ToString()))
	{
		return false;
	}

	TArray<TSharedRef<FOnlineBlockedPlayer>> Players;
	bool bWasSuccessful = BaseFriendsInterface->GetBlockedPlayers(*NetIdPlusToBaseNetId[UserId.ToString()], Players);
	for (TSharedRef<FOnlineBlockedPlayer> Player : Players)
	{
		OutBlockedPlayers.Add(GetBlockedPlayer(Player));
	}
	return bWasSuccessful;
}

void FOnlineUserEOSPlus::DumpBlockedPlayers() const
{
	BaseFriendsInterface->DumpBlockedPlayers();
	EOSFriendsInterface->DumpBlockedPlayers();
}

void FOnlineUserEOSPlus::SetFriendAlias(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FString& Alias, const FOnSetFriendAliasComplete& Delegate)
{
	if (NetIdPlusToBaseNetId.Contains(FriendId.ToString()))
	{
		BaseFriendsInterface->SetFriendAlias(LocalUserNum, *NetIdPlusToBaseNetId[FriendId.ToString()], ListName, Alias, Delegate);
		return;
	}
	if (NetIdPlusToEOSNetId.Contains(FriendId.ToString()))
	{
		EOSFriendsInterface->SetFriendAlias(LocalUserNum, *NetIdPlusToEOSNetId[FriendId.ToString()], ListName, Alias, Delegate);
		return;
	}
	Delegate.ExecuteIfBound(LocalUserNum, FriendId, ListName, FOnlineError(false));
}

void FOnlineUserEOSPlus::DeleteFriendAlias(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnDeleteFriendAliasComplete& Delegate)
{
	if (NetIdPlusToBaseNetId.Contains(FriendId.ToString()))
	{
		BaseFriendsInterface->DeleteFriendAlias(LocalUserNum, *NetIdPlusToBaseNetId[FriendId.ToString()], ListName, Delegate);
		return;
	}
	if (NetIdPlusToEOSNetId.Contains(FriendId.ToString()))
	{
		EOSFriendsInterface->DeleteFriendAlias(LocalUserNum, *NetIdPlusToEOSNetId[FriendId.ToString()], ListName, Delegate);
		return;
	}
	Delegate.ExecuteIfBound(LocalUserNum, FriendId, ListName, FOnlineError(false));
}

void FOnlineUserEOSPlus::DumpRecentPlayers() const
{
	BaseFriendsInterface->DumpRecentPlayers();
	EOSFriendsInterface->DumpRecentPlayers();
}

void FOnlineUserEOSPlus::OnPresenceReceived(const FUniqueNetId& UserId, const TSharedRef<FOnlineUserPresence>& Presence)
{
	if (!BaseNetIdToNetIdPlus.Contains(UserId.ToString()))
	{
		return;
	}
	TriggerOnPresenceReceivedDelegates(*BaseNetIdToNetIdPlus[UserId.ToString()], Presence);
}

void FOnlineUserEOSPlus::OnPresenceArrayUpdated(const FUniqueNetId& UserId, const TArray<TSharedRef<FOnlineUserPresence>>& NewPresenceArray)
{
	if (!BaseNetIdToNetIdPlus.Contains(UserId.ToString()))
	{
		return;
	}
	TriggerOnPresenceArrayUpdatedDelegates(*BaseNetIdToNetIdPlus[UserId.ToString()], NewPresenceArray);
}

void FOnlineUserEOSPlus::SetPresence(const FUniqueNetId& User, const FOnlineUserPresenceStatus& Status, const FOnPresenceTaskCompleteDelegate& Delegate)
{
	const FUniqueNetIdEOSPlusPtr NetIdPlus = GetNetIdPlus(User.ToString());
	if (!NetIdPlus.IsValid())
	{
		UE_LOG_ONLINE(Error, TEXT("Failed to find user (%s) in net id plus to base net id map"), *User.ToString());
		Delegate.ExecuteIfBound(User, false);
		return;
	}
	BasePresenceInterface->SetPresence(*NetIdPlus->GetBaseNetId(), Status,
		FOnPresenceTaskCompleteDelegate::CreateLambda([this, NetIdPlus, StatusCopy = FOnlineUserPresenceStatus(Status), IntermediateComplete = FOnPresenceTaskCompleteDelegate(Delegate)](const FUniqueNetId& UserId, const bool bWasSuccessful)
	{
		// Skip setting EAS presence if not mirrored or if we errored at the platform level or the EOS user isn't found
		if (!bWasSuccessful || !NetIdPlus->GetEOSNetId().IsValid() || !UEOSSettings::GetSettings().bMirrorPresenceToEAS)
		{
			IntermediateComplete.ExecuteIfBound(UserId, bWasSuccessful);
			return;
		}
		// Set the EAS version too
		EOSPresenceInterface->SetPresence(*NetIdPlus->GetEOSNetId(), StatusCopy,
			FOnPresenceTaskCompleteDelegate::CreateLambda([this, OnComplete = FOnPresenceTaskCompleteDelegate(IntermediateComplete)](const FUniqueNetId& UserId, const bool bWasSuccessful)
		{
			// The platform one is the one that matters so if we get here we succeeded earlier
			OnComplete.ExecuteIfBound(UserId, true);
		}));
	}));
}

void FOnlineUserEOSPlus::QueryPresence(const FUniqueNetId& User, const FOnPresenceTaskCompleteDelegate& Delegate)
{
	if (!NetIdPlusToBaseNetId.Contains(User.ToString()))
	{
		Delegate.ExecuteIfBound(User, false);
		return;
	}
	BasePresenceInterface->QueryPresence(*NetIdPlusToBaseNetId[User.ToString()], Delegate);
}

EOnlineCachedResult::Type FOnlineUserEOSPlus::GetCachedPresence(const FUniqueNetId& User, TSharedPtr<FOnlineUserPresence>& OutPresence)
{
	if (!NetIdPlusToBaseNetId.Contains(User.ToString()))
	{
		return EOnlineCachedResult::NotFound;
	}
	return BasePresenceInterface->GetCachedPresence(*NetIdPlusToBaseNetId[User.ToString()], OutPresence);
}

EOnlineCachedResult::Type FOnlineUserEOSPlus::GetCachedPresenceForApp(const FUniqueNetId& LocalUserId, const FUniqueNetId& User, const FString& AppId, TSharedPtr<FOnlineUserPresence>& OutPresence)
{
	if (!NetIdPlusToBaseNetId.Contains(LocalUserId.ToString()) || !NetIdPlusToBaseNetId.Contains(User.ToString()))
	{
		return EOnlineCachedResult::NotFound;
	}
	return BasePresenceInterface->GetCachedPresenceForApp(*NetIdPlusToBaseNetId[LocalUserId.ToString()], *NetIdPlusToBaseNetId[User.ToString()], AppId, OutPresence);
}
