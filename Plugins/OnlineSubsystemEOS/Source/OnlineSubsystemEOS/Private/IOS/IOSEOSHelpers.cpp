// Copyright Epic Games, Inc. All Rights Reserved.

#include "IOSEOSHelpers.h"

#if WITH_EOS_SDK

#include "OnlineSubsystemEOS.h"
#include "UserManagerEOS.h"

#if (defined(__IPHONE_13_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_13_0)
	#define PRESENTATIONCONTEXT_REQUIRED 1
#else
	#define PRESENTATIONCONTEXT_REQUIRED 0
#endif

#if PRESENTATIONCONTEXT_REQUIRED
#import <Foundation/Foundation.h>
#import <AuthenticationServices/AuthenticationServices.h>
#include "IOS/IOSAppDelegate.h"
#endif

#if PRESENTATIONCONTEXT_REQUIRED
@interface PresentationContext : NSObject <ASWebAuthenticationPresentationContextProviding>
{
}
@end


@implementation PresentationContext

- (ASPresentationAnchor)presentationAnchorForWebAuthenticationSession:(ASWebAuthenticationSession *)session
{
	if ([IOSAppDelegate GetDelegate].Window == nullptr)
	{
		NSLog(@"authorizationController: presentationAnchorForAuthorizationController: error window is NULL");
	}
	return [IOSAppDelegate GetDelegate].Window;
}

@end

static PresentationContext* PresentationContextProvider = nil;
#endif

FIOSEOSHelpers::~FIOSEOSHelpers()
{
#if PRESENTATIONCONTEXT_REQUIRED
	if (PresentationContextProvider != nil)
	{
		[PresentationContextProvider release];
		PresentationContextProvider = nil;
	}
#endif
}

FString FIOSEOSHelpers::PlatformCreateCacheDir(const FString &ArtifactName, const FString &EOSSettingsCacheDir)
{
	NSString *BundleIdentifier = [[NSBundle mainBundle] bundleIdentifier];
	NSString *CacheDirectory = NSTemporaryDirectory();									// Potentially use NSCachesDirectory
	CacheDirectory = [CacheDirectory stringByAppendingPathComponent:BundleIdentifier];

	FTCHARToUTF8 TCArtifactName(*ArtifactName);
	NSString *NSArtifactName = [NSString stringWithUTF8String:TCArtifactName.Get()];
	CacheDirectory = [CacheDirectory stringByAppendingPathComponent:NSArtifactName];

	FTCHARToUTF8 TCEOSSettingsCacheDir(*EOSSettingsCacheDir);
	NSString *NSEOSSettingsCacheDir = [NSString stringWithUTF8String:TCArtifactName.Get()];
	CacheDirectory = [CacheDirectory stringByAppendingPathComponent:NSEOSSettingsCacheDir];

	const char *CStrCacheDirectory = [CacheDirectory UTF8String];
	return FString(UTF8_TO_TCHAR(CStrCacheDirectory));
}

void FIOSEOSHelpers::PlatformAuthCredentials(EOS_Auth_Credentials &Credentials)
{
#if PRESENTATIONCONTEXT_REQUIRED
	if (PresentationContextProvider == nil)
	{
		PresentationContextProvider = [PresentationContext new];
	}
#endif

	CredentialsOptions.ApiVersion = EOS_IOS_AUTH_CREDENTIALSOPTIONS_API_LATEST;
#if PRESENTATIONCONTEXT_REQUIRED
	CredentialsOptions.PresentationContextProviding = (void*)CFBridgingRetain(PresentationContextProvider);		// SDK will release when consumed
#else
	CredentialsOptions.PresentationContextProviding = nullptr;
#endif

	Credentials.SystemAuthCredentialsOptions = (void*)&CredentialsOptions;
}

void FIOSEOSHelpers::PlatformTriggerLoginUI(FOnlineSubsystemEOS* InEOSSubsystem, const int ControllerIndex, bool bShowOnlineOnly, bool bShowSkipButton, const FOnLoginUIClosedDelegate& Delegate)
{
	EOSSubsystem = InEOSSubsystem;
	check(EOSSubsystem != nullptr);

	LoginUIClosedDelegate = Delegate;
	LoginCompleteDelegate = EOSSubsystem->UserManager->AddOnLoginCompleteDelegate_Handle(ControllerIndex, FOnLoginCompleteDelegate::CreateRaw(this, &FIOSEOSHelpers::OnLoginComplete));

	FOnlineAccountCredentials *Credentials = new FOnlineAccountCredentials(TEXT("accountportal"), TEXT(""), TEXT(""));
	EOSSubsystem->UserManager->Login(ControllerIndex, *Credentials);
}

void FIOSEOSHelpers::OnLoginComplete(int ControllerIndex, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& ErrorString)
{
	FOnlineError Error(bWasSuccessful);
	Error.SetFromErrorCode(ErrorString);

	check(EOSSubsystem != nullptr);

	EOSSubsystem->UserManager->ClearOnLoginCompleteDelegate_Handle(ControllerIndex, LoginCompleteDelegate);

	LoginUIClosedDelegate.ExecuteIfBound(UserId, ControllerIndex, Error);
}

#endif

