// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/ui/settings/reauthentication_module.h"

#import <LocalAuthentication/LocalAuthentication.h>

#import "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ReauthenticationModule {
  // Authentication context on which the authentication policy is evaluated.
  LAContext* _context;

  // Accessor allowing the module to request the update of the time when the
  // successful re-authentication was performed and to get the time of the last
  // successful re-authentication.
  __weak id<SuccessfulReauthTimeAccessor> _successfulReauthTimeAccessor;
}

- (instancetype)initWithSuccessfulReauthTimeAccessor:
    (id<SuccessfulReauthTimeAccessor>)successfulReauthTimeAccessor {
  DCHECK(successfulReauthTimeAccessor);
  self = [super init];
  if (self) {
    _context = [[LAContext alloc] init];
    _successfulReauthTimeAccessor = successfulReauthTimeAccessor;
  }
  return self;
}

- (BOOL)canAttemptReauth {
  // The authentication method is Touch ID or passcode.
  return
      [_context canEvaluatePolicy:LAPolicyDeviceOwnerAuthentication error:nil];
}

- (void)attemptReauthWithLocalizedReason:(NSString*)localizedReason
                                 handler:(void (^)(BOOL success))handler {
  if ([self isPreviousAuthValid]) {
    handler(YES);
    UMA_HISTOGRAM_ENUMERATION(
        "PasswordManager.ReauthToAccessPasswordInSettings",
        password_manager::metrics_util::REAUTH_SKIPPED,
        password_manager::metrics_util::REAUTH_COUNT);
    return;
  }

  _context = [[LAContext alloc] init];

  // No fallback option is provided.
  _context.localizedFallbackTitle = @"";

  __weak ReauthenticationModule* weakSelf = self;
  void (^replyBlock)(BOOL, NSError*) = ^(BOOL success, NSError* error) {
    dispatch_async(dispatch_get_main_queue(), ^{
      if (error) {
        // TODO(crbug.com/760235): remove once it's fixed.
        LOG(ERROR) << "Reauth error: "
                   << base::SysNSStringToUTF8(error.localizedDescription);
      }
      ReauthenticationModule* strongSelf = weakSelf;
      if (!strongSelf)
        return;
      if (success) {
        [strongSelf->_successfulReauthTimeAccessor updateSuccessfulReauthTime];
      }
      handler(success);
      UMA_HISTOGRAM_ENUMERATION(
          "PasswordManager.ReauthToAccessPasswordInSettings",
          success ? password_manager::metrics_util::REAUTH_SUCCESS
                  : password_manager::metrics_util::REAUTH_FAILURE,
          password_manager::metrics_util::REAUTH_COUNT);
    });
  };

  [_context evaluatePolicy:LAPolicyDeviceOwnerAuthentication
           localizedReason:localizedReason
                     reply:replyBlock];
}

- (BOOL)isPreviousAuthValid {
  BOOL previousAuthValid = NO;
  const int kIntervalForValidAuthInSeconds = 60;
  NSDate* lastSuccessfulReauthTime =
      [_successfulReauthTimeAccessor lastSuccessfulReauthTime];
  if (lastSuccessfulReauthTime) {
    NSDate* currentTime = [NSDate date];
    NSTimeInterval timeSincePreviousSuccessfulAuth =
        [currentTime timeIntervalSinceDate:lastSuccessfulReauthTime];
    if (timeSincePreviousSuccessfulAuth < kIntervalForValidAuthInSeconds) {
      previousAuthValid = YES;
    }
  }
  return previousAuthValid;
}

@end
